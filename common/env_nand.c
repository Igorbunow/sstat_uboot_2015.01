/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2008
 * Stuart Wood, Lab X Technologies <stuart.wood@labxtechnologies.com>
 *
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <nand.h>
#include <search.h>
#include <errno.h>


#ifdef CONFIG_MSTAR_ENV_NAND_OFFSET
    #ifdef CONFIG_MS_SPINAND
        #include "drvSPINAND.h"
        #include "spinand.h"
        extern int MDrv_SPINAND_GetPartOffset(U16 u16_PartType, U32* u32_Offset, U8 u8_backup);
    #endif
    #ifdef CONFIG_MS_NAND
        #include "drvNAND.h"
        extern U32 drvNAND_GetPartOffset(U16 u16_PartType, U32* u32_Offset);
    #endif
#endif

#if defined(CONFIG_CMD_SAVEENV) && defined(CONFIG_CMD_NAND)
#define CMD_SAVEENV
#elif defined(CONFIG_ENV_OFFSET_REDUND)
#error CONFIG_ENV_OFFSET_REDUND must have CONFIG_CMD_SAVEENV & CONFIG_CMD_NAND
#endif

#if defined(CONFIG_ENV_SIZE_REDUND) &&	\
	(CONFIG_ENV_SIZE_REDUND != CONFIG_ENV_SIZE)
#error CONFIG_ENV_SIZE_REDUND should be the same as CONFIG_ENV_SIZE
#endif

#ifndef CONFIG_ENV_RANGE
#define CONFIG_ENV_RANGE	CONFIG_ENV_SIZE
#endif

#if (defined(CONFIG_MS_NAND) && defined(CONFIG_MS_EMMC))
char *nand_env_name_spec = "NAND";
#else
char *env_name_spec = "NAND";
#endif

#if defined(CONFIG_MS_NAND) || defined(CONFIG_MS_SPINAND)
U32 ms_nand_env_offset = 0;
#ifdef CONFIG_ENV_OFFSET_REDUND
U32 ms_nand_env_redund_offset = 0;
#endif
#endif

#if defined(ENV_IS_EMBEDDED)
env_t *env_ptr = &environment;
#elif defined(CONFIG_NAND_ENV_DST)
env_t *env_ptr = (env_t *)CONFIG_NAND_ENV_DST;
#else /* ! ENV_IS_EMBEDDED */
#if (defined(CONFIG_MS_NAND) && defined(CONFIG_MS_EMMC))
extern env_t *env_ptr;
#else
env_t *env_ptr;
#endif
#endif /* ENV_IS_EMBEDDED */

DECLARE_GLOBAL_DATA_PTR;

/*
 * This is called before nand_init() so we can't read NAND to
 * validate env data.
 *
 * Mark it OK for now. env_relocate() in env_common.c will call our
 * relocate function which does the real validation.
 *
 * When using a NAND boot image (like sequoia_nand), the environment
 * can be embedded or attached to the U-Boot image in NAND flash.
 * This way the SPL loads not only the U-Boot image from NAND but
 * also the environment.
 */
#if (defined(CONFIG_MS_NAND) && defined(CONFIG_MS_EMMC))
int nand_env_init(void)
#else
int env_init(void)
#endif
{
#if defined(ENV_IS_EMBEDDED) || defined(CONFIG_NAND_ENV_DST)
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1;

#ifdef CONFIG_ENV_OFFSET_REDUND
	env_t *tmp_env2;

	tmp_env2 = (env_t *)((ulong)env_ptr + CONFIG_ENV_SIZE);
	crc2_ok = crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc;
#endif
	tmp_env1 = env_ptr;
	crc1_ok = crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc;

	if (!crc1_ok && !crc2_ok) {
		gd->env_addr	= 0;
		gd->env_valid	= 0;

		return 0;
	} else if (crc1_ok && !crc2_ok) {
		gd->env_valid = 1;
	}
#ifdef CONFIG_ENV_OFFSET_REDUND
	else if (!crc1_ok && crc2_ok) {
		gd->env_valid = 2;
	} else {
		/* both ok - check serial */
		if (tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if (tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if (tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if (tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;
	}

	if (gd->env_valid == 2)
		env_ptr = tmp_env2;
	else
#endif
	if (gd->env_valid == 1)
		env_ptr = tmp_env1;

	gd->env_addr = (ulong)env_ptr->data;

#else /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */
	gd->env_addr	= (ulong)&default_environment[0];
	gd->env_valid	= 1;
#endif /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */

	return 0;
}

#ifdef CMD_SAVEENV
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
static int writeenv(size_t offset, u_char *buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t amount_saved = 0;
	size_t blocksize, len;
	u_char *char_ptr;

	blocksize = nand_info[0].erasesize;
	len = min(blocksize, (size_t)CONFIG_ENV_SIZE);

	while (amount_saved < CONFIG_ENV_SIZE && offset < end) {
		if (nand_block_isbad(&nand_info[0], offset)) {
			offset += blocksize;
		} else {
			char_ptr = &buf[amount_saved];
			if (nand_write(&nand_info[0], offset, &len, char_ptr))
				return 1;

			offset += blocksize;
			amount_saved += len;
		}
	}
	if (amount_saved != CONFIG_ENV_SIZE)
		return 1;

	return 0;
}

struct env_location {
	const char *name;
	nand_erase_options_t erase_opts;
};

static int erase_and_write_env(const struct env_location *location,
		u_char *env_new)
{
	int ret = 0;

	printf("Erasing %s...\n", location->name);
	if (nand_erase_opts(&nand_info[0], &location->erase_opts))
		return 1;

	printf("Writing to %s... ", location->name);
	ret = writeenv(location->erase_opts.offset, env_new);
	puts(ret ? "FAILED!\n" : "OK\n");

	return ret;
}

#ifdef CONFIG_ENV_OFFSET_REDUND
static unsigned char env_flags;
#endif

#if (defined(CONFIG_MS_NAND) && defined(CONFIG_MS_EMMC))
int nand_saveenv(void)
#else
int saveenv(void)
#endif
{
	int	ret = 0;
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);
	int	env_idx = 0;
	static struct env_location location[] = {
		{
			.name = "NAND",
#ifndef	CONFIG_MSTAR_ENV_NAND_OFFSET
			.erase_opts = {
				.length = CONFIG_ENV_RANGE,
				.offset = CONFIG_ENV_OFFSET,

			},
#endif
		},
#ifdef CONFIG_ENV_OFFSET_REDUND
		{
			.name = "redundant NAND",
#ifndef	CONFIG_MSTAR_ENV_NAND_REDUND_OFFSET
			.erase_opts = {
				.length = CONFIG_ENV_RANGE,
				.offset = CONFIG_ENV_OFFSET_REDUND,
			},
#endif
		},
#endif
	};

#ifdef CONFIG_MSTAR_ENV_NAND_OFFSET
#ifdef CONFIG_MS_SPINAND
    if(MDrv_SPINAND_GetPartOffset(UNFD_PART_ENV, &ms_nand_env_offset, 0) != 0)
#endif
#ifdef CONFIG_MS_NAND
    if(drvNAND_GetPartOffset(UNFD_PART_ENV,&ms_nand_env_offset)!=UNFD_ST_SUCCESS)
#endif
    {
        printf("ERROR!! get NAND CONFIG_ENV_OFFSET failed!!\n");
        ms_nand_env_offset = 0;
        return -1;
    }
    memset(&location[0].erase_opts, 0 ,sizeof(struct nand_erase_options));
    location[0].erase_opts.length=CONFIG_ENV_RANGE;
    location[0].erase_opts.offset=CONFIG_MSTAR_ENV_NAND_OFFSET;
#endif
#ifdef CONFIG_MSTAR_ENV_NAND_REDUND_OFFSET
#ifdef CONFIG_MS_SPINAND
    if(MDrv_SPINAND_GetPartOffset(UNFD_PART_ENV, &ms_nand_env_redund_offset, 1) != 0)
    {
        printf("ERROR!! get NAND CONFIG_ENV_REDUND_OFFSET failed!!\n");
        ms_nand_env_redund_offset = 0;
        return -1;
    }
#endif
    memset(&location[1].erase_opts, 0 ,sizeof(struct nand_erase_options));
    location[1].erase_opts.length=CONFIG_ENV_RANGE;
    location[1].erase_opts.offset=CONFIG_MSTAR_ENV_NAND_REDUND_OFFSET;
#endif

	if (CONFIG_ENV_RANGE < CONFIG_ENV_SIZE)
		return 1;

	ret = env_export(env_new);
	if (ret)
		return ret;

#ifdef CONFIG_ENV_OFFSET_REDUND
	env_new->flags = ++env_flags; /* increase the serial */
	env_idx = (gd->env_valid == 1);
#endif

	ret = erase_and_write_env(&location[env_idx], (u_char *)env_new);
#ifdef CONFIG_ENV_OFFSET_REDUND
	if (!ret) {
		/* preset other copy for next write */
		gd->env_valid = gd->env_valid == 2 ? 1 : 2;
		return ret;
	}

	env_idx = (env_idx + 1) & 1;
	ret = erase_and_write_env(&location[env_idx], (u_char *)env_new);
	if (!ret)
		printf("Warning: primary env write failed,"
				" redundancy is lost!\n");
#endif

	return ret;
}
#endif /* CMD_SAVEENV */

static int readenv(size_t offset, u_char *buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t amount_loaded = 0;
	size_t blocksize, len;
	u_char *char_ptr;
	blocksize = nand_info[0].erasesize;
	if (!blocksize)
		return 1;

	len = min(blocksize, (size_t)CONFIG_ENV_SIZE);

	while (amount_loaded < CONFIG_ENV_SIZE && offset < end) {
		if (nand_block_isbad(&nand_info[0], offset)) {
			offset += blocksize;
		} else {
			char_ptr = &buf[amount_loaded];
			if (nand_read_skip_bad(&nand_info[0], offset,
					       &len, NULL,
					       nand_info[0].size, char_ptr))
				return 1;

			offset += blocksize;
			amount_loaded += len;
		}
	}

	if (amount_loaded != CONFIG_ENV_SIZE)
		return 1;

	return 0;
}

#ifdef CONFIG_ENV_OFFSET_OOB
int get_nand_env_oob(nand_info_t *nand, unsigned long *result)
{
#if defined(CONFIG_MSTAR_ENV_OFFSET)
	U32 offset;
	extern U32 drvNAND_GetPartOffset(U16 u16_PartType, U32* u32_Offset);
	if(drvNAND_GetPartOffset(UNFD_PART_ENV,&offset)==UNFD_ST_SUCCESS)
	{
		*result=offset;
	}
#else
	struct mtd_oob_ops ops;
	uint32_t oob_buf[ENV_OFFSET_SIZE / sizeof(uint32_t)];
	int ret;

	ops.datbuf	= NULL;
	ops.mode	= MTD_OOB_AUTO;
	ops.ooboffs	= 0;
	ops.ooblen	= ENV_OFFSET_SIZE;
	ops.oobbuf	= (void *)oob_buf;

	ret = nand->read_oob(nand, ENV_OFFSET_SIZE, &ops);
	if (ret) {
		printf("error reading OOB block 0\n");
		return ret;
	}

	if (oob_buf[0] == ENV_OOB_MARKER) {
		*result = oob_buf[1] * nand->erasesize;
	} else if (oob_buf[0] == ENV_OOB_MARKER_OLD) {
		*result = oob_buf[1];
	} else {
		printf("No dynamic environment marker in OOB block 0\n");
		return -ENOENT;
	}
#endif
	return 0;
}
#endif

#ifdef CONFIG_ENV_OFFSET_REDUND
#if (defined(CONFIG_MS_NAND) && defined(CONFIG_MS_EMMC) && defined(CONFIG_MS_SPINAND))
void nand_env_relocate_spec(void)
#else
void env_relocate_spec(void)
#endif
{
#if !defined(ENV_IS_EMBEDDED)
	int read1_fail = 0, read2_fail = 0;
	int crc1_ok = 0, crc2_ok = 0;
	env_t *ep, *tmp_env1, *tmp_env2;

	tmp_env1 = (env_t *)malloc(CONFIG_ENV_SIZE);
	tmp_env2 = (env_t *)malloc(CONFIG_ENV_SIZE);
	if (tmp_env1 == NULL || tmp_env2 == NULL) {
		puts("Can't allocate buffers for environment\n");
		set_default_env("!malloc() failed");
		goto done;
	}

#ifdef CONFIG_MSTAR_ENV_NAND_OFFSET
#ifdef CONFIG_MS_SPINAND
    if(MDrv_SPINAND_GetPartOffset(UNFD_PART_ENV, &ms_nand_env_offset, 0) != 0)
    {
        printf("ERROR!! get NAND CONFIG_ENV_OFFSET failed!!\n");
        ms_nand_env_offset = 0;
        return;
    }
#ifdef CONFIG_MSTAR_ENV_NAND_REDUND_OFFSET
    if(MDrv_SPINAND_GetPartOffset(UNFD_PART_ENV, &ms_nand_env_redund_offset, 1) != 0)
    {
        printf("ERROR!! get NAND CONFIG_ENV_REDUND_OFFSET failed!!\n");
        ms_nand_env_redund_offset = 0;
        return;
    }
#endif
#endif
#ifdef CONFIG_MS_NAND
    if(drvNAND_GetPartOffset(UNFD_PART_ENV,&ms_nand_env_offset)!=UNFD_ST_SUCCESS)
    {
        printf("ERROR!! get NAND CONFIG_ENV_OFFSET failed!!\n");
        ms_nand_env_offset = 0;
        return;
    }
#endif
#endif

	read1_fail = readenv(CONFIG_ENV_OFFSET, (u_char *) tmp_env1);
	read2_fail = readenv(CONFIG_ENV_OFFSET_REDUND, (u_char *) tmp_env2);

	if (read1_fail && read2_fail)
		puts("*** Error - No Valid Environment Area found\n");
	else if (read1_fail || read2_fail)
		puts("*** Warning - some problems detected "
		     "reading environment; recovered successfully\n");

	crc1_ok = !read1_fail &&
		(crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc);
	crc2_ok = !read2_fail &&
		(crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc);

	if (!crc1_ok && !crc2_ok) {
	#if 1
		set_default_env("!bad CRC");
		goto done;
	#else
		gd->env_valid = 1;	
	#endif
	} else if (crc1_ok && !crc2_ok) {
		gd->env_valid = 1;
	} else if (!crc1_ok && crc2_ok) {
		gd->env_valid = 2;
	} else {
		/* both ok - check serial */
		if (tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = 2;
		else if (tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = 1;
		else if (tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = 1;
		else if (tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = 2;
		else /* flags are equal - almost impossible */
			gd->env_valid = 1;
	}

	free(env_ptr);

	if (gd->env_valid == 1)
		ep = tmp_env1;
	else
		ep = tmp_env2;

	env_flags = ep->flags;
	env_import((char *)ep, 0);

done:
	free(tmp_env1);
	free(tmp_env2);

#endif /* ! ENV_IS_EMBEDDED */
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
/*
 * The legacy NAND code saved the environment in the first NAND
 * device i.e., nand_dev_desc + 0. This is also the behaviour using
 * the new NAND code.
 */
#if (defined(CONFIG_MS_NAND) && defined(CONFIG_MS_EMMC) && defined(CONFIG_MS_SPINAND))
void nand_env_relocate_spec(void)
#else
void env_relocate_spec(void)
#endif
{
#if !defined(ENV_IS_EMBEDDED)
	int ret;
	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);

#if defined(CONFIG_ENV_OFFSET_OOB)
	ret = get_nand_env_oob(&nand_info[0], &nand_env_oob_offset);
	/*
	 * If unable to read environment offset from NAND OOB then fall through
	 * to the normal environment reading code below
	 */
	if (!ret) {
		printf("Found Environment offset in OOB..\n");
	} else {
		set_default_env("!no env offset in OOB");
		return;
	}
#endif

#ifdef CONFIG_MS_SPINAND
    if(MDrv_SPINAND_GetPartOffset(UNFD_PART_ENV, &ms_nand_env_offset, 0) != 0)
#endif
#ifdef CONFIG_MS_NAND
	if(drvNAND_GetPartOffset(UNFD_PART_ENV,&ms_nand_env_offset)!=UNFD_ST_SUCCESS)
#endif
    {
        printf("ERROR!! get NAND CONFIG_ENV_OFFSET failed!!\n");
        set_default_env("!readenv() failed");
		ms_nand_env_offset = 0;
		return;
    }

	ret = readenv(CONFIG_MSTAR_ENV_NAND_OFFSET, (u_char *)buf);
	if (ret) {
		set_default_env("!readenv() failed");
		ms_nand_env_offset = 0;
		return;
	}



#ifdef ENV_SAVE_DEFAULT
    ret = env_import(buf, 1);

    if (!ret) // If env_import fail
    {
        saveenv();
    }
#else
    env_import(buf, 1);
#endif

#endif /* ! ENV_IS_EMBEDDED */
}
#endif /* CONFIG_ENV_OFFSET_REDUND */

/*
 * Copyright 2008 Freescale Semiconductor, Inc.
 *
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/fsl_law.h>
#include <asm/mmu.h>

/*
 * LAW(Local Access Window) configuration:
 *
 * 0x0000_0000     0x7fff_ffff     DDR                     2G
 * 0x8000_0000     0x9fff_ffff     PCI1 MEM                512M
 * 0xa000_0000     0xbfff_ffff     PCI2 MEM                512M
 * 0xe000_0000     0xe000_ffff     CCSR                    1M
 * 0xe200_0000     0xe2ff_ffff     PCI1 IO                 16M
 * 0xe300_0000     0xe3ff_ffff     PCI2 IO                 16M
 * 0xf000_0000     0xfaff_ffff     Local bus               128M
 * 0xfb00_0000     0xfb00_ffff     Config Latch            64K
 * 0xfc00_0000     0xffff_ffff     FLASH (boot bank)       64M
 *
 * Notes:
 *    CCSRBAR and L2-as-SRAM don't need a configured Local Access Window.
 *    If flash is 8M at default position (last 8M), no LAW needed.
 */

struct law_entry law_table[] = {
#ifndef CONFIG_SPD_EEPROM
	SET_LAW(CONFIG_SYS_DDR_SDRAM_BASE, LAW_SIZE_128M, LAW_TRGT_IF_DDR),
#endif
	SET_LAW(CONFIG_SYS_PCI1_MEM_PHYS, LAW_SIZE_512M, LAW_TRGT_IF_PCI_1),
	SET_LAW(CONFIG_SYS_PCI2_MEM_PHYS, LAW_SIZE_512M, LAW_TRGT_IF_PCI_2),
	SET_LAW(CONFIG_SYS_PCI1_IO_PHYS, LAW_SIZE_16M, LAW_TRGT_IF_PCI_1),
	SET_LAW(CONFIG_SYS_PCI2_IO_PHYS, LAW_SIZE_16M, LAW_TRGT_IF_PCI_2),
	/* Map the whole localbus, including flash and reset latch. */
	SET_LAW(CONFIG_SYS_LBC_OPTION_BASE, LAW_SIZE_256M, LAW_TRGT_IF_LBC),
};

int num_law_entries = ARRAY_SIZE(law_table);

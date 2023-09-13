/*
 * RealTek PHY drivers
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * author Andy Fleming
 */
#include <config.h>
#include <common.h>
#include <phy.h>

#define PHY_AUTONEGOTIATE_TIMEOUT 5000

/* RTL8211x PHY Status Register */
#define MIIM_RTL8211x_PHY_STATUS       0x11
#define MIIM_RTL8211x_PHYSTAT_SPEED    0xc000
#define MIIM_RTL8211x_PHYSTAT_GBIT     0x8000
#define MIIM_RTL8211x_PHYSTAT_100      0x4000
#define MIIM_RTL8211x_PHYSTAT_DUPLEX   0x2000
#define MIIM_RTL8211x_PHYSTAT_SPDDONE  0x0800
#define MIIM_RTL8211x_PHYSTAT_LINK     0x0400


/* RTL8201F PHY  Register */
/* Interrupt Indicators and SNR Display Register */
#define RTL8201F_INSR		0x1e
/* Page Select Register */
#define RTL8201F_PGSR		0x1f
/* Interrupt, WOL Enable, and LEDs Function Registers */
#define RTL8201F_INER		0x13
#define RTL8201F_INER_MASK	0x3800
#define RTL8201F_INER_LED_MASK	0x0030
#define RTL8201F_INER_LED_VAL	0x0000
/* Fiber Mode and Loopback Register */
#define RTL8201F_FMLR		0x1c
#define RTL8201F_FMLR_INIT	0x04
#define RTL8201F_FMLR_MASK	0x0026


/* RealTek RTL8201F */
static void rtl8201f_select_page(struct phy_device *phydev, int page)
{
	phy_write(phydev, MDIO_DEVAD_NONE, RTL8201F_PGSR, page);
}

static int rtl8201f_config(struct phy_device *phydev)
{
	int err;
	u16 reg_val;
	
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, BMCR_RESET);

	genphy_config_aneg(phydev);
	
	rtl8201f_select_page(phydev, 7);
	
	reg_val = phy_read(phydev, MDIO_DEVAD_NONE, RTL8201F_INER);
	
	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, MDIO_DEVAD_NONE, RTL8201F_INER, RTL8201F_INER_MASK |
				reg_val);
	else
		err = phy_write(phydev, MDIO_DEVAD_NONE, RTL8201F_INER, ~RTL8201F_INER_MASK &
				reg_val);

	rtl8201f_select_page(phydev, 0);
	
}


/* RealTek RTL8211x */
static int rtl8211x_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, BMCR_RESET);

	genphy_config_aneg(phydev);

	return 0;
}

static int rtl8211x_parse_status(struct phy_device *phydev)
{
	unsigned int speed;
	unsigned int mii_reg;

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_RTL8211x_PHY_STATUS);

	if (!(mii_reg & MIIM_RTL8211x_PHYSTAT_SPDDONE)) {
		int i = 0;

		/* in case of timeout ->link is cleared */
		phydev->link = 1;
		puts("Waiting for PHY realtime link");
		while (!(mii_reg & MIIM_RTL8211x_PHYSTAT_SPDDONE)) {
			/* Timeout reached ? */
			if (i > PHY_AUTONEGOTIATE_TIMEOUT) {
				puts(" TIMEOUT !\n");
				phydev->link = 0;
				break;
			}

			if ((i++ % 1000) == 0)
				putc('.');
			udelay(1000);	/* 1 ms */
			mii_reg = phy_read(phydev, MDIO_DEVAD_NONE,
					MIIM_RTL8211x_PHY_STATUS);
		}
		puts(" done\n");
		udelay(500000);	/* another 500 ms (results in faster booting) */
	} else {
		if (mii_reg & MIIM_RTL8211x_PHYSTAT_LINK)
			phydev->link = 1;
		else
			phydev->link = 0;
	}

	if (mii_reg & MIIM_RTL8211x_PHYSTAT_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	speed = (mii_reg & MIIM_RTL8211x_PHYSTAT_SPEED);

	switch (speed) {
	case MIIM_RTL8211x_PHYSTAT_GBIT:
		phydev->speed = SPEED_1000;
		break;
	case MIIM_RTL8211x_PHYSTAT_100:
		phydev->speed = SPEED_100;
		break;
	default:
		phydev->speed = SPEED_10;
	}

	return 0;
}

static int rtl8211x_startup(struct phy_device *phydev)
{
	/* Read the Status (2x to make sure link is right) */
	genphy_update_link(phydev);
	rtl8211x_parse_status(phydev);

	return 0;
}

/* Support for RTL8211B PHY */
static struct phy_driver RTL8211B_driver = {
	.name = "RealTek RTL8211B",
	.uid = 0x1cc910,
	.mask = 0xffffff,
	.features = PHY_GBIT_FEATURES,
	.config = &rtl8211x_config,
	.startup = &rtl8211x_startup,
	.shutdown = &genphy_shutdown,
};

/* Support for RTL8211E-VB-CG, RTL8211E-VL-CG and RTL8211EG-VB-CG PHYs */
static struct phy_driver RTL8211E_driver = {
	.name = "RealTek RTL8211E",
	.uid = 0x1cc915,
	.mask = 0xffffff,
	.features = PHY_GBIT_FEATURES,
	.config = &rtl8211x_config,
	.startup = &rtl8211x_startup,
	.shutdown = &genphy_shutdown,
};

/* Support for RTL8211DN PHY */
static struct phy_driver RTL8211DN_driver = {
	.name = "RealTek RTL8211DN",
	.uid = 0x1cc914,
	.mask = 0xffffff,
	.features = PHY_GBIT_FEATURES,
	.config = &rtl8211x_config,
	.startup = &rtl8211x_startup,
	.shutdown = &genphy_shutdown,
};

/* Support for RTL8201F PHY */
static struct phy_driver RTL8201F_driver = {
	.name = "RealTek RTL8201F",
	.uid = 0x1cc816,
	.mask = 0xffffff,
	.features = PHY_BASIC_FEATURES,
	.config = &rtl8201f_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

int phy_realtek_init(void)
{
	phy_register(&RTL8211B_driver);
	phy_register(&RTL8211E_driver);
	phy_register(&RTL8211DN_driver);
	phy_register(&RTL8201F_driver);

	return 0;
}

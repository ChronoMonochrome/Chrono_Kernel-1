/*
 * Copyright (C) 2011 ST-Ericsson
 *
 * Author: Pierre Peiffer <pierre.peiffer@stericsson.com> for ST-Ericsson.
 * Author: Olivier Germain <olivier.germain@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/nomadik.h>
#include <plat/pincfg.h>
#include <mach/devices.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <asm/io.h>
#include <trace/stm.h>
#include "pins-db8500.h"

static pin_cfg_t mop500_stm_mipi34_pins[] = {
	GPIO70_STMAPE_CLK | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
	GPIO71_STMAPE_DAT3 | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
	GPIO72_STMAPE_DAT2 | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
	GPIO73_STMAPE_DAT1 | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
	GPIO74_STMAPE_DAT0 | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
	GPIO75_U2_RXD | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
	GPIO76_U2_TXD | PIN_SLPM_USE_MUX_SETTINGS_IN_SLEEP,
};

static pin_cfg_t mop500_stm_mipi60_pins[] = {
	GPIO153_U2_RXD,
	GPIO154_U2_TXD,
	GPIO155_STMAPE_CLK,
	GPIO156_STMAPE_DAT3,
	GPIO157_STMAPE_DAT2,
	GPIO158_STMAPE_DAT1,
	GPIO159_STMAPE_DAT0,
};

static pin_cfg_t mop500_ske_pins[] = {
	GPIO153_KP_I7 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO154_KP_I6 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO155_KP_I5 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO156_KP_I4 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO161_KP_I3 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO162_KP_I2 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO163_KP_I1 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO164_KP_I0 | PIN_INPUT_PULLDOWN | PIN_SLPM_INPUT_PULLUP,
	GPIO157_KP_O7 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO158_KP_O6 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO159_KP_O5 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO160_KP_O4 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO165_KP_O3 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO166_KP_O2 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO167_KP_O1 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
	GPIO168_KP_O0 | PIN_INPUT_PULLUP | PIN_SLPM_OUTPUT_LOW,
};

static int stm_ste_disable_ape_on_mipi60(void)
{
	int retval;

	retval = nmk_config_pins_sleep(ARRAY_AND_SIZE(mop500_stm_mipi60_pins));
	if (retval)
		pr_err("STM: Failed to disable MIPI60\n");
	else {
		retval = nmk_config_pins(ARRAY_AND_SIZE(mop500_ske_pins));
		if (retval)
			pr_err("STM: Failed to enable SKE gpio\n");
	}
	return retval;
}

/*
 * Manage STM output pins connection (MIP34/MIPI60 connectors)
 */
#define PRCM_GPIOCR	(_PRCMU_BASE + 0x138)
#define PRCM_GPIOCR_DBG_STM_MOD_CMD1 0x800
#define PRCM_GPIOCR_DBG_UARTMOD_CMD0 0x1


static int stm_ste_connection(enum stm_connection_type con_type)
{
	int retval = -EINVAL;
	u32 gpiocr = readl(PRCM_GPIOCR);

	if (con_type != STM_DISCONNECT) {
		/*  Always enable MIPI34 GPIO pins */
		retval = nmk_config_pins(
				ARRAY_AND_SIZE(mop500_stm_mipi34_pins));
		if (retval) {
			pr_err("STM: Failed to enable MIPI34\n");
			return retval;
		}
	}

	switch (con_type) {
	case STM_DEFAULT_CONNECTION:
	case STM_STE_MODEM_ON_MIPI34_NONE_ON_MIPI60:
		/* Enable altC3 on GPIO70-74 (STMMOD) & GPIO75-76 (UARTMOD) */
		gpiocr |= (PRCM_GPIOCR_DBG_STM_MOD_CMD1
				| PRCM_GPIOCR_DBG_UARTMOD_CMD0);
		writel(gpiocr, PRCM_GPIOCR);
		retval = stm_ste_disable_ape_on_mipi60();
		break;

	case STM_STE_APE_ON_MIPI34_NONE_ON_MIPI60:
		/* Disable altC3 on GPIO70-74 (STMMOD) & GPIO75-76 (UARTMOD) */
		gpiocr &= ~(PRCM_GPIOCR_DBG_STM_MOD_CMD1
				| PRCM_GPIOCR_DBG_UARTMOD_CMD0);
		writel(gpiocr, PRCM_GPIOCR);
		retval = stm_ste_disable_ape_on_mipi60();
		break;

	case STM_STE_MODEM_ON_MIPI34_APE_ON_MIPI60:
		/* Enable altC3 on GPIO70-74 (STMMOD) and GPIO75-76 (UARTMOD) */
		gpiocr |= (PRCM_GPIOCR_DBG_STM_MOD_CMD1
				| PRCM_GPIOCR_DBG_UARTMOD_CMD0);
		writel(gpiocr, PRCM_GPIOCR);

		/* Enable APE on MIPI60 */
		retval = nmk_config_pins_sleep(ARRAY_AND_SIZE(mop500_ske_pins));
		if (retval)
			pr_err("STM: Failed to disable SKE GPIO\n");
		else {
			retval = nmk_config_pins(
					ARRAY_AND_SIZE(mop500_stm_mipi60_pins));
			if (retval)
				pr_err("STM: Failed to enable MIPI60\n");
		}
		break;

	case STM_DISCONNECT:
		retval = nmk_config_pins_sleep(
				ARRAY_AND_SIZE(mop500_stm_mipi34_pins));
		if (retval)
			pr_err("STM: Failed to disable MIPI34\n");

		retval = stm_ste_disable_ape_on_mipi60();
		break;

	default:
		pr_err("STM: bad connection type\n");
		break;
	}
	return retval;
}

/* Possible STM sources (masters) on ux500 */
enum stm_master {
	STM_ARM0 =	0,
	STM_ARM1 =	1,
	STM_SVA =	2,
	STM_SIA =	3,
	STM_SIA_XP70 =	4,
	STM_PRCMU =	5,
	STM_MCSBAG =	9
};

#define STM_ENABLE_ARM0		BIT(STM_ARM0)
#define STM_ENABLE_ARM1		BIT(STM_ARM1)
#define STM_ENABLE_SVA		BIT(STM_SVA)
#define STM_ENABLE_SIA		BIT(STM_SIA)
#define STM_ENABLE_SIA_XP70	BIT(STM_SIA_XP70)
#define STM_ENABLE_PRCMU	BIT(STM_PRCMU)
#define STM_ENABLE_MCSBAG	BIT(STM_MCSBAG)

/*
 * These are the channels used by NMF and some external softwares
 * expect the NMF traces to be output on these channels
 * For legacy reason, we need to reserve them.
 */
static const s16 stm_channels_reserved[] = {
	100,	/* NMF MPCEE channel */
	101,	/* NMF CM channel */
	151,	/* NMF HOSTEE channel */
};

/* On Ux500 we 2 consecutive STMs therefore 512 channels available */
static struct stm_platform_data stm_pdata = {
	.regs_phys_base       = U8500_STM_REG_BASE,
	.channels_phys_base   = U8500_STM_BASE,
	.id_mask              = 0x000fffff,   /* Ignore revisions differences */
	.channels_reserved    = stm_channels_reserved,
	.channels_reserved_sz = ARRAY_SIZE(stm_channels_reserved),
	/* Enable all except MCSBAG */
	.masters_enabled      = STM_ENABLE_ARM0 | STM_ENABLE_ARM1 |
				STM_ENABLE_SVA | STM_ENABLE_PRCMU |
				STM_ENABLE_SIA | STM_ENABLE_SIA_XP70,
	/* Provide function for MIPI34/MIPI60 STM connection */
	.stm_connection       = stm_ste_connection,
};

struct platform_device u8500_stm_device = {
	.name = "stm",
	.id = -1,
	.dev = {
		.platform_data = &stm_pdata,
	},
};

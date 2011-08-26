/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson MCDE Sony acx424akp DCS display driver
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */
#ifndef __MCDE_DISPLAY_SONY_ACX424AKP__H__
#define __MCDE_DISPLAY_SONY_ACX424AKP__H__

#include <linux/regulator/consumer.h>

#include "mcde_display.h"

enum display_panel_type {
	DISPLAY_NONE			= 0,
	DISPLAY_SONY_ACX424AKP          = 0x1b81,
};

struct  mcde_display_sony_acx424akp_platform_data {
	/* Platform info */
	int reset_gpio;
	bool reset_high;
	const char *regulator_id;
	int reset_delay; /* ms */
	int reset_low_delay; /* ms */
	int sleep_out_delay; /* ms */
	enum display_panel_type disp_panel; /* display panel types */

	/* Driver data */
	bool sony_acx424akp_platform_enable;
	struct regulator *regulator;
	int max_supply_voltage;
	int min_supply_voltage;
};

#endif /* __MCDE_DISPLAY_SONY_ACX424AKP__H__ */


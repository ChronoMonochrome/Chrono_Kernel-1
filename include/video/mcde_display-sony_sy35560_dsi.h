/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson MCDE Sony sy35560 DCS display driver
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */
#ifndef __MCDE_DISPLAY_SONY_SY35560__H__
#define __MCDE_DISPLAY_SONY_SY35560__H__

#include <linux/regulator/consumer.h>

#include "mcde_display.h"
#include <linux/workqueue.h>

/* period between ESD status checks */
#define SONY_SY35560_ESD_CHECK_PERIOD	msecs_to_jiffies(10000)

struct sony_sy35560_platform_data {
	/* Platform info */
	int reset_gpio;
	bool reset_high;
	const char *regulator_id;
	bool skip_init;

	/* Driver data */
	int max_supply_voltage;
	int min_supply_voltage;
};

struct sony_sy35560_device {
	struct mcde_display_device base;

	struct regulator *regulator;

	/* ESD workqueue */
	struct workqueue_struct *esd_wq;
	struct delayed_work esd_work;
};

#endif /* __MCDE_DISPLAY_SONY_SY35560__H__ */


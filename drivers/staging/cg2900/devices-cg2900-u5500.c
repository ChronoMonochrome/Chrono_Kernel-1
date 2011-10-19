/*
 * arch/arm/mach-ux500/devices-cg2900-u5500.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Authors:
 * Par-Gunnar Hjalmdahl (par-gunnar.p.hjalmdahl@stericsson.com) for ST-Ericsson.
 * Henrik Possung (henrik.possung@stericsson.com) for ST-Ericsson.
 * Josef Kindberg (josef.kindberg@stericsson.com) for ST-Ericsson.
 * Dariusz Szymszak (dariusz.xd.szymczak@stericsson.com) for ST-Ericsson.
 * Kjell Andersson (kjell.k.andersson@stericsson.com) for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Board specific device support for the Linux Bluetooth HCI H:4 Driver
 * for ST-Ericsson connectivity controller.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <linux/mfd/cg2900.h>
#include <linux/mfd/abx500/ab5500.h>

#include <mach/prcmu-db5500.h>

#include "devices-cg2900.h"

/* prcmu resout1 pin is used for CG2900 reset*/
void dcg2900_enable_chip(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;

	clk_enable(info->lpoclk);
	/*
	 * Due to a bug in CG2900 we cannot just set GPIO high to enable
	 * the chip. We must wait more than 100 msecs before enbling the
	 * chip.
	 * - Set PDB to low.
	 * - Wait for 100 msecs
	 * - Set PDB to high.
	 */
	prcmu_resetout(1, 0);
	schedule_timeout_uninterruptible(msecs_to_jiffies(
					CHIP_ENABLE_PDB_LOW_TIMEOUT));
	prcmu_resetout(1, 1);
}

void dcg2900_disable_chip(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;

	prcmu_resetout(1, 0);
	clk_disable(info->lpoclk);
}

int dcg2900_setup(struct cg2900_chip_dev *dev,
				struct dcg2900_info *info)
{
	info->lpoclk = clk_get(dev->dev, "lpoclk");
	if (IS_ERR(info->lpoclk))
		return PTR_ERR(info->lpoclk);

	return 0;
}

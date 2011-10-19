/*
 * arch/arm/mach-ux500/devices-cg2900-u8500.c
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

#include <asm/byteorder.h>
#include <asm-generic/errno-base.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <plat/pincfg.h>

#include "devices-cg2900.h"

void dcg2900_enable_chip(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;

	if (info->gbf_gpio == -1)
		return;

	/*
	 * Due to a bug in CG2900 we cannot just set GPIO high to enable
	 * the chip. We must wait more than 100 msecs before enbling the
	 * chip.
	 * - Set PDB to low.
	 * - Wait for 100 msecs
	 * - Set PDB to high.
	 */
	gpio_set_value(info->gbf_gpio, 0);
	schedule_timeout_uninterruptible(msecs_to_jiffies(
					CHIP_ENABLE_PDB_LOW_TIMEOUT));
	gpio_set_value(info->gbf_gpio, 1);
}

void dcg2900_disable_chip(struct cg2900_chip_dev *dev)
{
	struct dcg2900_info *info = dev->b_data;

	if (info->gbf_gpio != -1)
		gpio_set_value(info->gbf_gpio, 0);
}

int dcg2900_setup(struct cg2900_chip_dev *dev,
					struct dcg2900_info *info)
{
	int err = 0;
	struct resource *resource;
	const char *gbf_name;
	const char *bt_name = NULL;

	resource = platform_get_resource_byname(dev->pdev, IORESOURCE_IO,
						"gbf_ena_reset");
	if (!resource) {
		dev_err(dev->dev, "GBF GPIO does not exist\n");
		err = -EINVAL;
		goto err_handling;
	}

	info->gbf_gpio = resource->start;
	gbf_name = resource->name;

	resource = platform_get_resource_byname(dev->pdev, IORESOURCE_IO,
						"bt_enable");
	/* BT Enable GPIO may not exist */
	if (resource) {
		info->bt_gpio = resource->start;
		bt_name = resource->name;
	}

	/* Now setup the GPIOs */
	err = gpio_request(info->gbf_gpio, gbf_name);
	if (err < 0) {
		dev_err(dev->dev, "gpio_request failed with err: %d\n", err);
		goto err_handling;
	}

	err = gpio_direction_output(info->gbf_gpio, 0);
	if (err < 0) {
		dev_err(dev->dev, "gpio_direction_output failed with err: %d\n",
			err);
		goto err_handling_free_gpio_gbf;
	}

	if (!bt_name) {
		info->bt_gpio = -1;
		goto finished;
	}

	err = gpio_request(info->bt_gpio, bt_name);
	if (err < 0) {
		dev_err(dev->dev, "gpio_request failed with err: %d\n", err);
		goto err_handling_free_gpio_gbf;
	}

	err = gpio_direction_output(info->bt_gpio, 1);
	if (err < 0) {
		dev_err(dev->dev, "gpio_direction_output failed with err: %d\n",
			err);
		goto err_handling_free_gpio_bt;
	}

finished:

	return 0;

err_handling_free_gpio_bt:
	gpio_free(info->bt_gpio);
err_handling_free_gpio_gbf:
	gpio_free(info->gbf_gpio);
err_handling:

	return err;
}

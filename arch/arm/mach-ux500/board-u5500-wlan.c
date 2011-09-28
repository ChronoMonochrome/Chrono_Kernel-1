/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
  *Author: Bartosz Markowski <bartosz.markowski@tieto.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <plat/pincfg.h>
#include "pins.h"
#include <mach/cw1200_plat.h>

static void cw1200_release(struct device *dev);

static struct resource cw1200_u5500_resources[] = {
	{
		.start = NOMADIK_GPIO_TO_IRQ(129),
		.end = NOMADIK_GPIO_TO_IRQ(129),
		.flags = IORESOURCE_IRQ,
		.name = "cw1200_irq",
	},
};

static struct cw1200_platform_data cw1200_u5500_platform_data = { 0 };

static struct platform_device cw1200_device = {
	.name = "cw1200_wlan",
	.dev = {
		.platform_data = &cw1200_u5500_platform_data,
		.release = cw1200_release,
		.init_name = "cw1200_wlan",
	},
};

const struct cw1200_platform_data *cw1200_u5500_get_platform_data(void)
{
	return &cw1200_u5500_platform_data;
}
EXPORT_SYMBOL_GPL(cw1200_u5500_get_platform_data);

static int cw1200_pins_enable(bool enable)
{
	struct ux500_pins *pins = NULL;
	int ret = 0;

	pins = ux500_pins_get("sdi3");

	if (!pins) {
		printk(KERN_ERR "cw1200: Pins are not found. "
				"Check platform data.\n");
		return -ENOENT;
	}

	if (enable)
		ret = ux500_pins_enable(pins);
	else
		ret = ux500_pins_disable(pins);

	if (ret)
		printk(KERN_ERR "cw1200: Pins can not be %s: %d.\n",
				enable ? "enabled" : "disabled",
				ret);

	ux500_pins_put(pins);

	return ret;
}

int __init u5500_wlan_init(void)
{
	int ret;

	if (machine_is_u5500()) {
		cw1200_device.num_resources = ARRAY_SIZE(cw1200_u5500_resources);
		cw1200_device.resource = cw1200_u5500_resources;
	} else {
		dev_err(&cw1200_device.dev,
				"Unsupported mach type %d "
				"(check mach-types.h)\n",
				__machine_arch_type);
		return -ENOTSUPP;
	}

	cw1200_u5500_platform_data.mmc_id = "mmc2";
	cw1200_u5500_platform_data.irq = &cw1200_device.resource[0];

	cw1200_device.dev.release = cw1200_release;

	ret = cw1200_pins_enable(true);
	if (WARN_ON(ret))
		return ret;

	ret = platform_device_register(&cw1200_device);
	if (ret)
		cw1200_pins_enable(false);

	return ret;
}

static void cw1200_release(struct device *dev)
{
	cw1200_pins_enable(false);
}

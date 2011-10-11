/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/amba/bus.h>

#include <mach/hardware.h>
#include <mach/setup.h>
#include <linux/hwmem.h>

static struct hwmem_platform_data hwmem_pdata = {
	.start = 0,
	.size = 0,
};

static int __init early_hwmem(char *p)
{
	hwmem_pdata.size = memparse(p, &p);

	if (*p != '@')
		goto no_at;

	hwmem_pdata.start = memparse(p + 1, &p);

	return 0;

no_at:
	hwmem_pdata.size = 0;

	return -EINVAL;
}
early_param("hwmem", early_hwmem);

struct platform_device ux500_hwmem_device = {
	.name = "hwmem",
	.dev = {
		.platform_data = &hwmem_pdata,
	},
};

void __init amba_add_devices(struct amba_device *devs[], int num)
{
	int i;

	for (i = 0; i < num; i++) {
		struct amba_device *d = devs[i];
		amba_device_register(d, &iomem_resource);
	}
}

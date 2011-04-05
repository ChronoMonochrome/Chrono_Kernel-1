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

#ifdef CONFIG_STE_TRACE_MODEM
#include <linux/db8500-modem-trace.h>
#endif

#ifdef CONFIG_STE_TRACE_MODEM
static struct resource trace_resource = {
	.start	= 0,
	.end	= 0,
	.name	= "db8500-trace-area",
	.flags	= IORESOURCE_MEM
};

static struct db8500_trace_platform_data trace_pdata = {
	.ape_base = U8500_APE_BASE,
	.modem_base = U8500_MODEM_BASE,
};

struct platform_device u8500_trace_modem = {
	.name	= "db8500-modem-trace",
	.id = 0,
	.dev = {
		.init_name = "db8500-modem-trace",
		.platform_data = &trace_pdata,
	},
	.num_resources = 1,
	.resource = &trace_resource,
};

static int __init early_trace_modem(char *p)
{
	struct resource *data = &trace_resource;
	u32 size = memparse(p, &p);

	if (*p == '@')
		data->start = memparse(p + 1, &p);
	data->end = data->start + size;

	return 0;
}

early_param("mem_mtrace", early_trace_modem);
#endif

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

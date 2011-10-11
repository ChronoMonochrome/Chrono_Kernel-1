/*
 * Copyright (C) ST-Ericsson AB 2010
 *
 * MOP500/HREF500 ed/v1 Display platform devices
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <video/mcde.h>
#include <mach/prcmu-fw-api.h>

static struct resource mcde_resources[] = {
	[0] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_MCDE_BASE,
		.end   = U8500_MCDE_BASE + 0x1000 - 1, /* TODO: Fix size */
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK1_BASE,
		.end   = U8500_DSI_LINK1_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK2_BASE,
		.end   = U8500_DSI_LINK2_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK3_BASE,
		.end   = U8500_DSI_LINK3_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[4] = {
		.name  = MCDE_IRQ,
		.start = IRQ_DB8500_DISP,
		.end   = IRQ_DB8500_DISP,
		.flags = IORESOURCE_IRQ,
	},
};


static int mcde_platform_enable(void)
{
	return prcmu_enable_dsipll();
}

static int mcde_platform_disable(void)
{
	return prcmu_disable_dsipll();
}

static void dev_release_noop(struct device *dev)
{
	/* Do nothing */
}

static struct mcde_platform_data mcde_pdata = {
	.num_dsilinks = 3,
	/*
	 * [0] = 3: 24 bits DPI: connect LSB Ch B to D[0:7]
	 * [3] = 4: 24 bits DPI: connect MID Ch B to D[24:31]
	 * [4] = 5: 24 bits DPI: connect MSB Ch B to D[32:39]
	 *
	 * [1] = 3: TV out     : connect LSB Ch B to D[8:15]
	 */
#define DONT_CARE 0
	.outmux = { 3, 3, DONT_CARE, 4, 5 },
#undef DONT_CARE
	.syncmux = 0x00,  /* DPI channel A and B on output pins A and B resp */
	.regulator_vana_id = "v-ana",
	.regulator_mcde_epod_id = "vsupply",
	.regulator_esram_epod_id = "v-esram34",
	.clock_dsi_id = "hdmi",
	.clock_dsi_lp_id = "tv",
	.clock_dpi_id = "lcd",
	.clock_mcde_id = "mcde",
	.platform_enable = mcde_platform_enable,
	.platform_disable = mcde_platform_disable,
};

struct platform_device ux500_mcde_device = {
	.name = "mcde",
	.id = -1,
	.dev = {
		.release = dev_release_noop,
		.platform_data = &mcde_pdata,
	},
	.num_resources = ARRAY_SIZE(mcde_resources),
	.resource = mcde_resources,
};

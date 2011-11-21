/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/gpio/nomadik.h>

#ifdef CONFIG_FB_MCDE
#include <video/mcde_display.h>
#include <video/mcde_display-av8100.h>
#include <video/mcde_fb.h>
#endif

#include <mach/hardware.h>
#include <mach/pm.h>

#include "devices-common.h"

struct amba_device *
dbx500_add_amba_device(struct device *parent, const char *name,
		       resource_size_t base, int irq, void *pdata,
		       unsigned int periphid)
{
	struct amba_device *dev;
	int ret;

	dev = amba_device_alloc(name, base, SZ_4K);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	dev->dma_mask = DMA_BIT_MASK(32);
	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	dev->dev.pm_domain = &ux500_amba_dev_power_domain;

	dev->irq[0] = irq;

	dev->periphid = periphid;

	dev->dev.platform_data = pdata;

	dev->dev.parent = parent;

	ret = amba_device_add(dev, &iomem_resource);
	if (ret) {
		amba_device_put(dev);
		return ERR_PTR(ret);
	}

	return dev;
}

struct platform_device *
dbx500_add_platform_device_4k1irq(const char *name, int id,
				  resource_size_t base,
				  int irq, void *pdata)
{
	struct resource resources[] = {
		[0] = {
			.start	= base,
			.end	= base + SZ_4K - 1,
			.flags	= IORESOURCE_MEM,
		},
		[1] = {
			.start	= irq,
			.end	= irq,
			.flags	= IORESOURCE_IRQ,
		}
	};

	return dbx500_add_platform_device(name, id, pdata, resources,
					  ARRAY_SIZE(resources));
}

struct platform_device *
dbx500_add_platform_device_noirq(const char *name, int id,
				  resource_size_t base, void *pdata)
{
	struct resource resources[] = {
		[0] = {
			.start  = base,
			.end    = base + SZ_4K - 1,
			.flags  = IORESOURCE_MEM,
		}
	};

	return dbx500_add_platform_device(name, id, pdata, resources,
					  ARRAY_SIZE(resources));
}

static struct platform_device *
dbx500_add_gpio(struct device *parent, int id, resource_size_t addr, int irq,
		struct nmk_gpio_platform_data *pdata)
{
	struct resource resources[] = {
		{
			.start	= addr,
			.end	= addr + 127,
			.flags	= IORESOURCE_MEM,
		},
		{
			.start	= irq,
			.end	= irq,
			.flags	= IORESOURCE_IRQ,
		}
	};

	return platform_device_register_resndata(
		parent,
		"gpio",
		id,
		resources,
		ARRAY_SIZE(resources),
		pdata,
		sizeof(*pdata));
}

void dbx500_add_gpios(struct device *parent, resource_size_t *base, int num,
		      int irq, struct nmk_gpio_platform_data *pdata)
{
	int first = 0;
	int i;

	for (i = 0; i < num; i++, first += 32, irq++) {
		pdata->first_gpio = first;
		pdata->first_irq = NOMADIK_GPIO_TO_IRQ(first);
		pdata->num_gpio = 32;

		dbx500_add_gpio(parent, i, base[i], irq, pdata);
	}
}

#ifdef CONFIG_FB_MCDE
void hdmi_fb_onoff(struct mcde_display_device *ddev,
		bool enable, u8 cea, u8 vesa_cea_nr)
{
	struct fb_info *fbi;
	u16 w, h;
	u16 vw, vh;
	u32 rotate = FB_ROTATE_UR;
	struct display_driver_data *driver_data = dev_get_drvdata(&ddev->dev);

	dev_dbg(&ddev->dev, "%s\n", __func__);
	dev_dbg(&ddev->dev, "en:%d cea:%d nr:%d\n", enable, cea, vesa_cea_nr);

	if (enable) {
		if (ddev->enabled) {
			dev_dbg(&ddev->dev, "Display is already enabled.\n");
			return;
		}

		/* Create fb */
		if (ddev->fbi == NULL) {
			/* Note: change when dynamic buffering is available */
			int buffering = 2;

			/* Get default values */
			mcde_dss_get_native_resolution(ddev, &w, &h);
			vw = w;
			vh = h * buffering;

			if (vesa_cea_nr != 0)
				ddev->ceanr_convert(ddev, cea, vesa_cea_nr,
						buffering, &w, &h, &vw, &vh);

			fbi = mcde_fb_create(ddev, w, h, vw, vh,
				ddev->default_pixel_format, rotate);

			if (IS_ERR(fbi)) {
				dev_warn(&ddev->dev,
					"Failed to create fb for display %s\n",
							ddev->name);
				goto hdmi_fb_onoff_end;
			} else {
				dev_info(&ddev->dev,
					"Framebuffer created (%s)\n",
							ddev->name);
			}
			driver_data->fbdevname = (char *)dev_name(fbi->dev);
		}
	} else {
		if (!ddev->enabled) {
			dev_dbg(&ddev->dev, "Display %s is already disabled.\n",
					ddev->name);
			return;
		}
		mcde_fb_destroy(ddev);
	}

hdmi_fb_onoff_end:
	return;
}
EXPORT_SYMBOL(hdmi_fb_onoff);
#endif


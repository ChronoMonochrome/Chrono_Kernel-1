/*
 * Copyright (C) ST-Ericsson AB 2010
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/mfd/ab8500/denc.h>
#include <linux/workqueue.h>
#include <linux/dispdev.h>
#include <asm/mach-types.h>
#include <video/av8100.h>
#include <video/mcde_display.h>
#include <video/mcde_display-vuib500-dpi.h>
#include <video/mcde_display-sony_acx424akp_dsi.h>
#include <video/mcde_display-av8100.h>
#include <video/mcde_display-ab8500.h>
#include <video/mcde_fb.h>
#include <video/mcde_dss.h>
#include <plat/pincfg.h>
#include "pins-db8500.h"
#include "pins.h"
#include "board-mop500.h"

#define DSI_UNIT_INTERVAL_0	0x9
#define DSI_UNIT_INTERVAL_1	0x9
#define DSI_UNIT_INTERVAL_2	0x5

#ifdef CONFIG_FB_MCDE

/* The initialization of hdmi disp driver must be delayed in order to
 * ensure that inputclk will be available (needed by hdmi hw) */
#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
static struct delayed_work work_dispreg_hdmi;
#define DISPREG_HDMI_DELAY 6000
#endif

enum {
	PRIMARY_DISPLAY_ID,
	SECONDARY_DISPLAY_ID,
	FICTIVE_DISPLAY_ID,
	AV8100_DISPLAY_ID,
	AB8500_DISPLAY_ID,
	MCDE_NR_OF_DISPLAYS
};
static int display_initialized_during_boot;

static int __init startup_graphics_setup(char *str)
{

	if (get_option(&str, &display_initialized_during_boot) != 1)
		display_initialized_during_boot = 0;

	switch (display_initialized_during_boot) {
	case 1:
		pr_info("Startup graphics support\n");
		break;
	case 0:
	default:
		pr_info("No startup graphics supported\n");
		break;
	};

	return 1;
}
__setup("startup_graphics=", startup_graphics_setup);

#if defined(CONFIG_DISPLAY_AB8500_TERTIARY) ||\
					defined(CONFIG_DISPLAY_AV8100_TERTIARY)
static struct mcde_col_transform rgb_2_yCbCr_transform = {
	.matrix = {
		{0x0042, 0x0081, 0x0019},
		{0xffda, 0xffb6, 0x0070},
		{0x0070, 0xffa2, 0xffee},
	},
	.offset = {0x10, 0x80, 0x80},
};
#endif

#ifdef CONFIG_DISPLAY_FICTIVE
static struct mcde_display_device fictive_display = {
	.name = "mcde_disp_fictive",
	.id = FICTIVE_DISPLAY_ID,
	.fictive = true,
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
	.native_x_res = 1280,
	.native_y_res = 720,
};
#endif /* CONFIG_DISPLAY_FICTIVE */

#ifdef CONFIG_DISPLAY_GENERIC_DSI_PRIMARY
static struct mcde_display_dsi_platform_data samsung_s6d16d0_pdata0 = {
	.link = 0,
};

static struct mcde_display_device samsung_s6d16d0_display0 = {
	.name = "samsung_s6d16d0",
	.id = PRIMARY_DISPLAY_ID,
	.chnl_id = MCDE_CHNL_A,
	.fifo = MCDE_FIFO_A,
#ifdef CONFIG_MCDE_DISPLAY_PRIMARY_16BPP
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
#else
	.default_pixel_format = MCDE_OVLYPIXFMT_RGBA8888,
#endif
#ifdef CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_VSYNC
	.synchronized_update = true,
#else
	.synchronized_update = false,
#endif
	/* TODO: Remove rotation buffers once ESRAM driver is completed */
	.rotbuf1 = U8500_ESRAM_BASE + 0x20000 * 4,
	.rotbuf2 = U8500_ESRAM_BASE + 0x20000 * 4 + 0x10000,
	.dev = {
		.platform_data = &samsung_s6d16d0_pdata0,
	},
};
#endif /* CONFIG_DISPLAY_GENERIC_DSI_PRIMARY */

#ifdef CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY
static struct mcde_port sony_port0 = {
	.link = 0,
};

struct mcde_display_sony_acx424akp_platform_data
			sony_acx424akp_display0_pdata = {
	.reset_gpio = HREFV60_DISP2_RST_GPIO,
};

static struct mcde_display_device sony_acx424akp_display0 = {
	.name = "mcde_disp_sony_acx424akp",
	.id = PRIMARY_DISPLAY_ID,
	.port = &sony_port0,
	.chnl_id = MCDE_CHNL_A,
	/*
	 * A large fifo is needed when ddr is clocked down to 25% to not get
	 * latency problems.
	 */
	.fifo = MCDE_FIFO_A,
	.default_pixel_format = MCDE_OVLYPIXFMT_RGBA8888,
	.synchronized_update = true,
	.rotbuf1 = U8500_ESRAM_BASE + 0x20000 * 4,
	.rotbuf2 = U8500_ESRAM_BASE + 0x20000 * 4 + 0x10000,
	.dev = {
		.platform_data = &sony_acx424akp_display0_pdata,
	},
};
#endif /* CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY */

#ifdef CONFIG_DISPLAY_GENERIC_DSI_SECONDARY
static struct mcde_display_dsi_platform_data samsung_s6d16d0_pdata1 = {
	.link = 1,
};

static struct mcde_display_device samsung_s6d16d0_display1 = {
	.name = "samsung_s6d16d0",
	.id = SECONDARY_DISPLAY_ID,
	.chnl_id = MCDE_CHNL_C1,
	.fifo = MCDE_FIFO_C1,
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
#ifdef CONFIG_DISPLAY_GENERIC_DSI_SECONDARY_VSYNC
	.synchronized_update = true,
#else
	.synchronized_update = false,
#endif
	.dev = {
		.platform_data = &samsung_s6d16d0_pdata1,
	},
};
#endif /* CONFIG_DISPLAY_GENERIC_DSI_SECONDARY */

#ifdef CONFIG_MCDE_DISPLAY_DPI_PRIMARY
static struct mcde_port port0 = {
	.type = MCDE_PORTTYPE_DPI,
	.pixel_format = MCDE_PORTPIXFMT_DPI_24BPP,
	.ifc = 0,
	.link = 1,		/* DPI channel B can only be on link 1 */
	.sync_src = MCDE_SYNCSRC_OFF,   /* sync from output formatter  */
	.update_auto_trig = true,
	.phy = {
		.dpi = {
			.tv_mode = false,
			.clock_div = 2,
			.polarity = DPI_ACT_LOW_VSYNC | DPI_ACT_LOW_HSYNC,
		},
	},
};

static struct mcde_display_dpi_platform_data dpi_display0_pdata = {0};
static struct ux500_pins *dpi_pins;

static int dpi_display_platform_enable(struct mcde_display_device *ddev)
{
	int res;

	if (!dpi_pins) {
		dpi_pins = ux500_pins_get("mcde-dpi");
		if (!dpi_pins)
			return -EINVAL;
	}

	dev_info(&ddev->dev, "%s\n", __func__);
	res = ux500_pins_enable(dpi_pins);
	if (res)
		dev_warn(&ddev->dev, "Failure during %s\n", __func__);

	return res;
}

static int dpi_display_platform_disable(struct mcde_display_device *ddev)
{
	int res;

	dev_info(&ddev->dev, "%s\n", __func__);

	res = ux500_pins_disable(dpi_pins);
	if (res)
		dev_warn(&ddev->dev, "Failure during %s\n", __func__);

	return res;

}

static struct mcde_display_device dpi_display0 = {
	.name = "mcde_display_dpi",
	.id = 0,
	.port = &port0,
	.chnl_id = MCDE_CHNL_B,
	.fifo = MCDE_FIFO_B,
#ifdef CONFIG_MCDE_DISPLAY_PRIMARY_16BPP
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
#else
	.default_pixel_format = MCDE_OVLYPIXFMT_RGBA8888,
#endif
	.native_x_res = 640,
	.native_y_res = 480,
	/* .synchronized_update: Don't care: port is set to update_auto_trig */
	.dev = {
		.platform_data = &dpi_display0_pdata,
	},
	.platform_enable = dpi_display_platform_enable,
	.platform_disable = dpi_display_platform_disable,
};
#endif /* CONFIG_MCDE_DISPLAY_DPI_PRIMARY */

#ifdef CONFIG_DISPLAY_AB8500_TERTIARY
static struct mcde_port port_tvout1 = {
	.type = MCDE_PORTTYPE_DPI,
	.pixel_format = MCDE_PORTPIXFMT_DPI_24BPP,
	.ifc = 0,
	.link = 1,			/* channel B */
	.sync_src = MCDE_SYNCSRC_OFF,
	.update_auto_trig = true,
	.phy = {
		.dpi = {
			.bus_width = 4, /* DDR mode */
			.tv_mode = true,
			.clock_div = MCDE_PORT_DPI_NO_CLOCK_DIV,
		},
	},
};

static struct ab8500_display_platform_data ab8500_display_pdata = {
	.nr_regulators = 2,
	.regulator_id  = {"vtvout", "vcc-N2158"},
	.rgb_2_yCbCr_transform = &rgb_2_yCbCr_transform,
};

static struct ux500_pins *tvout_pins;

static int ab8500_platform_enable(struct mcde_display_device *ddev)
{
	int res = 0;

	if (!tvout_pins) {
		tvout_pins = ux500_pins_get("mcde-tvout");
		if (!tvout_pins)
			return -EINVAL;
	}

	dev_dbg(&ddev->dev, "%s\n", __func__);
	res = ux500_pins_enable(tvout_pins);
	if (res != 0)
		goto failed;

	return res;

failed:
	dev_warn(&ddev->dev, "Failure during %s\n", __func__);
	return res;
}

static int ab8500_platform_disable(struct mcde_display_device *ddev)
{
	int res;

	dev_dbg(&ddev->dev, "%s\n", __func__);

	res = ux500_pins_disable(tvout_pins);
	if (res != 0)
		goto failed;
	return res;

failed:
	dev_warn(&ddev->dev, "Failure during %s\n", __func__);
	return res;
}

struct mcde_display_device tvout_ab8500_display = {
	.name = "mcde_tv_ab8500",
	.id = AB8500_DISPLAY_ID,
	.port = &port_tvout1,
	.chnl_id = MCDE_CHNL_B,
	.fifo = MCDE_FIFO_B,
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
	.native_x_res = 720,
	.native_y_res = 576,
	/* .synchronized_update: Don't care: port is set to update_auto_trig */
	.dev = {
		.platform_data = &ab8500_display_pdata,
	},

	/*
	* We might need to describe the std here:
	* - there are different PAL / NTSC formats (do they require MCDE
	*   settings?)
	*/
	.platform_enable = ab8500_platform_enable,
	.platform_disable = ab8500_platform_disable,
};
#endif /* CONFIG_DISPLAY_AB8500_TERTIARY */

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
static struct mcde_port port2 = {
	.type = MCDE_PORTTYPE_DSI,
	.mode = MCDE_PORTMODE_CMD,
	.pixel_format = MCDE_PORTPIXFMT_DSI_24BPP,
	.ifc = 1,
	.link = 2,
#ifdef CONFIG_AV8100_HWTRIG_INT
	.sync_src = MCDE_SYNCSRC_TE0,
#endif
#ifdef CONFIG_AV8100_HWTRIG_I2SDAT3
	.sync_src = MCDE_SYNCSRC_TE1,
#endif
#ifdef CONFIG_AV8100_HWTRIG_DSI_TE
	.sync_src = MCDE_SYNCSRC_TE_POLLING,
#endif
#ifdef CONFIG_AV8100_HWTRIG_NONE
	.sync_src = MCDE_SYNCSRC_OFF,
#endif
	.update_auto_trig = true,
	.phy = {
		.dsi = {
			.virt_id = 0,
			.num_data_lanes = 2,
			.ui = DSI_UNIT_INTERVAL_2,
			.clk_cont = false,
			.data_lanes_swap = false,
		},
	},
	.hdmi_sdtv_switch = HDMI_SWITCH,
};

static struct mcde_display_hdmi_platform_data av8100_hdmi_pdata = {
	.reset_gpio = 0,
	.reset_delay = 1,
	.regulator_id = NULL, /* TODO: "display_main" */
	.cvbs_regulator_id = "vcc-N2158",
	.ddb_id = 1,
	.rgb_2_yCbCr_transform = &rgb_2_yCbCr_transform,
};

static struct ux500_pins *av8100_pins;
static int av8100_platform_enable(struct mcde_display_device *ddev)
{
	int res;

	dev_dbg(&ddev->dev, "%s\n", __func__);
	if (!av8100_pins) {
		av8100_pins = ux500_pins_get("av8100-hdmi");
		if (!av8100_pins) {
			res = -EINVAL;
			goto failed;
		}
	}

	res = ux500_pins_enable(av8100_pins);
	if (res != 0)
		goto failed;

	return res;

failed:
	dev_warn(&ddev->dev, "Failure during %s\n", __func__);
	return res;
}

static int av8100_platform_disable(struct mcde_display_device *ddev)
{
	int res;

	dev_dbg(&ddev->dev, "%s\n", __func__);

	res = ux500_pins_disable(av8100_pins);
	if (res != 0)
		goto failed;
	return res;

failed:
	dev_warn(&ddev->dev, "Failure during %s\n", __func__);
	return res;
}

struct mcde_display_device av8100_hdmi = {
	.name = "av8100_hdmi",
	.id = AV8100_DISPLAY_ID,
	.port = &port2,
	.chnl_id = MCDE_CHNL_B,
	.fifo = MCDE_FIFO_B,
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
	.native_x_res = 1280,
	.native_y_res = 720,
	.dev = {
		.platform_data = &av8100_hdmi_pdata,
	},
	.platform_enable = av8100_platform_enable,
	.platform_disable = av8100_platform_disable,
};

static void delayed_work_dispreg_hdmi(struct work_struct *ptr)
{
	if (mcde_display_device_register(&av8100_hdmi))
		pr_warning("Failed to register av8100_hdmi\n");
}
#endif /* CONFIG_DISPLAY_AV8100_TERTIARY */

/*
* This function will create the framebuffer for the display that is registered.
*/
static int display_postregistered_callback(struct notifier_block *nb,
	unsigned long event, void *dev)
{
	struct mcde_display_device *ddev = dev;
	u16 width, height;
	u16 virtual_width, virtual_height;
	u32 rotate = FB_ROTATE_UR;
	u32 rotate_angle = 0;
	struct fb_info *fbi;
#ifdef CONFIG_DISPDEV
	struct mcde_fb *mfb;
#endif

	if (event != MCDE_DSS_EVENT_DISPLAY_REGISTERED)
		return 0;

	if (ddev->id < 0 || ddev->id >= MCDE_NR_OF_DISPLAYS)
		return 0;

	mcde_dss_get_native_resolution(ddev, &width, &height);

	if (uib_is_u8500uibr3())
		rotate_angle = 0;
	else
		rotate_angle = \
			CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE;

#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY) || \
		defined(CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY)
	if (ddev->id == PRIMARY_DISPLAY_ID) {
		switch (rotate_angle) {
		case 0:
			rotate = FB_ROTATE_UR;
			break;
		case 90:
			rotate = FB_ROTATE_CW;
			swap(width, height);
			break;
		case 180:
			rotate = FB_ROTATE_UD;
			break;
		case 270:
			rotate = FB_ROTATE_CCW;
			swap(width, height);
			break;
		}
	}
#endif

	virtual_width = width;
	virtual_height = height * 2;

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
	if (ddev->id == AV8100_DISPLAY_ID) {
#ifdef CONFIG_MCDE_DISPLAY_HDMI_FB_AUTO_CREATE
		hdmi_fb_onoff(ddev, 1, 0, 0);
#endif
		goto display_postregistered_callback_end;
	}
#endif /* CONFIG_DISPLAY_AV8100_TERTIARY */

	/* Create frame buffer */
	fbi = mcde_fb_create(ddev,
		width, height,
		virtual_width, virtual_height,
		ddev->default_pixel_format,
		rotate);

	if (IS_ERR(fbi)) {
		dev_warn(&ddev->dev,
			"Failed to create fb for display %s\n",
					ddev->name);
		goto display_postregistered_callback_err;
	} else {
		dev_info(&ddev->dev, "Framebuffer created (%s)\n",
					ddev->name);
	}

#ifdef CONFIG_DISPDEV
	mfb = to_mcde_fb(fbi);

	/* Create a dispdev overlay for this display */
	if (dispdev_create(ddev, true, mfb->ovlys[0]) < 0) {
		dev_warn(&ddev->dev,
			"Failed to create disp for display %s\n",
					ddev->name);
		goto display_postregistered_callback_err;
	} else {
		dev_info(&ddev->dev, "Disp dev created for (%s)\n",
					ddev->name);
	}
#endif

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
display_postregistered_callback_end:
#endif
	return 0;

display_postregistered_callback_err:
	return -1;
}

static struct notifier_block display_nb = {
	.notifier_call = display_postregistered_callback,
};

/*
* This function is used to refresh the display (lcd, hdmi, tvout) with black
* when the framebuffer is registered.
* The main display will not be updated if startup graphics is displayed
* from u-boot.
*/
static int framebuffer_postregistered_callback(struct notifier_block *nb,
	unsigned long event, void *data)
{
	int ret = 0;
	struct fb_event *event_data = data;
	struct fb_info *info;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	struct mcde_fb *mfb;

	if (event != FB_EVENT_FB_REGISTERED)
		return 0;

	if (!event_data)
		return 0;

	info = event_data->info;
	mfb = to_mcde_fb(info);
	if (mfb->id == 0 && display_initialized_during_boot)
		goto out;

	var = info->var;
	fix = info->fix;
	var.yoffset = var.yoffset ? 0 : var.yres;
	if (info->fbops->fb_pan_display)
		ret = info->fbops->fb_pan_display(&var, info);
out:
	return ret;
}

static struct notifier_block framebuffer_nb = {
	.notifier_call = framebuffer_postregistered_callback,
};

static void setup_primary_display(void)
{
	/* Display reset GPIO is different depending on reference boards */
	if (machine_is_hrefv60())
		samsung_s6d16d0_pdata0.reset_gpio = HREFV60_DISP1_RST_GPIO;
	else
		samsung_s6d16d0_pdata0.reset_gpio = MOP500_DISP1_RST_GPIO;

	/* Not all STUIB supports VSYNC, disable vsync for STUIB */
#ifdef CONFIG_DISPLAY_GENERIC_DSI_PRIMARY
	if (uib_is_stuib())
		samsung_s6d16d0_display0.synchronized_update = false;
#endif
}

int __init init_display_devices(void)
{
	int ret = 0;

	if (!cpu_is_u8500())
		return ret;

	ret = fb_register_client(&framebuffer_nb);
	if (ret)
		pr_warning("Failed to register framebuffer notifier\n");

	ret = mcde_dss_register_notifier(&display_nb);
	if (ret)
		pr_warning("Failed to register dss notifier\n");
#ifdef CONFIG_DISPLAY_FICTIVE
	ret = mcde_display_device_register(&fictive_display);
	if (ret)
		pr_warning("Failed to register fictive display device\n");
#endif

	/* Set powermode to STANDBY if startup graphics is executed */
#ifdef CONFIG_DISPLAY_GENERIC_PRIMARY
	if (display_initialized_during_boot)
		samsung_s6d16d0_display0.power_mode = MCDE_DISPLAY_PM_STANDBY;
#endif
#ifdef CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY
	if (display_initialized_during_boot)
		sony_acx424akp_display0.power_mode = MCDE_DISPLAY_PM_STANDBY;
#endif

#if defined(CONFIG_DISPLAY_GENERIC_PRIMARY) || \
	defined(CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY)
	/*
	 * For reference platforms different panels are used
	 * depending on UIB
	 * UIB = User Interface Board
	 */
	setup_primary_display();

#ifdef CONFIG_DISPLAY_GENERIC_PRIMARY
	/* Samsung display for STUIB and U8500UIB */
	if (uib_is_u8500uib() || uib_is_stuib())
		ret = mcde_display_device_register(&samsung_s6d16d0_display0);
#endif
#ifdef CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY
	/* Sony display on U8500UIBV3 */
	if (uib_is_u8500uibr3())
		ret = mcde_display_device_register(&sony_acx424akp_display0);
#endif
	if (ret)
		pr_warning("Failed to register primary display device\n");
#endif

#ifdef CONFIG_DISPLAY_GENERIC_DSI_SECONDARY
	/* Display reset GPIO is different depending on reference boards */
	if (machine_is_hrefv60())
		samsung_s6d16d0_pdata1.reset_gpio = HREFV60_DISP2_RST_GPIO;
	else
		samsung_s6d16d0_pdata1.reset_gpio = MOP500_DISP2_RST_GPIO;
	ret = mcde_display_device_register(&samsung_s6d16d0_display1);
	if (ret)
		pr_warning("Failed to register sub display device\n");
#endif

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
	/* Snowball dont need this delay at all */
	if (machine_is_snowball())
		delayed_work_dispreg_hdmi(NULL);
	else {
		INIT_DELAYED_WORK_DEFERRABLE(&work_dispreg_hdmi,
				delayed_work_dispreg_hdmi);

		schedule_delayed_work(&work_dispreg_hdmi,
				msecs_to_jiffies(DISPREG_HDMI_DELAY));
	}
#endif
#ifdef CONFIG_DISPLAY_AB8500_TERTIARY
	ret = mcde_display_device_register(&tvout_ab8500_display);
	if (ret)
		pr_warning("Failed to register ab8500 tvout device\n");
#endif

	return ret;
}

module_init(init_display_devices);

#endif

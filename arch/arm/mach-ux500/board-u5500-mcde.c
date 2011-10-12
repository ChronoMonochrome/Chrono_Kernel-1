/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/dispdev.h>
#include <video/av8100.h>
#include <asm/mach-types.h>
#include <video/mcde_display.h>
#include <video/mcde_display-generic_dsi.h>
#include <video/mcde_display-sony_acx424akp_dsi.h>
#include <video/mcde_display-av8100.h>
#include <video/mcde_fb.h>
#include <video/mcde_dss.h>

#define DSI_UNIT_INTERVAL_0	0xA
#define DSI_UNIT_INTERVAL_2	0x5

enum {
#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY) || \
		defined(CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY)
	PRIMARY_DISPLAY_ID,
#endif
#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
	AV8100_DISPLAY_ID,
#endif
	MCDE_NR_OF_DISPLAYS
};


#ifdef CONFIG_FB_MCDE

/* The initialization of hdmi disp driver must be delayed in order to
 * ensure that inputclk will be available (needed by hdmi hw) */
#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
static struct delayed_work work_dispreg_hdmi;
#define DISPREG_HDMI_DELAY 6000
#endif

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

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
static struct mcde_col_transform rgb_2_yCbCr_transform = {
	.matrix = {
		{0x0042, 0x0081, 0x0019},
		{0xffda, 0xffb6, 0x0070},
		{0x0070, 0xffa2, 0xffee},
	},
	.offset = {0x10, 0x80, 0x80},
};
#endif

#ifdef CONFIG_DISPLAY_GENERIC_DSI_PRIMARY
static struct mcde_port port0 = {
	.type = MCDE_PORTTYPE_DSI,
	.mode = MCDE_PORTMODE_CMD,
	.pixel_format = MCDE_PORTPIXFMT_DSI_24BPP,
	.ifc = DSI_VIDEO_MODE,
	.link = 0,
	.sync_src = MCDE_SYNCSRC_BTA,
	.phy = {
		.dsi = {
			.virt_id = 0,
			.num_data_lanes = 2,
			.ui = DSI_UNIT_INTERVAL_0,
			.clk_cont = false,
			.data_lanes_swap = false,
		},
	},
};

struct mcde_display_generic_platform_data generic_display0_pdata = {
	.reset_gpio = 226,
	.reset_delay = 10,
	.sleep_out_delay = 140,
#ifdef CONFIG_REGULATOR
	.regulator_id = "v-display",
	.min_supply_voltage = 2500000, /* 2.5V */
	.max_supply_voltage = 2700000 /* 2.7V */
#endif
};

struct mcde_display_device generic_display0 = {
	.name = "mcde_disp_generic",
	.id = PRIMARY_DISPLAY_ID,
	.port = &port0,
	.chnl_id = MCDE_CHNL_A,
	.fifo = MCDE_FIFO_A,
#ifdef CONFIG_MCDE_DISPLAY_PRIMARY_16BPP
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB565,
#else
	.default_pixel_format = MCDE_OVLYPIXFMT_RGBA8888,
#endif
	.native_x_res = 864,
	.native_y_res = 480,
#ifdef CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_VSYNC
	.synchronized_update = true,
#else
	.synchronized_update = false,
#endif
	/* TODO: Remove rotation buffers once ESRAM driver is completed */
	.rotbuf1 = U5500_ESRAM_BASE + 0x20000 * 2,
	.rotbuf2 = U5500_ESRAM_BASE + 0x20000 * 2 + 0x10000,
	.dev = {
		.platform_data = &generic_display0_pdata,
	},
};
#endif /* CONFIG_DISPLAY_GENERIC_DSI_PRIMARY */

static struct mcde_port port1 = {
	.link = 0,
};

struct mcde_display_sony_acx424akp_platform_data \
			sony_acx424akp_display0_pdata = {
	.reset_gpio = 226,
};

struct mcde_display_device sony_acx424akp_display0 = {
	.name = "mcde_disp_sony_acx424akp",
	.id = PRIMARY_DISPLAY_ID,
	.port = &port1,
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
	.rotbuf1 = U5500_ESRAM_BASE + 0x20000 * 2,
	.rotbuf2 = U5500_ESRAM_BASE + 0x20000 * 2 + 0x10000,
	.dev = {
		.platform_data = &sony_acx424akp_display0_pdata,
	},
};

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
static struct mcde_port port2 = {
	.type = MCDE_PORTTYPE_DSI,
	.mode = MCDE_PORTMODE_CMD,
	.pixel_format = MCDE_PORTPIXFMT_DSI_24BPP,
	.ifc = DSI_VIDEO_MODE,
	.link = 1,
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

struct mcde_display_hdmi_platform_data av8100_hdmi_pdata = {
	.rgb_2_yCbCr_transform = &rgb_2_yCbCr_transform,
};

static struct mcde_display_device av8100_hdmi = {
	.name = "av8100_hdmi",
	.id = AV8100_DISPLAY_ID,
	.port = &port2,
	.chnl_id = MCDE_CHNL_B,
	.fifo = MCDE_FIFO_B,
	.default_pixel_format = MCDE_OVLYPIXFMT_RGB888,
	.native_x_res = 1280,
	.native_y_res = 720,
	.synchronized_update = false,
	.dev = {
		.platform_data = &av8100_hdmi_pdata,
	},
	.platform_enable = NULL,
	.platform_disable = NULL,
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

	struct mcde_display_sony_acx424akp_platform_data *pdata =
					ddev->dev.platform_data;

	if (event != MCDE_DSS_EVENT_DISPLAY_REGISTERED)
		return 0;

	if (ddev->id < PRIMARY_DISPLAY_ID || ddev->id >= MCDE_NR_OF_DISPLAYS)
		return 0;

	mcde_dss_get_native_resolution(ddev, &width, &height);

	if (pdata->disp_panel == DISPLAY_SONY_ACX424AKP)
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

#ifdef CONFIG_DISPLAY_AV8100_TRIPPLE_BUFFER
	if (ddev->id == AV8100_DISPLAY_ID)
		virtual_height = height * 3;
#endif
#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
	if (ddev->id == AV8100_DISPLAY_ID) {
#ifdef CONFIG_MCDE_DISPLAY_HDMI_FB_AUTO_CREATE
		hdmi_fb_onoff(ddev, 1, 0, 0);
#endif /* CONFIG_MCDE_DISPLAY_HDMI_FB_AUTO_CREATE */
		goto out;
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
			"Failed to create fb for display %s\n", ddev->name);
		goto display_postregistered_callback_err;
	} else {
		dev_info(&ddev->dev, "Framebuffer created (%s)\n", ddev->name);
	}

#ifdef CONFIG_DISPDEV
	mfb = to_mcde_fb(fbi);
	/* Create a dispdev overlay for this display */
	if (dispdev_create(ddev, true, mfb->ovlys[0]) < 0) {
		dev_warn(&ddev->dev,
			"Failed to create disp for display %s\n", ddev->name);
		goto display_postregistered_callback_err;
	} else {
		dev_info(&ddev->dev, "Disp dev created for (%s)\n", ddev->name);
	}
#endif

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
out:
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

int __init init_u5500_display_devices(void)
{
	int ret = 0;

	if (!cpu_is_u5500())
		return ret;

	ret = fb_register_client(&framebuffer_nb);
	if (ret)
		pr_warning("Failed to register framebuffer notifier\n");

	ret = mcde_dss_register_notifier(&display_nb);
	if (ret)
		pr_warning("Failed to register dss notifier\n");

#ifdef CONFIG_DISPLAY_GENERIC_PRIMARY
	if (cpu_is_u5500v1()) {
		if (display_initialized_during_boot)
			generic_display0.power_mode = MCDE_DISPLAY_PM_STANDBY;
		ret = mcde_display_device_register(&generic_display0);
		if (ret)
			pr_warning("Failed to register generic \
							display device 0\n");
	}
#endif

#ifdef CONFIG_DISPLAY_SONY_ACX424AKP_DSI_PRIMARY
	if (cpu_is_u5500v2()) {
		if (display_initialized_during_boot)
			sony_acx424akp_display0.power_mode = \
						 MCDE_DISPLAY_PM_STANDBY;
		ret = mcde_display_device_register(&sony_acx424akp_display0);
		if (ret)
			pr_warning("Failed to register sony acx424akp \
							display device 0\n");
	}
#endif

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
	INIT_DELAYED_WORK_DEFERRABLE(&work_dispreg_hdmi,
			delayed_work_dispreg_hdmi);

	schedule_delayed_work(&work_dispreg_hdmi,
			msecs_to_jiffies(DISPREG_HDMI_DELAY));
#endif

	return ret;
}

module_init(init_display_devices);
#endif

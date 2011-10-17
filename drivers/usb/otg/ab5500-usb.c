/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: Avinash Kumar <avinash.kumar@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/platform_device.h>
#include <linux/usb/otg.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mfd/abx500/ab5500.h>
#include <linux/mfd/abx500.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <mach/usb.h>
#include <linux/kernel_stat.h>
#include <mach/gpio.h>

/* AB5500 USB macros
 */
#define AB5500_USB_HOST_ENABLE 0x1
#define AB5500_USB_DEVICE_ENABLE 0x2
#define AB5500_MAIN_WATCHDOG_ENABLE 0x1
#define AB5500_MAIN_WATCHDOG_KICK 0x2
#define AB5500_MAIN_WATCHDOG_DISABLE 0x0
#define AB5500_USB_ADP_ENABLE 0x1
#define AB5500_WATCHDOG_DELAY 10
#define AB5500_WATCHDOG_DELAY_US 100
#define AB5500_PHY_DELAY_US 100
#define AB5500_MAIN_WDOG_CTRL_REG      0x01
#define AB5500_USB_LINE_STAT_REG       0x80
#define AB5500_USB_PHY_CTRL_REG        0x8A
#define AB5500_MAIN_WATCHDOG_ENABLE 0x1
#define AB5500_MAIN_WATCHDOG_KICK 0x2
#define AB5500_MAIN_WATCHDOG_DISABLE 0x0
#define AB5500_SYS_CTRL2_BLOCK	0x2

/* UsbLineStatus register bit masks */
#define AB5500_USB_LINK_STATUS_MASK_V1		0x78
#define AB5500_USB_LINK_STATUS_MASK_V2		0xF8

#define USB_PROBE_DELAY 1000 /* 1 seconds */

/* UsbLineStatus register - usb types */
enum ab8500_usb_link_status {
	USB_LINK_NOT_CONFIGURED,
	USB_LINK_STD_HOST_NC,
	USB_LINK_STD_HOST_C_NS,
	USB_LINK_STD_HOST_C_S,
	USB_LINK_HOST_CHG_NM,
	USB_LINK_HOST_CHG_HS,
	USB_LINK_HOST_CHG_HS_CHIRP,
	USB_LINK_DEDICATED_CHG,
	USB_LINK_ACA_RID_A,
	USB_LINK_ACA_RID_B,
	USB_LINK_ACA_RID_C_NM,
	USB_LINK_ACA_RID_C_HS,
	USB_LINK_ACA_RID_C_HS_CHIRP,
	USB_LINK_HM_IDGND,
	USB_LINK_OTG_HOST_NO_CURRENT,
	USB_LINK_NOT_VALID_LINK,
	USB_LINK_HM_IDGND_V2 = 18,
};

/**
 * ab5500_usb_mode - Different states of ab usb_chip
 *
 * Used for USB cable plug-in state machine
 */
enum ab5500_usb_mode {
	USB_IDLE,
	USB_DEVICE,
	USB_HOST,
	USB_DEDICATED_CHG,
};
struct ab5500_usb {
	struct otg_transceiver otg;
	struct device *dev;
	int irq_num_id_fall;
	int irq_num_vbus_rise;
	int irq_num_vbus_fall;
	int irq_num_link_status;
	unsigned vbus_draw;
	struct delayed_work dwork;
	struct work_struct phy_dis_work;
	unsigned long link_status_wait;
	int rev;
	int usb_cs_gpio;
	enum ab5500_usb_mode mode;
	struct clk *sysclk;
	struct regulator *v_ape;
	struct abx500_usbgpio_platform_data *usb_gpio;
	struct delayed_work work_usb_workaround;
	bool phy_enabled;
};

static int ab5500_usb_irq_setup(struct platform_device *pdev,
				struct ab5500_usb *ab);
static int ab5500_usb_boot_detect(struct ab5500_usb *ab);
static int ab5500_usb_link_status_update(struct ab5500_usb *ab);

static void ab5500_usb_phy_enable(struct ab5500_usb *ab, bool sel_host);

static inline struct ab5500_usb *xceiv_to_ab(struct otg_transceiver *x)
{
	return container_of(x, struct ab5500_usb, otg);
}

/**
 * ab5500_usb_wd_workaround() - Kick the watch dog timer
 *
 * This function used to Kick the watch dog timer
 */
static void ab5500_usb_wd_workaround(struct ab5500_usb *ab)
{
	abx500_set_register_interruptible(ab->dev,
			AB5500_SYS_CTRL2_BLOCK,
			AB5500_MAIN_WDOG_CTRL_REG,
			AB5500_MAIN_WATCHDOG_ENABLE);

		udelay(AB5500_WATCHDOG_DELAY_US);

	abx500_set_register_interruptible(ab->dev,
			AB5500_SYS_CTRL2_BLOCK,
			AB5500_MAIN_WDOG_CTRL_REG,
			(AB5500_MAIN_WATCHDOG_ENABLE
			 | AB5500_MAIN_WATCHDOG_KICK));

		udelay(AB5500_WATCHDOG_DELAY_US);

	abx500_set_register_interruptible(ab->dev,
			AB5500_SYS_CTRL2_BLOCK,
			AB5500_MAIN_WDOG_CTRL_REG,
			AB5500_MAIN_WATCHDOG_DISABLE);

		udelay(AB5500_WATCHDOG_DELAY_US);
}

static void ab5500_usb_phy_enable(struct ab5500_usb *ab, bool sel_host)
{
	u8 bit;
	/* Workaround for spurious interrupt to be checked with Hardware Team*/
	if (ab->phy_enabled == true)
		return;
	ab->phy_enabled = true;
	bit = sel_host ? AB5500_USB_HOST_ENABLE :
			AB5500_USB_DEVICE_ENABLE;

	ab->usb_gpio->enable();
	clk_enable(ab->sysclk);
	regulator_enable(ab->v_ape);

	if (!sel_host) {
		schedule_delayed_work_on(0,
					&ab->work_usb_workaround,
					msecs_to_jiffies(USB_PROBE_DELAY));
	}

	abx500_mask_and_set_register_interruptible(ab->dev,
			AB5500_BANK_USB,
			AB5500_USB_PHY_CTRL_REG,
			bit, bit);
}

static void ab5500_usb_phy_disable(struct ab5500_usb *ab, bool sel_host)
{
	u8 bit;
	/* Workaround for spurious interrupt to be checked with Hardware Team*/
	if (ab->phy_enabled == false)
		return;
	ab->phy_enabled = false;
	bit = sel_host ? AB5500_USB_HOST_ENABLE :
			AB5500_USB_DEVICE_ENABLE;

	abx500_mask_and_set_register_interruptible(ab->dev,
			AB5500_BANK_USB,
			AB5500_USB_PHY_CTRL_REG,
			bit, 0);
	/* Needed to disable the phy.*/
	ab5500_usb_wd_workaround(ab);
	clk_disable(ab->sysclk);
	regulator_disable(ab->v_ape);
	ab->usb_gpio->disable();
}

#define ab5500_usb_peri_phy_en(ab)	ab5500_usb_phy_enable(ab, false)
#define ab5500_usb_peri_phy_dis(ab)	ab5500_usb_phy_disable(ab, false)
#define ab5500_usb_host_phy_en(ab)	ab5500_usb_phy_enable(ab, true)
#define ab5500_usb_host_phy_dis(ab)	ab5500_usb_phy_disable(ab, true)

/* Work created after an link status update handler*/
static int ab5500_usb_link_status_update(struct ab5500_usb *ab)
{
	u8 val = 0;
	int ret = 0;
	int gpioval = 0;
	enum ab8500_usb_link_status lsts;
	enum usb_xceiv_events event = USB_IDLE;

	(void)abx500_get_register_interruptible(ab->dev,
			AB5500_BANK_USB, AB5500_USB_LINE_STAT_REG, &val);

	if (ab->rev == AB5500_2_0)
		lsts = (val & AB5500_USB_LINK_STATUS_MASK_V2) >> 3;
	else
		lsts = (val & AB5500_USB_LINK_STATUS_MASK_V1) >> 3;

	switch (lsts) {

	case USB_LINK_STD_HOST_NC:
	case USB_LINK_STD_HOST_C_NS:
	case USB_LINK_STD_HOST_C_S:
	case USB_LINK_HOST_CHG_NM:
	case USB_LINK_HOST_CHG_HS:
	case USB_LINK_HOST_CHG_HS_CHIRP:
		break;

	case USB_LINK_HM_IDGND:
		if (ab->rev == AB5500_2_0)
			break;

		/* enable usb chip Select */
		ret = gpio_direction_output(ab->usb_cs_gpio, gpioval);
		if (ret < 0) {
			dev_err(ab->dev, "usb_cs_gpio: gpio direction failed\n");
			gpio_free(ab->usb_cs_gpio);
			return ret;
		}
		gpio_set_value(ab->usb_cs_gpio, 1);

		ab5500_usb_host_phy_en(ab);

		ab->otg.default_a = true;
		event = USB_EVENT_ID;

		break;

	case USB_LINK_HM_IDGND_V2:
		if (!(ab->rev == AB5500_2_0))
			break;

		/* enable usb chip Select */
		ret = gpio_direction_output(ab->usb_cs_gpio, gpioval);
		if (ret < 0) {
			dev_err(ab->dev, "usb_cs_gpio: gpio direction failed\n");
			gpio_free(ab->usb_cs_gpio);
			return ret;
		}
		gpio_set_value(ab->usb_cs_gpio, 1);

		ab5500_usb_host_phy_en(ab);

		ab->otg.default_a = true;
		event = USB_EVENT_ID;

		break;
	default:
		break;
	}

	atomic_notifier_call_chain(&ab->otg.notifier, event, &ab->vbus_draw);

	return 0;
}

static void ab5500_usb_delayed_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ab5500_usb *ab = container_of(dwork, struct ab5500_usb, dwork);

	ab5500_usb_link_status_update(ab);
}

/**
 * This function is used to signal the completion of
 * USB Link status register update
 */
static irqreturn_t ab5500_usb_link_status_irq(int irq, void *data)
{
	struct ab5500_usb *ab = (struct ab5500_usb *) data;
	ab5500_usb_link_status_update(ab);

	return IRQ_HANDLED;
}

static irqreturn_t ab5500_usb_device_insert_irq(int irq, void *data)
{
	int ret = 0, val = 1;
	struct ab5500_usb *ab = (struct ab5500_usb *) data;

	enum usb_xceiv_events event;

	ab->mode = USB_DEVICE;

	ab5500_usb_peri_phy_en(ab);

	/* enable usb chip Select */
	event = USB_EVENT_VBUS;
	ret = gpio_direction_output(ab->usb_cs_gpio, val);
	if (ret < 0) {
		dev_err(ab->dev, "usb_cs_gpio: gpio direction failed\n");
		gpio_free(ab->usb_cs_gpio);
		return ret;
	}
	gpio_set_value(ab->usb_cs_gpio, 1);

	atomic_notifier_call_chain(&ab->otg.notifier, event, &ab->vbus_draw);

	return IRQ_HANDLED;
}

/**
 * This function used to remove the voltage for USB ab->dev mode.
 */
static irqreturn_t ab5500_usb_device_disconnect_irq(int irq, void *data)
{
	struct ab5500_usb *ab = (struct ab5500_usb *) data;
	/* disable usb chip Select */
	gpio_set_value(ab->usb_cs_gpio, 0);
	ab5500_usb_peri_phy_dis(ab);
	return IRQ_HANDLED;
}

/**
 * ab5500_usb_host_disconnect_irq : work handler for host cable insert.
 * @work: work structure
 *
 * This function is used to handle the host cable insert work.
 */
static irqreturn_t ab5500_usb_host_disconnect_irq(int irq, void *data)
{
	struct ab5500_usb *ab = (struct ab5500_usb *) data;
	/* disable usb chip Select */
	gpio_set_value(ab->usb_cs_gpio, 0);
	ab5500_usb_host_phy_dis(ab);
	return IRQ_HANDLED;
}

static void ab5500_usb_irq_free(struct ab5500_usb *ab)
{
	if (ab->irq_num_id_fall)
		free_irq(ab->irq_num_id_fall, ab);

	if (ab->irq_num_vbus_rise)
		free_irq(ab->irq_num_vbus_rise, ab);

	if (ab->irq_num_vbus_fall)
		free_irq(ab->irq_num_vbus_fall, ab);

	if (ab->irq_num_link_status)
		free_irq(ab->irq_num_link_status, ab);
}

/**
 * ab5500_usb_irq_setup : register USB callback handlers for ab5500
 * @mode: value for mode.
 *
 * This function is used to register USB callback handlers for ab5500.
 */
static int ab5500_usb_irq_setup(struct platform_device *pdev,
				struct ab5500_usb *ab)
{
	int ret = 0;
	int irq, err;

	if (!ab->dev)
		return -EINVAL;

	irq = platform_get_irq_byname(pdev, "usb_idgnd_f");
	if (irq < 0) {
		dev_err(&pdev->dev, "ID fall irq not found\n");
		err = irq;
		goto irq_fail;
	}
	ab->irq_num_id_fall = irq;

	irq = platform_get_irq_byname(pdev, "VBUS_F");
	if (irq < 0) {
		dev_err(&pdev->dev, "VBUS fall irq not found\n");
		err = irq;
		goto irq_fail;

	}
	ab->irq_num_vbus_fall = irq;

	irq = platform_get_irq_byname(pdev, "VBUS_R");
	if (irq < 0) {
		dev_err(&pdev->dev, "VBUS raise irq not found\n");
		err = irq;
		goto irq_fail;

	}
	ab->irq_num_vbus_rise = irq;

	irq = platform_get_irq_byname(pdev, "Link_Update");
	if (irq < 0) {
		dev_err(&pdev->dev, "Link Update irq not found\n");
		err = irq;
		goto irq_fail;
	}
	ab->irq_num_link_status = irq;

	ret = request_threaded_irq(ab->irq_num_link_status,
		NULL, ab5500_usb_link_status_irq,
		IRQF_NO_SUSPEND | IRQF_SHARED,
		"usb-link-status-update", ab);
	if (ret < 0) {
		printk(KERN_ERR "failed to set the callback"
				" handler for usb charge"
				" detect done\n");
		err = ret;
		goto irq_fail;
	}

	ret = request_threaded_irq(ab->irq_num_vbus_rise, NULL,
		ab5500_usb_device_insert_irq,
		IRQF_NO_SUSPEND | IRQF_SHARED,
		"usb-vbus-rise", ab);
	if (ret < 0) {
		printk(KERN_ERR "failed to set the callback"
				" handler for usb ab->dev"
				" insertion\n");
		err = ret;
		goto irq_fail;
	}

	ret = request_threaded_irq(ab->irq_num_vbus_fall, NULL,
		ab5500_usb_device_disconnect_irq,
		IRQF_NO_SUSPEND | IRQF_SHARED,
		"usb-vbus-fall", ab);
	if (ret < 0) {
		printk(KERN_ERR "failed to set the callback"
				" handler for usb ab->dev"
				" removal\n");
		err = ret;
		goto irq_fail;
	}

	ret = request_threaded_irq((ab->irq_num_id_fall), NULL,
		ab5500_usb_host_disconnect_irq,
		IRQF_NO_SUSPEND | IRQF_SHARED,
		"usb-id-fall", ab);
	if (ret < 0) {
		printk(KERN_ERR "failed to set the callback"
				" handler for usb host"
				" removal\n");
		err = ret;
		goto irq_fail;
	}

	ab5500_usb_wd_workaround(ab);
	return 0;

irq_fail:
	ab5500_usb_irq_free(ab);
	return err;
}

/**
 * ab5500_usb_boot_detect : detect the USB cable during boot time.
 * @mode: value for mode.
 *
 * This function is used to detect the USB cable during boot time.
 */
static int ab5500_usb_boot_detect(struct ab5500_usb *ab)
{
	int ret;
	int val = 1;
	int usb_status = 0;
	int gpioval = 0;
	enum ab8500_usb_link_status lsts;
	if (!ab->dev)
		return -EINVAL;

	abx500_mask_and_set_register_interruptible(ab->dev,
			AB5500_BANK_USB,
			AB5500_USB_PHY_CTRL_REG,
			AB5500_USB_DEVICE_ENABLE,
			AB5500_USB_DEVICE_ENABLE);

	udelay(AB5500_PHY_DELAY_US);

	abx500_mask_and_set_register_interruptible(ab->dev,
			AB5500_BANK_USB,
			AB5500_USB_PHY_CTRL_REG,
			AB5500_USB_DEVICE_ENABLE, 0);

	abx500_mask_and_set_register_interruptible(ab->dev,
			AB5500_BANK_USB,
			AB5500_USB_PHY_CTRL_REG,
			AB5500_USB_HOST_ENABLE,
			AB5500_USB_HOST_ENABLE);

	udelay(AB5500_PHY_DELAY_US);

	abx500_mask_and_set_register_interruptible(ab->dev,
			AB5500_BANK_USB,
			AB5500_USB_PHY_CTRL_REG,
			AB5500_USB_HOST_ENABLE, 0);

	(void)abx500_get_register_interruptible(ab->dev,
			AB5500_BANK_USB, AB5500_USB_LINE_STAT_REG, &usb_status);

	if (ab->rev == AB5500_2_0)
		lsts = (usb_status & AB5500_USB_LINK_STATUS_MASK_V2) >> 3;
	else
		lsts = (usb_status & AB5500_USB_LINK_STATUS_MASK_V1) >> 3;

	switch (lsts) {

	case USB_LINK_STD_HOST_NC:
	case USB_LINK_STD_HOST_C_NS:
	case USB_LINK_STD_HOST_C_S:
	case USB_LINK_HOST_CHG_NM:
	case USB_LINK_HOST_CHG_HS:
	case USB_LINK_HOST_CHG_HS_CHIRP:

		ab5500_usb_peri_phy_en(ab);

		/* enable usb chip Select */
		ret = gpio_direction_output(ab->usb_cs_gpio, val);
		if (ret < 0) {
			dev_err(ab->dev, "usb_cs_gpio: gpio direction failed\n");
			gpio_free(ab->usb_cs_gpio);
			return ret;
		}
		gpio_set_value(ab->usb_cs_gpio, 1);

		break;

	case USB_LINK_HM_IDGND:
	case USB_LINK_HM_IDGND_V2:
		/* enable usb chip Select */
		ret = gpio_direction_output(ab->usb_cs_gpio, gpioval);
		if (ret < 0) {
			dev_err(ab->dev, "usb_cs_gpio: gpio direction failed\n");
			gpio_free(ab->usb_cs_gpio);
			return ret;
		}
		gpio_set_value(ab->usb_cs_gpio, 1);
		ab5500_usb_host_phy_en(ab);

		break;
	default:
		break;
	}

	return 0;
}

static int ab5500_usb_set_power(struct otg_transceiver *otg, unsigned mA)
{
	struct ab5500_usb *ab;

	if (!otg)
		return -ENODEV;

	ab = xceiv_to_ab(otg);

	ab->vbus_draw = mA;

	atomic_notifier_call_chain(&ab->otg.notifier,
				USB_EVENT_VBUS, &ab->vbus_draw);
	return 0;
}

static int ab5500_usb_set_suspend(struct otg_transceiver *x, int suspend)
{
	/* TODO */
	return 0;
}

static int ab5500_usb_set_host(struct otg_transceiver *otg,
					struct usb_bus *host)
{
	struct ab5500_usb *ab;

	if (!otg)
		return -ENODEV;

	ab = xceiv_to_ab(otg);

	/* Some drivers call this function in atomic context.
	 * Do not update ab5500 registers directly till this
	 * is fixed.
	 */

	if (!host) {
		ab->otg.host = NULL;
		schedule_work(&ab->phy_dis_work);
	} else {
		ab->otg.host = host;
	}

	return 0;
}

static int ab5500_usb_set_peripheral(struct otg_transceiver *otg,
		struct usb_gadget *gadget)
{
	struct ab5500_usb *ab;

	if (!otg)
		return -ENODEV;

	ab = xceiv_to_ab(otg);

	/* Some drivers call this function in atomic context.
	 * Do not update ab5500 registers directly till this
	 * is fixed.
	 */

	if (!gadget) {
		ab->otg.gadget = NULL;
		schedule_work(&ab->phy_dis_work);
	} else {
		ab->otg.gadget = gadget;
	}

	return 0;
}

static int __devinit ab5500_usb_probe(struct platform_device *pdev)
{
	struct ab5500_usb	*ab;
	struct abx500_usbgpio_platform_data *usb_pdata =
				pdev->dev.platform_data;
	int err;
	int ret = -1;
	ab = kzalloc(sizeof *ab, GFP_KERNEL);
	if (!ab)
		return -ENOMEM;

	ab->dev			= &pdev->dev;
	ab->otg.dev		= ab->dev;
	ab->otg.label		= "ab5500";
	ab->otg.state		= OTG_STATE_B_IDLE;
	ab->otg.set_host	= ab5500_usb_set_host;
	ab->otg.set_peripheral	= ab5500_usb_set_peripheral;
	ab->otg.set_suspend	= ab5500_usb_set_suspend;
	ab->otg.set_power	= ab5500_usb_set_power;
	ab->usb_gpio		= usb_pdata;
	ab->mode			= USB_IDLE;

	platform_set_drvdata(pdev, ab);

	ATOMIC_INIT_NOTIFIER_HEAD(&ab->otg.notifier);

	/* v1: Wait for link status to become stable.
	 * all: Updates form set_host and set_peripheral as they are atomic.
	 */
	INIT_DELAYED_WORK(&ab->dwork, ab5500_usb_delayed_work);

	err = otg_set_transceiver(&ab->otg);
	if (err)
		dev_err(&pdev->dev, "Can't register transceiver\n");

	ab->usb_cs_gpio = ab->usb_gpio->usb_cs;

	ab->rev = abx500_get_chip_id(ab->dev);

	ab->sysclk = clk_get(ab->dev, "sysclk");
	if (IS_ERR(ab->sysclk)) {
		ret = PTR_ERR(ab->sysclk);
		ab->sysclk = NULL;
		return ret;
	}

	ab->v_ape = regulator_get(ab->dev, "v-ape");
	if (!ab->v_ape) {
		dev_err(ab->dev, "Could not get v-ape supply\n");

		return -EINVAL;
	}

	ab5500_usb_irq_setup(pdev, ab);

	ret = gpio_request(ab->usb_cs_gpio, "usb-cs");
	if (ret < 0)
		dev_err(&pdev->dev, "usb gpio request fail\n");

	/* Aquire GPIO alternate config struct for USB */
	err = ab->usb_gpio->get(ab->dev);
	if (err < 0)
		goto fail1;

	err = ab5500_usb_boot_detect(ab);
	if (err < 0)
		goto fail1;

	return 0;

fail1:
	ab5500_usb_irq_free(ab);
	kfree(ab);
	return err;
}

static int __devexit ab5500_usb_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver ab5500_usb_driver = {
	.driver		= {
		.name	= "ab5500-usb",
		.owner	= THIS_MODULE,
	},
	.probe		= ab5500_usb_probe,
	.remove		= __devexit_p(ab5500_usb_remove),
};

static int __init ab5500_usb_init(void)
{
	return platform_driver_register(&ab5500_usb_driver);
}
subsys_initcall(ab5500_usb_init);

static void __exit ab5500_usb_exit(void)
{
	platform_driver_unregister(&ab5500_usb_driver);
}
module_exit(ab5500_usb_exit);

MODULE_LICENSE("GPL v2");

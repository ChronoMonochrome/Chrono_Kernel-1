/*
 * drivers/usb/otg/ab8500_usb.c
 *
 * USB transceiver driver for AB8500 chip
 *
 * Copyright (C) 2010 ST-Ericsson AB
 * Mian Yousaf Kaukab <mian.yousaf.kaukab@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/otg.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/kernel_stat.h>
#include <linux/pm_qos.h>

#include <asm/io.h>

#include <mach/usb.h>

#define AB8500_MAIN_WD_CTRL_REG 0x01
#define AB8500_USB_LINE_STAT_REG 0x80
#define AB8500_USB_PHY_CTRL_REG 0x8A
#define AB8500_VBUS_CTRL_REG 0x82
#define AB8500_IT_SOURCE2_REG 0x01
#define AB8500_IT_SOURCE20_REG 0x13
#define AB8500_SRC_INT_USB_HOST 0x04
#define AB8500_SRC_INT_USB_DEVICE 0x80

#define AB8500_BIT_OTG_STAT_ID (1 << 0)
#define AB8500_BIT_PHY_CTRL_HOST_EN (1 << 0)
#define AB8500_BIT_PHY_CTRL_DEVICE_EN (1 << 1)
#define AB8500_BIT_WD_CTRL_ENABLE (1 << 0)
#define AB8500_BIT_WD_CTRL_KICK (1 << 1)
#define AB8500_BIT_VBUS_ENABLE (1 << 0)

#define AB8500_V1x_LINK_STAT_WAIT (HZ/10)
#define AB8500_WD_KICK_DELAY_US 100 /* usec */
#define AB8500_WD_V11_DISABLE_DELAY_US 100 /* usec */
#define AB8500_V20_31952_DISABLE_DELAY_US 100 /* usec */
#define AB8500_WD_V10_DISABLE_DELAY_MS 100 /* ms */

/* Registers in bank 0x11 */
#define AB8500_BANK12_ACCESS	0x00

/* Registers in bank 0x12 */
#define AB8500_USB_PHY_TUNE1	0x05
#define AB8500_USB_PHY_TUNE2	0x06
#define AB8500_USB_PHY_TUNE3	0x07

static struct pm_qos_request usb_pm_qos_latency;
static bool usb_pm_qos_is_latency_0;

#define USB_PROBE_DELAY 1000 /* 1 seconds */
#define USB_LIMIT (200) /* If we have more than 200 irqs per second */

#define PUBLIC_ID_BACKUPRAM1 (U8500_BACKUPRAM1_BASE + 0x0FC0)
#define MAX_USB_SERIAL_NUMBER_LEN 31

/* Usb line status register */
enum ab8500_usb_link_status {
	USB_LINK_NOT_CONFIGURED = 0,
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
	USB_LINK_RESERVED,
	USB_LINK_NOT_VALID_LINK
};

enum ab8500_usb_mode {
	USB_IDLE = 0,
	USB_PERIPHERAL,
	USB_HOST,
	USB_DEDICATED_CHG
};

struct ab8500_usb {
	struct usb_phy phy;
	struct device *dev;
	int irq_num_id_rise;
	int irq_num_id_fall;
	int irq_num_vbus_rise;
	int irq_num_vbus_fall;
	int irq_num_link_status;
	unsigned vbus_draw;
	struct delayed_work dwork;
	struct work_struct phy_dis_work;
	unsigned long link_status_wait;
	int rev;
	enum ab8500_usb_mode mode;
	struct clk *sysclk;
	struct regulator *v_ape;
	struct regulator *v_musb;
	struct regulator *v_ulpi;
	struct abx500_usbgpio_platform_data *usb_gpio;
	struct delayed_work work_usb_workaround;
	struct kobject *serial_number_kobj;
};

static inline struct ab8500_usb *phy_to_ab(struct usb_phy *x)
{
	return container_of(x, struct ab8500_usb, phy);
}

static void ab8500_usb_wd_workaround(struct ab8500_usb *ab)
{
	abx500_set_register_interruptible(ab->dev,
		AB8500_SYS_CTRL2_BLOCK,
		AB8500_MAIN_WD_CTRL_REG,
		AB8500_BIT_WD_CTRL_ENABLE);

	udelay(AB8500_WD_KICK_DELAY_US);

	abx500_set_register_interruptible(ab->dev,
		AB8500_SYS_CTRL2_BLOCK,
		AB8500_MAIN_WD_CTRL_REG,
		(AB8500_BIT_WD_CTRL_ENABLE
		| AB8500_BIT_WD_CTRL_KICK));

	if (ab->rev > 0x10) /* v2.0 v3.0 */
		udelay(AB8500_WD_V11_DISABLE_DELAY_US);

	abx500_set_register_interruptible(ab->dev,
		AB8500_SYS_CTRL2_BLOCK,
		AB8500_MAIN_WD_CTRL_REG,
		0);
}

static void ab8500_usb_load(struct work_struct *work)
{
	int cpu;
	unsigned int num_irqs = 0;
	static unsigned int old_num_irqs = UINT_MAX;
	struct delayed_work *work_usb_workaround = to_delayed_work(work);
	struct ab8500_usb *ab = container_of(work_usb_workaround,
				struct ab8500_usb, work_usb_workaround);

	for_each_online_cpu(cpu)
	num_irqs += kstat_irqs_cpu(IRQ_DB8500_USBOTG, cpu);

	if ((num_irqs > old_num_irqs) &&
		(num_irqs - old_num_irqs) > USB_LIMIT) {

		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
							"usb", 125);
		if (!usb_pm_qos_is_latency_0) {

			pm_qos_add_request(&usb_pm_qos_latency,
						PM_QOS_CPU_DMA_LATENCY, 0);
			usb_pm_qos_is_latency_0 = true;
		}
	} else {

		if (usb_pm_qos_is_latency_0) {

				pm_qos_remove_request(&usb_pm_qos_latency);
				usb_pm_qos_is_latency_0 = false;
		}

		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
							"usb", 25);
	}
	old_num_irqs = num_irqs;

	schedule_delayed_work_on(0,
				&ab->work_usb_workaround,
				msecs_to_jiffies(USB_PROBE_DELAY));
}

static void ab8500_usb_regulator_ctrl(struct ab8500_usb *ab, bool sel_host,
					bool enable)
{
	int ret = 0, volt = 0;

	if (enable) {
		regulator_enable(ab->v_ape);
		if (ab->rev >= 0x30) {
			ret = regulator_set_voltage(ab->v_ulpi,
						1300000, 1350000);
			if (ret < 0)
				dev_err(ab->dev, "Failed to set the Vintcore"
						" to 1.3V, ret=%d\n", ret);
			ret = regulator_set_optimum_mode(ab->v_ulpi,
								28000);
			if (ret < 0)
				dev_err(ab->dev, "Failed to set optimum mode"
						" (ret=%d)\n", ret);

		}
		regulator_enable(ab->v_ulpi);
		if (ab->rev >= 0x30) {
			volt = regulator_get_voltage(ab->v_ulpi);
			if ((volt != 1300000) && (volt != 1350000))
					dev_err(ab->dev, "Vintcore is not"
							" set to 1.3V"
							" volt=%d\n", volt);
		}
		regulator_enable(ab->v_musb);

	} else {
		regulator_disable(ab->v_musb);
		regulator_disable(ab->v_ulpi);
		regulator_disable(ab->v_ape);
	}
}


static void ab8500_usb_phy_enable(struct ab8500_usb *ab, bool sel_host)
{
	u8 bit;
	bit = sel_host ? AB8500_BIT_PHY_CTRL_HOST_EN :
			AB8500_BIT_PHY_CTRL_DEVICE_EN;

	ab->usb_gpio->enable();
	clk_enable(ab->sysclk);

	ab8500_usb_regulator_ctrl(ab, sel_host, true);

	prcmu_qos_update_requirement(PRCMU_QOS_APE_OPP,
				(char *)dev_name(ab->dev), 100);

	schedule_delayed_work_on(0,
					&ab->work_usb_workaround,
					msecs_to_jiffies(USB_PROBE_DELAY));

	abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				bit,
				bit);

}

static void ab8500_usb_wd_linkstatus(struct ab8500_usb *ab,u8 bit)
{
	/* Wrokaround for v2.0 bug # 31952 */
	if (ab->rev == 0x20) {
		abx500_mask_and_set_register_interruptible(ab->dev,
					AB8500_USB,
					AB8500_USB_PHY_CTRL_REG,
					bit,
					bit);
		udelay(AB8500_V20_31952_DISABLE_DELAY_US);
	}
}

static void ab8500_usb_phy_disable(struct ab8500_usb *ab, bool sel_host)
{
	u8 bit;
	bit = sel_host ? AB8500_BIT_PHY_CTRL_HOST_EN :
			AB8500_BIT_PHY_CTRL_DEVICE_EN;

	ab8500_usb_wd_linkstatus(ab,bit);

	abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				bit,
				0);

	/* Needed to disable the phy.*/
	ab8500_usb_wd_workaround(ab);

	clk_disable(ab->sysclk);

	ab8500_usb_regulator_ctrl(ab, sel_host, false);

	ab->usb_gpio->disable();

	prcmu_qos_update_requirement(PRCMU_QOS_APE_OPP,
				(char *)dev_name(ab->dev), 50);

	if (!sel_host) {

		cancel_delayed_work_sync(&ab->work_usb_workaround);
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
			"usb", 25);
	}
}

#define ab8500_usb_host_phy_en(ab)	ab8500_usb_phy_enable(ab, true)
#define ab8500_usb_host_phy_dis(ab)	ab8500_usb_phy_disable(ab, true)
#define ab8500_usb_peri_phy_en(ab)	ab8500_usb_phy_enable(ab, false)
#define ab8500_usb_peri_phy_dis(ab)	ab8500_usb_phy_disable(ab, false)

static int ab8500_usb_link_status_update(struct ab8500_usb *ab)
{
	u8 reg;
	enum ab8500_usb_link_status lsts;
	enum usb_phy_events event;

	abx500_get_register_interruptible(ab->dev,
			AB8500_USB,
			AB8500_USB_LINE_STAT_REG,
			&reg);

	lsts = (reg >> 3) & 0x0F;

	switch (lsts) {
	case USB_LINK_ACA_RID_B:
		event = USB_EVENT_RIDB;
	case USB_LINK_NOT_CONFIGURED:
	case USB_LINK_RESERVED:
	case USB_LINK_NOT_VALID_LINK:
		if (ab->mode == USB_HOST)
			ab8500_usb_host_phy_dis(ab);
		else if (ab->mode == USB_PERIPHERAL)
			ab8500_usb_peri_phy_dis(ab);
		ab->mode = USB_IDLE;
		ab->phy.otg.default_a = false;
		ab->vbus_draw = 0;
		if (event != USB_EVENT_RIDB)
			event = USB_EVENT_NONE;
		break;
	case USB_LINK_ACA_RID_C_NM:
	case USB_LINK_ACA_RID_C_HS:
	case USB_LINK_ACA_RID_C_HS_CHIRP:
		event = USB_EVENT_RIDC;
	case USB_LINK_STD_HOST_NC:
	case USB_LINK_STD_HOST_C_NS:
	case USB_LINK_STD_HOST_C_S:
	case USB_LINK_HOST_CHG_NM:
	case USB_LINK_HOST_CHG_HS:
	case USB_LINK_HOST_CHG_HS_CHIRP:
		if (ab->mode == USB_HOST) {
			ab->mode = USB_PERIPHERAL;
			ab8500_usb_host_phy_dis(ab);
			ux500_restore_context();
			ab8500_usb_peri_phy_en(ab);
		}
		if (ab->mode == USB_IDLE) {
			ab->mode = USB_PERIPHERAL;
			ux500_restore_context();
			ab8500_usb_peri_phy_en(ab);
		}
		if (event != USB_EVENT_RIDC)
			event = USB_EVENT_VBUS;
		break;

	case USB_LINK_ACA_RID_A:
		event = USB_EVENT_RIDA;
	case USB_LINK_HM_IDGND:
		if (ab->mode == USB_PERIPHERAL) {
			ab->mode = USB_HOST;
			ab8500_usb_peri_phy_dis(ab);
			ux500_restore_context();
			ab8500_usb_host_phy_en(ab);
		}
		if (ab->mode == USB_IDLE) {
			ab->mode = USB_HOST;
			ux500_restore_context();
			ab8500_usb_host_phy_en(ab);
		}
		ab->phy.otg->default_a = true;
		if (event != USB_EVENT_RIDA)
			event = USB_EVENT_ID;
		break;

	case USB_LINK_DEDICATED_CHG:
		/* TODO: vbus_draw */
		ab->mode = USB_DEDICATED_CHG;
		event = USB_EVENT_CHARGER;
		break;
	}

	atomic_notifier_call_chain(&ab->phy.notifier, event, &ab->vbus_draw);

	return 0;
}

static void ab8500_usb_delayed_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ab8500_usb *ab = container_of(dwork, struct ab8500_usb, dwork);

	ab8500_usb_link_status_update(ab);
}

static irqreturn_t ab8500_usb_disconnect_irq(int irq, void *data)
{
	struct ab8500_usb *ab = (struct ab8500_usb *) data;

	/* Link status will not be updated till phy is disabled. */
	if (ab->mode == USB_HOST)
		ab8500_usb_host_phy_dis(ab);
	else if (ab->mode == USB_PERIPHERAL)
		ab8500_usb_peri_phy_dis(ab);
	else if (ab->mode == USB_DEDICATED_CHG && ab->rev == 0x20) {
		ab8500_usb_wd_linkstatus(ab,AB8500_BIT_PHY_CTRL_DEVICE_EN);
		abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				AB8500_BIT_PHY_CTRL_DEVICE_EN,
				0);
	}
	ab->mode = USB_IDLE;

	return IRQ_HANDLED;
}

static irqreturn_t ab8500_usb_v20_link_status_irq(int irq, void *data)
{
	struct ab8500_usb *ab = (struct ab8500_usb *) data;

	ab8500_usb_link_status_update(ab);

	return IRQ_HANDLED;
}

static void ab8500_usb_phy_disable_work(struct work_struct *work)
{
	struct ab8500_usb *ab = container_of(work, struct ab8500_usb,
						phy_dis_work);

	if (!ab->phy.otg->host)
		ab8500_usb_host_phy_dis(ab);

	if (!ab->phy.otg->gadget)
		ab8500_usb_peri_phy_dis(ab);

}

static unsigned ab8500_eyediagram_workaroud(struct ab8500_usb *ab, unsigned mA)
{
	/* AB V2 has eye diagram issues when drawing more
	 * than 100mA from VBUS.So setting charging current
	 * to 100mA in case of standard host
	 */
	if ((ab->rev < 0x30) && (mA > 100))
		mA = 100;

	return mA;
}

static int ab8500_usb_set_power(struct usb_phy *phy, unsigned mA)
{
	struct ab8500_usb *ab;

	if (!phy)
		return -ENODEV;

	ab = phy_to_ab(phy);

	mA = ab8500_eyediagram_workaroud(ab, mA);

	ab->vbus_draw = mA;

	atomic_notifier_call_chain(&ab->phy.notifier,
				USB_EVENT_VBUS, &ab->vbus_draw);
	return 0;
}

/* TODO: Implement some way for charging or other drivers to read
 * ab->vbus_draw.
 */

static int ab8500_usb_set_suspend(struct usb_phy *x, int suspend)
{
	/* TODO */
	return 0;
}

static int ab8500_usb_set_peripheral(struct usb_otg *otg,
					struct usb_gadget *gadget)
{
	struct ab8500_usb *ab;

	if (!otg)
		return -ENODEV;

	ab = phy_to_ab(otg->phy);

	/* Some drivers call this function in atomic context.
	 * Do not update ab8500 registers directly till this
	 * is fixed.
	 */

	if (!gadget) {
		otg->gadget = NULL;
		schedule_work(&ab->phy_dis_work);
	} else {
		otg->gadget = gadget;

		/* Phy will not be enabled if cable is already
		 * plugged-in. Schedule to enable phy.
		 * Use same delay to avoid any race condition.
		 */
		schedule_delayed_work(&ab->dwork, ab->link_status_wait);
	}

	return 0;
}

static int ab8500_usb_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct ab8500_usb *ab;

	if (!otg)
		return -ENODEV;

	ab = phy_to_ab(otg->phy);

	/* Some drivers call this function in atomic context.
	 * Do not update ab8500 registers directly till this
	 * is fixed.
	 */

	if (!host) {
		otg->host = NULL;
		schedule_work(&ab->phy_dis_work);
	} else {
		otg->host = host;
		/* Phy will not be enabled if cable is already
		 * plugged-in. Schedule to enable phy.
		 * Use same delay to avoid any race condition.
		 */
		schedule_delayed_work(&ab->dwork, ab->link_status_wait);
	}

	return 0;
}
/**
 * ab8500_usb_boot_detect : detect the USB cable during boot time.
 * @device: value for device.
 *
 * This function is used to detect the USB cable during boot time.
 */
static int ab8500_usb_boot_detect(struct ab8500_usb *ab)
{
	int err;
	struct device *device = ab->dev;
	u8 usb_status = 0;
	u8 val = 0;

	/* Disabling PHY before selective enable or disable */
	abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				AB8500_BIT_PHY_CTRL_DEVICE_EN,
				AB8500_BIT_PHY_CTRL_DEVICE_EN);

	udelay(100);

	abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				AB8500_BIT_PHY_CTRL_DEVICE_EN,
				0);

	abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				AB8500_BIT_PHY_CTRL_HOST_EN,
				AB8500_BIT_PHY_CTRL_HOST_EN);

	udelay(100);

	abx500_mask_and_set_register_interruptible(ab->dev,
				AB8500_USB,
				AB8500_USB_PHY_CTRL_REG,
				AB8500_BIT_PHY_CTRL_HOST_EN,
				0);


	err = abx500_get_register_interruptible(device,
		AB8500_INTERRUPT, AB8500_IT_SOURCE20_REG,
		&usb_status);
	if (err < 0) {
		dev_err(device, "Read IT 20 failed\n");
		return err;
	}

	if (usb_status & AB8500_SRC_INT_USB_HOST)
		ab8500_usb_host_phy_en(ab);


	err = abx500_get_register_interruptible(device,
		AB8500_INTERRUPT, AB8500_IT_SOURCE2_REG,
		&usb_status);
	if (err < 0) {
		dev_err(device, "Read IT 2 failed\n");
		return err;
	}

	if (usb_status & AB8500_SRC_INT_USB_DEVICE) {
		/* Check if it is a dedicated charger */
		(void)abx500_get_register_interruptible(device,
		AB8500_USB, AB8500_USB_LINE_STAT_REG, &val);

		val = (val >> 3) & 0x0F;

	if (val == USB_LINK_DEDICATED_CHG)
		ab->mode = USB_DEDICATED_CHG;
	else
		ab8500_usb_peri_phy_en(ab);
	}

	return 0;
}

static void ab8500_usb_regulator_put(struct ab8500_usb *ab)
{

	if (ab->v_ape)
		regulator_put(ab->v_ape);

	if (ab->v_ulpi)
		regulator_put(ab->v_ulpi);

	if (ab->v_musb)
		regulator_put(ab->v_musb);
}

static int ab8500_usb_regulator_get(struct ab8500_usb *ab)
{
	int err;

	ab->v_ape = regulator_get(ab->dev, "v-ape");
	if (IS_ERR(ab->v_ape)) {
		dev_err(ab->dev, "Could not get v-ape supply\n");
		err = PTR_ERR(ab->v_ape);
		goto reg_error;
	}

	ab->v_ulpi = regulator_get(ab->dev, "vddulpivio18");
	if (IS_ERR(ab->v_ulpi)) {
		dev_err(ab->dev, "Could not get vddulpivio18 supply\n");
		err = PTR_ERR(ab->v_ulpi);
		goto reg_error;
	}

	ab->v_musb = regulator_get(ab->dev, "musb_1v8");
	if (IS_ERR(ab->v_musb)) {
		dev_err(ab->dev, "Could not get musb_1v8 supply\n");
		err = PTR_ERR(ab->v_musb);
		goto reg_error;
	}

	return 0;

reg_error:
	ab8500_usb_regulator_put(ab);
	return err;
}

static void ab8500_usb_irq_free(struct ab8500_usb *ab)
{
	if (ab->irq_num_id_rise)
		free_irq(ab->irq_num_id_rise, ab);

	if (ab->irq_num_id_fall)
		free_irq(ab->irq_num_id_fall, ab);

	if (ab->irq_num_vbus_rise)
		free_irq(ab->irq_num_vbus_rise, ab);

	if (ab->irq_num_vbus_fall)
		free_irq(ab->irq_num_vbus_fall, ab);

	if (ab->irq_num_link_status)
		free_irq(ab->irq_num_link_status, ab);
}

static int ab8500_usb_irq_setup(struct platform_device *pdev,
				struct ab8500_usb *ab)
{
	int err;
	int irq;

	if (ab->rev > 0x10) { /* 0x20 0x30 */
		irq = platform_get_irq_byname(pdev, "USB_LINK_STATUS");
		if (irq < 0) {
			err = irq;
			dev_err(&pdev->dev, "Link status irq not found\n");
			goto irq_fail;
		}

		err = request_threaded_irq(irq, NULL,
			ab8500_usb_v20_link_status_irq,
			IRQF_NO_SUSPEND | IRQF_SHARED,
			"usb-link-status", ab);
		if (err < 0) {
			dev_err(ab->dev,
				"request_irq failed for link status irq\n");
			return err;
		}
		ab->irq_num_link_status = irq;
	}

	irq = platform_get_irq_byname(pdev, "ID_WAKEUP_F");
	if (irq < 0) {
		err = irq;
		dev_err(&pdev->dev, "ID fall irq not found\n");
		return ab->irq_num_id_fall;
	}
	err = request_threaded_irq(irq, NULL,
		ab8500_usb_disconnect_irq,
		IRQF_NO_SUSPEND | IRQF_SHARED,
		"usb-id-fall", ab);
	if (err < 0) {
		dev_err(ab->dev, "request_irq failed for ID fall irq\n");
		goto irq_fail;
	}
	ab->irq_num_id_fall = irq;

	irq = platform_get_irq_byname(pdev, "VBUS_DET_F");
	if (irq < 0) {
		err = irq;
		dev_err(&pdev->dev, "VBUS fall irq not found\n");
		goto irq_fail;
	}
	err = request_threaded_irq(irq, NULL,
		ab8500_usb_disconnect_irq,
		IRQF_NO_SUSPEND | IRQF_SHARED,
		"usb-vbus-fall", ab);
	if (err < 0) {
		dev_err(ab->dev, "request_irq failed for Vbus fall irq\n");
		goto irq_fail;
	}
	ab->irq_num_vbus_fall = irq;

	return 0;

irq_fail:
	ab8500_usb_irq_free(ab);
	return err;
}

/* Sys interfaces */
static ssize_t usb_serial_number
		(struct kobject *kobj, struct attribute *attr, char *buf)
{
	u32 bufer[5];
	void __iomem *backup_ram = NULL;

	backup_ram = ioremap(PUBLIC_ID_BACKUPRAM1, 0x14);

	if (backup_ram) {
		bufer[0] = readl(backup_ram);
		bufer[1] = readl(backup_ram + 4);
		bufer[2] = readl(backup_ram + 8);
		bufer[3] = readl(backup_ram + 0x0c);
		bufer[4] = readl(backup_ram + 0x10);

		snprintf(buf, MAX_USB_SERIAL_NUMBER_LEN+1,
				"%.8X%.8X%.8X%.8X%.8X",
			bufer[0], bufer[1], bufer[2], bufer[3], bufer[4]);

		iounmap(backup_ram);
	} else
			printk(KERN_ERR "$$ ioremap failed\n");

	return strlen(buf);
}

static struct attribute usb_serial_number_attribute = \
			{.name = "serial_number", .mode = S_IRUGO};

static struct attribute *serial_number[] = {
	&usb_serial_number_attribute,
	NULL
};

const struct sysfs_ops usb_sysfs_ops = {
	.show  = usb_serial_number,
};

static struct kobj_type ktype_serial_number = {
	.sysfs_ops = &usb_sysfs_ops,
	.default_attrs = serial_number,
};

static int __devinit ab8500_usb_probe(struct platform_device *pdev)
{
	struct ab8500_usb	*ab;
	struct usb_otg		*otg;
	struct ab8500_platform_data *ab8500_pdata =
				dev_get_platdata(pdev->dev.parent);
	int err;
	int rev;
	int ret = -1;

	rev = abx500_get_chip_id(&pdev->dev);
	if (rev < 0) {
		dev_err(&pdev->dev, "Chip id read failed\n");
		return rev;
	} else if (rev < 0x20) {
		dev_err(&pdev->dev, "Unsupported AB8500 chip rev=%d\n", rev);
		return -ENODEV;
	}

	ab = kzalloc(sizeof *ab, GFP_KERNEL);
	if (!ab)
		return -ENOMEM;

	otg = kzalloc(sizeof *otg, GFP_KERNEL);
	if (!otg) {
		kfree(ab);
		return -ENOMEM;
	}

	ab->dev			= &pdev->dev;
	ab->rev			= rev;
	ab->phy.dev		= ab->dev;
	ab->phy.otg		= otg;
	ab->phy.label		= "ab8500";
	ab->phy.set_suspend	= ab8500_usb_set_suspend;
	ab->phy.set_power	= ab8500_usb_set_power;
	ab->phy.state		= OTG_STATE_B_IDLE;

	otg->phy		= &ab->phy;
	otg->set_host		= ab8500_usb_set_host;
	otg->set_peripheral	= ab8500_usb_set_peripheral;
	ab->usb_gpio		= ab8500_pdata->usb;

	platform_set_drvdata(pdev, ab);

	ATOMIC_INIT_NOTIFIER_HEAD(&ab->phy.notifier);

	/* v1: Wait for link status to become stable.
	 * all: Updates form set_host and set_peripheral as they are atomic.
	 */
	INIT_DELAYED_WORK(&ab->dwork, ab8500_usb_delayed_work);

	/* all: Disable phy when called from set_host and set_peripheral */
	INIT_WORK(&ab->phy_dis_work, ab8500_usb_phy_disable_work);

	INIT_DELAYED_WORK_DEFERRABLE(&ab->work_usb_workaround,
							ab8500_usb_load);
	err = ab8500_usb_regulator_get(ab);
	if (err)
		goto fail0;

	ab->sysclk = clk_get(ab->dev, "sysclk");
	if (IS_ERR(ab->sysclk)) {
		err = PTR_ERR(ab->sysclk);
		goto fail1;
	}

	err = ab8500_usb_irq_setup(pdev, ab);
	if (err < 0)
		goto fail2;

	err = usb_set_transceiver(&ab->phy);
	if (err) {
		dev_err(&pdev->dev, "Can't register transceiver\n");
		goto fail3;
	}

	/* Write Phy tuning values */
	if (ab->rev >= 0x30) {
		/* Enable the PBT/Bank 0x12 access */
		ret = abx500_set_register_interruptible(ab->dev,
							AB8500_DEVELOPMENT,
							AB8500_BANK12_ACCESS,
							0x01);
		if (ret < 0)
			printk(KERN_ERR "Failed to enable bank12"
						" access ret=%d\n", ret);

			ret = abx500_set_register_interruptible(ab->dev,
							AB8500_DEBUG,
							AB8500_USB_PHY_TUNE1,
							0xC8);
		if (ret < 0)
			printk(KERN_ERR "Failed to set PHY_TUNE1"
						" register ret=%d\n", ret);

			ret = abx500_set_register_interruptible(ab->dev,
							AB8500_DEBUG,
							AB8500_USB_PHY_TUNE2,
							0x00);
		if (ret < 0)
			printk(KERN_ERR "Failed to set PHY_TUNE2"
						" register ret=%d\n", ret);

			ret = abx500_set_register_interruptible(ab->dev,
							AB8500_DEBUG,
							AB8500_USB_PHY_TUNE3,
							0x78);

		if (ret < 0)
			printk(KERN_ERR "Failed to set PHY_TUNE3"
						" regester ret=%d\n", ret);

		/* Switch to normal mode/disable Bank 0x12 access */
			ret = abx500_set_register_interruptible(ab->dev,
							AB8500_DEVELOPMENT,
							AB8500_BANK12_ACCESS,
							0x00);

		if (ret < 0)
			printk(KERN_ERR "Failed to switch bank12"
						" access ret=%d\n", ret);
	}
	/* Needed to enable ID detection. */
	ab8500_usb_wd_workaround(ab);

	ab->serial_number_kobj = kzalloc(sizeof(struct kobject), GFP_KERNEL);

	if (ab->serial_number_kobj == NULL)
		ret = -ENOMEM;
	ab->serial_number_kobj->ktype = &ktype_serial_number;
	kobject_init(ab->serial_number_kobj, ab->serial_number_kobj->ktype);

	ret = kobject_set_name(ab->serial_number_kobj, "usb_serial_number");
	if (ret)
		kfree(ab->serial_number_kobj);

	ret = kobject_add(ab->serial_number_kobj, NULL, "usb_serial_number");
	if (ret)
		kfree(ab->serial_number_kobj);


	err = ab->usb_gpio->get(ab->dev);
	if (err < 0)
		goto fail3;

	prcmu_qos_add_requirement(PRCMU_QOS_APE_OPP,
			(char *)dev_name(ab->dev), 50);
	dev_info(&pdev->dev, "revision 0x%2x driver initialized\n", ab->rev);

	prcmu_qos_add_requirement(PRCMU_QOS_ARM_OPP, "usb", 25);

	err = ab8500_usb_boot_detect(ab);
	if (err < 0)
		goto fail3;

	return 0;
fail3:
	ab8500_usb_irq_free(ab);
fail2:
	clk_put(ab->sysclk);
fail1:
	ab8500_usb_regulator_put(ab);
fail0:
	kfree(otg);
	kfree(ab);
	return err;
}

static int __devexit ab8500_usb_remove(struct platform_device *pdev)
{
	struct ab8500_usb *ab = platform_get_drvdata(pdev);

	ab8500_usb_irq_free(ab);

	cancel_delayed_work_sync(&ab->dwork);

	cancel_work_sync(&ab->phy_dis_work);

	usb_set_transceiver(NULL);

	if (ab->mode == USB_HOST)
		ab8500_usb_host_phy_dis(ab);
	else if (ab->mode == USB_PERIPHERAL)
		ab8500_usb_peri_phy_dis(ab);

	clk_put(ab->sysclk);

	ab8500_usb_regulator_put(ab);

	ab->usb_gpio->put();

	platform_set_drvdata(pdev, NULL);

	kfree(ab->phy.otg);
	kfree(ab);

	return 0;
}

static struct platform_driver ab8500_usb_driver = {
	.probe		= ab8500_usb_probe,
	.remove		= __devexit_p(ab8500_usb_remove),
	.driver		= {
		.name	= "ab8500-usb",
		.owner	= THIS_MODULE,
	},
};

static int __init ab8500_usb_init(void)
{
	return platform_driver_register(&ab8500_usb_driver);
}
subsys_initcall(ab8500_usb_init);

static void __exit ab8500_usb_exit(void)
{
	platform_driver_unregister(&ab8500_usb_driver);
}
module_exit(ab8500_usb_exit);

MODULE_ALIAS("platform:ab8500_usb");
MODULE_AUTHOR("ST-Ericsson AB");
MODULE_DESCRIPTION("AB8500 usb transceiver driver");
MODULE_LICENSE("GPL");

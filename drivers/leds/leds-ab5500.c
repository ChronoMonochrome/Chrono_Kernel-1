/*
 * Copyright (C) 2011 ST-Ericsson SA.
 *
 * License Terms: GNU General Public License v2
 *
 * Driver for LED in ST-Ericsson AB5500 v1.0 Analog baseband Controller
 *
 * Author: Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>
 */

#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab5500.h>
#include <linux/leds-ab5500.h>
#include <linux/types.h>

#define AB5500LED_NAME		"ab5500-leds"

/* Register offsets */
#define AB5500_LED_REG_ENABLE	0x03
#define AB5500_LED_FADE_CTL	0x0D

/* LED-0 */
#define AB5500_LED0_PWM_DUTY	0x01
#define AB5500_LED0_PWMFREQ	0x02
#define AB5500_LED0_SINKCTL	0x0A

/* LED-1 */
#define AB5500_LED1_PWM_DUTY	0x05
#define AB5500_LED1_PWMFREQ	0x06
#define AB5500_LED1_SINKCTL	0x0B

/* LED-2 */
#define AB5500_LED2_PWM_DUTY	0x08
#define AB5500_LED2_PWMFREQ	0x09
#define AB5500_LED2_SINKCTL	0x0C

/* pwm duty cycle */
#define AB5500_LED_PWMDUTY_OFF	0x0
#define AB5500_LED_PWMDUTY_MAX	0x3FF
#define AB5500_LED_PWMDUTY_STEP	(AB5500_LED_PWMDUTY_MAX/LED_FULL)

/* pwm frequency */
#define AB5500_LED_PWMFREQ_MAX	0x0F	/* 373.39 @sysclk=26MHz */
#define AB5500_LED_PWMFREQ_SHIFT	4

/* LED sink current control */
#define AB5500_LED_SINKCURR_MAX	0x0F	/* 40mA */
#define AB5500_LED_SINKCURR_SHIFT	4

struct ab5500_led {
	u8 id;
	u8 max_current;
	u16 brt_val;
	enum ab5500_led_status status;
	struct led_classdev led_cdev;
	struct work_struct led_work;
};

struct ab5500_hvleds {
	struct mutex lock;
	struct device *dev;
	struct ab5500_hvleds_platform_data *pdata;
	struct ab5500_led leds[AB5500_HVLEDS_MAX];
};

static u8 ab5500_led_pwmduty_reg[] = {
			AB5500_LED0_PWM_DUTY,
			AB5500_LED1_PWM_DUTY,
			AB5500_LED2_PWM_DUTY,
};

static u8 ab5500_led_pwmfreq_reg[] = {
			AB5500_LED0_PWMFREQ,
			AB5500_LED1_PWMFREQ,
			AB5500_LED2_PWMFREQ,
};

static u8 ab5500_led_sinkctl_reg[] = {
			AB5500_LED0_SINKCTL,
			AB5500_LED1_SINKCTL,
			AB5500_LED2_SINKCTL
};

#define to_led(_x)	container_of(_x, struct ab5500_led, _x)

static inline struct ab5500_hvleds *led_to_hvleds(struct ab5500_led *led)
{
	return container_of(led, struct ab5500_hvleds, leds[led->id]);
}

static int ab5500_led_pwmduty_write(struct ab5500_hvleds *hvleds,
			unsigned int led_id, u16 val)
{
	int ret;
	int val_lsb = val & 0xFF;
	int val_msb = (val & 0x300) >> 8;

	mutex_lock(&hvleds->lock);

	dev_dbg(hvleds->dev, "ab5500-leds: reg[%d] w val = %d\n"
		 "reg[%d] w val = %d\n",
		 ab5500_led_pwmduty_reg[led_id] - 1, val_lsb,
		 ab5500_led_pwmduty_reg[led_id], val_msb);

	ret = abx500_set_register_interruptible(
			hvleds->dev, AB5500_BANK_LED,
			ab5500_led_pwmduty_reg[led_id] - 1, val_lsb);
	ret |= abx500_set_register_interruptible(
			hvleds->dev, AB5500_BANK_LED,
			ab5500_led_pwmduty_reg[led_id], val_msb);
	if (ret < 0)
		dev_err(hvleds->dev, "reg[%d] w failed: %d\n",
			ab5500_led_pwmduty_reg[led_id], ret);
	mutex_unlock(&hvleds->lock);

	return ret;
}

static int ab5500_led_pwmfreq_write(struct ab5500_hvleds *hvleds,
			unsigned int led_id, u8 val)
{
	int ret;

	val = (val & 0x0F) << AB5500_LED_PWMFREQ_SHIFT;

	mutex_lock(&hvleds->lock);

	dev_dbg(hvleds->dev, "ab5500-leds: reg[%d] w val=%d\n",
			ab5500_led_pwmfreq_reg[led_id], val);

	ret = abx500_set_register_interruptible(
			hvleds->dev, AB5500_BANK_LED,
			ab5500_led_pwmfreq_reg[led_id], val);
	if (ret < 0)
		dev_err(hvleds->dev, "reg[%d] w failed: %d\n",
			ab5500_led_pwmfreq_reg[led_id], ret);
	mutex_unlock(&hvleds->lock);

	return ret;
}

static int ab5500_led_sinkctl_write(struct ab5500_hvleds *hvleds,
			unsigned int led_id, u8 val)
{
	int ret;

	val = (val & 0x0F) << AB5500_LED_SINKCURR_SHIFT;

	mutex_lock(&hvleds->lock);

	dev_dbg(hvleds->dev, "ab5500-leds: reg[%d] w val=%d\n",
			ab5500_led_sinkctl_reg[led_id], val);

	ret = abx500_set_register_interruptible(
			hvleds->dev, AB5500_BANK_LED,
			ab5500_led_sinkctl_reg[led_id], val);
	if (ret < 0)
		dev_err(hvleds->dev, "reg[%d] w failed: %d\n",
			ab5500_led_sinkctl_reg[led_id], ret);
	mutex_unlock(&hvleds->lock);

	return ret;
}

static int ab5500_led_sinkctl_read(struct ab5500_hvleds *hvleds,
			unsigned int led_id)
{
	int ret;
	u8 val;

	mutex_lock(&hvleds->lock);
	ret = abx500_get_register_interruptible(
			hvleds->dev, AB5500_BANK_LED,
			ab5500_led_sinkctl_reg[led_id], &val);
	if (ret < 0) {
		dev_err(hvleds->dev, "reg[%d] r failed: %d\n",
			ab5500_led_sinkctl_reg[led_id], ret);
		mutex_unlock(&hvleds->lock);
		return ret;
	}
	val = (val & 0xF0) >> AB5500_LED_SINKCURR_SHIFT;
	mutex_unlock(&hvleds->lock);

	return val;
}

static void ab5500_led_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brt_val)
{
	struct ab5500_led *led = to_led(led_cdev);

	/* adjust LED_FULL to 10bit range */
	brt_val &= LED_FULL;
	led->brt_val = brt_val * AB5500_LED_PWMDUTY_STEP;
	schedule_work(&led->led_work);
}

static void ab5500_led_work(struct work_struct *led_work)
{
	struct ab5500_led *led = to_led(led_work);
	struct ab5500_hvleds *hvleds = led_to_hvleds(led);

	if (led->status == AB5500_LED_ON)
		ab5500_led_pwmduty_write(hvleds, led->id, led->brt_val);
}

static ssize_t ab5500_led_show_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int led_curr = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct ab5500_led *led = to_led(led_cdev);
	struct ab5500_hvleds *hvleds = led_to_hvleds(led);

	led_curr = ab5500_led_sinkctl_read(hvleds, led->id);

	if (led_curr < 0)
		return led_curr;

	return sprintf(buf, "%d\n", led_curr);
}

static ssize_t ab5500_led_store_current(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	int ret;
	unsigned long led_curr;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct ab5500_led *led = to_led(led_cdev);
	struct ab5500_hvleds *hvleds = led_to_hvleds(led);

	if (strict_strtoul(buf, 0, &led_curr))
		return -EINVAL;

	if (led_curr > led->max_current)
		led_curr = led->max_current;

	ret = ab5500_led_sinkctl_write(hvleds, led->id, led_curr);
	if (ret < 0)
		return ret;

	return len;
}

/* led class device attributes */
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUGO,
	       ab5500_led_show_current, ab5500_led_store_current);

static int ab5500_led_init_registers(struct ab5500_hvleds *hvleds)
{
	int ret = 0;
	unsigned int led_id;

	/*  fade - manual : dur mid : pwm duty mid */
	ret = abx500_set_register_interruptible(
				hvleds->dev, AB5500_BANK_LED,
				AB5500_LED_REG_ENABLE, true);
	if (ret < 0) {
		dev_err(hvleds->dev, "reg[%d] w failed: %d\n",
			       AB5500_LED_REG_ENABLE,	ret);
		return ret;
	}

	for (led_id = 0; led_id < AB5500_HVLEDS_MAX; led_id++) {
		/* Set pwm freq. and sink current to mid values */
		ret = ab5500_led_pwmfreq_write(
				hvleds, led_id, AB5500_LED_PWMFREQ_MAX);
		if (ret < 0)
			return ret;

		ret = ab5500_led_sinkctl_write(
				hvleds, led_id, AB5500_LED_SINKCURR_MAX);
		if (ret < 0)
			return ret;

		/* init led off */
		ret = ab5500_led_pwmduty_write(
				hvleds, led_id, AB5500_LED_PWMDUTY_OFF);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int ab5500_led_register_leds(struct device *dev,
			struct ab5500_hvleds_platform_data *pdata,
			struct ab5500_hvleds *hvleds)
{
	int i_led;
	int err;
	struct ab5500_led_conf *pled;
	struct ab5500_led *led;

	hvleds->dev = dev;
	hvleds->pdata = pdata;
	for (i_led = 0; i_led < AB5500_HVLEDS_MAX; i_led++) {
		pled = &pdata->leds[i_led];
		led = &hvleds->leds[i_led];

		INIT_WORK(&led->led_work, ab5500_led_work);

		led->id = pled->led_id;
		led->max_current = pled->max_current;
		led->status = pled->status;
		led->led_cdev.name = pled->name;
		led->led_cdev.brightness_set = ab5500_led_brightness_set;

		err = led_classdev_register(dev, &led->led_cdev);
		if (err < 0) {
			dev_err(dev, "Register led class failed: %d\n", err);
			goto bailout1;
		}

		err = device_create_file(led->led_cdev.dev,
						&dev_attr_led_current);
		if (err < 0) {
			dev_err(dev, "sysfs device creation failed: %d\n", err);
			goto bailout2;
		}
	}

	return err;
	for (; i_led >= 0; i_led--) {
		device_remove_file(led->led_cdev.dev, &dev_attr_led_current);
bailout2:
		led_classdev_unregister(&hvleds->leds[i_led].led_cdev);
bailout1:
		cancel_work_sync(&hvleds->leds[i_led].led_work);
	}
	return err;
}

static int __devinit ab5500_hvleds_probe(struct platform_device *pdev)
{
	struct ab5500_hvleds_platform_data *pdata = pdev->dev.platform_data;
	struct ab5500_hvleds *hvleds = NULL;
	int err = 0, i;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data required\n");
		err = -ENODEV;
		goto err_out;
	}

	hvleds = kzalloc(sizeof(struct ab5500_hvleds), GFP_KERNEL);
	if (hvleds == NULL) {
		err = -ENOMEM;
		goto err_out;
	}

	mutex_init(&hvleds->lock);

	/* init leds data and register led_classdev */
	err = ab5500_led_register_leds(&pdev->dev, pdata, hvleds);
	if (err < 0) {
		dev_err(&pdev->dev, "leds registeration failed\n");
		goto err_out;
	}

	/* init device registers and set initial led current */
	err = ab5500_led_init_registers(hvleds);
	if (err < 0) {
		dev_err(&pdev->dev, "reg init failed: %d\n", err);
		goto err_reg_init;
	}

	dev_info(&pdev->dev, "enabled\n");

	return err;

err_reg_init:
	for (i = 0; i < AB5500_HVLEDS_MAX; i++) {
		struct ab5500_led *led = &hvleds->leds[i];

		led_classdev_unregister(&led->led_cdev);
		device_remove_file(led->led_cdev.dev, &dev_attr_led_current);
		cancel_work_sync(&led->led_work);
	}
err_out:
	kfree(hvleds);
	return err;
}

static int __devexit ab5500_hvleds_remove(struct platform_device *pdev)
{
	struct ab5500_hvleds *hvleds = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < AB5500_HVLEDS_MAX; i++) {
		struct ab5500_led *led = &hvleds->leds[i];

		led_classdev_unregister(&led->led_cdev);
		device_remove_file(led->led_cdev.dev, &dev_attr_led_current);
		cancel_work_sync(&led->led_work);
	}
	kfree(hvleds);
	return 0;
}

static struct platform_driver ab5500_hvleds_driver = {
	.driver   = {
		   .name = AB5500LED_NAME,
		   .owner = THIS_MODULE,
	},
	.probe    = ab5500_hvleds_probe,
	.remove   = __devexit_p(ab5500_hvleds_remove),
};

static int __init ab5500_hvleds_module_init(void)
{
	return platform_driver_register(&ab5500_hvleds_driver);
}

static void __exit ab5500_hvleds_module_exit(void)
{
	platform_driver_unregister(&ab5500_hvleds_driver);
}

module_init(ab5500_hvleds_module_init);
module_exit(ab5500_hvleds_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>");
MODULE_DESCRIPTION("Driver for AB5500 HVLED");


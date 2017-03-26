/* drivers/misc/bln.c
 *
 * Copyright 2011  Michael Richter (alias neldar)
 * Copyright 2011  Adam Kent <adam@semicircular.net>
 * Copyright 2014  Jonathan Jason Dennis [Meticulus]
			theonejohnnyd@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Blink modes:
 * 0 = no blinking
 * 1 = blink backlight only
 * 2 = blink backlight + rear cam flash
 * 3 = blink rear cam flash only
 */

#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/earlysuspend.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/bln.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
#include <linux/wakelock.h>
#endif

static bool bln_enabled = true;
static bool bln_ongoing = false; /* ongoing LED Notification */
static int bln_blink_state = 1;
static unsigned int bln_blink_mode = 1; /* blink by default */
static int bln_blinkon_delay = 1000; /* blink on with 1000msec delay by default */
static int bln_blinkoff_delay = 1000; /* blink off with 1000msec delay by default */
static bool bln_suspended = false; /* is system suspended */
static struct bln_implementation *bln_imp = NULL;
static struct bln_implementation *bln_imp_flash = NULL;

static long unsigned int notification_led_mask = 0x0;

#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
static bool use_wakelock = false; /* i don't want to burn batteries */
static struct wake_lock bln_wake_lock;
#endif

#ifdef CONFIG_GENERIC_BLN_EMULATE_BUTTONS_LED
static bool buttons_led_enabled = false;
#endif

#define BACKLIGHTNOTIFICATION_VERSION 9

static int gen_all_leds_mask(void)
{
	int i = 0;
	int mask = 0x0;

	for(; i < bln_imp->led_count; i++)
		mask |= 1 << i;

	return mask;
}

static int get_led_mask(void) {
	return (notification_led_mask != 0) ? notification_led_mask : gen_all_leds_mask();
}

static void reset_bln_states(void)
{
	bln_blink_state = 0;
	bln_ongoing = false;
}

static void bln_enable_backlights(int mask)
{
	if (bln_imp && bln_blink_mode != 3){
		bln_imp->enable(mask);
	}

	if ((bln_blink_mode == 2 || bln_blink_mode == 3) && bln_imp_flash){
		bln_imp_flash->enable(mask);
	}
}

static void bln_disable_backlights(int mask)
{
	if (bln_imp && bln_blink_mode != 3){
		bln_imp->disable(mask);
	}
	
	if ((bln_blink_mode == 2 || bln_blink_mode == 3) && bln_imp_flash){
		bln_imp_flash->disable(mask);
	}
}

static void bln_power_on(void)
{
	if (likely(bln_imp && bln_imp->power_on)) {
#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
		if (use_wakelock && !wake_lock_active(&bln_wake_lock)) {
			wake_lock(&bln_wake_lock);
		}
#endif
		bln_imp->power_on();
	}
}

static void bln_power_off(void)
{
	if (likely(bln_imp && bln_imp->power_off)) {
		bln_imp->power_off();
#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
		if (wake_lock_active(&bln_wake_lock)) {
			wake_unlock(&bln_wake_lock);
		}
#endif
	}
}

static void bln_early_suspend(struct early_suspend *h)
{
	bln_suspended = true;
}

static void disable_led_notification(void)
{
	if (bln_ongoing) {
		bln_disable_backlights(gen_all_leds_mask());
		bln_power_off();
	}

	reset_bln_states();

	pr_info("%s: notification led disabled\n", __FUNCTION__);
}

static void bln_late_resume(struct early_suspend *h)
{
	bln_suspended = false;

	disable_led_notification();
}

static struct early_suspend bln_suspend_data = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = bln_early_suspend,
	.resume = bln_late_resume,
};

static void blink_thread(void)
{
	while(bln_suspended)
	{
		bln_enable_backlights(get_led_mask());
		msleep(bln_blinkon_delay);
		bln_disable_backlights(get_led_mask());
		msleep(bln_blinkoff_delay);
	}
}

static void enable_led_notification(void)
{
	if (!bln_enabled)
		return;

	/*
	* dont allow led notifications while the screen is on,
	* avoid to interfere with the normal buttons led
	*/
	if (!bln_suspended)
		return;
	
	/*
	* If we already have a blink thread going
	* don't start another one.
	*/
	if(bln_ongoing & bln_blink_mode)
		return;

	bln_ongoing = true;

	bln_power_on();
	if(!bln_blink_mode)
		bln_enable_backlights(get_led_mask());
	else
		kthread_run(&blink_thread, NULL,"bln_blink_thread");

	pr_info("%s: notification led enabled\n", __FUNCTION__);
}

static ssize_t backlightnotification_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
		
	if (unlikely(!bln_imp))
		ret = -1;

	if (bln_enabled)
		ret = 1;
	else
		ret = 0;
		
	return sprintf(buf, "%u\n", ret);
}

static ssize_t backlightnotification_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (unlikely(!bln_imp)) {
		pr_err("%s: no BLN implementation registered!\n", __FUNCTION__);
		return size;
	}

	if (sscanf(buf, "%u\n", &data) != 1) {
			pr_info("%s: input error\n", __FUNCTION__);
			return size;
	}

	pr_devel("%s: %u \n", __FUNCTION__, data);

	if (data == 1) {
		pr_info("%s: BLN function enabled\n", __FUNCTION__);
		bln_enabled = true;
	} else if (data == 0) {
		pr_info("%s: BLN function disabled\n", __FUNCTION__);
		bln_enabled = false;
		if (bln_ongoing)
			disable_led_notification();
	} else {
		pr_info("%s: invalid input range %u\n", __FUNCTION__,
				data);
	}

	return size;
}

static ssize_t notification_led_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (bln_ongoing ? 1 : 0));
}

static ssize_t notification_led_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) != 1) {
			pr_info("%s: input error\n", __FUNCTION__);
			return size;
	}

	if (data == 1)
		enable_led_notification();
	else if (data == 0)
		disable_led_notification();
	else
		pr_info("%s: wrong input %u\n", __FUNCTION__, data);

	return size;
}

static ssize_t notification_led_mask_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%lu\n", notification_led_mask);
}

static ssize_t notification_led_mask_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) != 1) {
			pr_info("%s: input error\n", __FUNCTION__);
			return size;
	}

	if (data & gen_all_leds_mask()) {
		notification_led_mask = data;
	} else {
		//TODO: correct error code
		return -1;
	}

	return size;
}

static ssize_t blink_mode_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", bln_blink_mode);
}

static ssize_t blink_mode_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}

	if (data >= 0 && data < 4) {
		bln_blink_mode = data;
	} else {
		pr_info("%s: wrong input %u\n", __FUNCTION__, data);
	}

	return size;
}

static ssize_t blink_control_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", bln_blink_state);
}

static ssize_t blink_control_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (!bln_ongoing)
		return size;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}

	/* reversed logic:
	 * 1 = leds off
	 * 0 = leds on
	 */
	if (data == 1) {
		bln_blink_state = 1;
		bln_disable_backlights(get_led_mask());
	} else if (data == 0) {
		bln_blink_state = 0;
		bln_enable_backlights(get_led_mask());
	} else {
		pr_info("%s: wrong input %u\n", __FUNCTION__, data);
	}

	return size;
}

static ssize_t backlightnotification_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", BACKLIGHTNOTIFICATION_VERSION);
}

static ssize_t led_count_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int ret = 0x0;

	if (bln_imp)
		ret = bln_imp->led_count;

	return sprintf(buf,"%u\n", ret);
}

#ifdef CONFIG_GENERIC_BLN_EMULATE_BUTTONS_LED
static ssize_t buttons_led_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (buttons_led_enabled ? 1 : 0));
}

static ssize_t buttons_led_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) != 1) {
			pr_info("%s: input error\n", __FUNCTION__);
			return size;
	}

	if (data == 1) {
		if (!bln_suspended) {
			buttons_led_enabled = true;
			bln_power_on();
			bln_enable_backlights(gen_all_leds_mask());
		}
	} else if (data == 0) {
		if (!bln_suspended) {
			buttons_led_enabled = false;
			bln_disable_backlights(gen_all_leds_mask());
		}
	} else {
		pr_info("%s: wrong input %u\n", __FUNCTION__, data);
	}

	return size;
}

static DEVICE_ATTR(buttons_led, S_IRUGO | S_IWUGO,
		buttons_led_status_read,
		buttons_led_status_write);
#endif

static DEVICE_ATTR(blink_control, S_IRUGO | S_IWUGO, blink_control_read,
		blink_control_write);

static DEVICE_ATTR(blink_mode, S_IRUGO | S_IWUGO, blink_mode_read,
		blink_mode_write);
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUGO,
		backlightnotification_status_read,
		backlightnotification_status_write);
static DEVICE_ATTR(led_count, S_IRUGO , led_count_read, NULL);
static DEVICE_ATTR(notification_led, S_IRUGO | S_IWUGO,
		notification_led_status_read,
		notification_led_status_write);
static DEVICE_ATTR(notification_led_mask, S_IRUGO | S_IWUGO,
		notification_led_mask_read,
		notification_led_mask_write);
static DEVICE_ATTR(version, S_IRUGO , backlightnotification_version, NULL);

static struct attribute *bln_notification_attributes[] = {
	&dev_attr_blink_control.attr,
	&dev_attr_blink_mode.attr,
	&dev_attr_enabled.attr,
	&dev_attr_led_count.attr,
	&dev_attr_notification_led.attr,
	&dev_attr_notification_led_mask.attr,
#ifdef CONFIG_GENERIC_BLN_EMULATE_BUTTONS_LED
	&dev_attr_buttons_led.attr,
#endif
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group bln_notification_group = {
	.attrs  = bln_notification_attributes,
};

static struct miscdevice bln_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "backlightnotification",
};

static ssize_t bln_blink_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	sprintf(buf, "blink status: %s\n", bln_blink_mode ? "on" : "off");
	sprintf(buf, "%sblink on delay: %d mscec\n", buf, bln_blinkon_delay);
	sprintf(buf, "%sblink off delay: %d msec\n", buf, bln_blinkoff_delay);

	return strlen(buf);
}

static ssize_t bln_blink_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int delay_tmp;
	
	if (!strncmp(&buf[0], "bln_ondelay=", 12)) {
		ret = sscanf(&buf[12], "%d", &delay_tmp);

		if ((!ret) || (delay_tmp < 1)) {
			pr_err("[BLN] invalid input - delay too short\n");
			return -EINVAL;
		}

		bln_blinkon_delay = delay_tmp;

		return count;
	}

	if (!strncmp(&buf[0], "bln_offdelay=", 13)) {
		ret = sscanf(&buf[13], "%d", &delay_tmp);

		if ((!ret) || (delay_tmp < 1)) {
			pr_err("[BLN] invalid input - delay too short\n");
			return -EINVAL;
		}

		bln_blinkoff_delay = delay_tmp;

		return count;
	}
	
	if (!strncmp(buf, "on", 2)) {
		bln_blink_mode = true;

		pr_err("[BLN] BLN Blink Mode on\n");

		return count;
	}

	if (!strncmp(buf, "off", 3)) {
		bln_blink_mode = false;

		pr_err("[BLN] BLN Blink Mode off\n");

		return count;
	}
	
	return count;
}

#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
static ssize_t bln_wakelock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	sprintf(buf, "status: %s\n", use_wakelock ? "on" : "off");
	return strlen(buf);
}


static ssize_t bln_wakelock_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (!strncmp(buf, "on", 2)) {
		use_wakelock = true;

		pr_err("[BLN] use_wakelock = true\n");

		return count;
	}

	if (!strncmp(buf, "off", 3)) {
		use_wakelock = false;

		pr_err("[BLN] use_wakelock = false\n");

		return count;
	}
}
#endif

static struct kobj_attribute bln_blink_interface = __ATTR(blink_mode, 0644, bln_blink_show, bln_blink_store);
#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
static struct kobj_attribute bln_wakelock_interface = __ATTR(bln_wakelock, 0644, bln_wakelock_show, bln_wakelock_store);
#endif

static struct attribute *bln_attrs[] = {
	&bln_blink_interface.attr,
#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
	&bln_wakelock_interface.attr,
#endif
	NULL,
};

static struct attribute_group bln_interface_group = {
	.attrs = bln_attrs,
};

static struct kobject *bln_kobject;

/**
 *	register_bln_implementation	- register a bln implementation of a touchkey device device
 *	@imp: bln implementation structure
 *
 *	Register a bln implementation with the bln kernel module.
 */
void register_bln_implementation(struct bln_implementation *imp)
{
	//TODO: more checks
	if (imp) {
		bln_imp = imp;
		printk(KERN_DEBUG "Registered BLN: button-backlight\n");
	}
}
EXPORT_SYMBOL(register_bln_implementation);

void register_bln_implementation_flash(struct bln_implementation *imp)
{
	//TODO: more checks
	if(imp){
		bln_imp_flash = imp;
		printk(KERN_DEBUG "Registered BLN: rearcam-flash\n");
	}
}
EXPORT_SYMBOL(register_bln_implementation_flash);

/**
 *	bln_is_ongoing - check if a bln (led) notification is ongoing
 */
bool bln_is_ongoing()
{
	return bln_ongoing;
}
EXPORT_SYMBOL(bln_is_ongoing);

static int __init bln_control_init(void)
{
	int ret;

	pr_info("%s misc_register(%s)\n", __FUNCTION__, bln_device.name);
	ret = misc_register(&bln_device);
	if (ret) {
		pr_err("%s misc_register(%s) fail\n", __FUNCTION__,
				bln_device.name);
		return 1;
	}

	/* add the bln attributes */
	if (sysfs_create_group(&bln_device.this_device->kobj,
				&bln_notification_group) < 0) {
		pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n",
				bln_device.name);
		return 1;
	}

	bln_kobject = kobject_create_and_add("bln", kernel_kobj);

	if (!bln_kobject) {
		return -ENOMEM;
	}

	ret = sysfs_create_group(bln_kobject, &bln_interface_group);

	if (ret) {
		kobject_put(bln_kobject);
	}

#ifdef CONFIG_GENERIC_BLN_USE_WAKELOCK
	wake_lock_init(&bln_wake_lock, WAKE_LOCK_SUSPEND, "bln_kernel_wake_lock");
#endif

	register_early_suspend(&bln_suspend_data);

	return 0;
}

device_initcall(bln_control_init);

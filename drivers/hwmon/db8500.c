/*
 * Copyright (C) ST-Ericsson SA 2010. All rights reserved.
 * This code is ST-Ericsson proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of ST-Ericsson SA.
 *
 * Author: WenHai Fang <wenhai.h.fang@stericsson.com> for
 * ST-Ericsson.
 * License terms: GNU Gereral Public License (GPL) version 2
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <mach/prcmu.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <mach/hardware.h>

/*
 * If DB8500 warm interrupt is set, user space will be notified.
 * If user space doesn't shut down the platform within this time
 * frame, this driver will. Time unit is ms.
 */
#define DEFAULT_POWER_OFF_DELAY 10000

/*
 * Default measure period to 0xFF x cycle32k
 */
#define DEFAULT_MEASURE_TIME 0xFF

/* This driver monitors DB thermal*/
#define NUM_SENSORS 1

struct db8500_temp {
	struct platform_device *pdev;
	struct device *hwmon_dev;
	unsigned char min[NUM_SENSORS];
	unsigned char max[NUM_SENSORS];
	unsigned char crit[NUM_SENSORS];
	unsigned short measure_time;
	struct delayed_work power_off_work;
	struct mutex lock;
	/* Delay (ms) before power off */
	unsigned long power_off_delay;
};

static void thermal_power_off(struct work_struct *work)
{
	struct db8500_temp *data = container_of(work, struct db8500_temp,
						power_off_work.work);

	dev_warn(&data->pdev->dev, "Power off due to DB8500 thermal warning\n");
	pm_power_off();
}

static ssize_t set_temp_power_off_delay(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int res;
	unsigned long delay_in_s;
	struct db8500_temp *data = dev_get_drvdata(dev);

	res = strict_strtoul(buf, 10, &delay_in_s);
	if (res < 0) {
		dev_warn(&data->pdev->dev, "Set power_off_delay wrong\n");
		return res;
	}

	mutex_lock(&data->lock);
	data->power_off_delay = delay_in_s * 1000;
	mutex_unlock(&data->lock);

	return count;
}

static ssize_t show_temp_power_off_delay(struct device *dev,
					 struct device_attribute *devattr,
					 char *buf)
{
	struct db8500_temp *data = dev_get_drvdata(dev);
	/* return time in s, not ms */
	return sprintf(buf, "%lu\n", (data->power_off_delay) / 1000);
}

/* HWMON sysfs interface */
static ssize_t show_name(struct device *dev, struct device_attribute *devattr,
			 char *buf)
{
	return sprintf(buf, "db8500\n");
}

/* set functions (RW nodes) */
static ssize_t set_min(struct device *dev, struct device_attribute *devattr,
		       const char *buf, size_t count)
{
	unsigned long val;
	struct db8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	val &= 0xFF;
	if (val > data->max[attr->index - 1])
		val = data->max[attr->index - 1];

	data->min[attr->index - 1] = val;

	(void)prcmu_config_hotmon(data->min[attr->index - 1],
			data->max[attr->index - 1]);
	mutex_unlock(&data->lock);
	return count;
}

static ssize_t set_max(struct device *dev, struct device_attribute *devattr,
		       const char *buf, size_t count)
{
	unsigned long val;
	struct db8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	val &= 0xFF;
	if (val < data->min[attr->index - 1])
		val = data->min[attr->index - 1];

	data->max[attr->index - 1] = val;

	(void)prcmu_config_hotmon(data->min[attr->index - 1],
		data->max[attr->index - 1]);
	mutex_unlock(&data->lock);

	return count;
}

static ssize_t set_crit(struct device *dev,
			    struct device_attribute *devattr,
			    const char *buf, size_t count)
{
	unsigned long val;
	struct db8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	val = (val > 255) ? 0xFF : val;

	data->crit[attr->index - 1] = val;
	(void)prcmu_config_hotdog(data->crit[attr->index - 1]);
	mutex_unlock(&data->lock);

	return count;
}

/* set functions (W nodes) */
static ssize_t start_temp(struct device *dev, struct device_attribute *devattr,
		       const char *buf, size_t count)
{
	unsigned long val;
	struct db8500_temp *data = dev_get_drvdata(dev);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	data->measure_time = val & 0xFFFF;
	mutex_unlock(&data->lock);

	(void)prcmu_start_temp_sense(data->measure_time);
	dev_dbg(&data->pdev->dev, "DB8500 thermal start measurement\n");
	return count;
}

static ssize_t stop_temp(struct device *dev, struct device_attribute *devattr,
		       const char *buf, size_t count)
{
	unsigned long val;
	struct db8500_temp *data = dev_get_drvdata(dev);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	(void)prcmu_stop_temp_sense();
	dev_dbg(&data->pdev->dev, "DB8500 thermal stop measurement\n");

	return count;
}

/*
 * show functions (RO nodes)
 * Notice that min/max/max_hyst refer to millivolts and not millidegrees
 */
static ssize_t show_min(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct db8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%d\n", data->min[attr->index - 1]);
}

static ssize_t show_max(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct db8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%d\n", data->max[attr->index - 1]);
}

static ssize_t show_crit(struct device *dev,
			     struct device_attribute *devattr, char *buf)
{
	struct db8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%d\n", data->crit[attr->index - 1]);
}


/*These node are not included in the kernel hwmon sysfs interface */
static SENSOR_DEVICE_ATTR(temp_power_off_delay, S_IRUGO | S_IWUSR,
			  show_temp_power_off_delay,
			  set_temp_power_off_delay, 0);

/* Chip name, required by hwmon*/
static SENSOR_DEVICE_ATTR(temp1_start, S_IWUSR, NULL, start_temp, 0);
static SENSOR_DEVICE_ATTR(temp1_stop, S_IWUSR, NULL, stop_temp, 0);
static SENSOR_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_min, S_IWUSR | S_IRUGO, show_min, set_min, 1);
static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, show_max, set_max, 1);
static SENSOR_DEVICE_ATTR(temp1_crit, S_IWUSR | S_IRUGO,
			show_crit, set_crit, 1);

static struct attribute *db8500_temp_attributes[] = {
	&sensor_dev_attr_temp_power_off_delay.dev_attr.attr,
	&sensor_dev_attr_name.dev_attr.attr,
	&sensor_dev_attr_temp1_start.dev_attr.attr,
	&sensor_dev_attr_temp1_stop.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	NULL
};

static const struct attribute_group db8500_temp_group = {
	.attrs = db8500_temp_attributes,
};

static irqreturn_t prcmu_hotmon_low_irq_handler(int irq, void *irq_data)
{
	struct platform_device *pdev = irq_data;
	sysfs_notify(&pdev->dev.kobj, NULL, "prcmu_hotmon_low alarm");
	dev_dbg(&pdev->dev, "DB8500 thermal low warning\n");
	return IRQ_HANDLED;
}

static irqreturn_t prcmu_hotmon_high_irq_handler(int irq, void *irq_data)
{
	unsigned long delay_in_jiffies;
	struct platform_device *pdev = irq_data;
	struct db8500_temp *data = platform_get_drvdata(pdev);

	sysfs_notify(&pdev->dev.kobj, NULL, "prcmu_hotmon_high alarm");
	dev_dbg(&pdev->dev, "DB8500 thermal warning, power off in %lu s\n",
		 (data->power_off_delay) / 1000);
	delay_in_jiffies = msecs_to_jiffies(data->power_off_delay);
	schedule_delayed_work(&data->power_off_work, delay_in_jiffies);
	return IRQ_HANDLED;
}

static int __devinit db8500_temp_probe(struct platform_device *pdev)
{
	struct db8500_temp *data;
	int err = 0, i;
	int irq;

	dev_dbg(&pdev->dev, "db8500_temp: Function db8500_temp_probe.\n");

	data = kzalloc(sizeof(struct db8500_temp), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	irq = platform_get_irq_byname(pdev, "IRQ_HOTMON_LOW");
	if (irq < 0) {
		dev_err(&pdev->dev, "Get IRQ_HOTMON_LOW failed\n");
		goto exit;
	}

	err = request_threaded_irq(irq, NULL, prcmu_hotmon_low_irq_handler,
		IRQF_NO_SUSPEND, "db8500_temp_low", pdev);
	if (err < 0) {
		dev_err(&pdev->dev, "db8500: Failed allocate HOTMON_LOW.\n");
		goto exit;
	} else {
		dev_dbg(&pdev->dev, "db8500: Succeed allocate HOTMON_LOW.\n");
	}

	irq = platform_get_irq_byname(pdev, "IRQ_HOTMON_HIGH");
	if (irq < 0) {
		dev_err(&pdev->dev, "Get IRQ_HOTMON_HIGH failed\n");
		goto exit;
	}

	err = request_threaded_irq(irq, NULL, prcmu_hotmon_high_irq_handler,
		IRQF_NO_SUSPEND, "db8500_temp_high", pdev);
	if (err < 0) {
		dev_err(&pdev->dev, "db8500: Failed allocate HOTMON_HIGH.\n");
		goto exit;
	} else {
		dev_dbg(&pdev->dev, "db8500: Succeed allocate HOTMON_HIGH.\n");
	}

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		goto exit;
	}

	for (i = 0; i < NUM_SENSORS; i++) {
		data->min[i] = 0;
		data->max[i] = 0xFF;
	}

	mutex_init(&data->lock);
	INIT_DELAYED_WORK(&data->power_off_work, thermal_power_off);

	data->pdev = pdev;
	data->power_off_delay = DEFAULT_POWER_OFF_DELAY;
	data->measure_time = DEFAULT_MEASURE_TIME;

	platform_set_drvdata(pdev, data);

	err = sysfs_create_group(&pdev->dev.kobj, &db8500_temp_group);
	if (err < 0) {
		dev_err(&pdev->dev, "Create sysfs group failed (%d)\n", err);
		goto exit_platform_data;
	}

	return 0;

exit_platform_data:
	platform_set_drvdata(pdev, NULL);
exit:
	kfree(data);
	return err;
}

static int __devexit db8500_temp_remove(struct platform_device *pdev)
{
	struct db8500_temp *data = platform_get_drvdata(pdev);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &db8500_temp_group);
	platform_set_drvdata(pdev, NULL);
	kfree(data);
	return 0;
}

/* No action required in suspend/resume, thus the lack of functions */
static struct platform_driver db8500_temp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "db8500_temp",
	},
	.probe = db8500_temp_probe,
	.remove = __devexit_p(db8500_temp_remove),
};

static int __init db8500_temp_init(void)
{
	return platform_driver_register(&db8500_temp_driver);
}

static void __exit db8500_temp_exit(void)
{
	platform_driver_unregister(&db8500_temp_driver);
}

MODULE_AUTHOR("WenHai Fang <wenhai.h.fang@stericsson.com>");
MODULE_DESCRIPTION("DB8500 temperature driver");
MODULE_LICENSE("GPL");

module_init(db8500_temp_init)
module_exit(db8500_temp_exit)

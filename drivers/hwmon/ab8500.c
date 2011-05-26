/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Martin Persson <martin.persson@stericsson.com> for
 * ST-Ericsson.
 * License terms: GNU Gereral Public License (GPL) version 2
 *
 * Note:
 *
 * AB8500 does not provide auto ADC, so to monitor the required
 * temperatures, a periodic work is used. It is more important
 * to not wake up the CPU than to perform this job, hence the use
 * of a deferred delay.
 *
 * A deferred delay for thermal monitor is considered safe because:
 * If the chip gets too hot during a sleep state it's most likely
 * due to external factors, such as the surrounding temperature.
 * I.e. no SW decisions will make any difference.
 *
 * If/when the AB8500 thermal warning temperature is reached (threshold
 * cannot be changed by SW), an interrupt is set and the driver
 * notifies user space via a sysfs event. If a shut down is not
 * triggered by user space within a certain time frame,
 * pm_power off is called.
 *
 * If/when AB8500 thermal shutdown temperature is reached a hardware
 * shutdown of the AB8500 will occur.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/mfd/abx500/ab8500-gpadc.h>
#include <linux/pm.h>

/*
 * If AB8500 warm interrupt is set, user space will be notified.
 * If user space doesn't shut down the platform within this time
 * frame, this driver will. Time unit is ms.
 */
#define DEFAULT_POWER_OFF_DELAY 10000

#define NUM_SENSORS 5

/* The driver monitors GPADC - ADC_AUX1 and ADC_AUX2 */
#define NUM_MONITORED_SENSORS 2

struct ab8500_temp {
	struct platform_device *pdev;
	struct device *hwmon_dev;
	struct ab8500_gpadc *gpadc;
	u8 gpadc_addr[NUM_SENSORS];
	unsigned long min[NUM_SENSORS];
	unsigned long max[NUM_SENSORS];
	unsigned long max_hyst[NUM_SENSORS];
	unsigned long crit[NUM_SENSORS];
	unsigned long min_alarm[NUM_SENSORS];
	unsigned long max_alarm[NUM_SENSORS];
	unsigned long max_hyst_alarm[NUM_SENSORS];
	unsigned long crit_alarm[NUM_SENSORS];
	struct delayed_work work;
	struct delayed_work power_off_work;
	struct mutex lock;
	/* Delay (ms) between temperature readings */
	unsigned long gpadc_monitor_delay;
	/* Delay (ms) before power off */
	unsigned long power_off_delay;
};

/*
 * Thresholds are considered inactive if set to 0.
 * To avoid confusion for user space applications,
 * the temp monitor delay is set to 0 if all thresholds
 * are 0.
 */
static bool find_active_thresholds(struct ab8500_temp *data)
{
	int i;
	for (i = 0; i < NUM_MONITORED_SENSORS; i++)
		if (data->max[i] != 0 || data->max_hyst[i] != 0
		    || data->min[i] != 0)
			return true;

	dev_dbg(&data->pdev->dev, "No active thresholds,"
		"cancel deferred job (if it exists)"
		"and reset temp monitor delay\n");
	mutex_lock(&data->lock);
	data->gpadc_monitor_delay = 0;
	cancel_delayed_work_sync(&data->work);
	mutex_unlock(&data->lock);
	return false;
}

static void thermal_power_off(struct work_struct *work)
{
	struct ab8500_temp *data = container_of(work, struct ab8500_temp,
						power_off_work.work);

	dev_warn(&data->pdev->dev, "Power off due to AB8500 thermal warning\n");
	pm_power_off();
}

static void gpadc_monitor(struct work_struct *work)
{
	unsigned long delay_in_jiffies;
	int val, i, ret;
	/* Container for alarm node name */
	char alarm_node[30];

	bool updated_min_alarm = false;
	bool updated_max_alarm = false;
	bool updated_max_hyst_alarm = false;
	struct ab8500_temp *data = container_of(work, struct ab8500_temp,
						work.work);

	for (i = 0; i < NUM_MONITORED_SENSORS; i++) {
		/* Thresholds are considered inactive if set to 0 */
		if (data->max[i] == 0 && data->max_hyst[i] == 0
		    && data->min[i] == 0)
			continue;

		val = ab8500_gpadc_convert(data->gpadc, data->gpadc_addr[i]);
		if (val < 0) {
			dev_err(&data->pdev->dev, "GPADC read failed\n");
			continue;
		}

		mutex_lock(&data->lock);
		if (data->min[i] != 0) {
			if (val < data->min[i]) {
				if (data->min_alarm[i] == 0) {
					data->min_alarm[i] = 1;
					updated_min_alarm = true;
				}
			} else {
				if (data->min_alarm[i] == 1) {
					data->min_alarm[i] = 0;
					updated_min_alarm = true;
				}
			}

		}
		if (data->max[i] != 0) {
			if (val > data->max[i]) {
				if (data->max_alarm[i] == 0) {
					data->max_alarm[i] = 1;
					updated_max_alarm = true;
				}
			} else {
				if (data->max_alarm[i] == 1) {
					data->max_alarm[i] = 0;
					updated_max_alarm = true;
				}
			}

		}
		if (data->max_hyst[i] != 0) {
			if (val > data->max_hyst[i]) {
				if (data->max_hyst_alarm[i] == 0) {
					data->max_hyst_alarm[i] = 1;
					updated_max_hyst_alarm = true;
				}
			} else {
				if (data->max_hyst_alarm[i] == 1) {
					data->max_hyst_alarm[i] = 0;
					updated_max_hyst_alarm = true;
				}
			}
		}
		mutex_unlock(&data->lock);

		/* hwmon attr index starts at 1, thus "i+1" below */
		if (updated_min_alarm) {
			ret = snprintf(alarm_node, 16, "temp%d_min_alarm",
				       (i + 1));
			if (ret < 0) {
				dev_err(&data->pdev->dev,
					"Unable to update alarm node (%d)",
					ret);
				break;
			}
			sysfs_notify(&data->pdev->dev.kobj, NULL, alarm_node);
		}
		if (updated_max_alarm) {
			ret = snprintf(alarm_node, 16, "temp%d_max_alarm",
				       (i + 1));
			if (ret < 0) {
				dev_err(&data->pdev->dev,
					"Unable to update alarm node (%d)",
					ret);
				break;
			}
			sysfs_notify(&data->pdev->dev.kobj, NULL, alarm_node);
		}
		if (updated_max_hyst_alarm) {
			ret = snprintf(alarm_node, 21, "temp%d_max_hyst_alarm",
				       (i + 1));
			if (ret < 0) {
				dev_err(&data->pdev->dev,
					"Unable to update alarm node (%d)",
					ret);
				break;
			}
			sysfs_notify(&data->pdev->dev.kobj, NULL, alarm_node);
		}
	}
	delay_in_jiffies = msecs_to_jiffies(data->gpadc_monitor_delay);
	schedule_delayed_work(&data->work, delay_in_jiffies);
}

static inline void gpadc_monitor_exit(struct platform_device *pdev)
{
	struct ab8500_temp *data = platform_get_drvdata(pdev);
	cancel_delayed_work_sync(&data->work);
}

static ssize_t set_temp_monitor_delay(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf, size_t count)
{
	int res;
	unsigned long delay_in_jiffies, delay_in_s;
	struct ab8500_temp *data = dev_get_drvdata(dev);

	if (!find_active_thresholds(data)) {
		dev_dbg(dev, "No thresholds to monitor, disregarding delay\n");
		return count;
	}
	res = strict_strtoul(buf, 10, &delay_in_s);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	data->gpadc_monitor_delay = delay_in_s * 1000;
	mutex_unlock(&data->lock);
	delay_in_jiffies = msecs_to_jiffies(data->gpadc_monitor_delay);
	schedule_delayed_work(&data->work, delay_in_jiffies);

	return count;
}

static ssize_t set_temp_power_off_delay(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int res;
	unsigned long delay_in_jiffies, delay_in_s;
	struct ab8500_temp *data = dev_get_drvdata(dev);

	res = strict_strtoul(buf, 10, &delay_in_s);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	data->power_off_delay = delay_in_s * 1000;
	mutex_unlock(&data->lock);
	delay_in_jiffies = msecs_to_jiffies(data->power_off_delay);

	return count;
}

static ssize_t show_temp_monitor_delay(struct device *dev,
				       struct device_attribute *devattr,
				       char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	/* return time in s, not ms */
	return sprintf(buf, "%lu\n", (data->gpadc_monitor_delay) / 1000);
}

static ssize_t show_temp_power_off_delay(struct device *dev,
					 struct device_attribute *devattr,
					 char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	/* return time in s, not ms */
	return sprintf(buf, "%lu\n", (data->power_off_delay) / 1000);
}

/* HWMON sysfs interface */
static ssize_t show_name(struct device *dev, struct device_attribute *devattr,
			 char *buf)
{
	/*
	 * To avoid confusion between sensor label and chip name, the function
	 * "show_label" is not used to return the chip name.
	 */
	return sprintf(buf, "ab8500\n");
}

static ssize_t show_label(struct device *dev,
			  struct device_attribute *devattr, char *buf)
{
	char *name;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int index = attr->index;

	/*
	 * Make sure these labels correspond to the attribute indexes
	 * used when calling SENSOR_DEVICE_ATRR.
	 * Temperature sensors outside ab8500 (read via GPADC) are marked
	 * with prefix ext_
	 */
	switch (index) {
	case 1:
		name = "ext_rtc_xtal";
		break;
	case 2:
		name = "ext_db8500";
		break;
	case 3:
		name = "bat_temp";
		break;
	case 4:
		name = "ab8500";
		break;
    case 5:
		name = "bat_ctrl";
		break;
	default:
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", name);
}

static ssize_t show_input(struct device *dev,
			  struct device_attribute *devattr, char *buf)
{
	int val;
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	u8 gpadc_addr = data->gpadc_addr[attr->index - 1];

	val = ab8500_gpadc_convert(data->gpadc, gpadc_addr);
	if (val < 0)
		dev_err(&data->pdev->dev, "GPADC read failed\n");

	return sprintf(buf, "%d\n", val);
}

/* set functions (RW nodes) */
static ssize_t set_min(struct device *dev, struct device_attribute *devattr,
		       const char *buf, size_t count)
{
	unsigned long val;
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	/*
	 * Threshold is considered inactive if set to 0
	 * hwmon attr index starts at 1, thus "attr->index-1" below
	 */
	if (val == 0)
		data->min_alarm[attr->index - 1] = 0;

	data->min[attr->index - 1] = val;
	mutex_unlock(&data->lock);
	if (val == 0)
		(void) find_active_thresholds(data);

	return count;
}

static ssize_t set_max(struct device *dev, struct device_attribute *devattr,
		       const char *buf, size_t count)
{
	unsigned long val;
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	/*
	 * Threshold is considered inactive if set to 0
	 * hwmon attr index starts at 1, thus "attr->index-1" below
	 */
	if (val == 0)
		data->max_alarm[attr->index - 1] = 0;

	data->max[attr->index - 1] = val;
	mutex_unlock(&data->lock);
	if (val == 0)
		(void) find_active_thresholds(data);

	return count;
}

static ssize_t set_max_hyst(struct device *dev,
			    struct device_attribute *devattr,
			    const char *buf, size_t count)
{
	unsigned long val;
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int res = strict_strtoul(buf, 10, &val);
	if (res < 0)
		return res;

	mutex_lock(&data->lock);
	/*
	 * Threshold is considered inactive if set to 0
	 * hwmon attr index starts at 1, thus "attr->index-1" below
	 */
	if (val == 0)
		data->max_hyst_alarm[attr->index - 1] = 0;

	data->max_hyst[attr->index - 1] = val;
	mutex_unlock(&data->lock);
	if (val == 0)
		(void) find_active_thresholds(data);

	return count;
}

/*
 * show functions (RO nodes)
 * Notice that min/max/max_hyst refer to millivolts and not millidegrees
 */
static ssize_t show_min(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->min[attr->index - 1]);
}

static ssize_t show_max(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->max[attr->index - 1]);
}

static ssize_t show_max_hyst(struct device *dev,
			     struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->max_hyst[attr->index - 1]);
}

/* Alarms */
static ssize_t show_min_alarm(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->min_alarm[attr->index - 1]);
}

static ssize_t show_max_alarm(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->max_alarm[attr->index - 1]);
}

static ssize_t show_max_hyst_alarm(struct device *dev,
				   struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->max_hyst_alarm[attr->index - 1]);
}

static ssize_t show_crit_alarm(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	struct ab8500_temp *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	/* hwmon attr index starts at 1, thus "attr->index-1" below */
	return sprintf(buf, "%ld\n", data->crit_alarm[attr->index - 1]);
}

/*These node are not included in the kernel hwmon sysfs interface */
static SENSOR_DEVICE_ATTR(temp_monitor_delay, S_IRUGO | S_IWUSR,
			  show_temp_monitor_delay, set_temp_monitor_delay, 0);
static SENSOR_DEVICE_ATTR(temp_power_off_delay, S_IRUGO | S_IWUSR,
			  show_temp_power_off_delay,
			  set_temp_power_off_delay, 0);

/* Chip name, required by hwmon*/
static SENSOR_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, 0);

/* GPADC - ADC_AUX1 */
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, show_label, NULL, 1);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, show_input, NULL, 1);
static SENSOR_DEVICE_ATTR(temp1_min, S_IWUSR | S_IRUGO, show_min, set_min, 1);
static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, show_max, set_max, 1);
static SENSOR_DEVICE_ATTR(temp1_max_hyst, S_IWUSR | S_IRUGO,
			  show_max_hyst, set_max_hyst, 1);
static SENSOR_DEVICE_ATTR(temp1_min_alarm, S_IRUGO, show_min_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(temp1_max_alarm, S_IRUGO, show_max_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(temp1_max_hyst_alarm, S_IRUGO,
			  show_max_hyst_alarm, NULL, 1);

/* GPADC - ADC_AUX2 */
static SENSOR_DEVICE_ATTR(temp2_label, S_IRUGO, show_label, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, show_input, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_min, S_IWUSR | S_IRUGO, show_min, set_min, 2);
static SENSOR_DEVICE_ATTR(temp2_max, S_IWUSR | S_IRUGO, show_max, set_max, 2);
static SENSOR_DEVICE_ATTR(temp2_max_hyst, S_IWUSR | S_IRUGO,
			  show_max_hyst, set_max_hyst, 2);
static SENSOR_DEVICE_ATTR(temp2_min_alarm, S_IRUGO, show_min_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_max_alarm, S_IRUGO, show_max_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_max_hyst_alarm, S_IRUGO,
			  show_max_hyst_alarm, NULL, 2);

/* GPADC - BTEMP_BALL */
static SENSOR_DEVICE_ATTR(temp3_label, S_IRUGO, show_label, NULL, 3);
static SENSOR_DEVICE_ATTR(temp3_input, S_IRUGO, show_input, NULL, 3);

/* AB8500 */
static SENSOR_DEVICE_ATTR(temp4_label, S_IRUGO, show_label, NULL, 4);
static SENSOR_DEVICE_ATTR(temp4_crit_alarm, S_IRUGO,
			  show_crit_alarm, NULL, 4);

/* GPADC - BAT_CTRL */
static SENSOR_DEVICE_ATTR(temp5_label, S_IRUGO, show_label, NULL, 5);
static SENSOR_DEVICE_ATTR(temp5_input, S_IRUGO, show_input, NULL, 5);

static struct attribute *ab8500_temp_attributes[] = {
	&sensor_dev_attr_temp_power_off_delay.dev_attr.attr,
	&sensor_dev_attr_temp_monitor_delay.dev_attr.attr,
	&sensor_dev_attr_name.dev_attr.attr,
	/* GPADC - ADC_AUX1 */
	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_max_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst_alarm.dev_attr.attr,
	/* GPADC - ADC_AUX2 */
	&sensor_dev_attr_temp2_label.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_min.dev_attr.attr,
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp2_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_max_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_max_hyst_alarm.dev_attr.attr,
	/* GPADC - BTEMP_BALL */
	&sensor_dev_attr_temp3_label.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	/* AB8500 */
	&sensor_dev_attr_temp4_label.dev_attr.attr,
	&sensor_dev_attr_temp4_crit_alarm.dev_attr.attr,
	/* GPADC - BAT_CTRL */
	&sensor_dev_attr_temp5_label.dev_attr.attr,
	&sensor_dev_attr_temp5_input.dev_attr.attr,
	NULL
};

static const struct attribute_group ab8500_temp_group = {
	.attrs = ab8500_temp_attributes,
};

static irqreturn_t ab8500_temp_irq_handler(int irq, void *irq_data)
{
	unsigned long delay_in_jiffies;
	struct platform_device *pdev = irq_data;
	struct ab8500_temp *data = platform_get_drvdata(pdev);

	/*
	 * Make sure the magic numbers below corresponds to the node
	 * used for AB8500 thermal warning from HW.
	 */
	mutex_lock(&data->lock);
	data->crit_alarm[3] = 1;
	mutex_unlock(&data->lock);
	sysfs_notify(&pdev->dev.kobj, NULL, "temp4_crit_alarm");
	dev_info(&pdev->dev, "AB8500 thermal warning, power off in %lu s\n",
		 data->power_off_delay);
	delay_in_jiffies = msecs_to_jiffies(data->power_off_delay);
	schedule_delayed_work(&data->power_off_work, delay_in_jiffies);
	return IRQ_HANDLED;
}

static int setup_irqs(struct platform_device *pdev)
{
	int ret;
	int irq = platform_get_irq_byname(pdev, "AB8500_TEMP_WARM");

	if (irq < 0)
		dev_err(&pdev->dev, "Get irq by name failed\n");

	ret = request_threaded_irq(irq, NULL, ab8500_temp_irq_handler,
				   IRQF_NO_SUSPEND, "ab8500-temp", pdev);
	if (ret < 0)
		dev_err(&pdev->dev, "Request threaded irq failed (%d)\n", ret);

	return ret;
}

static int __devinit ab8500_temp_probe(struct platform_device *pdev)
{
	struct ab8500_temp *data;
	int err;

	data = kzalloc(sizeof(struct ab8500_temp), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = setup_irqs(pdev);
	if (err < 0)
		goto exit;

	data->gpadc = ab8500_gpadc_get();

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		goto exit;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&data->work, gpadc_monitor);
	INIT_DELAYED_WORK(&data->power_off_work, thermal_power_off);

	/*
	 * Setup HW defined data.
	 *
	 * Reference hardware (HREF):
	 *
	 * GPADC - ADC_AUX1, connected to NTC R2148 next to RTC_XTAL on HREF
	 * GPADC - ADC_AUX2, connected to NTC R2150 near DB8500 on HREF
	 * Hence, temp#_min/max/max_hyst refer to millivolts and not
	 * millidegrees
	 *
	 * HREF HW does not support reading AB8500 temperature. BUT an
	 * AB8500 IRQ will be launched if die crit temp limit is reached.
	 *
	 * Also:
	 * Battery temperature (BatTemp and BatCtrl) thresholds will
	 * not be exposed via hwmon.
	 *
	 * Make sure indexes correspond to the attribute indexes
	 * used when calling SENSOR_DEVICE_ATRR
	 */
	data->gpadc_addr[0] = ADC_AUX1;
	data->gpadc_addr[1] = ADC_AUX2;
	data->gpadc_addr[2] = BTEMP_BALL;
	data->gpadc_addr[4] = BAT_CTRL;
	mutex_init(&data->lock);
	data->pdev = pdev;
	data->power_off_delay = DEFAULT_POWER_OFF_DELAY;

	platform_set_drvdata(pdev, data);

	err = sysfs_create_group(&pdev->dev.kobj, &ab8500_temp_group);
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

static int __devexit ab8500_temp_remove(struct platform_device *pdev)
{
	struct ab8500_temp *data = platform_get_drvdata(pdev);

	gpadc_monitor_exit(pdev);
	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &ab8500_temp_group);
	platform_set_drvdata(pdev, NULL);
	kfree(data);
	return 0;
}

/* No action required in suspend/resume, thus the lack of functions */
static struct platform_driver ab8500_temp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ab8500-temp",
	},
	.probe = ab8500_temp_probe,
	.remove = __devexit_p(ab8500_temp_remove),
};

static int __init ab8500_temp_init(void)
{
	return platform_driver_register(&ab8500_temp_driver);
}

static void __exit ab8500_temp_exit(void)
{
	platform_driver_unregister(&ab8500_temp_driver);
}

MODULE_AUTHOR("Martin Persson <martin.persson@stericsson.com>");
MODULE_DESCRIPTION("AB8500 temperature driver");
MODULE_LICENSE("GPL");

module_init(ab8500_temp_init)
module_exit(ab8500_temp_exit)

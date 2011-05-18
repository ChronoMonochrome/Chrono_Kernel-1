/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Jarmo K. Kuronen <jarmo.kuronen@symbio.com>
 *         for ST-Ericsson.
 *
 * License terms: GPL V2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500-gpadc.h>
#include <linux/mfd/abx500/ab8500-gpio.h>
#include <linux/gpio.h>
#include <mach/ab8500-accdet.h>
#ifdef CONFIG_SND_SOC_UX500_AB8500
#include <sound/ux500_ab8500.h>
#else
#define ux500_ab8500_jack_report(i)
#endif

#define MAX_DET_COUNT			10
#define MAX_VOLT_DIFF			30
#define MIN_MIC_POWER			-100

/* Unique value used to identify Headset button input device */
#define BTN_INPUT_UNIQUE_VALUE	"AB8500HsBtn"
#define BTN_INPUT_DEV_NAME	"AB8500 Hs Button"

#define DEBOUNCE_PLUG_EVENT_MS		100
#define DEBOUNCE_PLUG_RETEST_MS		25
#define DEBOUNCE_UNPLUG_EVENT_MS	0

/*
 * Register definition for accessory detection.
 */
#define AB8500_REGU_CTRL1_SPARE_REG	0x84
#define AB8500_ACC_DET_DB1_REG		0x80
#define AB8500_ACC_DET_DB2_REG		0x81
#define AB8500_ACC_DET_CTRL_REG		0x82
#define AB8500_IT_SOURCE5_REG		0x04

/* REGISTER: AB8500_ACC_DET_CTRL_REG */
#define BITS_ACCDETCTRL2_ENA		(0x20 | 0x10 | 0x08)
#define BITS_ACCDETCTRL1_ENA		(0x02 | 0x01)

/* REGISTER: AB8500_REGU_CTRL1_SPARE_REG */
#define BIT_REGUCTRL1SPARE_VAMIC1_GROUND	0x01

/* REGISTER: AB8500_IT_SOURCE5_REG */
#define BIT_ITSOURCE5_ACCDET1		0x04

/* After being loaded, how fast the first check is to be made */
#define INIT_DELAY_MS			3000

/* Voltage limits (mV) for various types of AV Accessories */
#define ACCESSORY_DET_VOL_DONTCARE	-1
#define ACCESSORY_HEADPHONE_DET_VOL_MIN	0
#define ACCESSORY_HEADPHONE_DET_VOL_MAX	40
#define ACCESSORY_CVIDEO_DET_VOL_MIN	41
#define ACCESSORY_CVIDEO_DET_VOL_MAX	105
#define ACCESSORY_CARKIT_DET_VOL_MIN	1100
#define ACCESSORY_CARKIT_DET_VOL_MAX	1300
#define ACCESSORY_HEADSET_DET_VOL_MIN	0
#define ACCESSORY_HEADSET_DET_VOL_MAX	200
#define ACCESSORY_OPENCABLE_DET_VOL_MIN	1730
#define ACCESSORY_OPENCABLE_DET_VOL_MAX	2150

/* Macros */

/*
 * Conviniency macros to check jack characteristics.
 */
#define jack_supports_mic(type) \
	(type == JACK_TYPE_HEADSET || type == JACK_TYPE_CARKIT)
#define jack_supports_spkr(type) \
	((type != JACK_TYPE_DISCONNECTED) && (type != JACK_TYPE_CONNECTED))
#define jack_supports_buttons(type) \
	((type == JACK_TYPE_HEADSET) ||\
	(type == JACK_TYPE_CARKIT) ||\
	(type == JACK_TYPE_OPENCABLE) ||\
	(type == JACK_TYPE_CONNECTED))


/* Enumerations */

/**
 * @JACK_TYPE_UNSPECIFIED Not known whether any accessories are connected.
 * @JACK_TYPE_DISCONNECTED No accessories connected.
 * @JACK_TYPE_CONNECTED Accessory is connected but functionality was unable to
 * detect the actual type. In this mode, possible button events are reported.
 * @JACK_TYPE_HEADPHONE Headphone type of accessory (spkrs only) connected
 * @JACK_TYPE_HEADSET Headset type of accessory (mic+spkrs) connected
 * @JACK_TYPE_CARKIT Carkit type of accessory connected
 * @JACK_TYPE_OPENCABLE Open cable connected
 * @JACK_TYPE_CVIDEO CVideo type of accessory connected.
 */
enum accessory_jack_type {
       JACK_TYPE_UNSPECIFIED,
       JACK_TYPE_DISCONNECTED,
       JACK_TYPE_CONNECTED,
       JACK_TYPE_HEADPHONE,
       JACK_TYPE_HEADSET,
       JACK_TYPE_CARKIT,
       JACK_TYPE_OPENCABLE,
       JACK_TYPE_CVIDEO
};

/**
 * @BUTTON_UNK Button state not known
 * @BUTTON_PRESSED Button "down"
 * @BUTTON_RELEASED Button "up"
 */
enum accessory_button_state {
	BUTTON_UNK,
	BUTTON_PRESSED,
	BUTTON_RELEASED
};

/**
 * @PLUG_IRQ Interrupt gen. when accessory plugged in
 * @UNPLUG_IRQ Interrupt gen. when accessory plugged out
 * @BUTTON_PRESS_IRQ Interrupt gen. when accessory button pressed.
 * @BUTTON_RELEASE_IRQ Interrupt gen. when accessory button released.
 */
enum accessory_irq {
	PLUG_IRQ,
	UNPLUG_IRQ,
	BUTTON_PRESS_IRQ,
	BUTTON_RELEASE_IRQ
};

/**
 * Enumerates the op. modes of the avcontrol switch
 * @AUDIO_IN Audio input is selected
 * @VIDEO_OUT Video output is selected
 * @NOT_SET The av-switch control signal is disconnected.
 */
enum accessory_avcontrol_dir {
	AUDIO_IN,
	VIDEO_OUT,
	NOT_SET,
};

/**
 * @REGULATOR_VAUDIO v-audio regulator
 * @REGULATOR_VAMIC1 v-amic1 regulator
 * @REGULATOR_AVSWITCH Audio/Video select switch regulator
 * @REGULATOR_ALL All regulators combined
 */
enum accessory_regulator {
	REGULATOR_NONE = 0x0,
	REGULATOR_VAUDIO = 0x1,
	REGULATOR_VAMIC1 = 0x2,
	REGULATOR_AVSWITCH = 0x4,
	REGULATOR_ALL = 0xFF
};

/* Structures */

/**
 * Describes an interrupt
 * @irq interrupt identifier
 * @name name of the irq in platform data
 * @isr interrupt service routine
 * @register are we currently registered to receive interrupts from this source.
 */
struct accessory_irq_descriptor {
	enum accessory_irq irq;
	const char *name;
	irq_handler_t isr;
	int registered;
};

/**
 * Encapsulates info of single regulator.
 * @id regulator identifier
 * @name name of the regulator
 * @enabled flag indicating whether regu is currently enabled.
 * @handle regulator handle
 */
struct accessory_regu_descriptor {
	enum accessory_regulator id;
	const char *name;
	int enabled;
	struct regulator *handle;
};

/**
 * Defines attributes for accessory detection operation.
 * @typename type as string
 * @type Type of accessory this task tests
 * @req_det_count How many times this particular type of accessory
 * needs to be detected in sequence in order to accept. Multidetection
 * implemented to avoid false detections during plug-in.
 * @meas_mv Should ACCDETECT2 input voltage be measured just before
 * making the decision or can cached voltage be used instead.
 * @minvol minimum voltage (mV) for decision
 * @maxvol maximum voltage (mV) for decision
 */
struct accessory_detect_task {
	const char *typename;
	enum accessory_jack_type type;
	int req_det_count;
	int meas_mv;
	int minvol;
	int maxvol;
};

/**
 * Device data, capsulates all relevant device data structures.
 *
 * @pdev pointer to platform device
 * @pdata Platform data
 * @gpadc interface for ADC data
 * @irq_work_queue Work queue for deferred interrupt processing
 *
 * @detect_work work item to perform detection work
 * @unplug_irq_work work item to process unplug event
 * @init_work work item to process initialization work.
 *
 * @btn_input_dev button input device used to report btn presses
 * @btn_state Current state of accessory button
 *
 * @jack_type type of currently connected accessory
 * @reported_jack_type previously reported jack type.
 * @jack_type_temp temporary storage for currently connected accessory
 *
 * @jack_det_count counter how many times in sequence the accessory
 * type detection has produced same result.
 * @total_jack_det_count after plug-in irq, how many times detection
 * has totally been made in order to detect the accessory type
 *
 * @detect_jiffies Used to save timestamp when detection was made. Timestamp
 * used to filter out spurious button presses that might occur during the
 * plug-in procedure.
 *
 * @accdet1_th_set flag to indicate whether accdet1 threshold and debounce
 * times are configured
 * @accdet2_th_set flag to indicate whether accdet2 thresholds are configured
 * @gpio35_dir_set flag to indicate whether GPIO35 (VIDEOCTRL) direction
 * has been configured.
 */
struct ab8500_ad {
	struct platform_device *pdev;
	struct ab8500_accdet_platform_data *pdata;
	struct ab8500_gpadc *gpadc;
	struct workqueue_struct *irq_work_queue;

	struct delayed_work detect_work;
	struct delayed_work unplug_irq_work;
	struct delayed_work init_work;

	struct input_dev *btn_input_dev;
	enum accessory_button_state btn_state;

	enum accessory_jack_type jack_type;
	enum accessory_jack_type reported_jack_type;
	enum accessory_jack_type jack_type_temp;

	int jack_det_count;
	int total_jack_det_count;

	unsigned long detect_jiffies;

	int accdet1_th_set;
	int accdet2_th_set;
	int gpio35_dir_set;
};

/* Forward declarations */

static void config_accdetect(struct ab8500_ad *dd);

static void release_irq(struct ab8500_ad *dd, enum accessory_irq irq_id);
static void claim_irq(struct ab8500_ad *dd, enum accessory_irq irq_id);

static irqreturn_t unplug_irq_handler(int irq, void *_userdata);
static irqreturn_t plug_irq_handler(int irq, void *_userdata);
static irqreturn_t button_press_irq_handler(int irq, void *_userdata);
static irqreturn_t button_release_irq_handler(int irq, void *_userdata);

static void unplug_irq_handler_work(struct work_struct *work);
static void detect_work(struct work_struct *work);
static void init_work(struct work_struct *work);

static enum accessory_jack_type detect(struct ab8500_ad *dd, int *required_det);
static void set_av_switch(struct ab8500_ad *dd,
		enum accessory_avcontrol_dir dir);

/* Static data initialization */

static struct accessory_detect_task detect_ops[] = {
	{
		.type = JACK_TYPE_DISCONNECTED,
		.typename = "DISCONNECTED",
		.meas_mv = 1,
		.req_det_count = 1,
		.minvol = ACCESSORY_DET_VOL_DONTCARE,
		.maxvol = ACCESSORY_DET_VOL_DONTCARE
	},
	{
		.type = JACK_TYPE_HEADPHONE,
		.typename = "HEADPHONE",
		.meas_mv = 1,
		.req_det_count = 1,
		.minvol = ACCESSORY_HEADPHONE_DET_VOL_MIN,
		.maxvol = ACCESSORY_HEADPHONE_DET_VOL_MAX
	},
	{
		.type = JACK_TYPE_CVIDEO,
		.typename = "CVIDEO",
		.meas_mv = 0,
		.req_det_count = 4,
		.minvol = ACCESSORY_CVIDEO_DET_VOL_MIN,
		.maxvol = ACCESSORY_CVIDEO_DET_VOL_MAX
	},
	{
		.type = JACK_TYPE_OPENCABLE,
		.typename = "OPENCABLE",
		.meas_mv = 0,
		.req_det_count = 4,
		.minvol = ACCESSORY_OPENCABLE_DET_VOL_MIN,
		.maxvol = ACCESSORY_OPENCABLE_DET_VOL_MAX
	},
	{
		.type = JACK_TYPE_CARKIT,
		.typename = "CARKIT",
		.meas_mv = 1,
		.req_det_count = 1,
		.minvol = ACCESSORY_CARKIT_DET_VOL_MIN,
		.maxvol = ACCESSORY_CARKIT_DET_VOL_MAX
	},
	{
		.type = JACK_TYPE_HEADSET,
		.typename = "HEADSET",
		.meas_mv = 0,
		.req_det_count = 2,
		.minvol = ACCESSORY_HEADSET_DET_VOL_MIN,
		.maxvol = ACCESSORY_HEADSET_DET_VOL_MAX
	},
	{
		.type = JACK_TYPE_CONNECTED,
		.typename = "CONNECTED",
		.meas_mv = 0,
		.req_det_count = 4,
		.minvol = ACCESSORY_DET_VOL_DONTCARE,
		.maxvol = ACCESSORY_DET_VOL_DONTCARE
	}
};

static struct accessory_regu_descriptor regu_desc[3] = {
	{
		.id = REGULATOR_VAUDIO,
		.name = "v-audio",
	},
	{
		.id = REGULATOR_VAMIC1,
		.name = "v-amic1",
	},
	{
		.id = REGULATOR_AVSWITCH,
		.name = "vcc-avswitch",
	},
};

static struct accessory_irq_descriptor irq_desc_norm[] = {
	{
		.irq = PLUG_IRQ,
		.name = "ACC_DETECT_1DB_F",
		.isr = plug_irq_handler,
	},
	{
		.irq = UNPLUG_IRQ,
		.name = "ACC_DETECT_1DB_R",
		.isr = unplug_irq_handler,
	},
	{
		.irq = BUTTON_PRESS_IRQ,
		.name = "ACC_DETECT_22DB_F",
		.isr = button_press_irq_handler,
	},
	{
		.irq = BUTTON_RELEASE_IRQ,
		.name = "ACC_DETECT_22DB_R",
		.isr = button_release_irq_handler,
	},
};

static struct accessory_irq_descriptor irq_desc_inverted[] = {
	{
		.irq = PLUG_IRQ,
		.name = "ACC_DETECT_1DB_R",
		.isr = plug_irq_handler,
	},
	{
		.irq = UNPLUG_IRQ,
		.name = "ACC_DETECT_1DB_F",
		.isr = unplug_irq_handler,
	},
	{
		.irq = BUTTON_PRESS_IRQ,
		.name = "ACC_DETECT_22DB_R",
		.isr = button_press_irq_handler,
	},
	{
		.irq = BUTTON_RELEASE_IRQ,
		.name = "ACC_DETECT_22DB_F",
		.isr = button_release_irq_handler,
	},
};

static struct accessory_irq_descriptor *irq_desc;

/*
 * textual represenation of the accessory type
 */
static const char *accessory_str(enum accessory_jack_type type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(detect_ops); i++)
		if (type == detect_ops[i].type)
			return detect_ops[i].typename;

	return "UNKNOWN?";
}

/*
 * enables regulator but only if it has not been enabled earlier.
 */
static void accessory_regulator_enable(enum accessory_regulator reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(regu_desc); i++) {
		if (reg & regu_desc[i].id) {
			if (!regu_desc[i].enabled) {
				if (!regulator_enable(regu_desc[i].handle))
					regu_desc[i].enabled = 1;
			}
		}
	}
}

/*
 * disables regulator but only if it has been previously enabled.
 */
static void accessory_regulator_disable(enum accessory_regulator reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(regu_desc); i++) {
		if (reg & regu_desc[i].id) {
			if (regu_desc[i].enabled) {
				if (!regulator_disable(regu_desc[i].handle))
					regu_desc[i].enabled = 0;
			}
		}
	}
}

/*
 * frees previously retrieved regulators.
 */
static void free_regulators(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(regu_desc); i++) {
		if (regu_desc[i].handle) {
			regulator_put(regu_desc[i].handle);
			regu_desc[i].handle = NULL;
		}
	}
}

/*
 * gets required regulators.
 */
static int create_regulators(struct ab8500_ad *dd)
{
	int i;
	int status = 0;

	for (i = 0; i < ARRAY_SIZE(regu_desc); i++) {
		struct regulator *regu =
			regulator_get(&dd->pdev->dev, regu_desc[i].name);
		if (IS_ERR(regu)) {
			status = PTR_ERR(regu);
			dev_err(&dd->pdev->dev,
				"%s: Failed to get supply '%s' (%d).\n",
				__func__, regu_desc[i].name, status);
			free_regulators();
			goto out;
		} else {
			regu_desc[i].handle = regu;
		}
	}

out:
	return status;
}

/*
 * configures accdet2 input on/off
 */
static void config_accdetect2_hw(struct ab8500_ad *dd, int enable)
{
	int ret = 0;

	if (!dd->accdet2_th_set) {
		/* Configure accdetect21+22 thresholds */
		ret = abx500_set_register_interruptible(&dd->pdev->dev,
				AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_DB2_REG,
				dd->pdata->accdet2122_th);
		if (ret < 0) {
			dev_err(&dd->pdev->dev,
				"%s: Failed to write reg (%d).\n", __func__,
					ret);
			goto out;
		} else {
			dd->accdet2_th_set = 1;
		}
	}

	/* Enable/Disable accdetect21 comparators + pullup */
	ret = abx500_mask_and_set_register_interruptible(
			&dd->pdev->dev,
			AB8500_ECI_AV_ACC,
			AB8500_ACC_DET_CTRL_REG,
			BITS_ACCDETCTRL2_ENA,
			enable ? BITS_ACCDETCTRL2_ENA : 0);

	if (ret < 0)
		dev_err(&dd->pdev->dev, "%s: Failed to update reg (%d).\n",
				__func__, ret);

out:
	return;
}

/*
 * configures accdet1 input on/off
 */
static void config_accdetect1_hw(struct ab8500_ad *dd, int enable)
{
	int ret;

	if (!dd->accdet1_th_set) {
		ret = abx500_set_register_interruptible(&dd->pdev->dev,
				AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_DB1_REG,
				dd->pdata->accdet1_dbth);
		if (ret < 0)
			dev_err(&dd->pdev->dev,
				"%s: Failed to write reg (%d).\n", __func__,
				ret);
		else
			dd->accdet1_th_set = 1;
	}

	/* enable accdetect1 comparator */
	ret = abx500_mask_and_set_register_interruptible(
				&dd->pdev->dev,
				AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_CTRL_REG,
				BITS_ACCDETCTRL1_ENA,
				enable ? BITS_ACCDETCTRL1_ENA : 0);

	if (ret < 0)
		dev_err(&dd->pdev->dev,
			"%s: Failed to update reg (%d).\n", __func__, ret);
}

/*
 * create input device for button press reporting
 */
static int create_btn_input_dev(struct ab8500_ad *dd)
{
	int err;

	dd->btn_input_dev = input_allocate_device();
	if (!dd->btn_input_dev) {
		dev_err(&dd->pdev->dev, "%s: Failed to allocate input dev.\n",
			__func__);
		err = -ENOMEM;
		goto out;
	}

	input_set_capability(dd->btn_input_dev,
				EV_KEY,
				dd->pdata->btn_keycode);

	dd->btn_input_dev->name = BTN_INPUT_DEV_NAME;
	dd->btn_input_dev->uniq = BTN_INPUT_UNIQUE_VALUE;
	dd->btn_input_dev->dev.parent = &dd->pdev->dev;

	err = input_register_device(dd->btn_input_dev);
	if (err) {
		dev_err(&dd->pdev->dev,
			"%s: register_input_device failed (%d).\n", __func__,
			err);
		input_free_device(dd->btn_input_dev);
		dd->btn_input_dev = NULL;
		goto out;
	}
out:
	return err;
}

/*
 * reports jack status
 */
static void report_jack_status(struct ab8500_ad *dd)
{
	int value = 0;

	/* Never report possible open cable */
	if (dd->jack_type == JACK_TYPE_OPENCABLE)
		goto out;

	/* Never report same state twice in a row */
	if (dd->jack_type == dd->reported_jack_type)
		goto out;
	dd->reported_jack_type = dd->jack_type;

	dev_info(&dd->pdev->dev, "Accessory: %s\n",
		accessory_str(dd->jack_type));

	if (dd->jack_type != JACK_TYPE_DISCONNECTED &&
		dd->jack_type != JACK_TYPE_UNSPECIFIED)
		value |= SND_JACK_MECHANICAL;
	if (jack_supports_mic(dd->jack_type))
		value |= SND_JACK_MICROPHONE;
	if (jack_supports_spkr(dd->jack_type))
		value |= (SND_JACK_HEADPHONE | SND_JACK_LINEOUT);
	if (dd->jack_type == JACK_TYPE_CVIDEO) {
		value |= SND_JACK_VIDEOOUT;
		set_av_switch(dd, VIDEO_OUT);
	}

	ux500_ab8500_jack_report(value);

out: return;
}

/*
 * returns the high level status whether some accessory is connected (1|0).
 */
static int detect_plugged_in(struct ab8500_ad *dd)
{
	u8 value = 0;

	int status = abx500_get_register_interruptible(
				&dd->pdev->dev,
				AB8500_INTERRUPT,
				AB8500_IT_SOURCE5_REG,
				&value);
	if (status < 0) {
		dev_err(&dd->pdev->dev, "%s: reg read failed (%d).\n",
			__func__, status);
		return 0;
	}

	if (dd->pdata->is_detection_inverted)
		return value & BIT_ITSOURCE5_ACCDET1 ? 1 : 0;
	else
		return value & BIT_ITSOURCE5_ACCDET1 ? 0 : 1;
}

/*
 * mic_line_voltage_stable - measures a relative stable voltage from spec. input
 */
static int meas_voltage_stable(struct ab8500_ad *dd, u8 input)
{
	int iterations = 2;
	int v1, v2, dv;

	v1 = ab8500_gpadc_convert(dd->gpadc, input);
	do {
		msleep(1);
		--iterations;
		v2 = ab8500_gpadc_convert(dd->gpadc, input);
		dv = abs(v2 - v1);
		v1 = v2;
	} while (iterations > 0 && dv > MAX_VOLT_DIFF);

	return v1;
}

/*
 * worker routine to handle accessory unplug case
 */
static void unplug_irq_handler_work(struct work_struct *work)
{
	struct ab8500_ad *dd = container_of(work,
		struct ab8500_ad, unplug_irq_work.work);

	dev_dbg(&dd->pdev->dev, "%s: Enter\n", __func__);

	dd->jack_type = dd->jack_type_temp = JACK_TYPE_DISCONNECTED;
	dd->jack_det_count = dd->total_jack_det_count = 0;
	dd->btn_state = BUTTON_UNK;
	config_accdetect(dd);

	accessory_regulator_disable(REGULATOR_ALL);

	report_jack_status(dd);
}

/*
 * interrupt service routine for accessory unplug.
 */
static irqreturn_t unplug_irq_handler(int irq, void *_userdata)
{
	struct ab8500_ad *dd = _userdata;

	dev_dbg(&dd->pdev->dev, "%s: Enter (irq=%d)\n", __func__, irq);

	queue_delayed_work(dd->irq_work_queue, &dd->unplug_irq_work,
			msecs_to_jiffies(DEBOUNCE_UNPLUG_EVENT_MS));

	return IRQ_HANDLED;
}

/*
 * interrupt service routine for accessory plug.
 */
static irqreturn_t plug_irq_handler(int irq, void *_userdata)
{
	struct ab8500_ad *dd = _userdata;

	dev_dbg(&dd->pdev->dev, "%s: Enter (irq=%d)\n",
		__func__, irq);

	switch (dd->jack_type) {
	case JACK_TYPE_DISCONNECTED:
	case JACK_TYPE_UNSPECIFIED:
		queue_delayed_work(dd->irq_work_queue, &dd->detect_work,
			msecs_to_jiffies(DEBOUNCE_PLUG_EVENT_MS));
		break;

	default:
		dev_err(&dd->pdev->dev, "%s: Unexpected plug IRQ\n", __func__);
		break;
	}

	return IRQ_HANDLED;
}

/*
 * worker routine to perform detection.
 */
static void detect_work(struct work_struct *work)
{
	int req_det_count = 1;
	enum accessory_jack_type new_type;
	struct ab8500_ad *dd = container_of(work,
		struct ab8500_ad, detect_work.work);

	dev_dbg(&dd->pdev->dev, "%s: Enter\n", __func__);

	set_av_switch(dd, AUDIO_IN);

	new_type = detect(dd, &req_det_count);

	dd->total_jack_det_count++;
	if (dd->jack_type_temp == new_type) {
		dd->jack_det_count++;
	} else {
		dd->jack_det_count = 1;
		dd->jack_type_temp = new_type;
	}

	if (dd->total_jack_det_count >= MAX_DET_COUNT) {
		dev_err(&dd->pdev->dev,
			"%s: MAX_DET_COUNT(=%d) reached. Bailing out.\n",
					__func__, MAX_DET_COUNT);
		queue_delayed_work(dd->irq_work_queue, &dd->unplug_irq_work,
				msecs_to_jiffies(DEBOUNCE_UNPLUG_EVENT_MS));
	} else if (dd->jack_det_count >= req_det_count) {
		dd->total_jack_det_count = dd->jack_det_count = 0;
		dd->jack_type = new_type;
		dd->detect_jiffies = jiffies;
		report_jack_status(dd);
		config_accdetect(dd);
	} else {
		queue_delayed_work(dd->irq_work_queue,
				&dd->detect_work,
				msecs_to_jiffies(DEBOUNCE_PLUG_RETEST_MS));
	}
}

/*
 * reports a button event (pressed, released).
 */
static void report_btn_event(struct ab8500_ad *dd, int down)
{
	input_report_key(dd->btn_input_dev, dd->pdata->btn_keycode, down);
	input_sync(dd->btn_input_dev);

	dev_dbg(&dd->pdev->dev, "HS-BTN: %s\n", down ? "PRESSED" : "RELEASED");
}

/*
 * interrupt service routine invoked when hs button is pressed down.
 */
static irqreturn_t button_press_irq_handler(int irq, void *_userdata)
{
	struct ab8500_ad *dd = _userdata;

	unsigned long accept_jiffies = dd->detect_jiffies +
			msecs_to_jiffies(1000);
	if (time_before(jiffies, accept_jiffies)) {
		dev_dbg(&dd->pdev->dev, "%s: Skipped spurious btn press.\n",
			__func__);
		return IRQ_HANDLED;
	}

	dev_dbg(&dd->pdev->dev, "%s: Enter (irq=%d)\n", __func__, irq);

	if (dd->jack_type == JACK_TYPE_OPENCABLE) {
		/* Someting got connected to open cable -> detect.. */
		config_accdetect2_hw(dd, 0);
		queue_delayed_work(dd->irq_work_queue, &dd->detect_work,
			msecs_to_jiffies(DEBOUNCE_PLUG_EVENT_MS));
		return IRQ_HANDLED;
	}

	if (dd->btn_state == BUTTON_PRESSED)
		return IRQ_HANDLED;

	if (jack_supports_buttons(dd->jack_type)) {
		dd->btn_state = BUTTON_PRESSED;
		report_btn_event(dd, 1);
	} else {
		dd->btn_state = BUTTON_UNK;
	}

	return IRQ_HANDLED;
}

/*
 * interrupts service routine invoked when hs button is released.
 */
static irqreturn_t button_release_irq_handler(int irq, void *_userdata)
{
	struct ab8500_ad *dd = _userdata;

	dev_dbg(&dd->pdev->dev, "%s: Enter (irq=%d)\n", __func__, irq);

	if (dd->jack_type == JACK_TYPE_OPENCABLE)
		return IRQ_HANDLED;

	if (dd->btn_state != BUTTON_PRESSED)
		return IRQ_HANDLED;

	if (jack_supports_buttons(dd->jack_type)) {
		report_btn_event(dd, 0);
		dd->btn_state = BUTTON_RELEASED;
	} else {
		dd->btn_state = BUTTON_UNK;
	}

	return IRQ_HANDLED;
}

/*
 * configures HW so that it is possible to make decision whether
 * accessory is connected or not.
 */
static void config_hw_test_plug_connected(struct ab8500_ad *dd, int enable)
{
	int ret;

	dev_dbg(&dd->pdev->dev, "%s:%d\n", __func__, enable);

	ret = ab8500_config_pull_up_or_down(&dd->pdev->dev,
					dd->pdata->video_ctrl_gpio, !enable);
	if (ret < 0) {
		dev_err(&dd->pdev->dev,
			"%s: Failed to update reg (%d).\n", __func__, ret);
		return;
	}

	if (enable)
		accessory_regulator_enable(REGULATOR_VAMIC1);
}

/*
 * configures HW so that carkit/headset detection can be accomplished.
 */
static void config_hw_test_basic_carkit(struct ab8500_ad *dd, int enable)
{
	int ret;

	dev_dbg(&dd->pdev->dev, "%s:%d\n", __func__, enable);

	if (enable)
		accessory_regulator_disable(REGULATOR_VAMIC1);

	/* Un-Ground the VAMic1 output when enabled */
	ret = abx500_mask_and_set_register_interruptible(
				&dd->pdev->dev,
				AB8500_REGU_CTRL1,
				AB8500_REGU_CTRL1_SPARE_REG,
				BIT_REGUCTRL1SPARE_VAMIC1_GROUND,
				enable ? BIT_REGUCTRL1SPARE_VAMIC1_GROUND : 0);
	if (ret < 0)
		dev_err(&dd->pdev->dev,
			"%s: Failed to update reg (%d).\n", __func__, ret);
}

/*
 * checks whether measured voltage is in given range. depending on arguments,
 * voltage might be re-measured or previously measured voltage is reused.
 */
static int mic_vol_in_range(struct ab8500_ad *dd,
			int lo, int hi, int force_read)
{
	static int mv = MIN_MIC_POWER;

	if (mv == -100 || force_read)
		mv = meas_voltage_stable(dd, ACC_DETECT2);

	return (mv >= lo && mv <= hi) ? 1 : 0;
}

/*
 * checks whether the currently connected HW is of given type.
 */
static int detect_hw(struct ab8500_ad *dd,
			struct accessory_detect_task *task)
{
	int status;

	switch (task->type) {
	case JACK_TYPE_DISCONNECTED:
		config_hw_test_plug_connected(dd, 1);
		status = !detect_plugged_in(dd);
		break;
	case JACK_TYPE_CONNECTED:
		config_hw_test_plug_connected(dd, 1);
		status = detect_plugged_in(dd);
		break;
	case JACK_TYPE_CARKIT:
		config_hw_test_basic_carkit(dd, 1);
		/* flow through.. */
	case JACK_TYPE_HEADPHONE:
	case JACK_TYPE_CVIDEO:
	case JACK_TYPE_HEADSET:
	case JACK_TYPE_OPENCABLE:
		status = mic_vol_in_range(dd,
					task->minvol,
					task->maxvol,
					task->meas_mv);
		break;
	default:
		status = 0;
	}

	return status;
}

/*
 * sets the av switch direction - audio-in vs video-out
 */
static void set_av_switch(struct ab8500_ad *dd,
		enum accessory_avcontrol_dir dir)
{
	int ret;

	dev_dbg(&dd->pdev->dev, "%s: Enter (%d)\n", __func__, dir);
	if (dir == NOT_SET) {
		ret = gpio_direction_input(dd->pdata->video_ctrl_gpio);
		dd->gpio35_dir_set = 0;
		ret = gpio_direction_output(dd->pdata->video_ctrl_gpio, 0);
	} else if (!dd->gpio35_dir_set) {
		ret = gpio_direction_output(dd->pdata->video_ctrl_gpio,
						dir == AUDIO_IN ? 1 : 0);
		if (ret < 0) {
			dev_err(&dd->pdev->dev,
				"%s: Output video ctrl signal failed (%d).\n",
								__func__, ret);
		} else {
			dd->gpio35_dir_set = 1;
			dev_dbg(&dd->pdev->dev, "AV-SWITCH: %s\n",
				dir == AUDIO_IN ? "AUDIO_IN" : "VIDEO_OUT");
		}
	} else {
		gpio_set_value(dd->pdata->video_ctrl_gpio,
						dir == AUDIO_IN ? 1 : 0);
	}
}

/*
 * Tries to detect the currently attached accessory
 */
static enum accessory_jack_type detect(struct ab8500_ad *dd,
			int *req_det_count)
{
	enum accessory_jack_type type = JACK_TYPE_DISCONNECTED;
	int i;

	accessory_regulator_enable(REGULATOR_VAUDIO | REGULATOR_AVSWITCH);

	for (i = 0; i < ARRAY_SIZE(detect_ops); ++i) {
		if (detect_hw(dd, &detect_ops[i])) {
			type = detect_ops[i].type;
			*req_det_count = detect_ops[i].req_det_count;
			break;
		}
	}

	config_hw_test_basic_carkit(dd, 0);
	config_hw_test_plug_connected(dd, 0);

	if (jack_supports_buttons(type))
		accessory_regulator_enable(REGULATOR_VAMIC1);
	else
		accessory_regulator_disable(REGULATOR_VAMIC1 |
						REGULATOR_AVSWITCH);

	accessory_regulator_disable(REGULATOR_VAUDIO);

	return type;
}

/*
 * registers to specific interrupt
 */
static void claim_irq(struct ab8500_ad *dd, enum accessory_irq irq_id)
{
	int ret;
	int irq;

	if (dd->pdata->is_detection_inverted)
		irq_desc = irq_desc_inverted;
	else
		irq_desc = irq_desc_norm;

	if (irq_desc[irq_id].registered)
		return;

	irq = platform_get_irq_byname(
			dd->pdev,
			irq_desc[irq_id].name);
	if (irq < 0) {
		dev_err(&dd->pdev->dev,
			"%s: Failed to get irq %s\n", __func__,
			irq_desc[irq_id].name);
		return;
	}

	ret = request_threaded_irq(irq,
				NULL,
				irq_desc[irq_id].isr,
				IRQF_NO_SUSPEND | IRQF_SHARED,
				irq_desc[irq_id].name,
				dd);
	if (ret != 0) {
		dev_err(&dd->pdev->dev,
			"%s: Failed to claim irq %s (%d)\n",
			__func__,
			irq_desc[irq_id].name,
			ret);
	} else {
		irq_desc[irq_id].registered = 1;
		dev_dbg(&dd->pdev->dev, "%s: %s\n",
			__func__, irq_desc[irq_id].name);
	}
}

/*
 * releases specific interrupt
 */
static void release_irq(struct ab8500_ad *dd, enum accessory_irq irq_id)
{
	int irq;

	if (dd->pdata->is_detection_inverted)
		irq_desc = irq_desc_inverted;
	else
		irq_desc = irq_desc_norm;

	if (!irq_desc[irq_id].registered)
		return;

	irq = platform_get_irq_byname(
			dd->pdev,
			irq_desc[irq_id].name);
	if (irq < 0) {
		dev_err(&dd->pdev->dev,
			"%s: Failed to get irq %s (%d)\n",
			__func__,
			irq_desc[irq_id].name, irq);
	} else {
		free_irq(irq, dd);
		irq_desc[irq_id].registered = 0;
		dev_dbg(&dd->pdev->dev, "%s: %s\n",
			__func__, irq_desc[irq_id].name);
	}
}

/*
 * configures interrupts + detection hardware to meet the requirements
 * set by currently attached accessory type.
 */
static void config_accdetect(struct ab8500_ad *dd)
{
	switch (dd->jack_type) {
	case JACK_TYPE_UNSPECIFIED:
		config_accdetect1_hw(dd, 1);
		config_accdetect2_hw(dd, 0);

		release_irq(dd, PLUG_IRQ);
		release_irq(dd, UNPLUG_IRQ);
		release_irq(dd, BUTTON_PRESS_IRQ);
		release_irq(dd, BUTTON_RELEASE_IRQ);
		set_av_switch(dd, NOT_SET);
		break;

	case JACK_TYPE_DISCONNECTED:
		set_av_switch(dd, NOT_SET);
	case JACK_TYPE_HEADPHONE:
	case JACK_TYPE_CVIDEO:
		config_accdetect1_hw(dd, 1);
		config_accdetect2_hw(dd, 0);

		claim_irq(dd, PLUG_IRQ);
		claim_irq(dd, UNPLUG_IRQ);
		release_irq(dd, BUTTON_PRESS_IRQ);
		release_irq(dd, BUTTON_RELEASE_IRQ);
		break;

	case JACK_TYPE_CONNECTED:
	case JACK_TYPE_HEADSET:
	case JACK_TYPE_CARKIT:
	case JACK_TYPE_OPENCABLE:
		config_accdetect1_hw(dd, 1);
		config_accdetect2_hw(dd, 1);

		release_irq(dd, PLUG_IRQ);
		claim_irq(dd, UNPLUG_IRQ);
		claim_irq(dd, BUTTON_PRESS_IRQ);
		claim_irq(dd, BUTTON_RELEASE_IRQ);
		break;

	default:
		dev_err(&dd->pdev->dev, "%s: Unknown type: %d\n",
			__func__, dd->jack_type);
	}
}

/*
 * Deferred initialization of the work.
 */
static void init_work(struct work_struct *work)
{
	struct ab8500_ad *dd = container_of(work,
		struct ab8500_ad, init_work.work);

	dev_dbg(&dd->pdev->dev, "%s: Enter\n", __func__);

	dd->jack_type = dd->reported_jack_type = JACK_TYPE_UNSPECIFIED;
	config_accdetect(dd);
	queue_delayed_work(dd->irq_work_queue,
				&dd->detect_work,
				msecs_to_jiffies(0));
}

/*
 * performs platform device initialization
 */
static int ab8500_accessory_init(struct platform_device *pdev)
{
	struct ab8500_ad *dd;
	struct ab8500_platform_data *plat;

	dev_dbg(&pdev->dev, "Enter: %s\n", __func__);

	dd = kzalloc(sizeof(struct ab8500_ad), GFP_KERNEL);
	if (!dd) {
		dev_err(&pdev->dev, "%s: Mem. alloc failed\n", __func__);
		goto fail_no_mem_for_devdata;
	}

	dd->pdev = pdev;
	dd->pdata = pdev->dev.platform_data;
	plat = dev_get_platdata(pdev->dev.parent);

	if (!plat || !plat->accdet) {
		dev_err(&pdev->dev, "%s: Failed to get accdet plat data.\n",
			__func__);
		goto fail_no_ab8500_dev;
	}
	dd->pdata = plat->accdet;

	if (dd->pdata->video_ctrl_gpio) {
		if (!gpio_is_valid(dd->pdata->video_ctrl_gpio)) {
			dev_err(&pdev->dev,
				"%s: Video ctrl GPIO invalid (%d).\n", __func__,
						dd->pdata->video_ctrl_gpio);
			goto fail_video_ctrl_gpio;
		}
		if (gpio_request(dd->pdata->video_ctrl_gpio, "Video Control")) {
			dev_err(&pdev->dev, "%s: Get video ctrl GPIO failed.\n",
								__func__);
			goto fail_video_ctrl_gpio;
		}
	}

	if (create_btn_input_dev(dd) < 0) {
		dev_err(&pdev->dev, "%s: create_button_input_dev failed.\n",
			__func__);
		goto fail_no_btn_input_dev;
	}

	if (create_regulators(dd) < 0) {
		dev_err(&pdev->dev, "%s: failed to create regulators\n",
			__func__);
		goto fail_no_regulators;
	}
	dd->btn_state = BUTTON_UNK;

	dd->irq_work_queue = create_singlethread_workqueue("ab8500_accdet_wq");
	if (!dd->irq_work_queue) {
		dev_err(&pdev->dev, "%s: Failed to create wq\n", __func__);
		goto fail_no_mem_for_wq;
	}
	dd->gpadc = ab8500_gpadc_get();

	INIT_DELAYED_WORK(&dd->detect_work, detect_work);
	INIT_DELAYED_WORK(&dd->unplug_irq_work, unplug_irq_handler_work);
	INIT_DELAYED_WORK(&dd->init_work, init_work);

	/* Deferred init/detect since no use for the info early in boot */
	queue_delayed_work(dd->irq_work_queue,
				&dd->init_work,
				msecs_to_jiffies(INIT_DELAY_MS));

	platform_set_drvdata(pdev, dd);

	return 0;

fail_no_mem_for_wq:
	free_regulators();
fail_no_regulators:
	input_unregister_device(dd->btn_input_dev);
fail_no_btn_input_dev:
	gpio_free(dd->pdata->video_ctrl_gpio);
fail_video_ctrl_gpio:
fail_no_ab8500_dev:
	kfree(dd);
fail_no_mem_for_devdata:

	return -ENOMEM;
}

/*
 * Performs platform device cleanup
 */
static void ab8500_accessory_cleanup(struct ab8500_ad *dd)
{
	dev_dbg(&dd->pdev->dev, "Enter: %s\n", __func__);

	dd->jack_type = JACK_TYPE_UNSPECIFIED;
	config_accdetect(dd);

	gpio_free(dd->pdata->video_ctrl_gpio);
	input_unregister_device(dd->btn_input_dev);
	free_regulators();

	cancel_delayed_work(&dd->detect_work);
	cancel_delayed_work(&dd->unplug_irq_work);
	cancel_delayed_work(&dd->init_work);
	flush_workqueue(dd->irq_work_queue);
	destroy_workqueue(dd->irq_work_queue);

	kfree(dd);
}

static int __devinit ab8500_acc_detect_probe(struct platform_device *pdev)
{
	return ab8500_accessory_init(pdev);
}


static int __devexit ab8500_acc_detect_remove(struct platform_device *pdev)
{
	ab8500_accessory_cleanup(platform_get_drvdata(pdev));
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#if defined(CONFIG_PM)
static u8 acc_det_ctrl_suspend_val;

static int ab8500_acc_detect_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct ab8500_ad *dd = platform_get_drvdata(pdev);
	int irq_id, irq;

	dev_dbg(&dd->pdev->dev, "%s: Enter\n", __func__);

	cancel_delayed_work_sync(&dd->unplug_irq_work);
	cancel_delayed_work_sync(&dd->detect_work);
	cancel_delayed_work_sync(&dd->init_work);

	if (dd->pdata->is_detection_inverted)
		irq_desc = irq_desc_inverted;
	else
		irq_desc = irq_desc_norm;

	for (irq_id = 0; irq_id < ARRAY_SIZE(irq_desc_norm); irq_id++) {
		if (irq_desc[irq_id].registered == 1) {
			irq = platform_get_irq_byname(
					dd->pdev,
					irq_desc[irq_id].name);

			disable_irq(irq);
		}
	}

	/* Turn off AccDetect comparators and pull-up */
	(void) abx500_get_register_interruptible(
			&dd->pdev->dev,
			AB8500_ECI_AV_ACC,
			AB8500_ACC_DET_CTRL_REG,
			&acc_det_ctrl_suspend_val);
	(void) abx500_set_register_interruptible(
			&dd->pdev->dev,
			AB8500_ECI_AV_ACC,
			AB8500_ACC_DET_CTRL_REG,
			0);
	return 0;
}

static int ab8500_acc_detect_resume(struct platform_device *pdev)
{
	struct ab8500_ad *dd = platform_get_drvdata(pdev);
	int irq_id, irq;

	dev_dbg(&dd->pdev->dev, "%s: Enter\n", __func__);

	/* Turn on AccDetect comparators and pull-up */
	(void) abx500_set_register_interruptible(
			&dd->pdev->dev,
			AB8500_ECI_AV_ACC,
			AB8500_ACC_DET_CTRL_REG,
			acc_det_ctrl_suspend_val);

	if (dd->pdata->is_detection_inverted)
		irq_desc = irq_desc_inverted;
	else
		irq_desc = irq_desc_norm;

	for (irq_id = 0; irq_id < ARRAY_SIZE(irq_desc_norm); irq_id++) {
		if (irq_desc[irq_id].registered == 1) {
			irq = platform_get_irq_byname(
					dd->pdev,
					irq_desc[irq_id].name);

			enable_irq(irq);

		}
	}

	/* After resume, reinitialize */
	dd->gpio35_dir_set = dd->accdet1_th_set = dd->accdet2_th_set = 0;
	queue_delayed_work(dd->irq_work_queue, &dd->init_work, 0);

	return 0;
}
#else
#define ab8500_acc_detect_suspend	NULL
#define ab8500_acc_detect_resume	NULL
#endif

static struct platform_driver ab8500_acc_detect_platform_driver = {
	.driver = {
		.name = "ab8500-acc-det",
		.owner = THIS_MODULE,
	},
	.probe = ab8500_acc_detect_probe,
	.remove	= __devexit_p(ab8500_acc_detect_remove),
	.suspend = ab8500_acc_detect_suspend,
	.resume = ab8500_acc_detect_resume,
};

static int __init ab8500_acc_detect_init(void)
{
	return platform_driver_register(&ab8500_acc_detect_platform_driver);
}

static void __exit ab8500_acc_detect_exit(void)
{
	platform_driver_unregister(&ab8500_acc_detect_platform_driver);
}

module_init(ab8500_acc_detect_init);
module_exit(ab8500_acc_detect_exit);

MODULE_DESCRIPTION("AB8500 AV Accessory detection driver");
MODULE_ALIAS("platform:ab8500-acc-det");
MODULE_AUTHOR("ST-Ericsson");
MODULE_LICENSE("GPL v2");

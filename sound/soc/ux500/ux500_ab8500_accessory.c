/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Jarmo K. Kuronen <jarmo.kuronen@symbio.com>
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/ab8500.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/ab8500/ab8500-gpadc.h>

/* Local definitions -----------------------------------------------*/
#define ARG_USED(x)  ((void)x)

/* How many times, in a row, same device type is to be evaluate in order
 to accept it. Also limits for configuration validation. */
#define REQUIRED_DET_MIN	2
#define REQUIRED_DET_MAX	10
#define REQUIRED_DET_DEF	4

/* Unique value used to identify Headset button input device */
#define BTN_INPUT_UNIQUE_VALUE	"AB8500HsBtn"
#define BTN_INPUT_DEV_NAME	"Headset button"

/* Timeout (ms) after jack type is checked after plug-in irq is received */
#define DEBOUNCE_PLUG_EVENT_MS		100
/* Timeout (ms) for subsequent plug checks used to make sure connected device
	is really detected properly */
#define DEBOUNCE_PLUG_RETEST_MS		50
/* Timeout after jack disconnect status is checked after plug-out det.*/
#define DEBOUNCE_UNPLUG_EVENT_MS	250

/*
* Register definition for accessory detection.
*/
#define AB8500_REGU_CTRL1_SPARE_REG	0x0384
#define AB8500_ECI_CTRL_REG		0x0800
#define AB8500_ECI_HOOKLEVEL_REG	0x0801
#define AB8500_ACC_DET_DB1_REG		0x0880
#define AB8500_ACC_DET_DB2_REG		0x0881
#define AB8500_ACC_DET_CTRL_REG		0x0882
#define AB8500_IT_SOURCE5_REG		0x0E04
#define AB8500_GPIO_PUD5_REG		0x1034
#define AB8500_GPIO_DIR5_REG		0x1014

/* REGISTER: AB8500_ACC_DET_CTRL_REG */
#define BIT_ACCDETCTRL_22_ENA			0x20
#define BIT_ACCDETCTRL_21_ENA			0x10
#define BIT_ACCDETCTRL_2PU_ENA			0x08
#define BIT_ACCDETCTRL_2PUS_ENA			0x02
#define BIT_ACCDETCTRL_1_ENA			0x01

/* REGISTER: AB8500_GPIO_DIR5_REG */
#define BIT_GPIO35_DIR_OUT		0x04

/* REGISTER: AB8500_REGU_CTRL1_SPARE_REG */
#define BIT_REGUCTRL1SPARE_VAMIC1_GROUND	0x01

/* REGISTER: AB8500_IT_SOURCE5_REG */
#define BIT_ITSOURCE5_ACCDET1			0x04

/* REGISTER: AB8500_ACC_DET_DB1_REG
*
* Accdetect1 debounce time limits, in milliseconds
*/
#define ACCDETECT1_DB_MIN	0
#define ACCDETECT1_DB_MAX	70
#define ACCDETECT1_DB_DEF	60
#define MASK_ACCDETECT1_DB	0x07

/* REGISTER: AB8500_ACC_DET_DB2_REG
* Accdetect1 threshold voltage limits, in millivolts */
#define ACCDETECT1_TH_MIN	300
#define ACCDETECT1_TH_MAX	1800
#define ACCDETECT1_TH_DEF	1800
#define MASK_ACCDETECT1_TH	0x78
/* Accdetect21 threshold voltage limits, in millivolts */
#define ACCDETECT21_TH_MIN	300
#define ACCDETECT21_TH_MAX	1800
#define ACCDETECT21_TH_DEF	1000
#define MASK_ACCDETECT21_TH	0x0F
/* Accdetect22 threshold voltage limits, in millivolts */
#define ACCDETECT22_TH_MIN	300
#define ACCDETECT22_TH_MAX	1800
#define ACCDETECT22_TH_DEF	1000
#define MASK_ACCDETECT22_TH	0xF0

/* After being loaded, how fast the first check is to be made */
#define INIT_DELAY_MS		5000

/* Name of the workqueue thread */
#define WORKQUEUE_NAME				"ab8500_av_wq"

/* Voltage limits (mV) for various types of AV Accessories */
#define ACCESSORY_DET_VOL_DONTCARE		-1
#define ACCESSORY_HEADPHONE_DET_VOL_MIN		0
#define ACCESSORY_HEADPHONE_DET_VOL_MAX		40
#define ACCESSORY_CVIDEO_DET_VOL_MIN		41
#define ACCESSORY_CVIDEO_DET_VOL_MAX		105
#define ACCESSORY_CARKIT_DET_VOL_MIN		1100
#define ACCESSORY_CARKIT_DET_VOL_MAX		1300
#define ACCESSORY_HEADSET_DET_VOL_MIN		0
#define ACCESSORY_HEADSET_DET_VOL_MAX		200
#define ACCESSORY_OPENCABLE_DET_VOL_MIN		1730
#define ACCESSORY_OPENCABLE_DET_VOL_MAX		2150

/* Macros ----------------------------------------------------------*/

/*
* Conviniency macros to check jack characteristics.
*/
#define jack_supports_mic(type) \
	(type == JACK_TYPE_HEADSET || type == JACK_TYPE_CARKIT)
#define jack_supports_spkr(type) \
	((type != JACK_DISCONNECTED) && (type != JACK_CONNECTED))
#define jack_supports_buttons(type) \
	((type == JACK_TYPE_HEADSET) ||\
	(type == JACK_TYPE_CARKIT) ||\
	(type == JACK_TYPE_OPENCABLE))

/* Enumerations -----------------------------------------------------*/

/* Possible states of a "standard" accessory button.. */
enum accessory_button_state {
	/* Uninitialized */
	ACCESSORY_BUTTON_UNSPECIFIED,
	/* Button is currently pressed down */
	ACCESSORY_BUTTON_PRESSED,
	/* Button is not currently pressed down */
	ACCESSORY_BUTTON_RELEASED
};

/* Two separate accessory detection inputs, one to detect
* plugin/plugout events and other to detect button events
* while plugged in
*/
enum accessory_detect_channel {
	ACCESSORY_DETECT_CHANNEL_1 = 1,
	ACCESSORY_DETECT_CHANNEL_2 = 2,
	ACCESSORY_DETECT_CHANNEL_ALL = 3
};

/* Regulators used in accessory detection */
enum accessory_regulator {
	ACCESSORY_REGULATOR_VAUDIO = 1,
	ACCESSORY_REGULATOR_VAMIC1 = 2,
	ACCESSORY_REGULATOR_ALL = 3
};

/* State of the jack and possible type */
enum accessory_jack_state {
	JACK_UNSPECIFIED,
	JACK_DISCONNECTED,
	JACK_CONNECTED,
	JACK_TYPE_HEADPHONE,
	JACK_TYPE_HEADSET,
	JACK_TYPE_CARKIT,
	JACK_TYPE_OPENCABLE,
	JACK_TYPE_CVIDEO,
	JACK_TYPE_ECI
};

/* Accessory detect operations enumerated */
enum accessory_op {
	ACCESSORY_TEST_DISCONNECTED,
	ACCESSORY_TEST_CONNECTED,
	ACCESSORY_TEST_HEADPHONE,
	ACCESSORY_TEST_ECI,
	ACCESSORY_TEST_CVIDEO,
	ACCESSORY_TEST_OPENCABLE,
	ACCESSORY_TEST_CARKIT,
	ACCESSORY_TEST_HEADSET
};

/*
* @E_PLUG_IRQ
* @E_UNPLUG_IRQ
* @E_BUTTON_PRESS_IRQ
* @E_BUTTON_RELEASE_IRQ
*/
enum accessory_irq {
	E_PLUG_IRQ,
	E_UNPLUG_IRQ,
	E_BUTTON_PRESS_IRQ,
	E_BUTTON_RELEASE_IRQ
};

/*
* @irq Interrupt enumeration
* @name Name of the interrupt as defined by the core driver.
* @handler Interrupt handler
* @registered flag indicating whether this particular interrupt
* is already registered or not.
*/
struct accessory_irq_descriptor {
	enum accessory_irq irq;
	const char *name;
	irq_handler_t handler;
	int registered;
};

/*
* Maps a detect operation to accessory state
* @operation
* @jack_state
* @meas_mv
* @minvol
* @maxvol
*/
struct accessory_op_to_jack_state {
	/* Operation to be performed */
	enum accessory_op		operation;
	/* If operation evals to true -> state is set to mentioned */
	enum accessory_jack_state	jack_state;
	/* Whether mic voltage should be remeasured during this step,
		if not set, the previously measured cached value is to be used
		when making the decision */
	int				meas_mv;
	/* Voltage limits to make the decision */
	int				minvol;
	int				maxvol;
};

/*
* Device data, capsulates all relevant device data structures.
*/
struct devdata {

	struct ab8500 *ab8500;

	struct platform_device *codec_p_dev;

	/* Codec device for register access etc. */
	struct device *codec_dev;

	struct snd_soc_jack jack;

	/* Worker thread for accessory detection purposes */
	struct workqueue_struct *irq_work_queue;

	/* Input device for button events */
	struct input_dev *btn_input_dev;

	/* Current plug status */
	enum accessory_jack_state jack_type;

	/* Indeed, we are checking the jack status x times in a row before
	   trusting the results.. due the bouncing that occurs during plugin */
	int  jack_det_count;
	enum accessory_jack_state jack_type_temp;

	/* Current state of the accessory button if any */
	enum accessory_button_state btn_state;

	/* threshold value for accdetect1 */
	u8				accdetect1th;
	/* debounce value for accdetect1 */
	u8				accdetect1db;
	/* threshold value for accdetect21 */
	u8				accdetect21th;
	/* threshold value for accdetect22 */
	u8				accdetect22th;
	/* How many detections requred in a row to accept */
	int				required_det;
	/* Vamic1 regulator */
	struct regulator		*vamic1_reg;
	/* Is vamic1 regulator currently held or not */
	int				vamic1_reg_enabled;
	/* VAudio regulator */
	struct regulator		*vaudio_reg;
	/* Is vaudio regulator currently held or not */
	int				vaudio_reg_enabled;
};

/* Forward declarations -------------------------------------------------*/

static irqreturn_t unplug_irq_handler(int irq, void *_userdata);
static irqreturn_t plug_irq_handler(int irq, void *_userdata);

static void config_accdetect(enum accessory_jack_state state);
static void release_irq(enum accessory_irq irq_id);
static void claim_irq(enum accessory_irq irq_id);

static void unplug_irq_handler_work(struct work_struct *work);
static void plug_irq_handler_work(struct work_struct *work);
static void deferred_init_handler_work(struct work_struct *work);
static enum accessory_jack_state detect(void);
static u8 ab8500_reg_read(u8 bank, u32 reg);

/* Local variables ----------------------------------------------------------*/
DECLARE_DELAYED_WORK(plug_irq_work, plug_irq_handler_work);
DECLARE_DELAYED_WORK(unplug_irq_work, unplug_irq_handler_work);
DECLARE_DELAYED_WORK(deferred_init_work, deferred_init_handler_work);

/* Device data - dynamically allocated during the init. */
static struct devdata *devdata;

/* Note, order of these detections actually count -> changing
the order might actually cause inproper detection results.
*/
static struct accessory_op_to_jack_state detect_ops[] = {
	/* Check is the PLUG connected */
	{
		ACCESSORY_TEST_DISCONNECTED,
		JACK_DISCONNECTED,
		1,
		ACCESSORY_DET_VOL_DONTCARE,
		ACCESSORY_DET_VOL_DONTCARE
	},
	/* Check is the type HEADPHONE ( no mic ) */
	{
		ACCESSORY_TEST_HEADPHONE,
		JACK_TYPE_HEADPHONE,
		1,
		ACCESSORY_HEADPHONE_DET_VOL_MIN,
		ACCESSORY_HEADPHONE_DET_VOL_MAX
	},
	/* Check with ECI communication whether device is present or not */
	{
		ACCESSORY_TEST_ECI,
		JACK_TYPE_ECI,
		0,
		ACCESSORY_DET_VOL_DONTCARE,
		ACCESSORY_DET_VOL_DONTCARE
	},
	/* Check is the VIDEOCABLE connected */
	{
		ACCESSORY_TEST_CVIDEO,
		JACK_TYPE_CVIDEO,
		0,
		ACCESSORY_CVIDEO_DET_VOL_MIN,
		ACCESSORY_CVIDEO_DET_VOL_MAX
	},
	/* Check is the OPEN CABLE CONNECTED */
	{
		ACCESSORY_TEST_OPENCABLE,
		JACK_TYPE_OPENCABLE,
		0,
		ACCESSORY_OPENCABLE_DET_VOL_MIN,
		ACCESSORY_OPENCABLE_DET_VOL_MAX
	},
	/* Check is the CARKIT connected */
	{
		ACCESSORY_TEST_CARKIT,
		JACK_TYPE_CARKIT,
		1,
		ACCESSORY_CARKIT_DET_VOL_MIN,
		ACCESSORY_CARKIT_DET_VOL_MAX
	},
	/* Check is HEADSET connected */
	{
		ACCESSORY_TEST_HEADSET,
		JACK_TYPE_HEADSET,
		0,
		ACCESSORY_HEADSET_DET_VOL_MIN,
		ACCESSORY_HEADSET_DET_VOL_MAX
	},
	/* Last but not least, check is some unsupported device connected */
	{
		ACCESSORY_TEST_CONNECTED,
		JACK_CONNECTED,
		0,
		ACCESSORY_DET_VOL_DONTCARE,
		ACCESSORY_DET_VOL_DONTCARE
	}
};

#ifdef DEBUG
#define dump_reg(txt_pre, txt_post, bank, reg)\
{\
	u8 val = ab8500_reg_read(bank, reg);\
	printk(KERN_INFO "  R(%s%s) = %02x\n", txt_pre, txt_post, val);\
}

void dump_regs(const char *txt)
{
#if 0
	dump_reg("383", " ACCESSORY", 3, 0x383);
	dump_reg("880", " ACCESSORY", 8, 0x880);
	dump_reg("881", " ACCESSORY", 8, 0x881);
	dump_reg("882", " ACCESSORY", 8, 0x882);
	dump_reg("e44", " ACCESSORY", 14, 0xE44);
#endif
}
#endif

static const char *accessory_str(enum accessory_jack_state status)
{
	const char *ret;

	switch (status) {
	case JACK_DISCONNECTED:
		ret = "DISCONNECTED";
		break;
	case JACK_CONNECTED:
		ret = "CONNECTED";
		break;
	case JACK_TYPE_HEADPHONE:
		ret = "HEADPHONE";
		break;
	case JACK_TYPE_HEADSET:
		ret = "HEADSET";
		break;
	case JACK_TYPE_CARKIT:
		ret = "CARKIT";
		break;
	case JACK_TYPE_OPENCABLE:
		ret = "OPENCABLE";
		break;
	case JACK_TYPE_CVIDEO:
		ret = "CVIDEO";
		break;
	case JACK_TYPE_ECI:
		ret = "ECI";
		break;
	default:
		ret = "ERROR";
	}

	return ret;
}

/*
* Enables specific accessory detection regulator intelligently so
* that reference counts are taken into account.
*/
static void accessory_regulator_enable(enum accessory_regulator reg)
{
	if (reg & ACCESSORY_REGULATOR_VAUDIO) {
		if (!devdata->vaudio_reg_enabled) {
			if (!regulator_enable(devdata->vaudio_reg))
				devdata->vaudio_reg_enabled = 1;
		}
	}

	if (reg & ACCESSORY_REGULATOR_VAMIC1) {
		if (!devdata->vamic1_reg_enabled) {
			if (!regulator_enable(devdata->vamic1_reg))
				devdata->vamic1_reg_enabled = 1;
		}
	}
}

/*
* Disables specific accessory detection related regulator intelligently so
* that reference counts are taken into account.
*/
static void accessory_regulator_disable(enum accessory_regulator reg)
{
	if (reg & ACCESSORY_REGULATOR_VAUDIO) {
		if (devdata->vaudio_reg_enabled) {
			if (!regulator_disable(devdata->vaudio_reg))
				devdata->vaudio_reg_enabled = 0;
		}
	}

	if (reg & ACCESSORY_REGULATOR_VAMIC1) {
		if (devdata->vamic1_reg_enabled) {
			if (!regulator_disable(devdata->vamic1_reg))
				devdata->vamic1_reg_enabled = 0;
		}
	}
}
static u8 ab8500_reg_read(u8 bank, u32 adr);

static int ab8500_reg_write(u8 bank, u32 adr, u8 data)
{
	int status = abx500_set_register_interruptible(
				devdata->codec_dev,
				bank,
				adr & 0xFF,
				data);

	if (status < 0)
		pr_err("%s: %x failed: %d\n", __func__, adr, status);

	return status;
}

/* Generic AB8500 register reading
 */
static u8 ab8500_reg_read(u8 bank, u32 adr)
{
	u8 value = 0;
	int status = abx500_get_register_interruptible(
				devdata->codec_dev,
				bank,
				adr & 0xFF,
				&value);
	if (status < 0)
		pr_err("%s: %x failed: %d\n", __func__, adr, status);

	return value;
}

/*
* ab8500_set_bits - reads value, applies bits and writes back
*/
static void ab8500_set_bits(u8 bank, u32 reg, u8 bits)
{
	u8 value = ab8500_reg_read(bank, reg);

	/* Check do we actually need to set any bits */
	if ((value & bits) == bits)
		return;
	value |= bits;
	ab8500_reg_write(bank, reg, value);
}

/*
* Clears the certain bits ( mask given ) from given register in given bank
*/
static void ab8500_clr_bits(u8 bank, u32 reg, u8 bits)
{
	u8 value = ab8500_reg_read(bank, reg);

	/* Check do we actually need to clear any bits */
	if ((value & bits) == 0)
		return;
	value &= ~bits;
	ab8500_reg_write(bank, reg, value);
}

/*
* Configures HW so that accessory detection input 2 is effective
*/
static void config_accdetect2_hw(int enable)
{
	if (enable) {
		ab8500_reg_write(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_DB2_REG,
				devdata->accdetect21th |
				devdata->accdetect22th);

		ab8500_set_bits(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_CTRL_REG,
				BIT_ACCDETCTRL_21_ENA  |
				BIT_ACCDETCTRL_2PU_ENA);

		accessory_regulator_enable(ACCESSORY_REGULATOR_ALL);

		ab8500_set_bits(AB8500_ECI_AV_ACC,
				AB8500_ECI_CTRL_REG,
				0x3a);
		/* @TODO: Check clearing this later on. */
		ab8500_reg_write(AB8500_ECI_AV_ACC,
				AB8500_ECI_HOOKLEVEL_REG,
				0x1f);
	} else {
		ab8500_reg_write(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_DB2_REG,
				devdata->accdetect21th |
				devdata->accdetect22th);

		ab8500_clr_bits(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_CTRL_REG,
				BIT_ACCDETCTRL_21_ENA |
				BIT_ACCDETCTRL_2PU_ENA);

		accessory_regulator_disable(ACCESSORY_REGULATOR_ALL);

		/* Set ECI bits accordingly.. NOTE: reg 800+801 are UNDOC. */
		ab8500_clr_bits(AB8500_ECI_AV_ACC,
				AB8500_ECI_CTRL_REG,
				0x3a);
	}
}

/*
* config_accdetect1 - configures accessory detection 1 hardware in order
* to be able to recognized plug-in/plug-out events.
*/
void config_accdetect1_hw(int enable)
{
	if (enable) {
		ab8500_reg_write(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_DB1_REG,
				devdata->accdetect1th | devdata->accdetect1db);

		/* enable accdetect1 comparator */
		ab8500_set_bits(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_CTRL_REG,
				BIT_ACCDETCTRL_1_ENA |
				BIT_ACCDETCTRL_2PUS_ENA);
	} else {
		/* Disable accdetect1 comparator */
		ab8500_clr_bits(AB8500_ECI_AV_ACC,
				AB8500_ACC_DET_CTRL_REG,
				BIT_ACCDETCTRL_1_ENA |
				BIT_ACCDETCTRL_2PUS_ENA);
	}
}

/*
* Configures hardware so that it is possible to receive
* "standard" button presses from headset/carkit accessories.
*/
static void start_wait_button_events(void)
{
	int stat;

	if (devdata && !devdata->btn_input_dev) {
		devdata->btn_input_dev = input_allocate_device();
		if (!devdata->btn_input_dev) {
			pr_err("%s: Failed to allocate input.\n", __func__);
			goto out;
		}
		input_set_capability(devdata->btn_input_dev,
					EV_KEY,
					KEY_PHONE);

		devdata->btn_input_dev->name = BTN_INPUT_DEV_NAME;
		devdata->btn_input_dev->uniq = BTN_INPUT_UNIQUE_VALUE;

		stat = input_register_device(devdata->btn_input_dev);
		if (stat) {
			pr_err("%s: register_input_device: %d\n",
				__func__,
				stat);
			input_free_device(devdata->btn_input_dev);
			devdata->btn_input_dev = NULL;
		}
	}
out:
	return;
}

/*
* stop_wait_button_events - stops waiting for button events.
*/
static void stop_wait_button_events(void)
{
	if (devdata && devdata->btn_input_dev) {
		input_unregister_device(devdata->btn_input_dev);
		devdata->btn_input_dev = NULL;
	}
}

int init_jackdev(struct snd_soc_codec *codec)
{
	int status;
	pr_info("%s: Enter\n", __func__);

	status = snd_soc_jack_new(codec,
				"Headset status",
				SND_JACK_HEADPHONE	|
				SND_JACK_MICROPHONE	|
				SND_JACK_HEADSET	|
				SND_JACK_LINEOUT	|
				SND_JACK_MECHANICAL	|
				SND_JACK_VIDEOOUT,
				&devdata->jack);

	pr_info("%s: snd_soc_jack_new = %d\n", __func__, status);

	return status;
}

void update_jack_status(enum accessory_jack_state jack_status)
{
	/* We can use "full" mask as we maintain the jack state by ourselves */
	const int mask = 0xFF;
	int value = 0;

	if (!devdata || !devdata->jack.jack)
		return;

	if (jack_status != JACK_DISCONNECTED && jack_status != JACK_UNSPECIFIED)
		value |= SND_JACK_MECHANICAL;
	if (jack_supports_mic(jack_status))
		value |= SND_JACK_MICROPHONE;
	if (jack_supports_spkr(jack_status))
		value |= (SND_JACK_HEADPHONE | SND_JACK_LINEOUT);
	if (jack_status == JACK_TYPE_CVIDEO)
		value |= SND_JACK_VIDEOOUT;

	snd_soc_jack_report(&devdata->jack, value, mask);
}

/*
* Returns the mic line voltage in millivolts.
*/
static int meas_voltage(u8 input)
{
	int raw_vol = ab8500_gpadc_convert(devdata->ab8500->gpadc, input);

	return (raw_vol * 2500)/1023;
}

/*
* Returns 1 if accessories are plugged in, 0 if not.
*/
static int detect_plugged_in(void)
{
	u8 val = ab8500_reg_read(AB8500_INTERRUPT, AB8500_IT_SOURCE5_REG);

	if (val & BIT_ITSOURCE5_ACCDET1)
		return 0;
	return 1;
}

/*
*  mic_line_voltage_stable - measures a relative stable voltage (dV/dT)
*  from mic1 input.
*/
static int meas_voltage_stable(u8 input)
{
	int iterations = 3;
	const int sleepms = 20;
	const int dmax = 30;
	int v1, v2, dv;

	v1 = meas_voltage(input);
	do {
		msleep(sleepms);
		--iterations;
		v2 = meas_voltage(input);
		dv = abs(v2 - v1);
		v1 = v2;
	} while (iterations > 0 && dv > dmax);

	return v1;
}

/*
* unplug_irq_handler_work - worked routine for unplug interrupt.
* run in context of the worker thread.
*/
static void unplug_irq_handler_work(struct work_struct *work)
{
	enum accessory_jack_state type;

	pr_info("%s: Enter\n", __func__);

	type = detect();

	if (type == JACK_DISCONNECTED) {
		/* If disconnected > do reset the state accordingly */
		config_accdetect(type);
		devdata->btn_state = ACCESSORY_BUTTON_UNSPECIFIED;
		stop_wait_button_events();
	}

	devdata->jack_type = type;
	devdata->jack_det_count = 0;

	/* Tell the userland what has just happened */
	update_jack_status(devdata->jack_type);

	pr_info("Accessory: %s\n", accessory_str(devdata->jack_type));
}

/*
* unplug_irq_handler - interrupt handler for unplug event.
*/
static irqreturn_t unplug_irq_handler(int irq, void *_userdata)
{
	pr_info("%s: Enter\n", __func__);

	ARG_USED(irq);
	ARG_USED(_userdata);
	queue_delayed_work(devdata->irq_work_queue,
			&unplug_irq_work,
			msecs_to_jiffies(DEBOUNCE_UNPLUG_EVENT_MS));

	return IRQ_HANDLED;
}

/*
* plug_irq_handler - Interrupt handler routing for plugging in the cable.
*/
static irqreturn_t plug_irq_handler(int irq, void *_userdata)
{
	pr_info("%s: Enter\n", __func__);

	ARG_USED(irq);
	ARG_USED(_userdata);
	queue_delayed_work(devdata->irq_work_queue,
				&plug_irq_work,
				msecs_to_jiffies(DEBOUNCE_PLUG_EVENT_MS));

	return IRQ_HANDLED;
}

/*
* plug_irq_handler_work - work processing for plug irq. Run in context
* of the worker thread.
*/
static void plug_irq_handler_work(struct work_struct *work)
{
	enum accessory_jack_state type;

	pr_info("%s: Enter\n", __func__);

	type = detect();

	if (devdata->jack_type_temp == type) {
		devdata->jack_det_count++;
	} else {
		devdata->jack_det_count = 1;
		devdata->jack_type_temp = type;
	}

	if (devdata->jack_det_count < devdata->required_det) {
		/* Perform the detection again until we are certain.. */
		/* @TODO: Should we have some kind of "total" limit in order
			not to go into loop in case different type is
			detected all the time. Not very likely though.*/
		queue_delayed_work(devdata->irq_work_queue,
				&plug_irq_work,
				msecs_to_jiffies(DEBOUNCE_PLUG_RETEST_MS));
	} else {
		pr_info("Accessory: %s\n", accessory_str(type));

		if (jack_supports_buttons(type) && type != JACK_TYPE_OPENCABLE)
			start_wait_button_events();

		/* Report status only if we were disconnected previously, also
			if opencable is detected, it is not reported sep.
			just yet */
		if (devdata->jack_type == JACK_DISCONNECTED) {
			devdata->jack_type = type;
			if (devdata->jack_type != JACK_TYPE_OPENCABLE)
				update_jack_status(devdata->jack_type);
		}
		config_accdetect(devdata->jack_type);
	}
}

static void report_btn_event(int pressed)
{
	if (devdata->btn_input_dev) {
		input_report_key(devdata->btn_input_dev, KEY_PHONE, pressed);
		input_sync(devdata->btn_input_dev);
		pr_info("HS-BUTTON: %s\n", pressed ? "PRESSED" : "RELEASED");
	}
}

/*
* button_press_irq_handler - irq handler when headset button press is detected.
*/
static irqreturn_t button_press_irq_handler(int irq, void *_userdata)
{
	pr_info("%s: Enter\n", __func__);
	ARG_USED(_userdata);

	if (devdata->jack_type == JACK_TYPE_OPENCABLE) {
		/* Simulate the scenario where plug would have
		   been connected... */
		plug_irq_handler(irq, _userdata);
		return IRQ_HANDLED;
	} else if (devdata->btn_state == ACCESSORY_BUTTON_RELEASED &&
		jack_supports_buttons(devdata->jack_type)) {
		report_btn_event(1);
	}

	/* Update button state */
	if (devdata->jack_type == JACK_DISCONNECTED)
		devdata->btn_state = ACCESSORY_BUTTON_UNSPECIFIED;
	else
		devdata->btn_state = ACCESSORY_BUTTON_PRESSED;

	return IRQ_HANDLED;
}

static irqreturn_t button_release_irq_handler(int irq, void *_userdata)
{
	pr_info("%s: Enter\n", __func__);

	ARG_USED(_userdata);
	ARG_USED(irq);

	/* Handle button presses only of headset and/or carkit */
	if (devdata->btn_state == ACCESSORY_BUTTON_PRESSED &&
		(devdata->jack_type == JACK_TYPE_HEADSET ||
		 devdata->jack_type == JACK_TYPE_CARKIT)) {
		report_btn_event(0);
	}
	if (devdata->jack_type == JACK_DISCONNECTED)
		devdata->btn_state = ACCESSORY_BUTTON_UNSPECIFIED;
	else
		devdata->btn_state = ACCESSORY_BUTTON_RELEASED;

	return IRQ_HANDLED;
}

/*
* config_hw_plug_connected - configures the hardware after plug
* insertion has been detected. That is necessary in order to be
* able to detect what type of plug-has been inserted.
*/
static void config_hw_test_plug_connected(int enable)
{
	if (enable) {
		ab8500_set_bits(AB8500_MISC, AB8500_GPIO_PUD5_REG, 0x04);
		accessory_regulator_enable(ACCESSORY_REGULATOR_VAMIC1);
	} else {
		ab8500_clr_bits(AB8500_MISC, AB8500_GPIO_PUD5_REG, 0x04);
		accessory_regulator_disable(ACCESSORY_REGULATOR_VAMIC1);
	}
}

/*
* config_hw_basic_carkit - configures AB8500 so that carkit detection can
* be made
*/
static void config_hw_test_basic_carkit(int enable)
{
	if (enable) {
		/* Disable mic bias that is mandatory for further detections*/
		accessory_regulator_disable(ACCESSORY_REGULATOR_VAMIC1);

		/* Ground the VAMic1 output when disabled ->
			zero input provided */
		ab8500_set_bits(AB8500_REGU_CTRL1,
				AB8500_REGU_CTRL1_SPARE_REG,
				BIT_REGUCTRL1SPARE_VAMIC1_GROUND);
	} else {
		/* NOTE: Here we do not re-enable the VAMic1 Regulator. */

		/* Un-Ground the VAMic1 output when enabled */
		ab8500_clr_bits(AB8500_REGU_CTRL1,
				AB8500_REGU_CTRL1_SPARE_REG,
				BIT_REGUCTRL1SPARE_VAMIC1_GROUND);
	}
}

/*
* mic_vol_in_range - measures somewhat stable mic1 voltage and returns it.
* Uses cached value if not explicitly requested not to do so ( force_read ).
*/
static int mic_vol_in_range(int lo, int hi, int force_read)
{
	static int mv = -100;

	if (mv == -100 || force_read)
		mv = meas_voltage_stable(ACC_DETECT2);

#ifdef DEBUG_VOLTAGE
	pr_info("mic: %dmV (l=%dmV h=%dmV)\n", mv, lo, hi);
#endif
	return (mv >= lo && mv <= hi) ? 1 : 0;
}

/*
* detect_hw - tries to detect specific type of connected hardware
*/
static int detect_hw(struct accessory_op_to_jack_state *op)
{
	int status;

	switch (op->operation) {
	case ACCESSORY_TEST_DISCONNECTED:
		config_hw_test_plug_connected(1);
		status = !detect_plugged_in();
		break;
	case ACCESSORY_TEST_CONNECTED:
		config_hw_test_plug_connected(1);
		status = detect_plugged_in();
		break;
	case ACCESSORY_TEST_ECI:
		/* @TODO: Integrate ECI support here */
		status = 0;
		break;
	case ACCESSORY_TEST_HEADPHONE:
	case ACCESSORY_TEST_CVIDEO:
	case ACCESSORY_TEST_CARKIT:
	case ACCESSORY_TEST_HEADSET:
	case ACCESSORY_TEST_OPENCABLE:
		/* Only do the config when testing carkit, does not harm for
		others but is unncessary anyway. */
		if (op->operation == ACCESSORY_TEST_CARKIT)
			config_hw_test_basic_carkit(1);
		status = mic_vol_in_range(op->minvol, op->maxvol, op->meas_mv);
		break;
	default:
		status = 0;
	}

	return status;
}

/*
* detect - detects the type of the currently connected accessory if any.
*/
static enum accessory_jack_state detect()
{
	enum accessory_jack_state status = JACK_DISCONNECTED;
	int i;

	for (i = 0; i < ARRAY_SIZE(detect_ops); ++i) {
		if (detect_hw(&detect_ops[i])) {
			status = detect_ops[i].jack_state;
			break;
		}
	}

	config_hw_test_basic_carkit(0);
	config_hw_test_plug_connected(0);

	return status;
}

static struct accessory_irq_descriptor irq_descriptors[] = {
	{E_PLUG_IRQ,		"ACC_DETECT_1DB_F",
			plug_irq_handler,	0},
	{E_UNPLUG_IRQ,		"ACC_DETECT_1DB_R",
			unplug_irq_handler,	0},
	{E_BUTTON_PRESS_IRQ,	"ACC_DETECT_22DB_F",
			button_press_irq_handler, 0},
	{E_BUTTON_RELEASE_IRQ,	"ACC_DETECT_22DB_R",
			button_release_irq_handler, 0},
};

/*
* Claims access to specific IRQ.
*/
static void claim_irq(enum accessory_irq irq_id)
{
	int ret;
	int irq;

	if (irq_descriptors[irq_id].registered)
		return;

	irq = platform_get_irq_byname(
			devdata->codec_p_dev,
			irq_descriptors[irq_id].name);
	if (irq < 0) {
		pr_err("%s: failed to get irq nbr %s\n", __func__,
			irq_descriptors[irq_id].name);
		return;
	}

	ret = request_threaded_irq(irq,
				NULL,
				irq_descriptors[irq_id].handler,
				0,
				irq_descriptors[irq_id].name,
				devdata);
	if (ret != 0) {
		pr_err("%s: Failed to claim irq %s (%d)\n",
				__func__,
				irq_descriptors[irq_id].name,
				ret);
	} else {
		irq_descriptors[irq_id].registered = 1;
		pr_info("%s: EnableIRQ %s\n",
			__func__, irq_descriptors[irq_id].name);
	}
}

/*
* Reclaims access to specific IRQ.
*/
static void release_irq(enum accessory_irq irq_id)
{
	int irq;

	if (!irq_descriptors[irq_id].registered)
		return;

	irq = platform_get_irq_byname(
			devdata->codec_p_dev,
			irq_descriptors[irq_id].name);
	if (irq < 0) {
		pr_err("%s: failed to get irq %s\n", __func__,
			irq_descriptors[irq_id].name);
	} else {
		free_irq(irq, devdata);
		irq_descriptors[irq_id].registered = 0;
		pr_info("%s: DisableIRQ %s\n",
			__func__, irq_descriptors[irq_id].name);
	}
}

static void config_accdetect_hw(enum accessory_jack_state state)
{
	switch (state) {
	case JACK_UNSPECIFIED:
		config_accdetect1_hw(1);
		config_accdetect2_hw(0);
		break;

	case JACK_DISCONNECTED:
	case JACK_CONNECTED:
	case JACK_TYPE_HEADPHONE:
	case JACK_TYPE_CVIDEO:
	case JACK_TYPE_ECI:
		/* Plug In/Out detection possible */
		config_accdetect1_hw(1);
		config_accdetect2_hw(0);
		break;

	case JACK_TYPE_HEADSET:
	case JACK_TYPE_CARKIT:
	case JACK_TYPE_OPENCABLE:
		/* Plug Out + Button Press/Release possible */
		config_accdetect1_hw(1);
		config_accdetect2_hw(1);
		break;

	default:
		pr_err("%s: Unspecified state: %d\n", __func__, state);
	}
}

static void config_accdetect_irq(enum accessory_jack_state state)
{
	switch (state) {
	case JACK_UNSPECIFIED:
		release_irq(E_PLUG_IRQ);
		release_irq(E_UNPLUG_IRQ);
		release_irq(E_BUTTON_PRESS_IRQ);
		release_irq(E_BUTTON_RELEASE_IRQ);
		break;

	case JACK_DISCONNECTED:
		release_irq(E_BUTTON_PRESS_IRQ);
		release_irq(E_BUTTON_RELEASE_IRQ);
		claim_irq(E_UNPLUG_IRQ);
		claim_irq(E_PLUG_IRQ);
		break;

	case JACK_CONNECTED:
	case JACK_TYPE_HEADPHONE:
	case JACK_TYPE_CVIDEO:
	case JACK_TYPE_ECI:
		release_irq(E_BUTTON_PRESS_IRQ);
		release_irq(E_BUTTON_RELEASE_IRQ);
		claim_irq(E_PLUG_IRQ);
		claim_irq(E_UNPLUG_IRQ);
		break;

	case JACK_TYPE_HEADSET:
	case JACK_TYPE_CARKIT:
	case JACK_TYPE_OPENCABLE:
		release_irq(E_PLUG_IRQ);
		claim_irq(E_UNPLUG_IRQ);
		claim_irq(E_BUTTON_PRESS_IRQ);
		claim_irq(E_BUTTON_RELEASE_IRQ);
		break;

	default:
		pr_err("%s: Unspecified state: %d\n", __func__, state);
	}
}

static void config_accdetect(enum accessory_jack_state state)
{
	config_accdetect_hw(state);
	config_accdetect_irq(state);
}

static int ab8500_accessory_configure(int accdetect1db,
					int accdetect1th,
					int accdetect21th,
					int accdetect22th,
					int detections)
{
	if (!devdata)
		return -1;

	if (accdetect1db >= ACCDETECT1_DB_MIN &&
		accdetect1db <= ACCDETECT1_DB_MAX) {
		devdata->accdetect1db =
			(accdetect1db / 10) & MASK_ACCDETECT1_DB;
		pr_info("%s: AccDetect1DB: %dmV > %02x\n", __func__,
			accdetect1db, devdata->accdetect1db);
	}
	if (accdetect1th >= ACCDETECT1_TH_MIN &&
		accdetect1th <= ACCDETECT1_TH_MAX) {
		devdata->accdetect1th =
			((accdetect1th / 100 - 3) << 3) & MASK_ACCDETECT1_TH;
		pr_info("%s: AccDetect1TH: %dmV > %02x\n", __func__,
			accdetect1th, devdata->accdetect1th);
	}
	if (accdetect21th >= ACCDETECT21_TH_MIN &&
		accdetect21th <= ACCDETECT21_TH_MAX) {
		devdata->accdetect21th =
			(accdetect21th / 100 - 3) & MASK_ACCDETECT21_TH;
		pr_info("%s: AccDetect21TH: %dmV > %02x\n", __func__,
			accdetect21th, devdata->accdetect21th);
	}
	if (accdetect22th >= ACCDETECT22_TH_MIN &&
		accdetect22th <= ACCDETECT22_TH_MAX) {
		devdata->accdetect22th =
			((accdetect22th / 100 - 3) << 4) & MASK_ACCDETECT22_TH;
		pr_info("%s: AccDetect22TH: %dmV > %02x\n", __func__,
			accdetect22th, devdata->accdetect22th);
	}
	if (detections >= REQUIRED_DET_MIN && detections <= REQUIRED_DET_MAX) {
		devdata->required_det = detections;
		pr_info("%s: detections_required: %d\n", __func__, detections);
	}

	return 0;
}

/*
* Deferred initialization of the work.
*/
static void deferred_init_handler_work(struct work_struct *work)
{
	pr_info("%s: Enter\n", __func__);

	ab8500_set_bits(AB8500_MISC, AB8500_GPIO_DIR5_REG, BIT_GPIO35_DIR_OUT);
	config_accdetect(JACK_UNSPECIFIED);
	plug_irq_handler_work(work);
}

static int init_platform_devices(struct snd_soc_codec *codec)
{
	devdata->ab8500 = dev_get_drvdata(codec->dev->parent);
	if (!devdata->ab8500 || !devdata->ab8500->gpadc) {
		pr_err("%s: failed to get ab8500 plat. driver\n", __func__);
		return -ENOENT;
	}
	devdata->codec_dev = codec->dev;
	devdata->codec_p_dev = to_platform_device(codec->dev);

	return 0;
}

int ab8500_accessory_init(struct snd_soc_codec *codec)
{
	pr_info("Enter: %s\n", __func__);

	if (devdata)
		return 0;

	if (!codec)
		goto fail_invalid_args;

	devdata = kzalloc(sizeof(struct devdata), GFP_KERNEL);
	if (!devdata) {
		pr_err("%s: Memory allocation failed\n", __func__);
		goto fail_no_mem_for_devdata;
	}

	/* Get vamic1 regulator */
	devdata->vamic1_reg = regulator_get(NULL, "v-amic1");
	if (IS_ERR(devdata->vamic1_reg)) {
		pr_err("%s: Failed to allocate regulator vamic1\n", __func__);
		devdata->vamic1_reg = NULL;
		goto fail_no_vamic1_regulator;
	}

	/* Get vaudio regulator */
	devdata->vaudio_reg = regulator_get(NULL, "v-audio");
	if (IS_ERR(devdata->vaudio_reg)) {
		pr_err("%s: Failed to allocate regulator vaudio\n", __func__);
		devdata->vaudio_reg = NULL;
		goto fail_no_vaudio_regulator;
	}

	/* Configure some default values that will always work somehow */
	ab8500_accessory_configure(
			ACCDETECT1_DB_DEF,
			ACCDETECT1_TH_DEF,
			ACCDETECT21_TH_DEF,
			ACCDETECT22_TH_DEF,
			REQUIRED_DET_DEF);

	if (init_jackdev(codec) < 0) {
		pr_err("%s: Failed to init jack input device\n", __func__);
		goto fail_no_jackdev;
	}

	if (init_platform_devices(codec) < 0) {
		pr_err("%s: platform dev failed\n", __func__);
		goto fail_no_platform;
	}

	devdata->btn_state = ACCESSORY_BUTTON_UNSPECIFIED;
	devdata->jack_type_temp = devdata->jack_type = JACK_DISCONNECTED;
	devdata->irq_work_queue = create_singlethread_workqueue(WORKQUEUE_NAME);
	if (!devdata->irq_work_queue) {
		pr_err("%s: Failed to create work queue\n", __func__);
		goto fail_no_mem_for_wq;
	}

	/* Perform deferred initialization. */
	queue_delayed_work(devdata->irq_work_queue,
				&deferred_init_work,
				msecs_to_jiffies(INIT_DELAY_MS));

	return 0;

fail_no_mem_for_wq:
fail_no_platform:
fail_no_jackdev:
	regulator_put(devdata->vaudio_reg);
fail_no_vaudio_regulator:
	regulator_put(devdata->vamic1_reg);
fail_no_vamic1_regulator:
	kfree(devdata);
	devdata = NULL;
fail_no_mem_for_devdata:
fail_invalid_args:
	return -ENOMEM;
}

void ab8500_accessory_cleanup(void)
{
	pr_info("Enter: %s\n", __func__);

	if (!devdata)
		return;

	config_accdetect(JACK_UNSPECIFIED);

	stop_wait_button_events();

	cancel_delayed_work(&plug_irq_work);
	cancel_delayed_work(&unplug_irq_work);
	cancel_delayed_work(&deferred_init_work);

	if (devdata->vamic1_reg)
		regulator_put(devdata->vamic1_reg);
	if (devdata->vaudio_reg)
		regulator_put(devdata->vaudio_reg);
	if (devdata->irq_work_queue) {
		flush_workqueue(devdata->irq_work_queue);
		destroy_workqueue(devdata->irq_work_queue);
	}

	kfree(devdata);
	devdata = NULL;
}

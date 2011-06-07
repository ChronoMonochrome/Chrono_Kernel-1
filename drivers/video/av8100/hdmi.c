/*
 * Copyright (C) ST-Ericsson AB 2010
 *
 * ST-Ericsson HDMI driver
 *
 * Author: Per Persson <per.xb.persson@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <video/av8100.h>
#include <video/hdmi.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include "hdmi_loc.h"
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>

#define SYSFS_EVENT_FILENAME "evread"

DEFINE_MUTEX(hdmi_events_mutex);
#define LOCK_HDMI_EVENTS mutex_lock(&hdmi_events_mutex)
#define UNLOCK_HDMI_EVENTS mutex_unlock(&hdmi_events_mutex)
#define EVENTS_MASK 0xFF

static int device_open;
static int events;
static int events_mask;
static bool events_received;
static wait_queue_head_t hdmi_event_wq;
struct device *hdmidev;

static ssize_t store_storeastext(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t store_plugdeten(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t store_edidread(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_edidread(struct device *dev, struct device_attribute *attr,
				char *buf);
static ssize_t store_ceceven(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_cecread(struct device *dev, struct device_attribute *attr,
				char *buf);
static ssize_t store_cecsend(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t store_infofrsend(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t store_hdcpeven(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_hdcpchkaesotp(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t store_hdcpfuseaes(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_hdcpfuseaes(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t store_hdcploadaes(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_hdcploadaes(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t store_hdcpauthencr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_hdcpauthencr(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t show_hdcpstateget(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t show_evread(struct device *dev, struct device_attribute *attr,
				char *buf);
static ssize_t store_evclr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t store_audiocfg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_plugstatus(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t store_poweronoff(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_poweronoff(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t store_evwakeup(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(storeastext, S_IWUSR, NULL, store_storeastext);
static DEVICE_ATTR(plugdeten, S_IWUSR, NULL, store_plugdeten);
static DEVICE_ATTR(edidread, S_IRUGO | S_IWUSR, show_edidread, store_edidread);
static DEVICE_ATTR(ceceven, S_IWUSR, NULL, store_ceceven);
static DEVICE_ATTR(cecread, S_IRUGO, show_cecread, NULL);
static DEVICE_ATTR(cecsend, S_IWUSR, NULL, store_cecsend);
static DEVICE_ATTR(infofrsend, S_IWUSR, NULL, store_infofrsend);
static DEVICE_ATTR(hdcpeven, S_IWUSR, NULL, store_hdcpeven);
static DEVICE_ATTR(hdcpchkaesotp, S_IRUGO, show_hdcpchkaesotp, NULL);
static DEVICE_ATTR(hdcpfuseaes, S_IRUGO | S_IWUSR, show_hdcpfuseaes,
		store_hdcpfuseaes);
static DEVICE_ATTR(hdcploadaes, S_IRUGO | S_IWUSR, show_hdcploadaes,
		store_hdcploadaes);
static DEVICE_ATTR(hdcpauthencr, S_IRUGO | S_IWUSR, show_hdcpauthencr,
		store_hdcpauthencr);
static DEVICE_ATTR(hdcpstateget, S_IRUGO, show_hdcpstateget, NULL);
static DEVICE_ATTR(evread, S_IRUGO, show_evread, NULL);
static DEVICE_ATTR(evclr, S_IWUSR, NULL, store_evclr);
static DEVICE_ATTR(audiocfg, S_IWUSR, NULL, store_audiocfg);
static DEVICE_ATTR(plugstatus, S_IRUGO, show_plugstatus, NULL);
static DEVICE_ATTR(poweronoff, S_IRUGO | S_IWUSR, show_poweronoff,
		store_poweronoff);
static DEVICE_ATTR(evwakeup, S_IWUSR, NULL, store_evwakeup);

/* Hex to int conversion */
static unsigned int htoi(const char *ptr)
{
	unsigned int value = 0;
	char ch = *ptr;

	if (!ptr)
		return 0;

	if (isdigit(ch))
		value = ch - '0';
	else
		value = toupper(ch) - 'A' + 10;

	value <<= 4;
	ch = *(++ptr);

	if (isdigit(ch))
		value += ch - '0';
	else
		value += toupper(ch) - 'A' + 10;

	return value;
}

static int event_enable(bool enable, enum hdmi_event ev)
{
	struct kobject *kobj = &hdmidev->kobj;

	dev_dbg(hdmidev, "enable_event %d %02x\n", enable, ev);
	if (enable)
		events_mask |= ev;
	else
		events_mask &= ~ev;

	if (events & ev) {
		/* Report pending event */
		/* Wake up application waiting for event via call to poll() */
		sysfs_notify(kobj, NULL, SYSFS_EVENT_FILENAME);

		LOCK_HDMI_EVENTS;
		events_received = true;
		UNLOCK_HDMI_EVENTS;

		wake_up_interruptible(&hdmi_event_wq);
	}

	return 0;
}

static int plugdeten(struct plug_detect *pldet)
{
	struct av8100_status status;
	u8 denc_off_time = 0;
	int retval;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	event_enable(pldet->hdmi_detect_enable != 0,
		HDMI_EVENT_HDMI_PLUGIN);
	event_enable(pldet->hdmi_detect_enable != 0,
		HDMI_EVENT_HDMI_PLUGOUT);

	av8100_reg_hdmi_5_volt_time_r(&denc_off_time, NULL, NULL);

	retval = av8100_reg_hdmi_5_volt_time_w(
			denc_off_time,
			pldet->hdmi_off_time,
			pldet->on_time);

	if (retval) {
		dev_err(hdmidev, "Failed to write the value to av8100 "
			"register\n");
		return -EFAULT;
	}

	return retval;
}

static int edidread(struct edid_read *edidread, u8 *len, u8 *data)
{
	union av8100_configuration config;
	struct av8100_status status;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	config.edid_section_readback_format.address = edidread->address;
	config.edid_section_readback_format.block_number = edidread->block_nr;

	dev_dbg(hdmidev, "addr:%0x blnr:%0x",
		config.edid_section_readback_format.address,
		config.edid_section_readback_format.block_number);

	if (av8100_conf_prep(AV8100_COMMAND_EDID_SECTION_READBACK,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_EDID_SECTION_READBACK,
		len, data, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	dev_dbg(hdmidev, "len:%0x\n", *len);

	return 0;
}

static int cecread(u8 *src, u8 *dest, u8 *data_len, u8 *data)
{
	union av8100_configuration config;
	struct av8100_status status;
	u8 buf_len;
	u8 buff[HDMI_CEC_READ_MAXSIZE];

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) !=	0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	if (av8100_conf_prep(AV8100_COMMAND_CEC_MESSAGE_READ_BACK,
			&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_CEC_MESSAGE_READ_BACK,
		&buf_len, buff, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	if (buf_len > 0) {
		*src = (buff[0] & 0xF0) >> 4;
		*dest = buff[0] & 0x0F;
		*data_len = buf_len - 1;
		memcpy(data, &buff[1], buf_len - 1);
	} else
		*data_len = 0;

	return 0;
}

static int cecsend(u8 src, u8 dest, u8 data_len, u8 *data)
{
	union av8100_configuration config;
	struct av8100_status status;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	config.cec_message_write_format.buffer[0] = ((src & 0x0F) << 4) +
			(dest & 0x0F);
	config.cec_message_write_format.buffer_length = data_len + 1;
	memcpy(&config.cec_message_write_format.buffer[1], data, data_len);

	if (av8100_conf_prep(AV8100_COMMAND_CEC_MESSAGE_WRITE,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_CEC_MESSAGE_WRITE,
		NULL, NULL, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	return 0;
}

static int infofrsend(u8 type, u8 version, u8 crc, u8 data_len, u8 *data)
{
	union av8100_configuration config;
	struct av8100_status status;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	if ((data_len < 1) || (data_len > HDMI_INFOFRAME_MAX_SIZE))
		return -EINVAL;

	config.infoframes_format.type = type;
	config.infoframes_format.version = version;
	config.infoframes_format.crc = crc;
	config.infoframes_format.length = data_len;
	memcpy(&config.infoframes_format.data, data, data_len);
	if (av8100_conf_prep(AV8100_COMMAND_INFOFRAMES,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_INFOFRAMES,
		NULL, NULL, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	return 0;
}

static int hdcpchkaesotp(u8 *crc, u8 *progged)
{
	union av8100_configuration config;
	struct av8100_status status;
	u8 buf_len;
	u8 buf[2];

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) !=
			0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	config.fuse_aes_key_format.fuse_operation = AV8100_FUSE_READ;
	memset(config.fuse_aes_key_format.key, 0, AV8100_FUSE_KEY_SIZE);
	if (av8100_conf_prep(AV8100_COMMAND_FUSE_AES_KEY,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_FUSE_AES_KEY,
		&buf_len, buf, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	if (buf_len == 2) {
		*crc = buf[0];
		*progged = buf[1];
	}

	return 0;
}

static int hdcpfuseaes(u8 *key, u8 crc, u8 *result)
{
	union av8100_configuration config;
	struct av8100_status status;
	u8 buf_len;
	u8 buf[2];

	/* Default not OK */
	*result = HDMI_RESULT_NOT_OK;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) !=
			0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	config.fuse_aes_key_format.fuse_operation = AV8100_FUSE_WRITE;
	memcpy(config.fuse_aes_key_format.key, key, AV8100_FUSE_KEY_SIZE);
	if (av8100_conf_prep(AV8100_COMMAND_FUSE_AES_KEY,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_FUSE_AES_KEY,
		&buf_len, buf, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	if (buf_len == 2) {
		dev_dbg(hdmidev, "buf[0]:%02x buf[1]:%02x\n", buf[0], buf[1]);
		if ((crc == buf[0]) && (buf[1] == 1))
			/* OK */
			*result = HDMI_RESULT_OK;
		else
			*result = HDMI_RESULT_CRC_MISMATCH;
	}

	return 0;
}

static int hdcploadaes(u8 block, u8 key_len, u8 *key, u8 *result, u8 *crc32)
{
	union av8100_configuration config;
	struct av8100_status status;
	u8 buf_len;
	u8 buf[CRC32_SIZE];

	/* Default not OK */
	*result = HDMI_RESULT_NOT_OK;

	dev_dbg(hdmidev, "%s block:%d\n", __func__, block);

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	config.hdcp_send_key_format.key_number = block;
	config.hdcp_send_key_format.data_len = key_len;
	memcpy(config.hdcp_send_key_format.data, key, key_len);
	if (av8100_conf_prep(AV8100_COMMAND_HDCP_SENDKEY, &config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_HDCP_SENDKEY,
		&buf_len, buf, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	if ((buf_len == CRC32_SIZE) && (crc32)) {
		memcpy(crc32, buf, CRC32_SIZE);
		dev_dbg(hdmidev, "crc32:%02x%02x%02x%02x\n",
			crc32[0], crc32[1], crc32[2], crc32[3]);
	}

	*result = HDMI_RESULT_OK;

	return 0;
}

static int hdcpauthencr(u8 auth_type, u8 encr_type, u8 *len, u8 *data)
{
	union av8100_configuration config;
	struct av8100_status status;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	switch (auth_type) {
	case HDMI_HDCP_AUTH_OFF:
	default:
		config.hdcp_management_format.req_type =
				AV8100_HDCP_AUTH_REQ_OFF;
		break;

	case HDMI_HDCP_AUTH_START:
		config.hdcp_management_format.req_type =
				AV8100_HDCP_AUTH_REQ_ON;
		break;

	case HDMI_HDCP_AUTH_REV_LIST_REQ:
		config.hdcp_management_format.req_type =
				AV8100_HDCP_REV_LIST_REQ;
		break;
	case HDMI_HDCP_AUTH_CONT:
		config.hdcp_management_format.req_type =
				AV8100_HDCP_AUTH_CONT;
		break;
	}

	switch (encr_type) {
	case HDMI_HDCP_ENCR_OESS:
	default:
		config.hdcp_management_format.encr_use =
				AV8100_HDCP_ENCR_USE_OESS;
		break;

	case HDMI_HDCP_ENCR_EESS:
		config.hdcp_management_format.encr_use =
				AV8100_HDCP_ENCR_USE_EESS;
		break;
	}

	if (av8100_conf_prep(AV8100_COMMAND_HDCP_MANAGEMENT,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_HDCP_MANAGEMENT,
		len, data, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	return 0;
}

static u8 events_read(void)
{
	int ret;

	LOCK_HDMI_EVENTS;
	ret = events;
	dev_dbg(hdmidev, "%s %02x\n", __func__, events);
	UNLOCK_HDMI_EVENTS;

	return ret;
}

static int events_clear(u8 ev)
{
	dev_dbg(hdmidev, "%s %02x\n", __func__, ev);

	LOCK_HDMI_EVENTS;
	events &= ~ev & EVENTS_MASK;
	UNLOCK_HDMI_EVENTS;

	return 0;
}

static int event_wakeup(void)
{
	struct kobject *kobj = &hdmidev->kobj;

	dev_dbg(hdmidev, "%s", __func__);

	LOCK_HDMI_EVENTS;
	events |= HDMI_EVENT_WAKEUP;
	events_received = true;
	UNLOCK_HDMI_EVENTS;

	/* Wake up application waiting for event via call to poll() */
	sysfs_notify(kobj, NULL, SYSFS_EVENT_FILENAME);
	wake_up_interruptible(&hdmi_event_wq);

	return 0;
}

static int audiocfg(struct audio_cfg *cfg)
{
	union av8100_configuration config;
	struct av8100_status status;

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup failed\n");
			return -EINVAL;
		}
	}

	if (status.av8100_state < AV8100_OPMODE_INIT) {
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
	}

	config.audio_input_format.audio_input_if_format	= cfg->if_format;
	config.audio_input_format.i2s_input_nb		= cfg->i2s_entries;
	config.audio_input_format.sample_audio_freq	= cfg->freq;
	config.audio_input_format.audio_word_lg		= cfg->word_length;
	config.audio_input_format.audio_format		= cfg->format;
	config.audio_input_format.audio_if_mode		= cfg->if_mode;
	config.audio_input_format.audio_mute		= cfg->mute;

	if (av8100_conf_prep(AV8100_COMMAND_AUDIO_INPUT_FORMAT,
		&config) != 0) {
		dev_err(hdmidev, "av8100_conf_prep FAIL\n");
		return -EINVAL;
	}

	if (av8100_conf_w(AV8100_COMMAND_AUDIO_INPUT_FORMAT,
		NULL, NULL, I2C_INTERFACE) != 0) {
		dev_err(hdmidev, "av8100_conf_w FAIL\n");
		return -EINVAL;
	}

	return 0;
}

/* sysfs */
static ssize_t store_storeastext(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if ((count != HDMI_STOREASTEXT_BIN_SIZE) &&
		(count != HDMI_STOREASTEXT_TEXT_SIZE) &&
		(count != HDMI_STOREASTEXT_TEXT_SIZE + 1))
		return -EINVAL;

	if ((count == HDMI_STOREASTEXT_BIN_SIZE) && (*buf == 0x1))
		hdmi_driver_data->store_as_hextext = true;
	else if (((count == HDMI_STOREASTEXT_TEXT_SIZE) ||
		(count == HDMI_STOREASTEXT_TEXT_SIZE + 1)) && (*buf == '0') &&
			(*(buf + 1) == '1')) {
		hdmi_driver_data->store_as_hextext = true;
	} else {
		hdmi_driver_data->store_as_hextext = false;
	}

	dev_dbg(hdmidev, "store_as_hextext:%0d\n",
		hdmi_driver_data->store_as_hextext);

	return count;
}

static ssize_t store_plugdeten(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct plug_detect plug_detect;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_PLUGDETEN_TEXT_SIZE) &&
			(count != HDMI_PLUGDETEN_TEXT_SIZE + 1))
			return -EINVAL;
		plug_detect.hdmi_detect_enable = htoi(buf + index);
		index += 2;
		plug_detect.on_time = htoi(buf + index);
		index += 2;
		plug_detect.hdmi_off_time = htoi(buf + index);
		index += 2;
	} else {
		if (count != HDMI_PLUGDETEN_BIN_SIZE)
			return -EINVAL;
		plug_detect.hdmi_detect_enable = *(buf + index++);
		plug_detect.on_time = *(buf + index++);
		plug_detect.hdmi_off_time = *(buf + index++);
	}

	if (plugdeten(&plug_detect))
		return -EINVAL;

	return count;
}

static ssize_t store_edidread(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct edid_read edid_read;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);
	dev_dbg(hdmidev, "count:%d\n", count);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_EDIDREAD_TEXT_SIZE) &&
			(count != HDMI_EDIDREAD_TEXT_SIZE + 1))
			return -EINVAL;
		edid_read.address = htoi(buf + index);
		index += 2;
		edid_read.block_nr = htoi(buf + index);
		index += 2;
	} else {
		if (count != HDMI_EDIDREAD_BIN_SIZE)
			return -EINVAL;
		edid_read.address = *(buf + index++);
		edid_read.block_nr = *(buf + index++);
	}

	if (edidread(&edid_read, &hdmi_driver_data->edid_data.buf_len,
			hdmi_driver_data->edid_data.buf))
		return -EINVAL;

	return count;
}

static ssize_t show_edidread(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int len;
	int index = 0;
	int cnt;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	len = hdmi_driver_data->edid_data.buf_len;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", len);
		index += 2;
	} else
		*(buf + index++) = len;

	dev_dbg(hdmidev, "len:%02x\n", len);

	cnt = 0;
	while (cnt < len) {
		if (hdmi_driver_data->store_as_hextext) {
			snprintf(buf + index, 3, "%02x",
				hdmi_driver_data->edid_data.buf[cnt]);
			index += 2;
		} else
			*(buf + index++) =
				hdmi_driver_data->edid_data.buf[cnt];

		dev_dbg(hdmidev, "%02x ",
			hdmi_driver_data->edid_data.buf[cnt]);

		cnt++;
	}

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t store_ceceven(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	bool enable = false;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_CECEVEN_TEXT_SIZE) &&
			(count != HDMI_CECEVEN_TEXT_SIZE + 1))
			return -EINVAL;
		if ((*buf == '0') && (*(buf + 1) == '1'))
			enable = true;
	} else {
		if (count != HDMI_CECEVEN_BIN_SIZE)
			return -EINVAL;
		if (*buf == 0x01)
			enable = true;
	}

	event_enable(enable, HDMI_EVENT_CEC | HDMI_EVENT_CECTXERR);

	return count;
}

static ssize_t show_cecread(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct cec_rw cec_read;
	int index = 0;
	int cnt;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (cecread(&cec_read.src, &cec_read.dest, &cec_read.length,
		cec_read.data))
		return -EINVAL;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", cec_read.src);
		index += 2;
		snprintf(buf + index, 3, "%02x", cec_read.dest);
		index += 2;
		snprintf(buf + index, 3, "%02x", cec_read.length);
		index += 2;
	} else {
		*(buf + index++) = cec_read.src;
		*(buf + index++) = cec_read.dest;
		*(buf + index++) = cec_read.length;
	}

	dev_dbg(hdmidev, "len:%02x\n", cec_read.length);

	cnt = 0;
	while (cnt < cec_read.length) {
		if (hdmi_driver_data->store_as_hextext) {
			snprintf(buf + index, 3, "%02x", cec_read.data[cnt]);
			index += 2;
		} else
			*(buf + index++) = cec_read.data[cnt];

		dev_dbg(hdmidev, "%02x ", cec_read.data[cnt]);

		cnt++;
	}

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t store_cecsend(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct cec_rw cec_w;
	int index = 0;
	int cnt;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count < HDMI_CECSEND_TEXT_SIZE_MIN) ||
			(count > HDMI_CECSEND_TEXT_SIZE_MAX))
			return -EINVAL;

		cec_w.src = htoi(buf + index);
		index += 2;
		cec_w.dest = htoi(buf + index);
		index += 2;
		cec_w.length = htoi(buf + index);
		index += 2;
		if (cec_w.length > HDMI_CEC_WRITE_MAXSIZE)
			return -EINVAL;
		cnt = 0;
		while (cnt < cec_w.length) {
			cec_w.data[cnt] = htoi(buf + index);
			index += 2;
			dev_dbg(hdmidev, "%02x ", cec_w.data[cnt]);
			cnt++;
		}
	} else {
		if ((count < HDMI_CECSEND_BIN_SIZE_MIN) ||
			(count > HDMI_CECSEND_BIN_SIZE_MAX))
			return -EINVAL;

		cec_w.src = *(buf + index++);
		cec_w.dest = *(buf + index++);
		cec_w.length = *(buf + index++);
		if (cec_w.length > HDMI_CEC_WRITE_MAXSIZE)
			return -EINVAL;
		memcpy(cec_w.data, buf + index, cec_w.length);
	}

	if (cecsend(cec_w.src,
			cec_w.dest,
			cec_w.length,
			cec_w.data))
		return -EINVAL;

	return count;
}

static ssize_t store_infofrsend(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct info_fr info_fr;
	int index = 0;
	int cnt;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count < HDMI_INFOFRSEND_TEXT_SIZE_MIN) ||
			(count > HDMI_INFOFRSEND_TEXT_SIZE_MAX))
			return -EINVAL;

		info_fr.type = htoi(&buf[index]);
		index += 2;
		info_fr.ver = htoi(&buf[index]);
		index += 2;
		info_fr.crc = htoi(&buf[index]);
		index += 2;
		info_fr.length = htoi(&buf[index]);
		index += 2;

		if (info_fr.length > HDMI_INFOFRAME_MAX_SIZE)
			return -EINVAL;
		cnt = 0;
		while (cnt < info_fr.length) {
			info_fr.data[cnt] = htoi(buf + index);
			index += 2;
			dev_dbg(hdmidev, "%02x ", info_fr.data[cnt]);
			cnt++;
		}
	} else {
		if ((count < HDMI_INFOFRSEND_BIN_SIZE_MIN) ||
			(count > HDMI_INFOFRSEND_BIN_SIZE_MAX))
			return -EINVAL;

		info_fr.type = *(buf + index++);
		info_fr.ver = *(buf + index++);
		info_fr.crc = *(buf + index++);
		info_fr.length = *(buf + index++);

		if (info_fr.length > HDMI_INFOFRAME_MAX_SIZE)
			return -EINVAL;
		memcpy(info_fr.data, buf + index, info_fr.length);
	}

	if (infofrsend(info_fr.type, info_fr.ver, info_fr.crc,
		info_fr.length, info_fr.data))
		return -EINVAL;

	return count;
}

static ssize_t store_hdcpeven(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	bool enable = false;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_HDCPEVEN_TEXT_SIZE) &&
			(count != HDMI_HDCPEVEN_TEXT_SIZE + 1))
			return -EINVAL;
		if ((*buf == '0') && (*(buf + 1) == '1'))
			enable = true;
	} else {
		if (count != HDMI_HDCPEVEN_BIN_SIZE)
			return -EINVAL;
		if (*buf == 0x01)
			enable = true;
	}

	event_enable(enable, HDMI_EVENT_HDCP);

	return count;
}

static ssize_t show_hdcpchkaesotp(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	u8 crc;
	u8 progged;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdcpchkaesotp(&crc, &progged))
		return -EINVAL;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", progged);
		index += 2;
	} else {
		*(buf + index++) = progged;
	}

	dev_dbg(hdmidev, "progged:%02x\n", progged);

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t store_hdcpfuseaes(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct hdcp_fuseaes hdcp_fuseaes;
	int index = 0;
	int cnt;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	/* Default not OK */
	hdmi_driver_data->fuse_result = HDMI_RESULT_NOT_OK;

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_HDCP_FUSEAES_TEXT_SIZE) &&
			(count != HDMI_HDCP_FUSEAES_TEXT_SIZE + 1))
			return -EINVAL;

		cnt = 0;
		while (cnt < HDMI_HDCP_FUSEAES_KEYSIZE) {
			hdcp_fuseaes.key[cnt] = htoi(buf + index);
			index += 2;
			dev_dbg(hdmidev, "%02x ", hdcp_fuseaes.key[cnt]);
			cnt++;
		}
		hdcp_fuseaes.crc = htoi(&buf[index]);
		index += 2;
		dev_dbg(hdmidev, "%02x ", hdcp_fuseaes.crc);
	} else {
		if (count != HDMI_HDCP_FUSEAES_BIN_SIZE)
			return -EINVAL;

		memcpy(hdcp_fuseaes.key, buf + index,
				HDMI_HDCP_FUSEAES_KEYSIZE);
		index += HDMI_HDCP_FUSEAES_KEYSIZE;
		hdcp_fuseaes.crc = *(buf + index++);
	}

	if (hdcpfuseaes(hdcp_fuseaes.key, hdcp_fuseaes.crc,
		&hdcp_fuseaes.result))
		return -EINVAL;

	dev_dbg(hdmidev, "fuseresult:%02x ", hdcp_fuseaes.result);

	hdmi_driver_data->fuse_result = hdcp_fuseaes.result;

	return count;
}

static ssize_t show_hdcpfuseaes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x",
			hdmi_driver_data->fuse_result);
		index += 2;
	} else
		*(buf + index++) = hdmi_driver_data->fuse_result;

	dev_dbg(hdmidev, "status:%02x\n", hdmi_driver_data->fuse_result);

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t store_hdcploadaes(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct hdcp_loadaesone hdcp_loadaes;
	int index = 0;
	int block_cnt;
	int cnt;
	u8 crc32_rcvd[CRC32_SIZE];
	u8 crc;
	u8 progged;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	/* Default not OK */
	hdmi_driver_data->loadaes_result = HDMI_RESULT_NOT_OK;

	if (hdcpchkaesotp(&crc, &progged))
		return -EINVAL;

	if (!progged) {
		/* AES is not fused */
		hdcp_loadaes.result = HDMI_AES_NOT_FUSED;
		goto store_hdcploadaes_err;
	}

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_HDCP_LOADAES_TEXT_SIZE) &&
			(count != HDMI_HDCP_LOADAES_TEXT_SIZE + 1)) {
			dev_err(hdmidev, "%s", "count mismatch\n");
			return -EINVAL;
		}

		/* AES */
		block_cnt = 0;
		while (block_cnt < HDMI_HDCP_AES_NR_OF_BLOCKS) {
			cnt = 0;
			while (cnt < HDMI_HDCP_AES_KEYSIZE) {
				hdcp_loadaes.key[cnt] =	htoi(buf + index);
				index += 2;
				dev_dbg(hdmidev, "%02x ",
					hdcp_loadaes.key[cnt]);
				cnt++;
			}

			if (hdcploadaes(block_cnt + HDMI_HDCP_AES_BLOCK_START,
					HDMI_HDCP_AES_KEYSIZE,
					hdcp_loadaes.key,
					&hdcp_loadaes.result,
					crc32_rcvd)) {
				dev_err(hdmidev, "%s %d\n",
					"hdcploadaes err aes block",
					block_cnt + HDMI_HDCP_AES_BLOCK_START);
				return -EINVAL;
			}

			if (hdcp_loadaes.result)
				goto store_hdcploadaes_err;

			block_cnt++;
		}

		/* KSV */
		memset(hdcp_loadaes.key, 0, HDMI_HDCP_AES_KSVZEROESSIZE);
		cnt = HDMI_HDCP_AES_KSVZEROESSIZE;
		while (cnt < HDMI_HDCP_AES_KSVSIZE +
				HDMI_HDCP_AES_KSVZEROESSIZE) {
			hdcp_loadaes.key[cnt] =
					htoi(&buf[index]);
			index += 2;
			dev_dbg(hdmidev, "%02x ", hdcp_loadaes.key[cnt]);
			cnt++;
		}

		if (hdcploadaes(HDMI_HDCP_KSV_BLOCK,
				HDMI_HDCP_AES_KSVSIZE +
				HDMI_HDCP_AES_KSVZEROESSIZE,
				hdcp_loadaes.key,
				&hdcp_loadaes.result,
				NULL)) {
			dev_err(hdmidev, "%s %d\n", "hdcploadaes err in ksv\n",
				block_cnt + HDMI_HDCP_AES_BLOCK_START);
			return -EINVAL;
		}

		if (hdcp_loadaes.result)
			goto store_hdcploadaes_err;

		/* CRC32 */
		for (cnt = 0; cnt < CRC32_SIZE; cnt++) {
			hdcp_loadaes.crc32[cnt] = htoi(buf + index);
			index += 2;
		}

		if (memcmp(hdcp_loadaes.crc32, crc32_rcvd, CRC32_SIZE)) {
			dev_dbg(hdmidev, "crc32exp:%02x%02x%02x%02x\n",
				hdcp_loadaes.crc32[0],
				hdcp_loadaes.crc32[1],
				hdcp_loadaes.crc32[2],
				hdcp_loadaes.crc32[3]);
			hdcp_loadaes.result = HDMI_RESULT_CRC_MISMATCH;
			goto store_hdcploadaes_err;
		}
	} else {
		if (count != HDMI_HDCP_LOADAES_BIN_SIZE) {
			dev_err(hdmidev, "%s", "count mismatch\n");
			return -EINVAL;
		}

		/* AES */
		block_cnt = 0;
		while (block_cnt < HDMI_HDCP_AES_NR_OF_BLOCKS) {
			memcpy(hdcp_loadaes.key, buf + index,
					HDMI_HDCP_AES_KEYSIZE);
			index += HDMI_HDCP_AES_KEYSIZE;

			if (hdcploadaes(block_cnt + HDMI_HDCP_AES_BLOCK_START,
					HDMI_HDCP_AES_KEYSIZE,
					hdcp_loadaes.key,
					&hdcp_loadaes.result,
					crc32_rcvd)) {
				dev_err(hdmidev, "%s %d\n",
					"hdcploadaes err aes block",
					block_cnt + HDMI_HDCP_AES_BLOCK_START);
				return -EINVAL;
			}

			if (hdcp_loadaes.result)
				goto store_hdcploadaes_err;

			block_cnt++;
		}

		/* KSV */
		memset(hdcp_loadaes.key, 0, HDMI_HDCP_AES_KSVZEROESSIZE);
		memcpy(hdcp_loadaes.key + HDMI_HDCP_AES_KSVZEROESSIZE,
				buf + index,
				HDMI_HDCP_AES_KSVSIZE);
		index += HDMI_HDCP_AES_KSVSIZE;

		if (hdcploadaes(HDMI_HDCP_KSV_BLOCK,
				HDMI_HDCP_AES_KSVSIZE +
				HDMI_HDCP_AES_KSVZEROESSIZE,
				hdcp_loadaes.key,
				&hdcp_loadaes.result,
				NULL)) {
			dev_err(hdmidev, "%s %d\n", "hdcploadaes err in ksv\n",
				block_cnt + HDMI_HDCP_AES_BLOCK_START);
			return -EINVAL;
		}

		memcpy(hdcp_loadaes.crc32, buf + index, CRC32_SIZE);
		index += CRC32_SIZE;

		/* CRC32 */
		if (memcmp(hdcp_loadaes.crc32, crc32_rcvd, CRC32_SIZE)) {
			dev_dbg(hdmidev, "crc32exp:%02x%02x%02x%02x\n",
				hdcp_loadaes.crc32[0],
				hdcp_loadaes.crc32[1],
				hdcp_loadaes.crc32[2],
				hdcp_loadaes.crc32[3]);
			hdcp_loadaes.result = HDMI_RESULT_CRC_MISMATCH;
		}
	}

store_hdcploadaes_err:
	hdmi_driver_data->loadaes_result = hdcp_loadaes.result;
	return count;
}

static ssize_t show_hdcploadaes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x",
			hdmi_driver_data->loadaes_result);
		index += 2;
	} else
		*(buf + index++) = hdmi_driver_data->loadaes_result;

	dev_dbg(hdmidev, "result:%02x\n", hdmi_driver_data->loadaes_result);

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t store_hdcpauthencr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct hdcp_authencr hdcp_authencr;
	int index = 0;
	u8 crc;
	u8 progged;
	int result = HDMI_RESULT_OK;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	/* Default not OK */
	hdmi_driver_data->authencr.result = HDMI_RESULT_NOT_OK;

	if (hdcpchkaesotp(&crc, &progged))
		return -EINVAL;

	if (!progged) {
		/* AES is not fused */
		result = HDMI_AES_NOT_FUSED;
		goto store_hdcpauthencr_err;
	}

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_HDCPAUTHENCR_TEXT_SIZE) &&
			(count != HDMI_HDCPAUTHENCR_TEXT_SIZE + 1))
			return -EINVAL;

		hdcp_authencr.auth_type = htoi(buf + index);
		index += 2;
		hdcp_authencr.encr_type = htoi(buf + index);
		index += 2;
	} else {
		if (count != HDMI_HDCPAUTHENCR_BIN_SIZE)
			return -EINVAL;

		hdcp_authencr.auth_type = *(buf + index++);
		hdcp_authencr.encr_type = *(buf + index++);
	}

	if (hdcpauthencr(hdcp_authencr.auth_type, hdcp_authencr.encr_type,
		 &hdmi_driver_data->authencr.buf_len,
		 hdmi_driver_data->authencr.buf))
		return -EINVAL;

store_hdcpauthencr_err:
	hdmi_driver_data->authencr.result = result;
	return count;
}

static ssize_t show_hdcpauthencr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int len;
	int index = 0;
	int cnt;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	len = hdmi_driver_data->authencr.buf_len;
	if (len > AUTH_BUF_LEN)
		len = AUTH_BUF_LEN;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x",
			hdmi_driver_data->authencr.result);
		index += 2;
	} else
		*(buf + index++) = hdmi_driver_data->authencr.result;

	cnt = 0;
	while (cnt < len) {
		if (hdmi_driver_data->store_as_hextext) {
			snprintf(buf + index, 3, "%02x",
				hdmi_driver_data->authencr.buf[cnt]);
			index += 2;

			dev_dbg(hdmidev, "%02x ",
				hdmi_driver_data->authencr.buf[cnt]);

		} else
			*(buf + index++) = hdmi_driver_data->authencr.buf[cnt];

		cnt++;
	}

	dev_dbg(hdmidev, "result:%02x\n", hdmi_driver_data->authencr.result);

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t show_hdcpstateget(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	u8 hdcp_state;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (av8100_reg_gen_status_r(NULL, NULL, NULL, NULL, NULL, &hdcp_state))
			return -EINVAL;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", hdcp_state);
		index += 2;
	} else
		*(buf + index++) = hdcp_state;

	dev_dbg(hdmidev, "status:%02x\n", hdcp_state);

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t show_evread(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int index = 0;
	u8 ev;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	ev = events_read();

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", ev);
		index += 2;
	} else
		*(buf + index++) = ev;

	if (hdmi_driver_data->store_as_hextext)
		index++;

	/* Events are read: clear events */
	events_clear(EVENTS_MASK);

	return index;
}

static ssize_t store_evclr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	u8 ev;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_EVCLR_TEXT_SIZE) &&
			(count != HDMI_EVCLR_TEXT_SIZE + 1))
			return -EINVAL;

		ev = htoi(&buf[index]);
		index += 2;
	} else {
		if (count != HDMI_EVCLR_BIN_SIZE)
			return -EINVAL;

		ev = *(buf + index++);
	}

	events_clear(ev);

	return count;
}

static ssize_t store_audiocfg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	struct audio_cfg audio_cfg;
	int index = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_AUDIOCFG_TEXT_SIZE) &&
			(count != HDMI_AUDIOCFG_TEXT_SIZE + 1))
			return -EINVAL;

		audio_cfg.if_format = htoi(&buf[index]);
		index += 2;
		audio_cfg.i2s_entries = htoi(&buf[index]);
		index += 2;
		audio_cfg.freq = htoi(&buf[index]);
		index += 2;
		audio_cfg.word_length = htoi(&buf[index]);
		index += 2;
		audio_cfg.format = htoi(&buf[index]);
		index += 2;
		audio_cfg.if_mode = htoi(&buf[index]);
		index += 2;
		audio_cfg.mute = htoi(&buf[index]);
		index += 2;
	} else {
		if (count != HDMI_AUDIOCFG_BIN_SIZE)
			return -EINVAL;

		audio_cfg.if_format = *(buf + index++);
		audio_cfg.i2s_entries = *(buf + index++);
		audio_cfg.freq = *(buf + index++);
		audio_cfg.word_length = *(buf + index++);
		audio_cfg.format = *(buf + index++);
		audio_cfg.if_mode = *(buf + index++);
		audio_cfg.mute = *(buf + index++);
	}

	audiocfg(&audio_cfg);

	return count;
}

static ssize_t show_plugstatus(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int index = 0;
	struct av8100_status av8100_status;
	u8 plstat;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	av8100_status = av8100_status_get();
	plstat = av8100_status.av8100_plugin_status == AV8100_HDMI_PLUGIN;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", plstat);
		index += 2;
	} else
		*(buf + index++) = plstat;

	if (hdmi_driver_data->store_as_hextext)
		index++;

	return index;
}

static ssize_t store_poweronoff(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hdmi_driver_data *hdmi_driver_data;
	bool enable = false;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	if (hdmi_driver_data->store_as_hextext) {
		if ((count != HDMI_POWERONOFF_TEXT_SIZE) &&
			(count != HDMI_POWERONOFF_TEXT_SIZE + 1))
			return -EINVAL;
		if ((*buf == '0') && (*(buf + 1) == '1'))
			enable = true;
	} else {
		if (count != HDMI_POWERONOFF_BIN_SIZE)
			return -EINVAL;
		if (*buf == 0x01)
			enable = true;
	}

	if (enable == 0) {
		if (av8100_powerdown() != 0) {
			dev_err(hdmidev, "av8100_powerdown FAIL\n");
			return -EINVAL;
		}
	} else {
		if (av8100_powerup() != 0) {
			dev_err(hdmidev, "av8100_powerup FAIL\n");
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t show_poweronoff(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hdmi_driver_data *hdmi_driver_data;
	int index = 0;
	struct av8100_status status;
	u8 power_state;

	dev_dbg(hdmidev, "%s\n", __func__);

	hdmi_driver_data = dev_get_drvdata(dev);

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_SCAN)
		power_state = 0;
	else
		power_state = 1;

	if (hdmi_driver_data->store_as_hextext) {
		snprintf(buf + index, 3, "%02x", power_state);
		index += 3;
	} else {
		*(buf + index++) = power_state;
	}

	return index;
}

static ssize_t store_evwakeup(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	dev_dbg(hdmidev, "%s\n", __func__);

	event_wakeup();

	return count;
}

static int hdmi_open(struct inode *inode, struct file *filp)
{
	if (device_open)
		return -EBUSY;

	device_open++;

	return 0;
}

static int hdmi_release(struct inode *inode, struct file *filp)
{
	if (device_open)
		device_open--;

	return 0;
}

/* ioctl */
static int hdmi_ioctl(struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	u8 value = 0;
	struct plug_detect plug_detect;
	struct edid_read edid_read;
	struct cec_rw cec_read;
	struct cec_rw cec_send;
	struct info_fr info_fr;
	struct hdcp_fuseaes hdcp_fuseaes;
	struct hdcp_loadaesall hdcp_loadaesall;
	int block_cnt;
	struct hdcp_loadaesone hdcp_loadaesone;
	struct hdcp_authencr hdcp_authencr;
	struct audio_cfg audio_cfg;
	union av8100_configuration config;
	struct hdmi_register reg;
	struct hdmi_command_register command_reg;
	struct av8100_status status;
	u8 aes_status;

	switch (cmd) {
	case IOC_PLUG_DETECT_ENABLE:
		if (copy_from_user(&plug_detect, (void *)arg,
			sizeof(struct plug_detect)))
			return -EINVAL;

		if (plugdeten(&plug_detect))
			return -EINVAL;
		break;

	case IOC_EDID_READ:
		if (copy_from_user(&edid_read, (void *)arg,
				sizeof(struct edid_read)))
			return -EINVAL;

		if (edidread(&edid_read, &edid_read.data_length,
				edid_read.data))
			return -EINVAL;

		if (copy_to_user((void *)arg, (void *)&edid_read,
			sizeof(struct edid_read))) {
			return -EINVAL;
		}
		break;

	case IOC_CEC_EVENT_ENABLE:
		if (copy_from_user(&value, (void *)arg, sizeof(u8)))
			return -EINVAL;

		event_enable(value != 0, HDMI_EVENT_CEC | HDMI_EVENT_CECTXERR);
		break;

	case IOC_CEC_READ:
		if (cecread(&cec_read.src, &cec_read.dest, &cec_read.length,
			cec_read.data))
			return -EINVAL;

		if (copy_to_user((void *)arg, (void *)&cec_read,
			sizeof(struct cec_rw))) {
			return -EINVAL;
		}
		break;

	case IOC_CEC_SEND:
		if (copy_from_user(&cec_send, (void *)arg,
				sizeof(struct cec_rw)))
			return -EINVAL;

		if (cecsend(cec_send.src,
				cec_send.dest,
				cec_send.length,
				cec_send.data))
			return -EINVAL;
		break;

	case IOC_INFOFRAME_SEND:
		if (copy_from_user(&info_fr, (void *)arg,
				sizeof(struct info_fr)))
			return -EINVAL;

		if (infofrsend(info_fr.type, info_fr.ver, info_fr.crc,
			info_fr.length, info_fr.data))
			return -EINVAL;
		break;

	case IOC_HDCP_EVENT_ENABLE:
		if (copy_from_user(&value, (void *)arg, sizeof(u8)))
			return -EINVAL;

		event_enable(value != 0, HDMI_EVENT_HDCP);
		break;

	case IOC_HDCP_CHKAESOTP:
		if (hdcpchkaesotp(&value, &aes_status))
			return -EINVAL;

		if (copy_to_user((void *)arg, (void *)&aes_status,
			sizeof(u8))) {
			return -EINVAL;
		}
		break;

	case IOC_HDCP_FUSEAES:
		if (copy_from_user(&hdcp_fuseaes, (void *)arg,
				sizeof(struct hdcp_fuseaes)))
			return -EINVAL;

		if (hdcpfuseaes(hdcp_fuseaes.key, hdcp_fuseaes.crc,
				&hdcp_fuseaes.result))
				return -EINVAL;

		if (copy_to_user((void *)arg, (void *)&hdcp_fuseaes,
			sizeof(struct hdcp_fuseaes))) {
			return -EINVAL;
		}
		break;

	case IOC_HDCP_LOADAES:
		if (copy_from_user(&hdcp_loadaesall, (void *)arg,
				sizeof(struct hdcp_loadaesall)))
			return -EINVAL;

		if (hdcpchkaesotp(&value, &aes_status))
			return -EINVAL;

		if (!aes_status) {
			/* AES is not fused */
			hdcp_loadaesone.result = HDMI_AES_NOT_FUSED;
			goto ioc_hdcploadaes_err;
		}

		/* AES */
		block_cnt = 0;
		while (block_cnt < HDMI_HDCP_AES_NR_OF_BLOCKS) {
			memcpy(hdcp_loadaesone.key, hdcp_loadaesall.key +
					block_cnt * HDMI_HDCP_AES_KEYSIZE,
					HDMI_HDCP_AES_KEYSIZE);

			if (hdcploadaes(block_cnt + HDMI_HDCP_AES_BLOCK_START,
					HDMI_HDCP_AES_KEYSIZE,
					hdcp_loadaesone.key,
					&hdcp_loadaesone.result,
					hdcp_loadaesone.crc32))
				return -EINVAL;

			if (hdcp_loadaesone.result)
				return -EINVAL;

			block_cnt++;
		}

		/* KSV */
		memset(hdcp_loadaesone.key, 0, HDMI_HDCP_AES_KSVZEROESSIZE);
		memcpy(hdcp_loadaesone.key + HDMI_HDCP_AES_KSVZEROESSIZE,
				hdcp_loadaesall.ksv, HDMI_HDCP_AES_KSVSIZE);

		if (hdcploadaes(HDMI_HDCP_KSV_BLOCK,
				HDMI_HDCP_AES_KSVSIZE +
				HDMI_HDCP_AES_KSVZEROESSIZE,
				hdcp_loadaesone.key,
				&hdcp_loadaesone.result,
				NULL))
			return -EINVAL;

		if (hdcp_loadaesone.result)
			return -EINVAL;

		/* CRC32 */
		if (memcmp(hdcp_loadaesall.crc32, hdcp_loadaesone.crc32,
				CRC32_SIZE)) {
			dev_dbg(hdmidev, "crc32exp:%02x%02x%02x%02x\n",
				hdcp_loadaesall.crc32[0],
				hdcp_loadaesall.crc32[1],
				hdcp_loadaesall.crc32[2],
				hdcp_loadaesall.crc32[3]);
			hdcp_loadaesone.result = HDMI_RESULT_CRC_MISMATCH;
			goto ioc_hdcploadaes_err;
		}

ioc_hdcploadaes_err:
		hdcp_loadaesall.result = hdcp_loadaesone.result;

		if (copy_to_user((void *)arg, (void *)&hdcp_loadaesall,
			sizeof(struct hdcp_loadaesall))) {
			return -EINVAL;
		}
		break;

	case IOC_HDCP_AUTHENCR_REQ:
		if (copy_from_user(&hdcp_authencr, (void *)arg,
				sizeof(struct hdcp_authencr)))
			return -EINVAL;

		/* Default not OK */
		hdcp_authencr.result = HDMI_RESULT_NOT_OK;

		if (hdcpchkaesotp(&value, &aes_status))
			return -EINVAL;

		if (!aes_status) {
			/* AES is not fused */
			hdcp_authencr.result = HDMI_AES_NOT_FUSED;
			break;
		}

		if (hdcpauthencr(hdcp_authencr.auth_type,
				hdcp_authencr.encr_type,
				&value,
				hdcp_authencr.revoc_list))
			return -EINVAL;

		hdcp_authencr.result = HDMI_RESULT_OK;
		break;

	case IOC_HDCP_STATE_GET:
		if (av8100_reg_gen_status_r(NULL, NULL, NULL, NULL, NULL,
				&value))
				return -EINVAL;

		if (copy_to_user((void *)arg, (void *)&value,
			sizeof(u8))) {
			return -EINVAL;
		}
		break;

	case IOC_EVENTS_READ:
		value = events_read();

		if (copy_to_user((void *)arg, (void *)&value,
			sizeof(u8))) {
			return -EINVAL;
		}

		/* Events are read: clear events */
		events_clear(EVENTS_MASK);
		break;

	case IOC_EVENTS_CLEAR:
		if (copy_from_user(&value, (void *)arg, sizeof(u8)))
			return -EINVAL;

		events_clear(value);
		break;

	case IOC_AUDIO_CFG:
		if (copy_from_user(&audio_cfg, (void *)arg,
				sizeof(struct audio_cfg)))
			return -EINVAL;

		audiocfg(&audio_cfg);
		break;

	case IOC_PLUG_STATUS:
		status = av8100_status_get();
		value = status.av8100_plugin_status == AV8100_HDMI_PLUGIN;

		if (copy_to_user((void *)arg, (void *)&value,
			sizeof(u8))) {
			return -EINVAL;
		}
		break;

	case IOC_POWERONOFF:
		/* Get desired power state on or off */
		if (copy_from_user(&value, (void *)arg, sizeof(u8)))
			return -EINVAL;

		if (value == 0) {
			if (av8100_powerdown() != 0) {
				dev_err(hdmidev, "av8100_powerdown FAIL\n");
				return -EINVAL;
			}
		} else {
			if (av8100_powerup() != 0) {
				dev_err(hdmidev, "av8100_powerup FAIL\n");
				return -EINVAL;
			}
		}
		break;

	case IOC_EVENT_WAKEUP:
		/* Trigger event */
		event_wakeup();
		break;

	case IOC_POWERSTATE:
		status = av8100_status_get();
		value = status.av8100_state >= AV8100_OPMODE_SCAN;

		if (copy_to_user((void *)arg, (void *)&value,
						sizeof(u8))) {
			return -EINVAL;
		}
		break;

	/* Internal */
	case IOC_HDMI_ENABLE_INTERRUPTS:
		av8100_disable_interrupt();
		if (av8100_enable_interrupt() != 0) {
			dev_err(hdmidev, "av8100_conf_get FAIL\n");
			return -EINVAL;
		}
		break;

	case IOC_HDMI_DOWNLOAD_FW:
		if (av8100_download_firmware(NULL, 0, I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100 dl fw FAIL\n");
			return -EINVAL;
		}
		break;

	case IOC_HDMI_ONOFF:
		/* Get desired HDMI mode on or off */
		if (copy_from_user(&value, (void *)arg, sizeof(u8)))
			return -EFAULT;

		if (av8100_conf_get(AV8100_COMMAND_HDMI, &config) != 0) {
			dev_err(hdmidev, "av8100_conf_get FAIL\n");
			return -EINVAL;
		}
		if (value == 0)
			config.hdmi_format.hdmi_mode = AV8100_HDMI_OFF;
		else
			config.hdmi_format.hdmi_mode = AV8100_HDMI_ON;

		if (av8100_conf_prep(AV8100_COMMAND_HDMI, &config) != 0) {
			dev_err(hdmidev, "av8100_conf_prep FAIL\n");
			return -EINVAL;
		}
		if (av8100_conf_w(AV8100_COMMAND_HDMI, NULL, NULL,
			I2C_INTERFACE) != 0) {
			dev_err(hdmidev, "av8100_conf_w FAIL\n");
			return -EINVAL;
		}
		break;

	case IOC_HDMI_REGISTER_WRITE:
		if (copy_from_user(&reg, (void *)arg,
			sizeof(struct hdmi_register))) {
			return -EINVAL;
		}

		if (av8100_reg_w(reg.offset, reg.value) != 0) {
			dev_err(hdmidev, "hdmi_register_write FAIL\n");
			return -EINVAL;
		}
		break;

	case IOC_HDMI_REGISTER_READ:
		if (copy_from_user(&reg, (void *)arg,
			sizeof(struct hdmi_register))) {
			return -EINVAL;
		}

		if (av8100_reg_r(reg.offset, &reg.value) != 0) {
			dev_err(hdmidev, "hdmi_register_write FAIL\n");
			return -EINVAL;
		}

		if (copy_to_user((void *)arg, (void *)&reg,
			sizeof(struct hdmi_register))) {
			return -EINVAL;
		}
		break;

	case IOC_HDMI_STATUS_GET:
		status = av8100_status_get();

		if (copy_to_user((void *)arg, (void *)&status,
			sizeof(struct av8100_status))) {
			return -EINVAL;
		}
		break;

	case IOC_HDMI_CONFIGURATION_WRITE:
		if (copy_from_user(&command_reg, (void *)arg,
				sizeof(struct hdmi_command_register)) != 0) {
			dev_err(hdmidev, "IOC_HDMI_CONFIGURATION_WRITE "
				"fail 1\n");
			command_reg.return_status = EINVAL;
		} else {
			command_reg.return_status = 0;
			if (av8100_conf_w_raw(command_reg.cmd_id,
					command_reg.buf_len,
					command_reg.buf,
					&(command_reg.buf_len),
					command_reg.buf) != 0) {
				dev_err(hdmidev, "IOC_HDMI_CONFIGURATION_WRITE "
					"fail 2\n");
				command_reg.return_status = EINVAL;
			}
		}

		if (copy_to_user((void *)arg, (void *)&command_reg,
			sizeof(struct hdmi_command_register)) != 0) {
			return -EINVAL;
		}
		break;

	default:
		break;
	}

	return 0;
}

static long hdmi_unlocked_ioctl(struct file *file, unsigned int cmd, 
							unsigned long arg)
{
	int ret;

	lock_kernel();
	ret = hdmi_ioctl(file, cmd, arg);
	unlock_kernel();

	return ret;
}

static unsigned int
hdmi_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	dev_dbg(hdmidev, "%s\n", __func__);

	poll_wait(filp, &hdmi_event_wq , wait);

	LOCK_HDMI_EVENTS;
	if (events_received == true) {
		events_received = false;
		mask = POLLIN | POLLRDNORM;
	}
	UNLOCK_HDMI_EVENTS;

	return mask;
}

static const struct file_operations hdmi_fops = {
	.owner =    THIS_MODULE,
	.open =     hdmi_open,
	.release =  hdmi_release,
	.unlocked_ioctl = hdmi_unlocked_ioctl,
	.poll = hdmi_poll
};

static struct miscdevice hdmi_miscdev = {
	MISC_DYNAMIC_MINOR,
	"hdmi",
	&hdmi_fops
};

/* Event callback function called by hw driver */
void hdmi_event(enum av8100_hdmi_event ev)
{
	int events_old;
	int events_new;
	struct kobject *kobj = &hdmidev->kobj;

	dev_dbg(hdmidev, "hdmi_event %02x\n", ev);

	LOCK_HDMI_EVENTS;

	events_old = events;

	/* Set event */
	switch (ev) {
	case AV8100_HDMI_EVENT_HDMI_PLUGIN:
		events &= ~HDMI_EVENT_HDMI_PLUGOUT;
		events |= HDMI_EVENT_HDMI_PLUGIN;
		break;

	case AV8100_HDMI_EVENT_HDMI_PLUGOUT:
		events &= ~HDMI_EVENT_HDMI_PLUGIN;
		events |= HDMI_EVENT_HDMI_PLUGOUT;
		break;

	case AV8100_HDMI_EVENT_CEC:
		events |= HDMI_EVENT_CEC;
		break;

	case AV8100_HDMI_EVENT_HDCP:
		events |= HDMI_EVENT_HDCP;
		break;

	case AV8100_HDMI_EVENT_CECTXERR:
		events |= HDMI_EVENT_CECTXERR;
		break;

	default:
		break;
	}

	events_new = events_mask & events;

	UNLOCK_HDMI_EVENTS;

	dev_dbg(hdmidev, "hdmi events:%02x, events_old:%02x mask:%02x\n",
			events_new, events_old, events_mask);

	if (events_new != events_old) {
		/* Wake up application waiting for event via call to poll() */
		sysfs_notify(kobj, NULL, SYSFS_EVENT_FILENAME);

		LOCK_HDMI_EVENTS;
		events_received = true;
		UNLOCK_HDMI_EVENTS;

		wake_up_interruptible(&hdmi_event_wq);
	}
}
EXPORT_SYMBOL(hdmi_event);

int __init hdmi_init(void)
{
	int ret;
	struct hdmi_driver_data *hdmi_driver_data;

	ret = misc_register(&hdmi_miscdev);
	if (ret)
		goto hdmi_init_out;

	hdmidev = hdmi_miscdev.this_device;

	hdmi_driver_data =
		kzalloc(sizeof(struct hdmi_driver_data), GFP_KERNEL);

	if (!hdmi_driver_data)
		return -ENOMEM;

	dev_set_drvdata(hdmidev, hdmi_driver_data);

	/* Default sysfs file format is hextext */
	hdmi_driver_data->store_as_hextext = true;

	init_waitqueue_head(&hdmi_event_wq);

	if (device_create_file(hdmidev, &dev_attr_storeastext))
		dev_info(hdmidev, "Unable to create storeastext attribute\n");
	if (device_create_file(hdmidev, &dev_attr_plugdeten))
		dev_info(hdmidev, "Unable to create plugdeten attribute\n");
	if (device_create_file(hdmidev, &dev_attr_edidread))
		dev_info(hdmidev, "Unable to create edidread attribute\n");
	if (device_create_file(hdmidev, &dev_attr_ceceven))
		dev_info(hdmidev, "Unable to create ceceven attribute\n");
	if (device_create_file(hdmidev, &dev_attr_cecread))
		dev_info(hdmidev, "Unable to create cecread attribute\n");
	if (device_create_file(hdmidev, &dev_attr_cecsend))
		dev_info(hdmidev, "Unable to create cecsend attribute\n");
	if (device_create_file(hdmidev, &dev_attr_infofrsend))
		dev_info(hdmidev, "Unable to create infofrsend attribute\n");
	if (device_create_file(hdmidev, &dev_attr_hdcpeven))
		dev_info(hdmidev, "Unable to create hdcpeven attribute\n");
	if (device_create_file(hdmidev, &dev_attr_hdcpchkaesotp))
		dev_info(hdmidev, "Unable to create hdcpchkaesotp attribute\n");
	if (device_create_file(hdmidev, &dev_attr_hdcpfuseaes))
		dev_info(hdmidev, "Unable to create hdcpfuseaes attribute\n");
	if (device_create_file(hdmidev, &dev_attr_hdcploadaes))
		dev_info(hdmidev, "Unable to create hdcploadaes attribute\n");
	if (device_create_file(hdmidev, &dev_attr_hdcpauthencr))
		dev_info(hdmidev, "Unable to create hdcpauthreq attribute\n");
	if (device_create_file(hdmidev, &dev_attr_hdcpstateget))
		dev_info(hdmidev, "Unable to create hdcpstateget attribute\n");
	if (device_create_file(hdmidev, &dev_attr_evread))
		dev_info(hdmidev, "Unable to create evread attribute\n");
	if (device_create_file(hdmidev, &dev_attr_evclr))
		dev_info(hdmidev, "Unable to create evclr attribute\n");
	if (device_create_file(hdmidev, &dev_attr_audiocfg))
		dev_info(hdmidev, "Unable to create audiocfg attribute\n");
	if (device_create_file(hdmidev, &dev_attr_plugstatus))
		dev_info(hdmidev, "Unable to create plugstatus attribute\n");
	if (device_create_file(hdmidev, &dev_attr_poweronoff))
		dev_info(hdmidev, "Unable to create poweronoff attribute\n");
	if (device_create_file(hdmidev, &dev_attr_evwakeup))
		dev_info(hdmidev, "Unable to create evwakeup attribute\n");

	/* Register event callback */
	av8100_hdmi_event_cb_set(hdmi_event);

hdmi_init_out:
	return ret;
}
late_initcall(hdmi_init);

void hdmi_exit(void)
{
	struct hdmi_driver_data *hdmi_driver_data;

	/* Deregister event callback */
	av8100_hdmi_event_cb_set(NULL);

	device_remove_file(hdmidev, &dev_attr_storeastext);
	device_remove_file(hdmidev, &dev_attr_plugdeten);
	device_remove_file(hdmidev, &dev_attr_edidread);
	device_remove_file(hdmidev, &dev_attr_ceceven);
	device_remove_file(hdmidev, &dev_attr_cecread);
	device_remove_file(hdmidev, &dev_attr_cecsend);
	device_remove_file(hdmidev, &dev_attr_infofrsend);
	device_remove_file(hdmidev, &dev_attr_hdcpeven);
	device_remove_file(hdmidev, &dev_attr_hdcpchkaesotp);
	device_remove_file(hdmidev, &dev_attr_hdcpfuseaes);
	device_remove_file(hdmidev, &dev_attr_hdcploadaes);
	device_remove_file(hdmidev, &dev_attr_hdcpauthencr);
	device_remove_file(hdmidev, &dev_attr_hdcpstateget);
	device_remove_file(hdmidev, &dev_attr_evread);
	device_remove_file(hdmidev, &dev_attr_evclr);
	device_remove_file(hdmidev, &dev_attr_audiocfg);
	device_remove_file(hdmidev, &dev_attr_plugstatus);
	device_remove_file(hdmidev, &dev_attr_poweronoff);
	device_remove_file(hdmidev, &dev_attr_evwakeup);

	hdmi_driver_data = dev_get_drvdata(hdmidev);
	kfree(hdmi_driver_data);

	misc_deregister(&hdmi_miscdev);
}

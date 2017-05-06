/*
 *  max517.c - Support for Maxim MAX517, MAX518 and MAX519
 *
 *  Copyright (C) 2010, 2011 Roland Stigge <stigge@antcom.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>

<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
#include "../iio.h"
#include "dac.h"

#include "max517.h"
=======
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/dac/max517.h>
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c

#define MAX517_DRV_NAME	"max517"

/* Commands */
#define COMMAND_CHANNEL0	0x00
#define COMMAND_CHANNEL1	0x01 /* for MAX518 and MAX519 */
#define COMMAND_PD		0x08 /* Power Down */

enum max517_device_ids {
	ID_MAX517,
	ID_MAX518,
	ID_MAX519,
};

struct max517_data {
	struct i2c_client	*client;
	unsigned short		vref_mv[2];
};

/*
 * channel: bit 0: channel 1
 *          bit 1: channel 2
 * (this way, it's possible to set both channels at once)
 */
static int max517_set_value(struct iio_dev *indio_dev,
	long val, int channel)
{
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
	struct iio_dev *dev_info = dev_get_drvdata(dev);
	struct max517_data *data = iio_dev_get_devdata(dev_info);
=======
	struct max517_data *data = iio_priv(indio_dev);
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c
	struct i2c_client *client = data->client;
	u8 outbuf[2];
	int res;

	if (val < 0 || val > 255)
		return -EINVAL;

	outbuf[0] = channel;
	outbuf[1] = val;

	res = i2c_master_send(client, outbuf, 2);
	if (res < 0)
		return res;
	else if (res != 2)
		return -EIO;
	else
		return 0;
}
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
static IIO_DEVICE_ATTR_NAMED(out1and2_raw, out1&2_raw, S_IWUSR, NULL,
		max517_set_value_both, -1);
=======
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c

static int max517_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val,
			   int *val2,
			   long m)
{
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
	struct iio_dev *dev_info = dev_get_drvdata(dev);
	struct max517_data *data = iio_dev_get_devdata(dev_info);
	/* Corresponds to Vref / 2^(bits) */
	unsigned int scale_uv = (data->vref_mv[channel - 1] * 1000) >> 8;

	return sprintf(buf, "%d.%03d\n", scale_uv / 1000, scale_uv % 1000);
=======
	struct max517_data *data = iio_priv(indio_dev);
	unsigned int scale_uv;

	switch (m) {
	case IIO_CHAN_INFO_SCALE:
		/* Corresponds to Vref / 2^(bits) */
		scale_uv = (data->vref_mv[chan->channel] * 1000) >> 8;
		*val =  scale_uv / 1000000;
		*val2 = scale_uv % 1000000;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		break;
	}
	return -EINVAL;
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c
}

static int max517_write_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int val, int val2, long mask)
{
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
	return max517_show_scale(dev, attr, buf, 1);
}
static IIO_DEVICE_ATTR(out1_scale, S_IRUGO, max517_show_scale1, NULL, 0);
=======
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = max517_set_value(indio_dev, val, chan->channel);
		break;
	default:
		ret = -EINVAL;
		break;
	}
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c

	return ret;
}
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
static IIO_DEVICE_ATTR(out2_scale, S_IRUGO, max517_show_scale2, NULL, 0);

/* On MAX517 variant, we have one output */
static struct attribute *max517_attributes[] = {
	&iio_dev_attr_out1_raw.dev_attr.attr,
	&iio_dev_attr_out1_scale.dev_attr.attr,
	NULL
};

static struct attribute_group max517_attribute_group = {
	.attrs = max517_attributes,
};

/* On MAX518 and MAX519 variant, we have two outputs */
static struct attribute *max518_attributes[] = {
	&iio_dev_attr_out1_raw.dev_attr.attr,
	&iio_dev_attr_out1_scale.dev_attr.attr,
	&iio_dev_attr_out2_raw.dev_attr.attr,
	&iio_dev_attr_out2_scale.dev_attr.attr,
	&iio_dev_attr_out1and2_raw.dev_attr.attr,
	NULL
};

static struct attribute_group max518_attribute_group = {
	.attrs = max518_attributes,
};
=======
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c

static int max517_suspend(struct i2c_client *client, pm_message_t mesg)
{
	u8 outbuf = COMMAND_PD;

	return i2c_master_send(client, &outbuf, 1);
}

static int max517_resume(struct i2c_client *client)
{
	u8 outbuf = 0;

	return i2c_master_send(client, &outbuf, 1);
}

static const struct iio_info max517_info = {
	.read_raw = max517_read_raw,
	.write_raw = max517_write_raw,
	.driver_module = THIS_MODULE,
};

#define MAX517_CHANNEL(chan) {				\
	.type = IIO_VOLTAGE,				\
	.indexed = 1,					\
	.output = 1,					\
	.channel = (chan),				\
	.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT |	\
	IIO_CHAN_INFO_SCALE_SEPARATE_BIT,		\
	.scan_type = IIO_ST('u', 8, 8, 0),		\
}

static const struct iio_chan_spec max517_channels[] = {
	MAX517_CHANNEL(0),
	MAX517_CHANNEL(1)
};

static int __devinit max517_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max517_data *data;
	struct max517_platform_data *platform_data = client->dev.platform_data;
	int err;

	data = kzalloc(sizeof(struct max517_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);

	data->client = client;

	data->indio_dev = iio_allocate_device(0);
	if (data->indio_dev == NULL) {
		err = -ENOMEM;
		goto exit_free_data;
	}

	/* establish that the iio_dev is a child of the i2c device */
	data->indio_dev->dev.parent = &client->dev;

	/* reduced channel set for MAX517 */
	if (id->driver_data == ID_MAX517)
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
		data->indio_dev->info = &max517_info;
	else
		data->indio_dev->info = &max518_info;
	data->indio_dev->dev_data = (void *)(data);
	data->indio_dev->modes = INDIO_DIRECT_MODE;
=======
		indio_dev->num_channels = 1;
	else
		indio_dev->num_channels = 2;
	indio_dev->channels = max517_channels;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &max517_info;
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c

	/*
	 * Reference voltage on MAX518 and default is 5V, else take vref_mv
	 * from platform_data
	 */
	if (id->driver_data == ID_MAX518 || !platform_data) {
		data->vref_mv[0] = data->vref_mv[1] = 5000; /* mV */
	} else {
		data->vref_mv[0] = platform_data->vref_mv[0];
		data->vref_mv[1] = platform_data->vref_mv[1];
	}

	err = iio_device_register(data->indio_dev);
	if (err)
		goto exit_free_device;

	dev_info(&client->dev, "DAC registered\n");

	return 0;

exit_free_device:
	iio_free_device(data->indio_dev);
exit_free_data:
	kfree(data);
exit:
	return err;
}

static int __devexit max517_remove(struct i2c_client *client)
{
	struct max517_data *data = i2c_get_clientdata(client);

	iio_free_device(data->indio_dev);
	kfree(data);

	return 0;
}

static const struct i2c_device_id max517_id[] = {
	{ "max517", ID_MAX517 },
	{ "max518", ID_MAX518 },
	{ "max519", ID_MAX519 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max517_id);

static struct i2c_driver max517_driver = {
	.driver = {
		.name	= MAX517_DRV_NAME,
	},
	.probe		= max517_probe,
<<<<<<< HEAD:drivers/staging/iio/dac/max517.c
	.remove		= max517_remove,
	.suspend	= max517_suspend,
	.resume		= max517_resume,
=======
	.remove		=  __devexit_p(max517_remove),
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/max517.c
	.id_table	= max517_id,
};

static int __init max517_init(void)
{
	return i2c_add_driver(&max517_driver);
}

static void __exit max517_exit(void)
{
	i2c_del_driver(&max517_driver);
}

MODULE_AUTHOR("Roland Stigge <stigge@antcom.de>");
MODULE_DESCRIPTION("MAX517/MAX518/MAX519 8-bit DAC");
MODULE_LICENSE("GPL");

module_init(max517_init);
module_exit(max517_exit);

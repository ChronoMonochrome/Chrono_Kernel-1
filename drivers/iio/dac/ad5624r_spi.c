/*
 * AD5624R, AD5644R, AD5664R Digital to analog convertors spi driver
 *
 * Copyright 2010-2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/regulator/consumer.h>

<<<<<<< HEAD:drivers/staging/iio/dac/ad5624r_spi.c
#include "../iio.h"
#include "../sysfs.h"
#include "dac.h"
#include "ad5624r.h"

static const struct ad5624r_chip_info ad5624r_chip_info_tbl[] = {
	[ID_AD5624R3] = {
		.bits = 12,
		.int_vref_mv = 1250,
	},
	[ID_AD5644R3] = {
		.bits = 14,
		.int_vref_mv = 1250,
	},
	[ID_AD5664R3] = {
		.bits = 16,
		.int_vref_mv = 1250,
	},
	[ID_AD5624R5] = {
		.bits = 12,
		.int_vref_mv = 2500,
	},
	[ID_AD5644R5] = {
		.bits = 14,
		.int_vref_mv = 2500,
	},
	[ID_AD5664R5] = {
		.bits = 16,
		.int_vref_mv = 2500,
	},
};
=======
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#include "ad5624r.h"
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/ad5624r_spi.c

static int ad5624r_spi_write(struct spi_device *spi,
			     u8 cmd, u8 addr, u16 val, u8 len)
{
	u32 data;
	u8 msg[3];

	/*
	 * The input shift register is 24 bits wide. The first two bits are
	 * don't care bits. The next three are the command bits, C2 to C0,
	 * followed by the 3-bit DAC address, A2 to A0, and then the
	 * 16-, 14-, 12-bit data-word. The data-word comprises the 16-,
	 * 14-, 12-bit input code followed by 0, 2, or 4 don't care bits,
	 * for the AD5664R, AD5644R, and AD5624R, respectively.
	 */
	data = (0 << 22) | (cmd << 19) | (addr << 16) | (val << (16 - len));
	msg[0] = data >> 16;
	msg[1] = data >> 8;
	msg[2] = data;

	return spi_write(spi, msg, 3);
}

static ssize_t ad5624r_write_dac(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	long readin;
	int ret;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad5624r_state *st = iio_dev_get_devdata(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	ret = strict_strtol(buf, 10, &readin);
	if (ret)
		return ret;

	ret = ad5624r_spi_write(st->us, AD5624R_CMD_WRITE_INPUT_N_UPDATE_N,
				this_attr->address, readin,
				st->chip_info->bits);
	return ret ? ret : len;
}

static const char * const ad5624r_powerdown_modes[] = {
	"1kohm_to_gnd",
	"100kohm_to_gnd",
	"three_state"
};

static int ad5624r_get_powerdown_mode(struct iio_dev *indio_dev,
	const struct iio_chan_spec *chan)
{
<<<<<<< HEAD:drivers/staging/iio/dac/ad5624r_spi.c
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad5624r_state *st = iio_dev_get_devdata(indio_dev);
=======
	struct ad5624r_state *st = iio_priv(indio_dev);
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/ad5624r_spi.c

	return st->pwr_down_mode;
}

static int ad5624r_set_powerdown_mode(struct iio_dev *indio_dev,
	const struct iio_chan_spec *chan, unsigned int mode)
{
<<<<<<< HEAD:drivers/staging/iio/dac/ad5624r_spi.c
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad5624r_state *st = iio_dev_get_devdata(indio_dev);
	int ret;
=======
	struct ad5624r_state *st = iio_priv(indio_dev);
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/ad5624r_spi.c

	st->pwr_down_mode = mode;

	return 0;
}

static const struct iio_enum ad5624r_powerdown_mode_enum = {
	.items = ad5624r_powerdown_modes,
	.num_items = ARRAY_SIZE(ad5624r_powerdown_modes),
	.get = ad5624r_get_powerdown_mode,
	.set = ad5624r_set_powerdown_mode,
};

static ssize_t ad5624r_read_dac_powerdown(struct iio_dev *indio_dev,
	uintptr_t private, const struct iio_chan_spec *chan, char *buf)
{
<<<<<<< HEAD:drivers/staging/iio/dac/ad5624r_spi.c
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad5624r_state *st = iio_dev_get_devdata(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
=======
	struct ad5624r_state *st = iio_priv(indio_dev);
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/ad5624r_spi.c

	return sprintf(buf, "%d\n",
			!!(st->pwr_down_mask & (1 << chan->channel)));
}

static ssize_t ad5624r_write_dac_powerdown(struct iio_dev *indio_dev,
	uintptr_t private, const struct iio_chan_spec *chan, const char *buf,
	size_t len)
{
	bool pwr_down;
	int ret;
<<<<<<< HEAD:drivers/staging/iio/dac/ad5624r_spi.c
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad5624r_state *st = iio_dev_get_devdata(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
=======
	struct ad5624r_state *st = iio_priv(indio_dev);
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/ad5624r_spi.c

	ret = strtobool(buf, &pwr_down);
	if (ret)
		return ret;

	if (pwr_down)
		st->pwr_down_mask |= (1 << chan->channel);
	else
		st->pwr_down_mask &= ~(1 << chan->channel);

	ret = ad5624r_spi_write(st->us, AD5624R_CMD_POWERDOWN_DAC, 0,
				(st->pwr_down_mode << 4) |
				st->pwr_down_mask, 16);

	return ret ? ret : len;
}

<<<<<<< HEAD:drivers/staging/iio/dac/ad5624r_spi.c
static ssize_t ad5624r_show_scale(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad5624r_state *st = iio_dev_get_devdata(indio_dev);
	/* Corresponds to Vref / 2^(bits) */
	unsigned int scale_uv = (st->vref_mv * 1000) >> st->chip_info->bits;

	return sprintf(buf, "%d.%03d\n", scale_uv / 1000, scale_uv % 1000);
}
static IIO_DEVICE_ATTR(out_scale, S_IRUGO, ad5624r_show_scale, NULL, 0);

static IIO_DEV_ATTR_OUT_RAW(0, ad5624r_write_dac, AD5624R_ADDR_DAC0);
static IIO_DEV_ATTR_OUT_RAW(1, ad5624r_write_dac, AD5624R_ADDR_DAC1);
static IIO_DEV_ATTR_OUT_RAW(2, ad5624r_write_dac, AD5624R_ADDR_DAC2);
static IIO_DEV_ATTR_OUT_RAW(3, ad5624r_write_dac, AD5624R_ADDR_DAC3);

static IIO_DEVICE_ATTR(out_powerdown_mode, S_IRUGO |
			S_IWUSR, ad5624r_read_powerdown_mode,
			ad5624r_write_powerdown_mode, 0);

static IIO_CONST_ATTR(out_powerdown_mode_available,
			"1kohm_to_gnd 100kohm_to_gnd three_state");

#define IIO_DEV_ATTR_DAC_POWERDOWN(_num, _show, _store, _addr)		\
	IIO_DEVICE_ATTR(out##_num##_powerdown,				\
			S_IRUGO | S_IWUSR, _show, _store, _addr)

static IIO_DEV_ATTR_DAC_POWERDOWN(0, ad5624r_read_dac_powerdown,
				   ad5624r_write_dac_powerdown, 0);
static IIO_DEV_ATTR_DAC_POWERDOWN(1, ad5624r_read_dac_powerdown,
				   ad5624r_write_dac_powerdown, 1);
static IIO_DEV_ATTR_DAC_POWERDOWN(2, ad5624r_read_dac_powerdown,
				   ad5624r_write_dac_powerdown, 2);
static IIO_DEV_ATTR_DAC_POWERDOWN(3, ad5624r_read_dac_powerdown,
				   ad5624r_write_dac_powerdown, 3);

static struct attribute *ad5624r_attributes[] = {
	&iio_dev_attr_out0_raw.dev_attr.attr,
	&iio_dev_attr_out1_raw.dev_attr.attr,
	&iio_dev_attr_out2_raw.dev_attr.attr,
	&iio_dev_attr_out3_raw.dev_attr.attr,
	&iio_dev_attr_out0_powerdown.dev_attr.attr,
	&iio_dev_attr_out1_powerdown.dev_attr.attr,
	&iio_dev_attr_out2_powerdown.dev_attr.attr,
	&iio_dev_attr_out3_powerdown.dev_attr.attr,
	&iio_dev_attr_out_powerdown_mode.dev_attr.attr,
	&iio_const_attr_out_powerdown_mode_available.dev_attr.attr,
	&iio_dev_attr_out_scale.dev_attr.attr,
	NULL,
};

static const struct attribute_group ad5624r_attribute_group = {
	.attrs = ad5624r_attributes,
};

static const struct iio_info ad5624r_info = {
	.attrs = &ad5624r_attribute_group,
=======
static const struct iio_info ad5624r_info = {
	.write_raw = ad5624r_write_raw,
	.read_raw = ad5624r_read_raw,
>>>>>>> 419e9266884fa853179ab726c27a63a9d3ae46e3:drivers/iio/dac/ad5624r_spi.c
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec_ext_info ad5624r_ext_info[] = {
	{
		.name = "powerdown",
		.read = ad5624r_read_dac_powerdown,
		.write = ad5624r_write_dac_powerdown,
	},
	IIO_ENUM("powerdown_mode", true, &ad5624r_powerdown_mode_enum),
	IIO_ENUM_AVAILABLE("powerdown_mode", &ad5624r_powerdown_mode_enum),
	{ },
};

#define AD5624R_CHANNEL(_chan, _bits) { \
	.type = IIO_VOLTAGE, \
	.indexed = 1, \
	.output = 1, \
	.channel = (_chan), \
	.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT | \
		     IIO_CHAN_INFO_SCALE_SHARED_BIT, \
	.address = (_chan), \
	.scan_type = IIO_ST('u', (_bits), 16, 16 - (_bits)), \
	.ext_info = ad5624r_ext_info, \
}

#define DECLARE_AD5624R_CHANNELS(_name, _bits) \
	const struct iio_chan_spec _name##_channels[] = { \
		AD5624R_CHANNEL(0, _bits), \
		AD5624R_CHANNEL(1, _bits), \
		AD5624R_CHANNEL(2, _bits), \
		AD5624R_CHANNEL(3, _bits), \
}

static DECLARE_AD5624R_CHANNELS(ad5624r, 12);
static DECLARE_AD5624R_CHANNELS(ad5644r, 14);
static DECLARE_AD5624R_CHANNELS(ad5664r, 16);

static const struct ad5624r_chip_info ad5624r_chip_info_tbl[] = {
	[ID_AD5624R3] = {
		.channels = ad5624r_channels,
		.int_vref_mv = 1250,
	},
	[ID_AD5624R5] = {
		.channels = ad5624r_channels,
		.int_vref_mv = 2500,
	},
	[ID_AD5644R3] = {
		.channels = ad5644r_channels,
		.int_vref_mv = 1250,
	},
	[ID_AD5644R5] = {
		.channels = ad5644r_channels,
		.int_vref_mv = 2500,
	},
	[ID_AD5664R3] = {
		.channels = ad5664r_channels,
		.int_vref_mv = 1250,
	},
	[ID_AD5664R5] = {
		.channels = ad5664r_channels,
		.int_vref_mv = 2500,
	},
};

static int __devinit ad5624r_probe(struct spi_device *spi)
{
	struct ad5624r_state *st;
	int ret, voltage_uv = 0;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (st == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	spi_set_drvdata(spi, st);

	st->reg = regulator_get(&spi->dev, "vcc");
	if (!IS_ERR(st->reg)) {
		ret = regulator_enable(st->reg);
		if (ret)
			goto error_put_reg;

		voltage_uv = regulator_get_voltage(st->reg);
	}

	st->chip_info =
		&ad5624r_chip_info_tbl[spi_get_device_id(spi)->driver_data];

	if (voltage_uv)
		st->vref_mv = voltage_uv / 1000;
	else
		st->vref_mv = st->chip_info->int_vref_mv;

	st->us = spi;
	st->indio_dev = iio_allocate_device(0);
	if (st->indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_disable_reg;
	}
	st->indio_dev->dev.parent = &spi->dev;
	st->indio_dev->name = spi_get_device_id(spi)->name;
	st->indio_dev->info = &ad5624r_info;
	st->indio_dev->dev_data = (void *)(st);
	st->indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_device_register(st->indio_dev);
	if (ret)
		goto error_free_dev;

	ret = ad5624r_spi_write(spi, AD5624R_CMD_INTERNAL_REFER_SETUP, 0,
				!!voltage_uv, 16);
	if (ret)
		goto error_free_dev;

	return 0;

error_free_dev:
	iio_free_device(st->indio_dev);
error_disable_reg:
	if (!IS_ERR(st->reg))
		regulator_disable(st->reg);
error_put_reg:
	if (!IS_ERR(st->reg))
		regulator_put(st->reg);

	kfree(st);
error_ret:
	return ret;
}

static int __devexit ad5624r_remove(struct spi_device *spi)
{
	struct ad5624r_state *st = spi_get_drvdata(spi);

	iio_device_unregister(st->indio_dev);

	if (!IS_ERR(st->reg)) {
		regulator_disable(st->reg);
		regulator_put(st->reg);
	}

	kfree(st);

	return 0;
}

static const struct spi_device_id ad5624r_id[] = {
	{"ad5624r3", ID_AD5624R3},
	{"ad5644r3", ID_AD5644R3},
	{"ad5664r3", ID_AD5664R3},
	{"ad5624r5", ID_AD5624R5},
	{"ad5644r5", ID_AD5644R5},
	{"ad5664r5", ID_AD5664R5},
	{}
};

static struct spi_driver ad5624r_driver = {
	.driver = {
		   .name = "ad5624r",
		   .owner = THIS_MODULE,
		   },
	.probe = ad5624r_probe,
	.remove = __devexit_p(ad5624r_remove),
	.id_table = ad5624r_id,
};

static __init int ad5624r_spi_init(void)
{
	return spi_register_driver(&ad5624r_driver);
}
module_init(ad5624r_spi_init);

static __exit void ad5624r_spi_exit(void)
{
	spi_unregister_driver(&ad5624r_driver);
}
module_exit(ad5624r_spi_exit);

MODULE_AUTHOR("Barry Song <21cnbao@gmail.com>");
MODULE_DESCRIPTION("Analog Devices AD5624/44/64R DAC spi driver");
MODULE_LICENSE("GPL v2");

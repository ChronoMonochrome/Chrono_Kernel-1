/*
 * AD7298 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/delay.h>
<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c

#include "../iio.h"
#include "../sysfs.h"
#include "../ring_generic.h"
#include "adc.h"

#include "ad7298.h"

static struct iio_chan_spec ad7298_channels[] = {
	IIO_CHAN(IIO_TEMP, 0, 1, 0, NULL, 0, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SEPARATE),
		 9, AD7298_CH_TEMP, IIO_ST('s', 32, 32, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 0, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 0, 0, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 1, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 1, 1, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 2, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 2, 2, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 3, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 3, 3, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 4, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 4, 4, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 5, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 5, 5, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 6, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 6, 6, IIO_ST('u', 12, 16, 0), 0),
	IIO_CHAN(IIO_IN, 0, 1, 0, NULL, 7, 0,
		 (1 << IIO_CHAN_INFO_SCALE_SHARED),
		 7, 7, IIO_ST('u', 12, 16, 0), 0),
=======
#include <linux/module.h>
#include <linux/interrupt.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include <linux/platform_data/ad7298.h>

#define AD7298_WRITE	(1 << 15) /* write to the control register */
#define AD7298_REPEAT	(1 << 14) /* repeated conversion enable */
#define AD7298_CH(x)	(1 << (13 - (x))) /* channel select */
#define AD7298_TSENSE	(1 << 5) /* temperature conversion enable */
#define AD7298_EXTREF	(1 << 2) /* external reference enable */
#define AD7298_TAVG	(1 << 1) /* temperature sensor averaging enable */
#define AD7298_PDD	(1 << 0) /* partial power down enable */

#define AD7298_MAX_CHAN		8
#define AD7298_BITS		12
#define AD7298_STORAGE_BITS	16
#define AD7298_INTREF_mV	2500

#define AD7298_CH_TEMP		9

#define RES_MASK(bits)	((1 << (bits)) - 1)

struct ad7298_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	unsigned			ext_ref;
	struct spi_transfer		ring_xfer[10];
	struct spi_transfer		scan_single_xfer[3];
	struct spi_message		ring_msg;
	struct spi_message		scan_single_msg;
	/*
	 * DMA (thus cache coherency maintenance) requires the
	 * transfer buffers to live in their own cache lines.
	 */
	__be16				rx_buf[12] ____cacheline_aligned;
	__be16				tx_buf[2];
};

#define AD7298_V_CHAN(index)						\
	{								\
		.type = IIO_VOLTAGE,					\
		.indexed = 1,						\
		.channel = index,					\
		.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT |		\
		IIO_CHAN_INFO_SCALE_SHARED_BIT,				\
		.address = index,					\
		.scan_index = index,					\
		.scan_type = {						\
			.sign = 'u',					\
			.realbits = 12,					\
			.storagebits = 16,				\
			.endianness = IIO_BE,				\
		},							\
	}

static const struct iio_chan_spec ad7298_channels[] = {
	{
		.type = IIO_TEMP,
		.indexed = 1,
		.channel = 0,
		.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT |
			IIO_CHAN_INFO_SCALE_SEPARATE_BIT |
			IIO_CHAN_INFO_OFFSET_SEPARATE_BIT,
		.address = AD7298_CH_TEMP,
		.scan_index = -1,
		.scan_type = {
			.sign = 's',
			.realbits = 32,
			.storagebits = 32,
		},
	},
	AD7298_V_CHAN(0),
	AD7298_V_CHAN(1),
	AD7298_V_CHAN(2),
	AD7298_V_CHAN(3),
	AD7298_V_CHAN(4),
	AD7298_V_CHAN(5),
	AD7298_V_CHAN(6),
	AD7298_V_CHAN(7),
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

/**
 * ad7298_update_scan_mode() setup the spi transfer buffer for the new scan mask
 **/
static int ad7298_update_scan_mode(struct iio_dev *indio_dev,
	const unsigned long *active_scan_mask)
{
	struct ad7298_state *st = iio_priv(indio_dev);
	int i, m;
	unsigned short command;
	int scan_count;

	/* Now compute overall size */
	scan_count = bitmap_weight(active_scan_mask, indio_dev->masklength);

	command = AD7298_WRITE | st->ext_ref;

	for (i = 0, m = AD7298_CH(0); i < AD7298_MAX_CHAN; i++, m >>= 1)
		if (test_bit(i, active_scan_mask))
			command |= m;

	st->tx_buf[0] = cpu_to_be16(command);

	/* build spi ring message */
	st->ring_xfer[0].tx_buf = &st->tx_buf[0];
	st->ring_xfer[0].len = 2;
	st->ring_xfer[0].cs_change = 1;
	st->ring_xfer[1].tx_buf = &st->tx_buf[1];
	st->ring_xfer[1].len = 2;
	st->ring_xfer[1].cs_change = 1;

	spi_message_init(&st->ring_msg);
	spi_message_add_tail(&st->ring_xfer[0], &st->ring_msg);
	spi_message_add_tail(&st->ring_xfer[1], &st->ring_msg);

	for (i = 0; i < scan_count; i++) {
		st->ring_xfer[i + 2].rx_buf = &st->rx_buf[i];
		st->ring_xfer[i + 2].len = 2;
		st->ring_xfer[i + 2].cs_change = 1;
		spi_message_add_tail(&st->ring_xfer[i + 2], &st->ring_msg);
	}
	/* make sure last transfer cs_change is not set */
	st->ring_xfer[i + 1].cs_change = 0;

	return 0;
}

/**
 * ad7298_trigger_handler() bh of trigger launched polling to ring buffer
 *
 * Currently there is no option in this driver to disable the saving of
 * timestamps within the ring.
 **/
static irqreturn_t ad7298_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct ad7298_state *st = iio_priv(indio_dev);
	s64 time_ns = 0;
	int b_sent;

	b_sent = spi_sync(st->spi, &st->ring_msg);
	if (b_sent)
		goto done;

	if (indio_dev->scan_timestamp) {
		time_ns = iio_get_time_ns();
		memcpy((u8 *)st->rx_buf + indio_dev->scan_bytes - sizeof(s64),
			&time_ns, sizeof(time_ns));
	}

	iio_push_to_buffers(indio_dev, (u8 *)st->rx_buf);

done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int ad7298_scan_direct(struct ad7298_state *st, unsigned ch)
{
	int ret;
	st->tx_buf[0] = cpu_to_be16(AD7298_WRITE | st->ext_ref |
				   (AD7298_CH(0) >> ch));

	ret = spi_sync(st->spi, &st->scan_single_msg);
	if (ret)
		return ret;

	return be16_to_cpu(st->rx_buf[0]);
}

static int ad7298_scan_temp(struct ad7298_state *st, int *val)
{
<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c
	int tmp, ret;
=======
	int ret;
	__be16 buf;
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c

	tmp = cpu_to_be16(AD7298_WRITE | AD7298_TSENSE |
			  AD7298_TAVG | st->ext_ref);

	ret = spi_write(st->spi, (u8 *)&tmp, 2);
	if (ret)
		return ret;

	tmp = 0;

	ret = spi_write(st->spi, (u8 *)&tmp, 2);
	if (ret)
		return ret;

	usleep_range(101, 1000); /* sleep > 100us */

	ret = spi_read(st->spi, (u8 *)&tmp, 2);
	if (ret)
		return ret;

<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c
	tmp = be16_to_cpu(tmp) & RES_MASK(AD7298_BITS);
=======
	*val = sign_extend32(be16_to_cpu(buf), 11);
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c

	return 0;
}

static int ad7298_get_ref_voltage(struct ad7298_state *st)
{
	int vref;

	if (st->ext_ref) {
		vref = regulator_get_voltage(st->reg);
		if (vref < 0)
			return vref;

		return vref / 1000;
	} else {
		return AD7298_INTREF_mV;
	}
}

static int ad7298_read_raw(struct iio_dev *dev_info,
			   struct iio_chan_spec const *chan,
			   int *val,
			   int *val2,
			   long m)
{
	int ret;
<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c
	struct ad7298_state *st = iio_priv(dev_info);
	unsigned int scale_uv;
=======
	struct ad7298_state *st = iio_priv(indio_dev);
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c

	switch (m) {
	case 0:
		mutex_lock(&dev_info->mlock);
		if (iio_ring_enabled(dev_info)) {
			if (chan->address == AD7298_CH_TEMP)
				ret = -ENODEV;
			else
				ret = ad7298_scan_from_ring(dev_info,
							    chan->address);
		} else {
			if (chan->address == AD7298_CH_TEMP)
				ret = ad7298_scan_temp(st, val);
			else
				ret = ad7298_scan_direct(st, chan->address);
		}
		mutex_unlock(&dev_info->mlock);

		if (ret < 0)
			return ret;

		if (chan->address != AD7298_CH_TEMP)
			*val = ret & RES_MASK(AD7298_BITS);

		return IIO_VAL_INT;
<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c
	case (1 << IIO_CHAN_INFO_SCALE_SHARED):
		scale_uv = (st->int_vref_mv * 1000) >> AD7298_BITS;
		*val =  scale_uv / 1000;
		*val2 = (scale_uv % 1000) * 1000;
		return IIO_VAL_INT_PLUS_MICRO;
	case (1 << IIO_CHAN_INFO_SCALE_SEPARATE):
		*val =  1;
		*val2 = 0;
		return IIO_VAL_INT_PLUS_MICRO;
=======
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_VOLTAGE:
			*val = ad7298_get_ref_voltage(st);
			*val2 = chan->scan_type.realbits;
			return IIO_VAL_FRACTIONAL_LOG2;
		case IIO_TEMP:
			*val = ad7298_get_ref_voltage(st);
			*val2 = 10;
			return IIO_VAL_FRACTIONAL;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		*val = 1093 - 2732500 / ad7298_get_ref_voltage(st);
		return IIO_VAL_INT;
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c
	}
	return -EINVAL;
}

static const struct iio_info ad7298_info = {
	.read_raw = &ad7298_read_raw,
	.driver_module = THIS_MODULE,
};

static int ad7298_probe(struct spi_device *spi)
{
	struct ad7298_platform_data *pdata = spi->dev.platform_data;
	struct ad7298_state *st;
<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c
	int ret, regdone = 0;
	struct iio_dev *indio_dev = iio_allocate_device(sizeof(*st));
=======
	struct iio_dev *indio_dev = iio_device_alloc(sizeof(*st));
	int ret;
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c

	if (indio_dev == NULL)
		return -ENOMEM;

	st = iio_priv(indio_dev);

	if (pdata && pdata->ext_ref)
		st->ext_ref = AD7298_EXTREF;

	if (st->ext_ref) {
		st->reg = regulator_get(&spi->dev, "vref");
		if (IS_ERR(st->reg)) {
			ret = PTR_ERR(st->reg);
			goto error_free;
		}
		ret = regulator_enable(st->reg);
		if (ret)
			goto error_put_reg;
	}

	spi_set_drvdata(spi, indio_dev);

	st->spi = spi;

	indio_dev->name = spi_get_device_id(spi)->name;
	indio_dev->dev.parent = &spi->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = ad7298_channels;
	indio_dev->num_channels = ARRAY_SIZE(ad7298_channels);
	indio_dev->info = &ad7298_info;

	/* Setup default message */

	st->scan_single_xfer[0].tx_buf = &st->tx_buf[0];
	st->scan_single_xfer[0].len = 2;
	st->scan_single_xfer[0].cs_change = 1;
	st->scan_single_xfer[1].tx_buf = &st->tx_buf[1];
	st->scan_single_xfer[1].len = 2;
	st->scan_single_xfer[1].cs_change = 1;
	st->scan_single_xfer[2].rx_buf = &st->rx_buf[0];
	st->scan_single_xfer[2].len = 2;

	spi_message_init(&st->scan_single_msg);
	spi_message_add_tail(&st->scan_single_xfer[0], &st->scan_single_msg);
	spi_message_add_tail(&st->scan_single_xfer[1], &st->scan_single_msg);
	spi_message_add_tail(&st->scan_single_xfer[2], &st->scan_single_msg);

	ret = iio_triggered_buffer_setup(indio_dev, NULL,
			&ad7298_trigger_handler, NULL);
	if (ret)
		goto error_disable_reg;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_disable_reg;
	regdone = 1;

	ret = iio_ring_buffer_register_ex(indio_dev->ring, 0,
					  &ad7298_channels[1], /* skip temp0 */
					  ARRAY_SIZE(ad7298_channels) - 1);
	if (ret)
		goto error_cleanup_ring;

	return 0;

error_cleanup_ring:
	iio_triggered_buffer_cleanup(indio_dev);
error_disable_reg:
	if (st->ext_ref)
		regulator_disable(st->reg);
error_put_reg:
	if (st->ext_ref)
		regulator_put(st->reg);
<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c

	if (regdone)
		iio_device_unregister(indio_dev);
	else
		iio_free_device(indio_dev);
=======
error_free:
	iio_device_free(indio_dev);
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c

	return ret;
}

static int ad7298_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct ad7298_state *st = iio_priv(indio_dev);

<<<<<<< HEAD:drivers/staging/iio/adc/ad7298_core.c
	iio_ring_buffer_unregister(indio_dev->ring);
	ad7298_ring_cleanup(indio_dev);
	iio_device_unregister(indio_dev);
	if (!IS_ERR(st->reg)) {
=======
	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	if (st->ext_ref) {
>>>>>>> 19f949f:drivers/iio/adc/ad7298.c
		regulator_disable(st->reg);
		regulator_put(st->reg);
	}
	iio_device_unregister(indio_dev);

	return 0;
}

static const struct spi_device_id ad7298_id[] = {
	{"ad7298", 0},
	{}
};

static struct spi_driver ad7298_driver = {
	.driver = {
		.name	= "ad7298",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= ad7298_probe,
	.remove		= ad7298_remove,
	.id_table	= ad7298_id,
};

static int __init ad7298_init(void)
{
	return spi_register_driver(&ad7298_driver);
}
module_init(ad7298_init);

static void __exit ad7298_exit(void)
{
	spi_unregister_driver(&ad7298_driver);
}
module_exit(ad7298_exit);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("Analog Devices AD7298 ADC");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:ad7298");

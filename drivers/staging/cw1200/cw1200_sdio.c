/*
 * Mac80211 SDIO driver for ST-Ericsson CW1200 device
 *
 * Copyright (c) 2010, ST-Ericsson
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/spinlock.h>

#include <net/mac80211.h>

#include "cw1200.h"
#include "sbus.h"

MODULE_AUTHOR("Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>");
MODULE_DESCRIPTION("mac80211 ST-Ericsson CW1200 SDIO driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cw1200_wlan");

/* Module parameter for MMC interface to probe the driver on. */
static char device_name[10] = "mmc3";
module_param_string(device, device_name, sizeof(device_name), S_IRUGO);
MODULE_PARM_DESC(device, "SDIO interface device is connected to");

#define CW1200_GPIO_LOW		(0)
#define CW1200_GPIO_HIGH	(1)

struct sbus_priv {
	struct sdio_func	*func;
	struct cw1200_common	*core;
	spinlock_t		lock;
	sbus_irq_handler	irq_handler;
	void			*irq_priv;
};

static const struct sdio_device_id if_sdio_ids[] = {
	{ SDIO_DEVICE(SDIO_ANY_ID, SDIO_ANY_ID) },
	{ /* end: all zeroes */			},
};

/* sbus_ops implemetation */

static int cw1200_sdio_memcpy_fromio(struct sbus_priv *self,
				     unsigned int addr,
				     void *dst, int count)
{
	int ret = sdio_memcpy_fromio(self->func, dst, addr, count);
	if (ret) {
		printk(KERN_ERR "!!! Can't read %d bytes from 0x%.8X. Err %d.\n",
				count, addr, ret);
	}
	return ret;
}

static int cw1200_sdio_memcpy_toio(struct sbus_priv *self,
				   unsigned int addr,
				   const void *src, int count)
{
	return sdio_memcpy_toio(self->func, addr, (void *)src, count);
}

static void cw1200_sdio_lock(struct sbus_priv *self)
{
	sdio_claim_host(self->func);
}

static void cw1200_sdio_unlock(struct sbus_priv *self)
{
	sdio_release_host(self->func);
}

static void cw1200_sdio_irq_handler(struct sdio_func *func)
{
	struct sbus_priv *self = sdio_get_drvdata(func);
	unsigned long flags;

	BUG_ON(!self);
	spin_lock_irqsave(&self->lock, flags);
	if (self->irq_handler)
		self->irq_handler(self->irq_priv);
	spin_unlock_irqrestore(&self->lock, flags);
}

static int cw1200_sdio_irq_subscribe(struct sbus_priv *self,
				     sbus_irq_handler handler,
				     void *priv)
{
	int ret;
	unsigned long flags;

	if (!handler)
		return -EINVAL;

	spin_lock_irqsave(&self->lock, flags);
	self->irq_priv = priv;
	self->irq_handler = handler;
	spin_unlock_irqrestore(&self->lock, flags);

	printk(KERN_DEBUG "SW IRQ subscribe\n");
	sdio_claim_host(self->func);
	ret = sdio_claim_irq(self->func, cw1200_sdio_irq_handler);
	sdio_release_host(self->func);
	return ret;
}

static int cw1200_sdio_irq_unsubscribe(struct sbus_priv *self)
{
	int ret;
	unsigned long flags;

	WARN_ON(!self->irq_handler);
	if (!self->irq_handler)
		return 0;

	printk(KERN_DEBUG "SW IRQ unsubscribe\n");
	sdio_claim_host(self->func);
	ret = sdio_release_irq(self->func);
	sdio_release_host(self->func);

	spin_lock_irqsave(&self->lock, flags);
	self->irq_priv = NULL;
	self->irq_handler = NULL;
	spin_unlock_irqrestore(&self->lock, flags);

	return ret;
}

#ifdef CW1200_U8500_PLATFORM
static int cw1200_detect_card(void)
{
	/* HACK!!!
	 * Rely on mmc->class_dev.class set in mmc_alloc_host
	 * Tricky part: a new mmc hook is being (temporary) created
	 * to discover mmc_host class.
	 * Do you know more elegant way how to enumerate mmc_hosts?
	 */

	struct mmc_host *mmc = NULL;
	struct class_dev_iter iter;
	struct device *dev;

	mmc = mmc_alloc_host(0, NULL);
	if (!mmc)
		return -ENOMEM;

	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev) {
			printk(KERN_ERR "CW1200: %s is not found.\n",
				device_name);
			break;
		} else {
			struct mmc_host *host = container_of(dev,
				struct mmc_host, class_dev);

			if (dev_name(&host->class_dev) &&
				strcmp(dev_name(&host->class_dev),
					device_name))
				continue;

			mmc_detect_change(host, 10);
			break;
		}
	}
	mmc_free_host(mmc);
	return 0;
}

static int cw1200_sdio_off(void)
{
	gpio_set_value(215, CW1200_GPIO_LOW);
	cw1200_detect_card();
	gpio_free(215);
	return 0;
}

static int cw1200_sdio_on(void)
{
	gpio_request(215, "cw1200_sdio");
	gpio_direction_output(215, 1);
	gpio_set_value(215, CW1200_GPIO_HIGH);
	cw1200_detect_card();
	return 0;
}

static int cw1200_sdio_reset(struct sbus_priv *self)
{
	cw1200_sdio_off();
	mdelay(1000);
	cw1200_sdio_on();
	return 0;
}

#else  /* CW1200_U8500_PLATFORM */

static int cw1200_sdio_off(void)
{
	return 0;
}

static int cw1200_sdio_on(void)
{
	return 0;
}

static int cw1200_sdio_reset(struct sbus_priv *self)
{
	return 0;
}

#endif /* CW1200_U8500_PLATFORM */

static size_t cw1200_align_size(struct sbus_priv *self, size_t size)
{
	return sdio_align_size(self->func, size);
}

static struct sbus_ops cw1200_sdio_sbus_ops = {
	.sbus_memcpy_fromio	= cw1200_sdio_memcpy_fromio,
	.sbus_memcpy_toio	= cw1200_sdio_memcpy_toio,
	.lock			= cw1200_sdio_lock,
	.unlock			= cw1200_sdio_unlock,
	.irq_subscribe		= cw1200_sdio_irq_subscribe,
	.irq_unsubscribe	= cw1200_sdio_irq_unsubscribe,
	.reset			= cw1200_sdio_reset,
	.align_size		= cw1200_align_size,
};

/* Probe Function to be called by SDIO stack when device is discovered */
static int cw1200_sdio_probe(struct sdio_func *func,
			      const struct sdio_device_id *id)
{
	struct sbus_priv *self;
	int status;
	cw1200_dbg(CW1200_DBG_INIT, "Probe called\n");

	self = kzalloc(sizeof(*self), GFP_KERNEL);
	if (!self) {
		cw1200_dbg(CW1200_DBG_ERROR, "Can't allocate SDIO sbus_priv.");
		return -ENOMEM;
	}

	spin_lock_init(&self->lock);
	self->func = func;
	sdio_set_drvdata(func, self);
	sdio_claim_host(func);
	sdio_enable_func(func);
	sdio_release_host(func);

	status = cw1200_probe(&cw1200_sdio_sbus_ops,
			      self, &func->dev, &self->core);
	if (status) {
		sdio_claim_host(func);
		sdio_disable_func(func);
		sdio_release_host(func);
		sdio_set_drvdata(func, NULL);
		kfree(self);
	}

	return status;
}

/* Disconnect Function to be called by SDIO stack when device is disconnected */
static void cw1200_sdio_disconnect(struct sdio_func *func)
{
	struct sbus_priv *self = sdio_get_drvdata(func);

	if (self) {
		if (self->core) {
			cw1200_release(self->core);
			self->core = NULL;
		}
		sdio_claim_host(func);
		sdio_disable_func(func);
		sdio_release_host(func);
		sdio_set_drvdata(func, NULL);
		kfree(self);
	}
}

static struct sdio_driver sdio_driver = {
	.name		= "cw1200_wlan",
	.id_table	= if_sdio_ids,
	.probe		= cw1200_sdio_probe,
	.remove		= cw1200_sdio_disconnect,
};

/* Init Module function -> Called by insmod */
static int __init cw1200_sdio_init(void)
{
	int ret = sdio_register_driver(&sdio_driver);
	if (ret)
		return ret;
	ret = cw1200_sdio_on();
	if (ret)
		sdio_unregister_driver(&sdio_driver);
	return ret;
}

/* Called at Driver Unloading */
static void __exit cw1200_sdio_exit(void)
{
	sdio_unregister_driver(&sdio_driver);
	cw1200_sdio_off();
}


module_init(cw1200_sdio_init);
module_exit(cw1200_sdio_exit);

/*----------------------------------------------------------------------------*/
/*  copyright STMicroelectronics, 2007.                                       */
/*                                                                            */
/* This program is free software; you can redistribute it and/or modify it    */
/* under the terms of the GNU General Public License as published by the Free */
/* Software Foundation; either version 2.1 of the License, or (at your option)*/
/* any later version.                                                         */
/*                                                                            */
/* This program is distributed in the hope that it will be useful, but        */
/* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY */
/* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License    */
/* for more details.                                                          */
/*                                                                            */
/* You should have received a copy of the GNU General Public License          */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*----------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/i2s/i2s.h>
#include <linux/platform_device.h>

/*******************************************************************************/
static DEFINE_MUTEX(core_lock);

static void i2sdev_release(struct device *dev)
{
	struct i2s_device *i2s = to_i2s_device(dev);

	if (i2s->controller)
		put_device(&(i2s->controller->dev));
	kfree(dev);
}
static ssize_t
modalias_show(struct device *dev, struct device_attribute *a, char *buf)
{
	const struct i2s_device *i2s = to_i2s_device(dev);
	return sprintf(buf, "%s\n", i2s->modalias);
}

static struct device_attribute i2s_dev_attrs[] = {
	__ATTR_RO(modalias),
	__ATTR_NULL,
};

/* modalias support makes "modprobe $MODALIAS" new-style hotplug work,
 * and the sysfs version makes coldplug work too.
 */
static const struct i2s_device_id *i2s_match_id(const struct i2s_device_id *id,
						const struct i2s_device *device)
{
	while (id->name[0]) {
		if (strcmp(device->modalias, id->name) == 0)
			return id;
		id++;
	}
	return NULL;
}

static int i2s_match_device(struct device *dev, struct device_driver *drv)
{
	const struct i2s_device *device = to_i2s_device(dev);
	struct i2s_driver *driver = to_i2s_driver(drv);
	if (driver->id_table)
		return i2s_match_id(driver->id_table, device) != NULL;
	return 0;
}

static int i2s_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	const struct i2s_device *i2s = to_i2s_device(dev);

	add_uevent_var(env, "MODALIAS=%s", i2s->modalias);
	return 0;
}

#ifdef	CONFIG_PM
static int i2s_suspend(struct device *dev, pm_message_t message)
{
	int value = 0;
	struct i2s_driver *drv = to_i2s_driver(dev->driver);

	/* suspend will stop irqs and dma; no more i/o */
	if (drv) {
		if (drv->suspend)
			value = drv->suspend(to_i2s_device(dev), message);
		else
			dev_dbg(dev, "... can't suspend\n");
	}
	return value;
}

static int i2s_resume(struct device *dev)
{
	int value = 0;
	struct i2s_driver *drv = to_i2s_driver(dev->driver);

	/* resume may restart the i/o queue */
	if (drv) {
		if (drv->resume)
			value = drv->resume(to_i2s_device(dev));
		else
			dev_dbg(dev, "... can't resume\n");
	}
	return value;
}

#else
#define i2s_suspend	NULL
#define i2s_resume	NULL
#endif

/*This bus is designed to handle various protocols supported by the MSP- ARM Primecell IP
 * such as
 * I2s, PCM, AC97, TDM .... (refer to the data sheet for the complete list.
 * Current MSP driver has the above ones coded.
 * */
struct bus_type i2s_bus_type = {
	.name = "i2s",
	.dev_attrs = i2s_dev_attrs,
	.match = i2s_match_device,
	.uevent = i2s_uevent,
	.suspend = i2s_suspend,
	.resume = i2s_resume,
};

EXPORT_SYMBOL_GPL(i2s_bus_type);

static int i2s_drv_probe(struct device *dev)
{
	const struct i2s_driver *sdrv = to_i2s_driver(dev->driver);

	return sdrv->probe(to_i2s_device(dev));
}

static int i2s_drv_remove(struct device *dev)
{
	const struct i2s_driver *sdrv = to_i2s_driver(dev->driver);

	return sdrv->remove(to_i2s_device(dev));
}

static void i2s_drv_shutdown(struct device *dev)
{
	const struct i2s_driver *sdrv = to_i2s_driver(dev->driver);

	sdrv->shutdown(to_i2s_device(dev));
}

/**
 * i2s_register_driver - register a I2S driver
 * @sdrv: the driver to register
 * Context: can sleep
 */
int i2s_register_driver(struct i2s_driver *sdrv)
{
	sdrv->driver.bus = &i2s_bus_type;
	if (sdrv->probe)
		sdrv->driver.probe = i2s_drv_probe;
	if (sdrv->remove)
		sdrv->driver.remove = i2s_drv_remove;
	if (sdrv->shutdown)
		sdrv->driver.shutdown = i2s_drv_shutdown;
	return driver_register(&sdrv->driver);
}

EXPORT_SYMBOL_GPL(i2s_register_driver);

/******************************************************************************/
struct board_i2s_combined_info {
	struct i2s_board_info board_info;
	struct i2s_device *i2s_dev_p;
};
struct boardinfo {
	struct list_head list;
	unsigned n_board_info;
	struct board_i2s_combined_info board_i2s_info[0];
};

static LIST_HEAD(board_list);
static DEFINE_MUTEX(board_lock);

/*
 * Get an i2s device. Used in MSP LTP tests.
 */
struct i2s_device *i2s_get_device_from_boardinfo(int chip_select)
{
	struct boardinfo *bi;
	struct i2s_device *i2s_dev_p = NULL;

	mutex_lock(&board_lock);
	list_for_each_entry(bi, &board_list, list) {
		struct board_i2s_combined_info *chip = bi->board_i2s_info;
		unsigned n;

		for (n = bi->n_board_info; n > 0; n--, chip++)
			if (chip->board_info.chip_select == chip_select) {
				i2s_dev_p = chip->i2s_dev_p;
				break;
			}
		if (i2s_dev_p != NULL)
			break;
	}
	mutex_unlock(&board_lock);

	return i2s_dev_p;
}

EXPORT_SYMBOL_GPL(i2s_get_device_from_boardinfo);

/* I2S devices should normally not be created by I2S device drivers; that
 * would make them board-specific.  Similarly with I2S master drivers.
 * Device registration normally goes into like arch/.../mach.../board-YYY.c
 * with other readonly (flashable) information about mainboard devices.
 */
struct i2s_device *i2s_alloc_device(struct device *device)
{
	struct i2s_device *i2s;
	struct device *dev = device->parent;

	get_device(device);
	i2s = kzalloc(sizeof *i2s, GFP_KERNEL);
	if (!i2s) {
		dev_err(dev, "cannot alloc i2s_device\n");
		return NULL;
	}

	i2s->dev.parent = dev;
	i2s->dev.bus = &i2s_bus_type;
	i2s->dev.release = i2sdev_release;
	device_initialize(&i2s->dev);
	return i2s;
}

EXPORT_SYMBOL_GPL(i2s_alloc_device);

/**
 * i2s_add_device - Add i2s_device allocated with i2s_alloc_device
 * @i2s: i2s_device to register
 *
 * Companion function to i2s_alloc_device.  Devices allocated with
 * i2s_alloc_device can be added onto the i2s bus with this function.
 *
 * Returns 0 on success; negative errno on failure
 */
int i2s_add_device(struct i2s_device *i2s)
{
	static DEFINE_MUTEX(i2s_add_lock);
	struct device *dev = i2s->dev.parent;
	int status;

	dev_set_name(&i2s->dev, "%s.%u", "i2s", i2s->chip_select);

	mutex_lock(&i2s_add_lock);

	if (bus_find_device_by_name(&i2s_bus_type, NULL, dev_name(&i2s->dev))
	    != NULL) {
		dev_err(dev, "chipselect %d already in use\n",
			i2s->chip_select);
		status = -EBUSY;
		goto done;
	}

	/* Device may be bound to an active driver when this returns */
	status = device_add(&i2s->dev);
	if (status < 0)
		dev_err(dev, "can't %s %s, status %d\n",
			"add", dev_name(&i2s->dev), status);
	else
		dev_dbg(dev, "registered child %s\n", dev_name(&i2s->dev));

      done:
	mutex_unlock(&i2s_add_lock);
	return status;
}

EXPORT_SYMBOL_GPL(i2s_add_device);

/**
 * i2s_new_device - instantiate one new I2S device
 * @i2s_cont: Controller to which device is connected
 * @chip: Describes the I2S device
 * Context: can sleep
 *
 * On typical mainboards, this is purely internal; and it's not needed
 * after board init creates the hard-wired devices.  Some development
 * platforms may not be able to use i2s_register_board_info though, and
 * this is exported so that driver could add devices (which it would
 * learn about out-of-band).
 *
 * Returns the new device, or NULL.
 */
struct i2s_device *i2s_new_device(struct i2s_controller *i2s_cont,
				  struct i2s_board_info *chip)
{
	struct i2s_device *proxy;
	int status;

	/* NOTE:  caller did any chip->bus_num checks necessary.
	 *
	 * Also, unless we change the return value convention to use
	 * error-or-pointer (not NULL-or-pointer), troubleshootability
	 * suggests syslogged diagnostics are best here (ugh).
	 */

	proxy = i2s_alloc_device(&i2s_cont->dev);
	if (!proxy)
		return NULL;

	WARN_ON(strlen(chip->modalias) >= sizeof(proxy->modalias));

	proxy->chip_select = chip->chip_select;
	strlcpy(proxy->modalias, chip->modalias, sizeof(proxy->modalias));
	proxy->dev.platform_data = (void *)chip->platform_data;
	proxy->controller = i2s_cont;

	status = i2s_add_device(proxy);
	if (status < 0) {
		kfree(proxy);
		return NULL;
	}

	return proxy;
}

EXPORT_SYMBOL_GPL(i2s_new_device);

/**
 * i2s_register_board_info - register I2S devices for a given board
 * @info: array of chip descriptors
 * @n: how many descriptors are provided
 * Context: can sleep
 *
 * Board-specific early init code calls this (probably during arch_initcall)
 * with segments of the I2S device table.  Any device nodes are created later,
 * after the relevant parent I2S controller (id) is defined.  We keep
 * this table of devices forever, so that reloading a controller driver will
 * not make Linux forget about these hard-wired devices.
 *
 */
int __init
i2s_register_board_info(struct i2s_board_info const *info, unsigned n)
{
	int i;
	struct boardinfo *bi;

	bi = kmalloc(sizeof(*bi) + (n * sizeof(struct board_i2s_combined_info)), GFP_KERNEL);
	if (!bi)
		return -ENOMEM;
	bi->n_board_info = n;

	for (i = 0; i < n; i++)
		memcpy(&bi->board_i2s_info[i].board_info, &info[i], sizeof *info);

	mutex_lock(&board_lock);
	list_add_tail(&bi->list, &board_list);
	mutex_unlock(&board_lock);
	return 0;
}

/**
 * scan_boardinfo - Scan, creates and registered new i2s device structure.
 * @i2s_cont: i2s controller structure
 * Context: process
 *
 * It will scan the device list that may be registered statically using
 * register_board_info func in arch specific directory and call
 * i2s_new_device to create and registered i2s device over i2s bus. It is
 * called by i2s_add_controller function.
 *
 * Returns void.
 */
static void scan_boardinfo(struct i2s_controller *i2s_cont)
{
	struct boardinfo *bi;

	mutex_lock(&board_lock);
	list_for_each_entry(bi, &board_list, list) {
		struct board_i2s_combined_info *chip = bi->board_i2s_info;
		unsigned n;

		for (n = bi->n_board_info; n > 0; n--, chip++) {
			if (chip->board_info.chip_select != i2s_cont->id)
				continue;
			/* NOTE: this relies on i2s_new_device to
			 * issue diagnostics when given bogus inputs
			 */
			chip->i2s_dev_p = i2s_new_device(i2s_cont, &chip->board_info);
		}
	}
	mutex_unlock(&board_lock);
}

/******************************************************************************/
/**I2S Controller inittialization*/
static void i2s_controller_dev_release(struct device *dev)
{
	struct i2s_controller *i2s_cont;
	i2s_cont = container_of(dev, struct i2s_controller, dev);
	kfree(i2s_cont);
}

static ssize_t
show_controller_name(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct i2s_controller *cont = to_i2s_controller(dev);
	return sprintf(buf, "%s\n", cont->name);
}

static struct device_attribute i2s_controller_attrs[] = {
	__ATTR(name, S_IRUGO, show_controller_name, NULL),
	{},
};

static struct class i2s_controller_class = {
	.owner = THIS_MODULE,
	.name = "i2s-controller",
	.dev_attrs = i2s_controller_attrs,
};

static int i2s_register_controller(struct i2s_controller *cont)
{
	int res = 0;
	mutex_init(&cont->bus_lock);

	mutex_lock(&core_lock);

	/* Add the controller to the driver core.
	 * If the parent pointer is not set up,
	 * we add this controller to the host bus.
	 */
	if (cont->dev.parent == NULL) {
		cont->dev.parent = &platform_bus;
		pr_debug("I2S controller driver [%s] forgot to specify "
			 "physical device\n", cont->name);
	}
	dev_set_name(&cont->dev, "I2Scrlr-%d", cont->id);
	cont->dev.release = &i2s_controller_dev_release;
	cont->dev.class = &i2s_controller_class;
	res = device_register(&cont->dev);
	if (res)
		goto out_unlock;

	dev_dbg(&cont->dev, "controller [%s] registered\n", cont->name);
	scan_boardinfo(cont);
      out_unlock:
	mutex_unlock(&core_lock);
	return res;
}

/**
 * i2s_add_controller - declare i2s controller, use dynamic bus number
 * @controller: the controller to add
 * Context: can sleep
 *
 */
int i2s_add_controller(struct i2s_controller *controller)
{
	return i2s_register_controller(controller);
}

EXPORT_SYMBOL(i2s_add_controller);

static int __unregister(struct device *dev, void *controller_dev)
{
	/* note: before about 2.6.14-rc1 this would corrupt memory: */
	if (dev != controller_dev)
		i2s_unregister_device(to_i2s_device(dev));
	return 0;
}

/**
 * i2s_del_controller - unregister I2S controller
 * @cont: the controller being unregistered
 * Context: can sleep
 *
 * This unregisters an I2S controller which was previously registered
 * by @i2s_add_controller.
 */
int i2s_del_controller(struct i2s_controller *cont)
{
	int res = 0;
	int dummy;
	mutex_lock(&core_lock);

	dummy = device_for_each_child(cont->dev.parent, &cont->dev,
				      __unregister);
	device_unregister(&cont->dev);
	mutex_unlock(&core_lock);
	return res;
}

EXPORT_SYMBOL(i2s_del_controller);

/******************************************************************************/
/*I2S interface apis*/

/**
 * i2s_transfer - Main i2s transfer function.
 * @i2s_cont: i2s controller structure passed by client driver.
 * @message: i2s message structure contains transceive info.
 * Context: process or interrupt.
 *
 * This API is called by client i2s driver as i2s_xfer funtion. It will handle
 * main i2s transfer over i2s bus. The controller should registered its own
 * functions using i2s algorithm structure.
 *
 * Returns error(-1) in case of failure or success(0).
 */
int i2s_transfer(struct i2s_controller *i2s_cont, struct i2s_message *message)
{
	return i2s_cont->algo->cont_transfer(i2s_cont, message);

}

EXPORT_SYMBOL(i2s_transfer);

/**
 * i2s_cleanup - Close the current i2s connection btw controller and client.
 * @i2s_cont: i2s controller structure
 * @flag: It indicates the functionality that needs to be disabled.
 * Context: process
 *
 * This API will disable and reset the controller's configuration. Reset the
 * controller so that i2s client driver can reconfigure with new configuration.
 * Controller should release all the necessary resources which was acquired
 * during setup.
 *
 * Returns error(-1) in case of failure or success(0).
 */
int i2s_cleanup(struct i2s_controller *i2s_cont, i2s_flag flag)
{
	int status = 0;
	status = i2s_cont->algo->cont_cleanup(i2s_cont, flag);
	if (status)
		return -1;
	else
		return 0;
}

EXPORT_SYMBOL(i2s_cleanup);

/**
 * i2s_setup - configures and enables the I2S controller.
 * @i2s_cont: i2s controller sent by i2s device.
 * @config: specifies the configuration parameters.
 *
 * This function configures the I2S controller with the client configuration.
 * Controller was already registered on I2S bus by some master controller
 * driver.
 *
 * Returns error(-1) in case of failure else success(0)
 */
int i2s_setup(struct i2s_controller *i2s_cont, void *config)
{
	return i2s_cont->algo->cont_setup(i2s_cont, config);
}

EXPORT_SYMBOL(i2s_setup);

/**
 * i2s_hw_status - Get the current hw status for the i2s controller.
 * @i2s_cont: i2s controller structure passed by client driver.
 * Context: process or interrupt.
 *
 * This API is called by client i2s driver to find out current hw status.
 * The controller should registered its own functions using i2s algorithm structure.
 *
 * Returns current hw status register.
 */
int i2s_hw_status(struct i2s_controller *i2s_cont)
{
	return i2s_cont->algo->cont_hw_status(i2s_cont);
}

/**
 * i2s_get_pointer - Get the current dma_addr_t for the i2s controller.
 * @i2s_cont: i2s controller structure passed by client driver.
 * @i2s_direction: Specifies TX or RX direction.
 * Context: process or interrupt.
 *
 * This API is called by client i2s driver to return a dma_addr_t corresponding
 * to the position of the DMA-controller.
 * The controller should registered its own functions using i2s algorithm structure.
 *
 * Returns current hw status register.
 */
dma_addr_t i2s_get_pointer(struct i2s_controller *i2s_cont,
			enum i2s_direction_t i2s_direction)
{
	return i2s_cont->algo->cont_get_pointer(i2s_cont, i2s_direction);
}

/******************************************************************************/

static int __init i2s_init(void)
{
	int status;

	status = bus_register(&i2s_bus_type);
	if (status < 0)
		goto err0;

	status = class_register(&i2s_controller_class);
	if (status < 0)
		goto err1;
	return 0;

      err1:
	bus_unregister(&i2s_bus_type);
      err0:
	return status;
}

static void __exit i2s_exit(void)
{
	class_unregister(&i2s_controller_class);
	bus_unregister(&i2s_bus_type);
}

subsys_initcall(i2s_init);
module_exit(i2s_exit);

MODULE_AUTHOR("Sandeep Kaushik, <sandeep-mmc.kaushik@st.com>");
MODULE_LICENSE("GPL");

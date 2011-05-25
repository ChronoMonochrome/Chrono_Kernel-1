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
/* or FITNES                                                                  */
/* FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      */
/* details.                                                                   */
/*                                                                            */
/* You should have received a copy of the GNU General Public License          */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*----------------------------------------------------------------------------*/

#ifndef __LINUX_I2S_H
#define __LINUX_I2S_H

/*
 * INTERFACES between I2S controller-side drivers and I2S infrastructure.
 */
extern struct bus_type i2s_bus_type;
#define I2S_NAME_SIZE 48
/**
 * struct i2s_device - Controller side proxy for an I2S slave device
 * @dev: Driver model representation of the device.
 * @controller: I2S controller used with the device.
 * @chip_select: Chipselect, distinguishing chips handled by @controller.
 * @modalias: Name of the driver to use with this device, or an alias
 *	for that name.  This appears in the sysfs "modalias" attribute
 *	for driver coldplugging, and in uevents used for hotplugging
 *
 * A @i2s_device is used to interchange data between an I2S slave
 *
 * In @dev, the platform_data is used to hold information about this
 * device that's meaningful to the device's protocol driver, but not
 * to its controller.
 */
struct i2s_device {
	struct device dev;
	struct i2s_controller *controller;
	u8 chip_select;
	char modalias[32];
};
struct i2s_board_info {
	/* the device name and module name are coupled, like platform_bus;
	 * "modalias" is normally the driver name.
	 *
	 * platform_data goes to i2s_device.dev.platform_data,
	 */
	char modalias[32];
	const void *platform_data;
	u16 id;
	u16 chip_select;
};

#ifdef	CONFIG_STM_I2S
extern int
i2s_register_board_info(struct i2s_board_info const *info, unsigned n);
#else
/* board init code may ignore whether I2S is configured or not */
static inline int
i2s_register_board_info(struct i2s_board_info const *info, unsigned n)
{
	return 0;
}
#endif

static inline struct i2s_device *to_i2s_device(struct device *dev)
{
	return dev ? container_of(dev, struct i2s_device, dev) : NULL;
}

static inline struct i2s_device *i2s_dev_get(struct i2s_device *i2s)
{
	return (i2s && get_device(&i2s->dev)) ? i2s : NULL;
}

static inline void i2s_dev_put(struct i2s_device *i2s)
{
	if (i2s)
		put_device(&i2s->dev);
}

static inline void i2s_set_drvdata(struct i2s_device *i2s, void *data)
{
	dev_set_drvdata(&i2s->dev, data);
}

static inline void *i2s_get_drvdata(struct i2s_device *i2s)
{
	return dev_get_drvdata(&i2s->dev);
}

struct i2s_device_id {
	char name[I2S_NAME_SIZE];
	/*currently not used may be used in future */
	u32 device_id;
	u32 vendor_id;
};

/**
 * struct i2s_driver - Host side "protocol" driver
 */
struct i2s_driver {
	int (*probe) (struct i2s_device *i2s);
	int (*remove) (struct i2s_device *i2s);
	void (*shutdown) (struct i2s_device *i2s);
	int (*suspend) (struct i2s_device *i2s, pm_message_t mesg);
	int (*resume) (struct i2s_device *i2s);
	struct device_driver driver;
	const struct i2s_device_id *id_table;

};

static inline struct i2s_driver *to_i2s_driver(struct device_driver *drv)
{
	return drv ? container_of(drv, struct i2s_driver, driver) : NULL;
}

extern int i2s_register_driver(struct i2s_driver *sdrv);

/**
 * i2s_unregister_driver - reverse effect of i2s_register_driver
 * @sdrv: the driver to unregister
 * Context: can sleep
 */
static inline void i2s_unregister_driver(struct i2s_driver *sdrv)
{
	if (sdrv)
		driver_unregister(&sdrv->driver);
}

/**I2S controller parameters*/

enum i2s_direction_t {
	I2S_DIRECTION_TX = 0,
	I2S_DIRECTION_RX = 1,
	I2S_DIRECTION_BOTH = 2
};

enum i2s_transfer_mode_t {
	I2S_TRANSFER_MODE_SINGLE_DMA = 0,
	I2S_TRANSFER_MODE_CYCLIC_DMA = 1,
	I2S_TRANSFER_MODE_INF_LOOPBACK = 2,
	I2S_TRANSFER_MODE_NON_DMA = 4,
};

struct i2s_message {
	enum i2s_transfer_mode_t i2s_transfer_mode;
	enum i2s_direction_t i2s_direction;
	void *txdata;
	void *rxdata;
	size_t txbytes;
	size_t rxbytes;
	int dma_flag;
	int tx_offset;
	int rx_offset;
	/* cyclic dma */
	bool cyclic_dma;
	dma_addr_t buf_addr;
	size_t buf_len;
	size_t period_len;
};

typedef enum {
	DISABLE_ALL = 0,
	DISABLE_TRANSMIT = 1,
	DISABLE_RECEIVE = 2,
} i2s_flag;

struct i2s_algorithm {
	int (*cont_setup) (struct i2s_controller *i2s_cont, void *config);
	int (*cont_transfer) (struct i2s_controller *i2s_cont,
			      struct i2s_message *message);
	int (*cont_cleanup) (struct i2s_controller *i2s_cont, i2s_flag flag);
	int (*cont_hw_status) (struct i2s_controller *i2s_cont);
	dma_addr_t (*cont_get_pointer) (struct i2s_controller *i2s_cont,
					enum i2s_direction_t i2s_direction);
};

struct i2s_controller {
	struct module *owner;
	unsigned int id;
	unsigned int class;
	const struct i2s_algorithm *algo; /* the algorithm to access the bus */
	void *data;
	struct mutex bus_lock;
	struct device dev; /* the controller device */
	char name[48];
};
#define to_i2s_controller(d) container_of(d, struct i2s_controller, dev)

static inline void *i2s_get_contdata(struct i2s_controller *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void i2s_set_contdata(struct i2s_controller *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

extern int i2s_add_controller(struct i2s_controller *controller);
extern int i2s_del_controller(struct i2s_controller *controller);
extern int i2s_setup(struct i2s_controller *i2s_cont, void *config);
extern int i2s_transfer(struct i2s_controller *i2s_cont,
			struct i2s_message *message);
extern int i2s_cleanup(struct i2s_controller *i2s_cont, i2s_flag flag);
extern int i2s_hw_status(struct i2s_controller *i2s_cont);
extern dma_addr_t i2s_get_pointer(struct i2s_controller *i2s_cont,
				enum i2s_direction_t i2s_direction);

extern struct i2s_device *i2s_get_device_from_boardinfo(int chip_select); /* used in MSP LTP tests */
extern struct i2s_device *i2s_alloc_device(struct device *dev);

extern int i2s_add_device(struct i2s_device *i2s);

static inline void i2s_unregister_device(struct i2s_device *i2s)
{
	if (i2s)
		device_unregister(&i2s->dev);
}

#endif /* __LINUX_I2S_H */

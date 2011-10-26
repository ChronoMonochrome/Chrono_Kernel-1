/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL), version 2
 */

#include <linux/gpio.h>
#include <linux/lsm303dlh.h>
#include <linux/l3g4200d.h>
#include <linux/i2c.h>
#include <linux/input/lps001wp.h>
#include <asm/mach-types.h>

#include "board-mop500.h"

/*
 * LSM303DLH accelerometer + magnetometer sensors
 */
static struct lsm303dlh_platform_data __initdata lsm303dlh_pdata = {
	.name_a = "lsm303dlh.0",
	.name_m = "lsm303dlh.1",
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negative_x = 1,
	.negative_y = 1,
	.negative_z = 0,
};

static struct l3g4200d_gyr_platform_data  __initdata l3g4200d_pdata_u8500 = {
	.name_gyr = "l3g4200d",
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negative_x = 0,
	.negative_y = 0,
	.negative_z = 1,
};

static struct lps001wp_prs_platform_data __initdata lps001wp_pdata = {
	.poll_interval = 500,
	.min_interval = 10,
};

static struct i2c_board_info __initdata mop500_i2c2_devices[] = {
	{
		/* LSM303DLH Accelerometer */
		I2C_BOARD_INFO("lsm303dlh_a", 0x18),
		.platform_data = &lsm303dlh_pdata,
	},
	{
		/* LSM303DLH Magnetometer */
		I2C_BOARD_INFO("lsm303dlh_m", 0x1E),
		.platform_data = &lsm303dlh_pdata,
	},
	{
		/* L3G4200D Gyroscope */
		I2C_BOARD_INFO("l3g4200d", 0x68),
		.platform_data = &l3g4200d_pdata_u8500,
	},
	{
		/* LSP001WM Barometer */
		I2C_BOARD_INFO("lps001wp_prs_sysfs", 0x5C),
		.platform_data = &lps001wp_pdata,
	},
};

/*
 * Register/Add i2c sensors
 */
void mop500_sensors_i2c_add(int busnum, struct i2c_board_info const *info,
		unsigned n)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	int i;

	adap = i2c_get_adapter(busnum);
	if (!adap) {
		/* We have no i2c adapter yet lets create it. */
		pr_info(__FILE__ ": Creating i2c adapter %d\n", busnum);
		i2c_register_board_info(busnum, info, n);
		return;
	}

	for (i = 0; i < n; i++) {
		client = i2c_new_device(adap, &info[i]);
		if (!client)
			pr_err(__FILE__ ": failed to register %s to i2c%d\n",
					info[i].type,
					busnum);
	}

	i2c_put_adapter(adap);
}


void __init mop500_sensors_init(void)
{
	if (machine_is_hrefv60()) {
		lsm303dlh_pdata.irq_a1 = HREFV60_ACCEL_INT1_GPIO;
		lsm303dlh_pdata.irq_a2 = HREFV60_ACCEL_INT2_GPIO;
		lsm303dlh_pdata.irq_m = HREFV60_MAGNET_DRDY_GPIO;
	} else if (machine_is_snowball()) {
		lsm303dlh_pdata.irq_a1 = SNOWBALL_ACCEL_INT1_GPIO;
		lsm303dlh_pdata.irq_a2 = SNOWBALL_ACCEL_INT2_GPIO;
		lsm303dlh_pdata.irq_m = SNOWBALL_MAGNET_DRDY_GPIO;
	} else {
		lsm303dlh_pdata.irq_a1 = GPIO_ACCEL_INT1;
		lsm303dlh_pdata.irq_a2 = GPIO_ACCEL_INT2;
		lsm303dlh_pdata.irq_m = GPIO_MAGNET_DRDY;
	}

	mop500_sensors_i2c_add(2, mop500_i2c2_devices,
			ARRAY_SIZE(mop500_i2c2_devices));
}

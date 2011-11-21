/*

 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL), version 2
 */

#define pr_fmt(fmt)	"mop500-uib: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>

#include "board-mop500.h"

enum mop500_uib {
	STUIB,
	U8500UIB,
	U8500UIB_R3,
	NO_UIB,
};

struct uib {
	const char *name;
	const char *option;
	void (*init)(void);
};

static u8 type_of_uib = NO_UIB;

static struct uib __initdata mop500_uibs[] = {
	[STUIB] = {
		.name	= "ST-UIB",
		.option	= "stuib",
		.init	= mop500_stuib_init,
	},
	[U8500UIB] = {
		.name	= "U8500-UIB",
		.option	= "u8500uib",
		.init	= mop500_u8500uib_init,
	},
#ifdef CONFIG_TOUCHSCREEN_CYTTSP_SPI
	[U8500UIB_R3] = {
		.name   = "U8500-UIBR3",
		.option = "u8500uibr3",
		.init   = mop500_u8500uib_r3_init,
	},
#endif
};

static struct uib __initdata *mop500_uib;

static int __init mop500_uib_setup(char *str)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mop500_uibs); i++) {
		struct uib *uib = &mop500_uibs[i];

		if (!strcmp(str, uib->option)) {
			mop500_uib = uib;
			break;
		}
	}

	if (i == ARRAY_SIZE(mop500_uibs))
		pr_err("invalid uib= option (%s)\n", str);

	return 1;
}
__setup("uib=", mop500_uib_setup);

/*
 * The UIBs are detected after the I2C host controllers are registered, so
 * i2c_register_board_info() can't be used.
 */
void mop500_uib_i2c_add(int busnum, struct i2c_board_info const *info,
		unsigned n)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	int i;

	adap = i2c_get_adapter(busnum);
	if (!adap) {
		pr_err("failed to get adapter i2c%d\n", busnum);
		return;
	}

	for (i = 0; i < n; i++) {
		client = i2c_new_device(adap, &info[i]);
		if (!client)
			pr_err("failed to register %s to i2c%d\n",
					info[i].type, busnum);
	}

	i2c_put_adapter(adap);
}

static void __init __mop500_uib_init(struct uib *uib, const char *why)
{
	pr_info("%s (%s)\n", uib->name, why);

	if (strcmp("stuib", uib->option) == 0)
		type_of_uib = STUIB;
	else if (strcmp("u8500uib", uib->option) == 0)
		type_of_uib = U8500UIB;
	else if (strcmp("u8500uibr3", uib->option) == 0)
		type_of_uib = U8500UIB_R3;

	uib->init();
}

int uib_is_stuib(void)
{
	return (type_of_uib == STUIB);
}

int uib_is_u8500uib(void)
{
	return (type_of_uib == U8500UIB);
}

int uib_is_u8500uibr3(void)
{
	return (type_of_uib == U8500UIB_R3);
}

/*
 * Detect the UIB attached based on the presence or absence of i2c devices.
 */
static int __init mop500_uib_init(void)
{
	struct uib *uib = mop500_uibs;
	struct i2c_adapter *i2c0;
	struct i2c_adapter *i2c3;
	int ret;

	/* snowball and non u8500 cpus dont have uib */
	if (!cpu_is_u8500() || machine_is_snowball())
		return -ENODEV;

	i2c0 = i2c_get_adapter(0);
	if (!i2c0) {
		__mop500_uib_init(&mop500_uibs[STUIB],
				"fallback, could not get i2c0");
		return -ENODEV;
	}

	/* U8500-UIB has the TC35893 at 0x44 on I2C0, the ST-UIB doesn't. */
	ret = i2c_smbus_xfer(i2c0, 0x44, 0, I2C_SMBUS_WRITE, 0,
			I2C_SMBUS_QUICK, NULL);
	i2c_put_adapter(i2c0);
	i2c3 = i2c_get_adapter(3);
	if (ret == 0) {
		if (!i2c3) {
			__mop500_uib_init(&mop500_uibs[STUIB],
					"fallback, could not get i2c3");
			return -ENODEV;
		}
		ret = i2c_smbus_xfer(i2c3, 0x4B, 0, I2C_SMBUS_WRITE, 0,
				I2C_SMBUS_QUICK, NULL);
		i2c_put_adapter(i2c3);
		if (ret == 0)
			uib = &mop500_uibs[U8500UIB];
		else
			uib = &mop500_uibs[U8500UIB_R3];
	}
	else {
		ret = i2c_smbus_xfer(i2c3, 0x5C, 0, I2C_SMBUS_WRITE, 0,
				I2C_SMBUS_QUICK, NULL);
		i2c_put_adapter(i2c3);
		if (ret == 0)
			uib = &mop500_uibs[STUIB];
	}
	__mop500_uib_init(uib, "detected");

	return 0;
}

module_init(mop500_uib_init);

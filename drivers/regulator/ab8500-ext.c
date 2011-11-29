/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Authors: Bengt Jonsson <bengt.g.jonsson@stericsson.com>
 *
 * This file is based on drivers/regulator/ab8500.c
 *
 * AB8500 external regulators
 *
 * ab8500-ext supports the following regulators:
 * - VextSupply3
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/regulator/ab8500.h>

/**
 * struct ab8500_ext_regulator_info - ab8500 regulator information
 * @dev: device pointer
 * @desc: regulator description
 * @regulator_dev: regulator device
 * @is_enabled: status of regulator (on/off)
 * @fixed_uV: typical voltage (for fixed voltage supplies)
 * @update_bank: bank to control on/off
 * @update_reg: register to control on/off
 * @update_mask: mask to enable/disable and set mode of regulator
 * @update_val: bits holding the regulator current mode
 * @update_val_en: bits to set EN pin active (LPn pin deactive)
 *                 normally this means high power mode
 * @update_val_en_lp: bits to set EN pin active and LPn pin active
 *                    normally this means low power mode
 * @delay: startup delay in ms
 */
struct ab8500_ext_regulator_info {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *regulator;
	bool is_enabled;
	int fixed_uV;
	u8 update_bank;
	u8 update_reg;
	u8 update_mask;
	u8 update_val;
	u8 update_val_en;
	u8 update_val_en_lp;
};

static int ab8500_ext_regulator_enable(struct regulator_dev *rdev)
{
	int ret;
	struct ab8500_ext_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_mask_and_set_register_interruptible(info->dev,
		info->update_bank, info->update_reg,
		info->update_mask, info->update_val);
	if (ret < 0)
		dev_err(rdev_get_dev(rdev),
			"couldn't set enable bits for regulator\n");

	info->is_enabled = true;

	dev_dbg(rdev_get_dev(rdev), "%s-enable (bank, reg, mask, value):"
		" 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		info->desc.name, info->update_bank, info->update_reg,
		info->update_mask, info->update_val);

	return ret;
}

static int ab8500_ext_regulator_disable(struct regulator_dev *rdev)
{
	int ret;
	struct ab8500_ext_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_mask_and_set_register_interruptible(info->dev,
		info->update_bank, info->update_reg,
		info->update_mask, 0x0);
	if (ret < 0)
		dev_err(rdev_get_dev(rdev),
			"couldn't set disable bits for regulator\n");

	info->is_enabled = false;

	dev_dbg(rdev_get_dev(rdev), "%s-disable (bank, reg, mask, value):"
		" 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		info->desc.name, info->update_bank, info->update_reg,
		info->update_mask, 0x0);

	return ret;
}

static int ab8500_ext_regulator_is_enabled(struct regulator_dev *rdev)
{
	int ret;
	struct ab8500_ext_regulator_info *info = rdev_get_drvdata(rdev);
	u8 regval;

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_get_register_interruptible(info->dev,
		info->update_bank, info->update_reg, &regval);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev),
			"couldn't read 0x%x register\n", info->update_reg);
		return ret;
	}

	dev_dbg(rdev_get_dev(rdev), "%s-is_enabled (bank, reg, mask, value):"
		" 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		info->desc.name, info->update_bank, info->update_reg,
		info->update_mask, regval);

	if (regval & info->update_mask)
		info->is_enabled = true;
	else
		info->is_enabled = false;

	return info->is_enabled;
}

static int ab8500_ext_fixed_get_voltage(struct regulator_dev *rdev)
{
	struct ab8500_ext_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	return info->fixed_uV;
}

static int ab8500_ext_list_voltage(struct regulator_dev *rdev,
				   unsigned selector)
{
	struct ab8500_ext_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	/* return the uV for the fixed regulators */
	if (info->fixed_uV)
		return info->fixed_uV;

	return -EINVAL;
}

static struct regulator_ops ab8500_ext_regulator_ops = {
	.enable			= ab8500_ext_regulator_enable,
	.disable		= ab8500_ext_regulator_disable,
	.is_enabled		= ab8500_ext_regulator_is_enabled,
	.get_voltage		= ab8500_ext_fixed_get_voltage,
	.list_voltage		= ab8500_ext_list_voltage,
};


static struct ab8500_ext_regulator_info
		ab8500_ext_regulator_info[AB8500_NUM_EXT_REGULATORS] = {
	[AB8500_EXT_SUPPLY3] = {
		.desc = {
			.name		= "VEXTSUPPLY3",
			.ops		= &ab8500_ext_regulator_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_EXT_SUPPLY3,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.fixed_uV		= 3400000,
		.update_bank		= 0x04,
		.update_reg		= 0x08,
		.update_mask		= 0x30,
		.update_val		= 0x10,
		.update_val_en		= 0x10,
		.update_val_en_lp	= 0x30,
	},
};

__devinit int ab8500_ext_regulator_init(struct platform_device *pdev)
{
	struct ab8500 *ab8500 = dev_get_drvdata(pdev->dev.parent);
	struct ab8500_platform_data *ppdata;
	struct ab8500_regulator_platform_data *pdata;
	int i, err;

	if (!ab8500) {
		dev_err(&pdev->dev, "null mfd parent\n");
		return -EINVAL;
	}
	ppdata = dev_get_platdata(ab8500->dev);
	if (!ppdata) {
		dev_err(&pdev->dev, "null parent pdata\n");
		return -EINVAL;
	}

	pdata = ppdata->regulator;
	if (!pdata) {
		dev_err(&pdev->dev, "null pdata\n");
		return -EINVAL;
	}

	/* make sure the platform data has the correct size */
	if (pdata->num_ext_regulator != ARRAY_SIZE(ab8500_ext_regulator_info)) {
		dev_err(&pdev->dev, "Configuration error: size mismatch.\n");
		return -EINVAL;
	}

	/* check for AB8500 2.x */
	if (abx500_get_chip_id(&pdev->dev) < 0x30) {
		struct ab8500_ext_regulator_info *info;

		/* VextSupply3LPn is inverted on AB8500 2.x */
		info = &ab8500_ext_regulator_info[AB8500_EXT_SUPPLY3];
		info->update_val = 0x30;
		info->update_val_en = 0x30;
		info->update_val_en_lp = 0x10;
	}

	/* register all regulators */
	for (i = 0; i < ARRAY_SIZE(ab8500_ext_regulator_info); i++) {
		struct ab8500_ext_regulator_info *info = NULL;

		/* assign per-regulator data */
		info = &ab8500_ext_regulator_info[i];
		info->dev = &pdev->dev;

		/* register regulator with framework */
		info->regulator = regulator_register(&info->desc, &pdev->dev,
				&pdata->ext_regulator[i], info, NULL);
		if (IS_ERR(info->regulator)) {
			err = PTR_ERR(info->regulator);
			dev_err(&pdev->dev, "failed to register regulator %s\n",
					info->desc.name);
			/* when we fail, un-register all earlier regulators */
			while (--i >= 0) {
				info = &ab8500_ext_regulator_info[i];
				regulator_unregister(info->regulator);
			}
			return err;
		}

		dev_dbg(rdev_get_dev(info->regulator),
			"%s-probed\n", info->desc.name);
	}

	return 0;
}

__devexit int ab8500_ext_regulator_exit(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ab8500_ext_regulator_info); i++) {
		struct ab8500_ext_regulator_info *info = NULL;
		info = &ab8500_ext_regulator_info[i];

		dev_vdbg(rdev_get_dev(info->regulator),
			"%s-remove\n", info->desc.name);

		regulator_unregister(info->regulator);
	}

	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Bengt Jonsson <bengt.g.jonsson@stericsson.com>");
MODULE_DESCRIPTION("AB8500 external regulator driver");
MODULE_ALIAS("platform:ab8500-ext-regulator");

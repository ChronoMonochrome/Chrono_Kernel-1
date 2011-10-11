/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 *
 * AB8500 Power-On Key handler
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/mfd/ab8500.h>
#include <linux/slab.h>

/**
 * struct ab8500_ponkey_info - ab8500 ponkey information
 * @input_dev: pointer to input device
 * @ab8500: ab8500 parent
 * @irq_dbf: irq number for falling transition
 * @irq_dbr: irq number for rising transition
 */
struct ab8500_ponkey_info {
	struct input_dev	*idev;
	struct ab8500		*ab8500;
	int			irq_dbf;
	int			irq_dbr;
};

/* AB8500 gives us an interrupt when ONKEY is held */
static irqreturn_t ab8500_ponkey_handler(int irq, void *data)
{
	struct ab8500_ponkey_info *info = data;

	if (irq == info->irq_dbf)
		input_report_key(info->idev, KEY_POWER, true);
	else if (irq == info->irq_dbr)
		input_report_key(info->idev, KEY_POWER, false);

	input_sync(info->idev);

	return IRQ_HANDLED;
}

static int __devinit ab8500_ponkey_probe(struct platform_device *pdev)
{
	struct ab8500 *ab8500 = dev_get_drvdata(pdev->dev.parent);
	struct ab8500_ponkey_info *info;
	int irq_dbf, irq_dbr, ret;

	irq_dbf = platform_get_irq_byname(pdev, "ONKEY_DBF");
	if (irq_dbf < 0) {
		dev_err(&pdev->dev, "No IRQ for ONKEY_DBF,error=%d\n", irq_dbf);
		return irq_dbf;
	}

	irq_dbr = platform_get_irq_byname(pdev, "ONKEY_DBR");
	if (irq_dbr < 0) {
		dev_err(&pdev->dev, "No IRQ for ONKEY_DBR,error=%d\n", irq_dbr);
		return irq_dbr;
	}

	info = kzalloc(sizeof(struct ab8500_ponkey_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->ab8500 = ab8500;
	info->irq_dbf = irq_dbf;
	info->irq_dbr = irq_dbr;

	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(ab8500->dev, "Failed to allocate input dev\n");
		ret = -ENOMEM;
		goto out;
	}

	info->idev->name = "AB8500 POn(PowerOn) Key";
	info->idev->dev.parent = &pdev->dev;
	info->idev->evbit[0] = BIT_MASK(EV_KEY);
	info->idev->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);

	ret = input_register_device(info->idev);
	if (ret) {
		dev_err(ab8500->dev, "Can't register input device: %d\n", ret);
		goto out_unfreedevice;
	}

	ret = request_threaded_irq(info->irq_dbf, NULL, ab8500_ponkey_handler,
					IRQF_NO_SUSPEND, "ab8500-ponkey-dbf",
					info);
	if (ret < 0) {
		dev_err(ab8500->dev, "Failed to request dbf IRQ#%d: %d\n",
				info->irq_dbf, ret);
		goto out_unregisterdevice;
	}

	ret = request_threaded_irq(info->irq_dbr, NULL, ab8500_ponkey_handler,
					IRQF_NO_SUSPEND, "ab8500-ponkey-dbr",
					info);
	if (ret < 0) {
		dev_err(ab8500->dev, "Failed to request dbr IRQ#%d: %d\n",
				info->irq_dbr, ret);
		goto out_irq_dbf;
	}

	platform_set_drvdata(pdev, info);

	return 0;

out_irq_dbf:
	free_irq(info->irq_dbf, info);
out_unregisterdevice:
	input_unregister_device(info->idev);
	info->idev = NULL;
out_unfreedevice:
	input_free_device(info->idev);
out:
	kfree(info);
	return ret;
}

static int __devexit ab8500_ponkey_remove(struct platform_device *pdev)
{
	struct ab8500_ponkey_info *info = platform_get_drvdata(pdev);

	free_irq(info->irq_dbf, info);
	free_irq(info->irq_dbr, info);
	input_unregister_device(info->idev);
	kfree(info);
	return 0;
}

static struct platform_driver ab8500_ponkey_driver = {
	.driver		= {
		.name	= "ab8500-poweron-key",
		.owner	= THIS_MODULE,
	},
	.probe		= ab8500_ponkey_probe,
	.remove		= __devexit_p(ab8500_ponkey_remove),
};

static int __init ab8500_ponkey_init(void)
{
	return platform_driver_register(&ab8500_ponkey_driver);
}
module_init(ab8500_ponkey_init);

static void __exit ab8500_ponkey_exit(void)
{
	platform_driver_unregister(&ab8500_ponkey_driver);
}
module_exit(ab8500_ponkey_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sundar Iyer <sundar.iyer@stericsson.com>");
MODULE_DESCRIPTION("ST-Ericsson AB8500 Power-ON(Pon) Key driver");

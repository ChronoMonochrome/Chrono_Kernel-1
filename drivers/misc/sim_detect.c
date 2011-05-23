/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: BIBEK BASU <bibek.basu@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/modem/modem_client.h>
#include <mach/sim_detect.h>

/* time in millisec */
#define TIMER_DELAY	10
struct sim_detect{
	struct work_struct	timer_expired;
	struct device	*dev;
	struct modem *modem;
	struct hrtimer timer;
};

static void inform_modem_release(struct work_struct *work)
{
	struct sim_detect *sim_detect =
		container_of(work, struct sim_detect, timer_expired);

	/* call Modem Access Framework api to release modem */
	modem_release(sim_detect->modem);
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	struct sim_detect *sim_detect =
		container_of(timer, struct sim_detect, timer);

	schedule_work(&sim_detect->timer_expired);
	return HRTIMER_NORESTART;
}

static irqreturn_t sim_activity_irq(int irq, void *dev)
{
	struct sim_detect *sim_detect = dev;

	/* call Modem Access Framework api to acquire modem */
	modem_request(sim_detect->modem);
	/* start the timer for 10ms */
	hrtimer_start(&sim_detect->timer,
			ktime_set(0, TIMER_DELAY*NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
/**
 * sim_detect_suspend() - This routine puts the Sim detect in to sustend state.
 * @dev:	pointer to device structure.
 *
 * This routine checks the current ongoing communication with Modem by
 * examining the modem_get_usage and work_pending state.
 * accordingly prevents suspend if modem communication
 * is on-going.
 */
int sim_detect_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sim_detect *sim_detect = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s called...\n", __func__);
	/* if modem is accessed, event system suspend */
	if (modem_get_usage(sim_detect->modem)
			|| work_pending(&sim_detect->timer_expired))
		return -EBUSY;
	else
		return 0;
}

static const struct dev_pm_ops sim_detect_dev_pm_ops = {
	.suspend = sim_detect_suspend,
};
#endif

static int __devinit sim_detect_probe(struct platform_device *pdev)
{
	struct sim_detect_platform_data *plat = dev_get_platdata(&pdev->dev);
	struct sim_detect *sim_detect;
	int ret;

	sim_detect = kzalloc(sizeof(struct sim_detect), GFP_KERNEL);
	if (sim_detect == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	sim_detect->dev = &pdev->dev;
	INIT_WORK(&sim_detect->timer_expired, inform_modem_release);
	hrtimer_init(&sim_detect->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sim_detect->timer.function = timer_callback;
	sim_detect->modem = modem_get(sim_detect->dev, "u8500-shrm-modem");
	if (IS_ERR(sim_detect->modem)) {
		ret = PTR_ERR(sim_detect->modem);
		dev_err(sim_detect->dev, "Could not retrieve the modem\n");
		goto out_free;
	}
	platform_set_drvdata(pdev, sim_detect);
	ret = request_threaded_irq(plat->irq_num,
		       NULL, sim_activity_irq,
		       IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		       "sim activity", sim_detect);
	if (ret < 0)
		goto out_free_irq;
out_free_irq:
	modem_put(sim_detect->modem);
	platform_set_drvdata(pdev, NULL);
out_free:
	kfree(sim_detect);
	return ret;
}

static int __devexit sim_detect_remove(struct platform_device *pdev)
{
	struct sim_detect *sim_detect = platform_get_drvdata(pdev);

	modem_put(sim_detect->modem);
	platform_set_drvdata(pdev, NULL);
	kfree(sim_detect);
	return 0;
}

static struct platform_driver sim_detect_driver = {
	.driver = {
		.name = "sim_detect",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &sim_detect_dev_pm_ops,
#endif
	},
	.probe = sim_detect_probe,
	.remove = __devexit_p(sim_detect_remove),
};

static int __init sim_detect_init(void)
{
	return platform_driver_register(&sim_detect_driver);
}
module_init(sim_detect_init);

static void __exit sim_detect_exit(void)
{
	platform_driver_unregister(&sim_detect_driver);
}
module_exit(sim_detect_exit);

MODULE_AUTHOR("BIBEK BASU <bibek.basu@stericsson.com>");
MODULE_DESCRIPTION("Detects SIM Hot Swap and wakes modem");
MODULE_ALIAS("SIM DETECT INTERRUPT driver");
MODULE_LICENSE("GPL v2");

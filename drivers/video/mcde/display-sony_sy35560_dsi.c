/*
 * Copyright (C) ST-Ericsson AB 2010
 *
 * ST-Ericsson MCDE Sony sy35560 DCS display driver
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/err.h>

#include <video/mcde_dss.h>
#include <video/mcde_display.h>
#include <video/mcde_display-sony_sy35560_dsi.h>

static int counter = 1;

/*
 * Work queue function for ESD check
 *
 * read display registers and perform power off/on/init sequence
 * if register value indicate HW failure
 */
static void sony_sy35560_esd_work_fn(struct work_struct *work)
{
	u8  value;
	struct sony_sy35560_device *dev = container_of(work,
		struct sony_sy35560_device,
		esd_work.work);


	mcde_dsi_dcs_write(((struct mcde_display_device *)dev)->chnl_state,
		0x00, &value, 1);
	pr_info("%s: %d) Read register 0x00, value 0x%.2X\n",
		__func__, counter, value);

	/* for now there are no registers available for ESD status check
	 * just perform power off/on/init every 4th work queue task
	 */

	if (counter % 4 == 0) {
		bool vsync_enabled;

		pr_info("%s:Resetting display....", __func__);

		/* get current display state */
		vsync_enabled = ((struct mcde_display_device *)dev)->
			get_synchronized_update(
			(struct mcde_display_device *)dev);

		/* wait a while to make sure finish refresh before resetting */
		mdelay(10);

		((struct mcde_display_device *)dev)->set_power_mode(
			(struct mcde_display_device *)dev,
			MCDE_DISPLAY_PM_OFF);
		((struct mcde_display_device *)dev)->set_power_mode(
			(struct mcde_display_device *)dev,
			MCDE_DISPLAY_PM_STANDBY);

		if (vsync_enabled) {
			u8 m = 0;
			mcde_dsi_dcs_write(
				((struct mcde_display_device *)dev)->chnl_state,
				DCS_CMD_SET_TEAR_ON, &m, 1);
			mcde_dss_set_synchronized_update(
				(struct mcde_display_device *)dev, 0);

			/* refresh display */
			((struct mcde_display_device *)dev)->update(
				(struct mcde_display_device *)dev);

			mcde_dss_set_synchronized_update(
				(struct mcde_display_device *)dev, 1);
		} else {
			/* need to wait a while before turning on the display */
			mdelay(5);

			((struct mcde_display_device *)dev)->set_power_mode(
				(struct mcde_display_device *)dev,
				MCDE_DISPLAY_PM_ON);
		}
	}
	counter++;

	queue_delayed_work(((struct sony_sy35560_device *)dev)->esd_wq,
		&(((struct sony_sy35560_device *)dev)->esd_work),
		SONY_SY35560_ESD_CHECK_PERIOD);
}

static int __devinit sony_sy35560_probe(struct mcde_display_device *dev)
{
	struct mcde_chnl_state *chnl;

	u32 id;
	u8  id1, id2, id3;
	int len = 1;
	int ret = 0;
	int readret = 0;

	/* create a workqueue for ESD status check */
	((struct sony_sy35560_device *)dev)->esd_wq =
		create_singlethread_workqueue("sony_esd");
	if (((struct sony_sy35560_device *)dev)->esd_wq == NULL) {
		dev_warn(&dev->dev, "can't create ESD workqueue\n");
		ret = -ENOMEM;
		goto out;
	}
	INIT_DELAYED_WORK(&(((struct sony_sy35560_device *)dev)->esd_work),
		sony_sy35560_esd_work_fn);

	/* Acquire MCDE resources */
	chnl = mcde_chnl_get(dev->chnl_id, dev->fifo, dev->port);
	if (IS_ERR(chnl)) {
		ret = PTR_ERR(chnl);
		dev_warn(&dev->dev, "Failed to acquire MCDE channel\n");
		goto out;
	}

	/* plugnplay: use registers DAh, DBh and DCh to detect display */
	readret = mcde_dsi_dcs_read(chnl, 0xDA, &id1, &len);
	if (!readret)
		readret = mcde_dsi_dcs_read(chnl, 0xDB, &id2, &len);
	if (!readret)
		readret = mcde_dsi_dcs_read(chnl, 0xDC, &id3, &len);

	if (readret) {
		dev_info(&dev->dev,
			"mcde_dsi_dcs_read failed to read display ID\n");
		goto read_fail;
	}

	id = (id3 << 16) | (id2 << 8) | id1;

	switch (id) {
	case 0x018101:
		dev_info(&dev->dev,
			"Cygnus Cut1 display (ID 0x%.6X) probed\n", id);
		/* add display specific initialization here */
		break;

	case 0x028101:
		dev_info(&dev->dev,
			"Cygnus Cut2 display (ID 0x%.6X) probed\n", id);
		/* add display specific initialization here */
		break;

	default:
		dev_info(&dev->dev,
			"Display with id 0x%.6X probed\n", id);
		break;
	}

read_fail:
	/* close  MCDE channel */
	mcde_chnl_put(chnl);
	chnl = NULL;
out:
	return ret;
}

static int __devexit sony_sy35560_remove(struct mcde_display_device *dev)
{
	dev->set_power_mode(dev, MCDE_DISPLAY_PM_OFF);

	cancel_delayed_work(&(((struct sony_sy35560_device *)dev)->esd_work));
	destroy_workqueue(((struct sony_sy35560_device *)dev)->esd_wq);
	return 0;
}

static int sony_sy35560_resume(struct mcde_display_device *ddev)
{
	int ret;
	/* set_power_mode will handle call platform_enable */
	ret = ddev->set_power_mode(ddev, MCDE_DISPLAY_PM_STANDBY);
	if (ret < 0)
		dev_warn(&ddev->dev, "%s:Failed to resume display\n"
			, __func__);

	queue_delayed_work(((struct sony_sy35560_device *)ddev)->esd_wq,
		&(((struct sony_sy35560_device *)ddev)->esd_work),
		SONY_SY35560_ESD_CHECK_PERIOD);

	return ret;
}

static int sony_sy35560_suspend(struct mcde_display_device *ddev,
			pm_message_t state)
{
	int ret;
	/* set_power_mode will handle call platform_disable */
	ret = ddev->set_power_mode(ddev, MCDE_DISPLAY_PM_OFF);
	if (ret < 0)
		dev_warn(&ddev->dev, "%s:Failed to suspend display\n"
			, __func__);

	cancel_delayed_work(&(((struct sony_sy35560_device *)ddev)->esd_work));

	return ret;
}

static struct mcde_display_driver sony_sy35560_driver = {
	.probe	= sony_sy35560_probe,
	.remove = sony_sy35560_remove,
	.suspend = sony_sy35560_suspend,
	.resume = sony_sy35560_resume,
	.driver = {
		.name	= "mcde_disp_sony",
	},
};

/* Module init */

static int __init mcde_display_sony_sy35560_init(void)
{
	pr_info("%s\n", __func__);

	return mcde_display_driver_register(&sony_sy35560_driver);
}
module_init(mcde_display_sony_sy35560_init);

static void __exit mcde_display_sony_sy35560_exit(void)
{
	pr_info("%s\n", __func__);

	mcde_display_driver_unregister(&sony_sy35560_driver);
}
module_exit(mcde_display_sony_sy35560_exit);

MODULE_AUTHOR("Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ST-Ericsson MCDE Sony sy35560 DCS display driver");


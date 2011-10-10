/*
 * Mac80211 power management API for ST-Ericsson CW1200 drivers
 *
 * Copyright (c) 2011, ST-Ericsson
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include "cw1200.h"
#include "pm.h"
#include "sta.h"
#include "bh.h"
#include "sbus.h"

static int cw1200_suspend_late(struct device *dev);
static void cw1200_pm_release(struct device *dev);
static int cw1200_pm_probe(struct platform_device *pdev);

/* private */
struct cw1200_suspend_state {
	unsigned long bss_loss_tmo;
	unsigned long connection_loss_tmo;
	unsigned long join_tmo;
	unsigned long direct_probe;
	unsigned long link_id_gc;
};

static const struct dev_pm_ops cw1200_pm_ops = {
	.suspend_noirq = cw1200_suspend_late,
};

static struct platform_driver cw1200_power_driver = {
	.probe = cw1200_pm_probe,
	.driver = {
		.name = "cw1200_power",
		.pm = &cw1200_pm_ops,
	},
};

static int cw1200_pm_init_common(struct cw1200_pm_state *pm,
				  struct cw1200_common *priv)
{
	int ret;

	spin_lock_init(&pm->lock);
	ret = platform_driver_register(&cw1200_power_driver);
	if (ret)
		return ret;
	pm->pm_dev = platform_device_alloc("cw1200_power", 0);
	if (!pm->pm_dev) {
		platform_driver_unregister(&cw1200_power_driver);
		return ENOMEM;
	}

	pm->pm_dev->dev.platform_data = priv;
	ret = platform_device_add(pm->pm_dev);
	if (ret) {
		kfree(pm->pm_dev);
		pm->pm_dev = NULL;
	}

	return ret;
}

static void cw1200_pm_deinit_common(struct cw1200_pm_state *pm)
{
	platform_driver_unregister(&cw1200_power_driver);
	if (pm->pm_dev) {
		pm->pm_dev->dev.platform_data = NULL;
		platform_device_unregister(pm->pm_dev);
		pm->pm_dev = NULL;
	}
}

#ifdef CONFIG_WAKELOCK

int cw1200_pm_init(struct cw1200_pm_state *pm,
		   struct cw1200_common *priv)
{
	int ret = cw1200_pm_init_common(pm, priv);
	if (!ret)
		wake_lock_init(&pm->wakelock,
			WAKE_LOCK_SUSPEND, "cw1200_wlan");
	return ret;
}

void cw1200_pm_deinit(struct cw1200_pm_state *pm)
{
	if (wake_lock_active(&pm->wakelock))
		wake_unlock(&pm->wakelock);
	wake_lock_destroy(&pm->wakelock);
	cw1200_pm_deinit_common(pm);
}

void cw1200_pm_stay_awake(struct cw1200_pm_state *pm,
			  unsigned long tmo)
{
	long cur_tmo;
	spin_lock_bh(&pm->lock);
	cur_tmo = pm->wakelock.expires - jiffies;
	if (!wake_lock_active(&pm->wakelock) ||
			cur_tmo < (long)tmo)
		wake_lock_timeout(&pm->wakelock, tmo);
	spin_unlock_bh(&pm->lock);
}

#else /* CONFIG_WAKELOCK */

static void cw1200_pm_stay_awake_tmo(unsigned long arg)
{
}

int cw1200_pm_init(struct cw1200_pm_state *pm,
		   struct cw1200_common *priv)
{
	int ret = cw1200_pm_init_common(pm, priv);
	if (!ret) {
		init_timer(&pm->stay_awake);
		pm->stay_awake.data = (unsigned long)pm;
		pm->stay_awake.function = cw1200_pm_stay_awake_tmo;
	}
	return ret;
}

void cw1200_pm_deinit(struct cw1200_pm_state *pm)
{
	del_timer_sync(&pm->stay_awake);
	cw1200_pm_deinit_common(pm);
}

void cw1200_pm_stay_awake(struct cw1200_pm_state *pm,
			  unsigned long tmo)
{
	long cur_tmo;
	spin_lock_bh(&pm->lock);
	cur_tmo = pm->stay_awake.expires - jiffies;
	if (!timer_pending(&pm->stay_awake) ||
			cur_tmo < (long)tmo)
		mod_timer(&pm->stay_awake, jiffies + tmo);
	spin_unlock_bh(&pm->lock);
}

#endif /* CONFIG_WAKELOCK */

static long cw1200_suspend_work(struct delayed_work *work)
{
	int ret = cancel_delayed_work(work);
	long tmo;
	if (ret > 0) {
		/* Timer is pending */
		tmo = work->timer.expires - jiffies;
		if (tmo < 0)
			tmo = 0;
	} else {
		tmo = -1;
	}
	return tmo;
}

static int cw1200_resume_work(struct cw1200_common *priv,
			       struct delayed_work *work,
			       unsigned long tmo)
{
	if ((long)tmo < 0)
		return 1;

	return queue_delayed_work(priv->workqueue, work, tmo);
}

static int cw1200_suspend_late(struct device *dev)
{
	struct cw1200_common *priv = dev->platform_data;
	if (atomic_read(&priv->bh_rx)) {
		wiphy_dbg(priv->hw->wiphy,
			"%s: Suspend interrupted.\n",
			__func__);
		return -EAGAIN;
	}
	return 0;
}

static void cw1200_pm_release(struct device *dev)
{
}

static int cw1200_pm_probe(struct platform_device *pdev)
{
	pdev->dev.release = cw1200_pm_release;
	return 0;
}

int cw1200_wow_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan)
{
	struct cw1200_common *priv = hw->priv;
	struct cw1200_pm_state *pm_state = &priv->pm_state;
	struct cw1200_suspend_state *state;
	int ret;

#ifndef CONFIG_WAKELOCK
	spin_lock_bh(&pm_state->lock);
	ret = timer_pending(&pm_state->stay_awake);
	spin_unlock_bh(&pm_state->lock);
	if (ret)
		return -EAGAIN;
#endif

	/* Do not suspend when datapath is not idle */
	if (priv->tx_queue_stats.num_queued)
		return -EBUSY;

	/* Make sure there is no configuration requests in progress. */
	if (!mutex_trylock(&priv->conf_mutex))
		return -EBUSY;

	/* Ensure pending operations are done.
	 * Note also that wow_suspend must return in ~2.5sec, before
	 * watchdog is triggered. */
	if (priv->channel_switch_in_progress)
		goto revert1;

	/* Do not suspend when join work is scheduled */
	if (work_pending(&priv->join_work))
		goto revert1;

	/* Do not suspend when scanning */
	if (down_trylock(&priv->scan.lock))
		goto revert1;

	/* Lock TX. */
	wsm_lock_tx_async(priv);
	if (priv->hw_bufs_used)
		goto revert3;

	/* Allocate state */
	state = kzalloc(sizeof(struct cw1200_suspend_state), GFP_KERNEL);
	if (!state)
		goto revert3;

	/* Store delayed work states. */
	state->bss_loss_tmo =
		cw1200_suspend_work(&priv->bss_loss_work);
	state->connection_loss_tmo =
		cw1200_suspend_work(&priv->connection_loss_work);
	state->join_tmo =
		cw1200_suspend_work(&priv->join_timeout);
	state->direct_probe =
		cw1200_suspend_work(&priv->scan.probe_work);
	state->link_id_gc =
		cw1200_suspend_work(&priv->link_id_gc_work);

	/* Stop serving thread */
	if (cw1200_bh_suspend(priv))
		goto revert4;

	ret = timer_pending(&priv->mcast_timeout);
	if (ret)
		goto revert5;

	/* Store suspend state */
	pm_state->suspend_state = state;

	/* Enable IRQ wake */
	ret = priv->sbus_ops->power_mgmt(priv->sbus_priv, true);
	if (ret) {
		wiphy_err(priv->hw->wiphy,
			"%s: PM request failed: %d. WoW is disabled.\n",
			__func__, ret);
		cw1200_wow_resume(hw);
		return -EBUSY;
	}

	/* Force resume if event is coming from the device. */
	if (atomic_read(&priv->bh_rx)) {
		cw1200_wow_resume(hw);
		return -EAGAIN;
	}

	return 0;

revert5:
	WARN_ON(cw1200_bh_resume(priv));
revert4:
	cw1200_resume_work(priv, &priv->bss_loss_work,
			state->bss_loss_tmo);
	cw1200_resume_work(priv, &priv->connection_loss_work,
			state->connection_loss_tmo);
	cw1200_resume_work(priv, &priv->join_timeout,
			state->join_tmo);
	cw1200_resume_work(priv, &priv->scan.probe_work,
			state->direct_probe);
	cw1200_resume_work(priv, &priv->link_id_gc_work,
			state->link_id_gc);
	kfree(state);
revert3:
	wsm_unlock_tx(priv);
	up(&priv->scan.lock);
revert1:
	mutex_unlock(&priv->conf_mutex);
	return -EBUSY;
}

int cw1200_wow_resume(struct ieee80211_hw *hw)
{
	struct cw1200_common *priv = hw->priv;
	struct cw1200_pm_state *pm_state = &priv->pm_state;
	struct cw1200_suspend_state *state;

	state = pm_state->suspend_state;
	pm_state->suspend_state = NULL;

	/* Disable IRQ wake */
	priv->sbus_ops->power_mgmt(priv->sbus_priv, false);

	/* Resume BH thread */
	WARN_ON(cw1200_bh_resume(priv));

	/* Resume delayed work */
	cw1200_resume_work(priv, &priv->bss_loss_work,
			state->bss_loss_tmo);
	cw1200_resume_work(priv, &priv->connection_loss_work,
			state->connection_loss_tmo);
	cw1200_resume_work(priv, &priv->join_timeout,
			state->join_tmo);
	cw1200_resume_work(priv, &priv->scan.probe_work,
			state->direct_probe);
	cw1200_resume_work(priv, &priv->link_id_gc_work,
			state->link_id_gc);

	/* Unlock datapath */
	wsm_unlock_tx(priv);

	/* Unlock scan */
	up(&priv->scan.lock);

	/* Unlock configuration mutex */
	mutex_unlock(&priv->conf_mutex);

	/* Free memory */
	kfree(state);

	return 0;
}

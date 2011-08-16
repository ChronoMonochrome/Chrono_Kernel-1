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

/* private */
struct cw1200_suspend_state {
	unsigned long bss_loss_tmo;
	unsigned long connection_loss_tmo;
	unsigned long join_tmo;
};

static struct dev_pm_ops cw1200_pm_ops = {
	.suspend_noirq = cw1200_suspend_late,
};
static struct platform_driver cw1200_power_driver = {
	.driver.name = "cw1200_power",
	.driver.pm = &cw1200_pm_ops,
};
static struct platform_device cw1200_power_device = {
	.name = "cw1200_power",
};

static void cw1200_pm_init_common(struct cw1200_pm_state *pm,
				  struct cw1200_common *priv)
{
	spin_lock_init(&pm->lock);
	cw1200_power_device.dev.platform_data = priv;
	platform_device_register(&cw1200_power_device);
	platform_driver_register(&cw1200_power_driver);
}

static void cw1200_pm_deinit_common(struct cw1200_pm_state *pm)
{
	platform_driver_unregister(&cw1200_power_driver);
	platform_device_unregister(&cw1200_power_device);
}

#ifdef CONFIG_WAKELOCK

void cw1200_pm_init(struct cw1200_pm_state *pm,
		    struct cw1200_common *priv)
{
	cw1200_pm_init_common(pm, priv);
	wake_lock_init(&pm->wakelock,
		WAKE_LOCK_SUSPEND, "cw1200_wlan");
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

static void cw1200_pm_stay_awake_tmo(unsigned long)
{
}

void cw1200_pm_init(struct cw1200_pm_state *pm)
{
	cw1200_init_common(pm);
	init_timer(&pm->stay_awake);
	pm->stay_awake.data = (unsigned long)pm;
	pm->stay_awake.function = cw1200_pm_stay_awake_tmo;
}

void cw1200_pm_deinit(struct cw1200_pm_state *pm)
{
	del_timer_sync(&pm->stay_awake);
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

int cw1200_wow_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan)
{
	struct cw1200_common *priv = hw->priv;
	struct cw1200_pm_state *pm_state = &priv->pm_state;
	struct cw1200_suspend_state *state;
	int ret;

#ifndef CONFIG_WAKELOCK
	spin_lock_bh(&pm->lock);
	ret = timer_pending(&pm->stay_awake);
	spin_unlock_bh(&pm->lock);
	if (ret)
		return -EAGAIN;
#endif

	/* Ensure pending operations are done. */
	ret = wait_event_interruptible_timeout(
		priv->channel_switch_done,
		!priv->channel_switch_in_progress, 3 * HZ);
	if (WARN_ON(!ret))
		return -ETIMEDOUT;
	else if (WARN_ON(ret < 0))
		return ret;

	/* Flush and lock TX. */
	ret = __cw1200_flush(priv, false);
	if (WARN_ON(ret < 0))
		return ret;

	/* Allocate state */
	state = kzalloc(sizeof(struct cw1200_suspend_state), GFP_KERNEL);
	if (!state) {
		wsm_unlock_tx(priv);
		return -ENOMEM;
	}

	/* Store delayed work states. */
	state->bss_loss_tmo =
		cw1200_suspend_work(&priv->bss_loss_work);
	state->connection_loss_tmo =
		cw1200_suspend_work(&priv->connection_loss_work);
	state->join_tmo =
		cw1200_suspend_work(&priv->join_timeout);

	/* Flush workqueue */
	flush_workqueue(priv->workqueue);

	/* Stop serving thread */
	cw1200_bh_suspend(priv);

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
	cw1200_bh_resume(priv);

	/* Resume delayed work */
	cw1200_resume_work(priv, &priv->bss_loss_work,
			state->bss_loss_tmo);
	cw1200_resume_work(priv, &priv->connection_loss_work,
			state->connection_loss_tmo);
	cw1200_resume_work(priv, &priv->join_timeout,
			state->join_tmo);

	/* Unlock datapath */
	wsm_unlock_tx(priv);

	/* Free memory */
	kfree(state);

	return 0;
}

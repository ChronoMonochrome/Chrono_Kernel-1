/*
 * Mac80211 power management interface for ST-Ericsson CW1200 mac80211 drivers
 *
 * Copyright (c) 2011, ST-Ericsson
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PM_H_INCLUDED
#define PM_H_INCLUDED

/* ******************************************************************** */
/* mac80211 API								*/

#ifdef CONFIG_PM
int cw1200_wow_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan);
int cw1200_wow_resume(struct ieee80211_hw *hw);
#endif /* CONFIG_PM */

#endif

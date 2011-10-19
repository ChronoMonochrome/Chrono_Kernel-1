/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Marcel Tunnissen <marcel.tuennissen@stericsson.com> for ST-Ericsson
 *
 * MOP500 board specific initialization for regulators
 */

#ifndef __BOARD_MOP500_MCDE_H
#define __BOARD_MOP500_MCDE_H

#include <video/mcde_display.h>

#ifdef CONFIG_DISPLAY_AB8500_TERTIARY
extern struct mcde_display_device tvout_ab8500_display;
#endif

#ifdef CONFIG_DISPLAY_AV8100_TERTIARY
extern struct mcde_display_device av8100_hdmi;
#endif

#endif /* __BOARD_MOP500_MCDE_H */

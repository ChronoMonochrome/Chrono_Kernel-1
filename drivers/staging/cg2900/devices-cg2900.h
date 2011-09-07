/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: Par-Gunnar Hjalmdahl <par-gunnar.p.hjalmdahl@stericsson.com>
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef __DEVICES_CG2900_H
#define __DEVICES_CG2900_H

#include "cg2900.h"

/**
 * enum cg2900_gpio_pull_sleep - GPIO pull setting in sleep.
 * @CG2900_NO_PULL:	Normal input in sleep (no pull up or down).
 * @CG2900_PULL_UP:	Pull up in sleep.
 * @CG2900_PULL_DN:	Pull down in sleep.
 */
enum cg2900_gpio_pull_sleep {
	CG2900_NO_PULL,
	CG2900_PULL_UP,
	CG2900_PULL_DN
};

/**
 * dcg2900_init_platdata() - Initializes platform data with callback functions.
 * @data:	Platform data.
 */
extern void dcg2900_init_platdata(struct cg2900_platform_data *data);

#endif /* __DEVICES_CG2900_H */

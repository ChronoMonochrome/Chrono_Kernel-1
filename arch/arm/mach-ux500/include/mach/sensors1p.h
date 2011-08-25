
/*
 * Copyright (C) 2009-2010 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 * Header file for 1 pin gpio sensors;
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com>
 *
 */

#ifndef __ASM_ARCH_SFH7741_H
#define __ASM_ARCH_SFH7741_H

struct sensor_config {
	int pin;
	int startup_time; /* in ms */
	char regulator[32];
};

struct sensors1p_config {
	struct sensor_config hal;
	struct sensor_config proximity;
};

#endif

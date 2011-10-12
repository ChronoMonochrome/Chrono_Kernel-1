/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Rickard Evertsson <rickard.evertsson@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 * Use this file to customize your reboot / sw reset reasons. Add, remove or
 * modify reasons in reboot_reasons[].
 */

#include <linux/kernel.h>
#include <mach/reboot_reasons.h>

struct reboot_reason reboot_reasons[] = {
	{"crash", SW_RESET_CRASH},
	{"", SW_RESET_NORMAL}, /* Normal Boot */
};

unsigned int reboot_reasons_size = ARRAY_SIZE(reboot_reasons);

/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License, version 2
 * Author: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 */

#ifndef __DB5500_KEYPAD_H
#define __DB5500_KEYPAD_H

#include <linux/input/matrix_keypad.h>

/**
 * struct db5500_keypad_platform_data - structure for platform specific data
 * @keymap_data: matrix scan code table for keycodes
 * @debounce_ms: platform specific debounce time
 * @no_autorepeat: flag for auto repetition
 */
struct db5500_keypad_platform_data {
	const struct matrix_keymap_data *keymap_data;
	u8 debounce_ms;
	bool no_autorepeat;
};

#endif

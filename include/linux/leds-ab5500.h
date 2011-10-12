/*
 * Copyright (C) 2011 ST-Ericsson SA.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for HVLED in ST-Ericsson AB5500 Analog baseband Controller
 *
 * Author: Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>
 */

#define AB5500_HVLED0		0
#define AB5500_HVLED1		1
#define AB5500_HVLED2		2
#define AB5500_HVLEDS_MAX	3

enum ab5500_led_status {
	AB5500_LED_OFF = 0x00,
	AB5500_LED_ON,
};

struct ab5500_led_conf {
	char *name;
	u8 led_id;
	enum ab5500_led_status status;
	u8 max_current;
};

struct ab5500_hvleds_platform_data {
	bool hw_blink;
	struct ab5500_led_conf leds[AB5500_HVLEDS_MAX];
};

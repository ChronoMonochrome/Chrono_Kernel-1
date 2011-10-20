/*
* Overview:
*   Header File defining vibrator kernel space interface
*
* Copyright (C) 2010 ST Ericsson
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef _STE_AUDIO_IO_VIBRATOR_H_
#define _STE_AUDIO_IO_VIBRATOR_H_

/* Client definitions which can use vibrator, defined as bitmask */
#define STE_AUDIOIO_CLIENT_AUDIO_L	1
#define STE_AUDIOIO_CLIENT_AUDIO_R	2
#define STE_AUDIOIO_CLIENT_FF_VIBRA     4
#define STE_AUDIOIO_CLIENT_TIMED_VIBRA  8

/*
 * Define vibrator's maximum speed allowed
 * Duty cycle supported by vibrator's PWM is 0-100
 */
#define STE_AUDIOIO_VIBRATOR_MAX_SPEED	100

/* Vibrator speed structure */
struct ste_vibra_speed {
	unsigned char positive;
	unsigned char negative;
};

/* Vibrator control function - uses PWM source */
int ste_audioio_vibrator_pwm_control(int client,
	struct ste_vibra_speed left_speed, struct ste_vibra_speed right_speed);

#endif

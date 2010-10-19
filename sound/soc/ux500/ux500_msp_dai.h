/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ola Lilja ola.o.lilja@stericsson.com,
 *         Roger Nilsson roger.xr.nilsson@stericsson.com
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef UX500_msp_dai_H
#define UX500_msp_dai_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/i2s/i2s.h>

#define UX500_NBR_OF_DAI 3

#define UX500_I2S_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |	\
			SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define UX500_I2S_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

#define FRAME_PER_SINGLE_SLOT_8_KHZ		31
#define FRAME_PER_SINGLE_SLOT_16_KHZ	124
#define FRAME_PER_SINGLE_SLOT_44_1_KHZ	63
#define FRAME_PER_SINGLE_SLOT_48_KHZ	49
#define FRAME_PER_2_SLOTS				31
#define FRAME_PER_8_SLOTS				138
#define FRAME_PER_16_SLOTS				277

#define UX500_MSP_INTERNAL_CLOCK_FREQ 40000000;
#define UX500_MSP_MIN_CHANNELS		1
#define UX500_MSP_MAX_CHANNELS		8

struct ux500_msp_dai_private {
	spinlock_t lock;
	struct i2s_device *i2s;
	unsigned int fmt;
	unsigned int tx_mask;
	unsigned int rx_mask;
	int slots;
	int slot_width;
};

extern struct snd_soc_dai ux500_msp_dai[UX500_NBR_OF_DAI];

int ux500_msp_dai_i2s_send_data(void *data, size_t bytes, int dai_idx);
int ux500_msp_dai_i2s_receive_data(void *data, size_t bytes, int dai_idx);

#endif

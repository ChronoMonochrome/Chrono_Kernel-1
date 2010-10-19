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
#ifndef UX500_PCM_H
#define UX500_PCM_H

#include <mach/msp.h>

#define UX500_PLATFORM_BUFFER_SIZE	(64*1024)

#define UX500_PLATFORM_MIN_RATE_PLAYBACK 8000
#define UX500_PLATFORM_MAX_RATE_PLAYBACK 48000
#define UX500_PLATFORM_MIN_RATE_CAPTURE	8000
#define UX500_PLATFORM_MAX_RATE_CAPTURE	48000

#define UX500_PLATFORM_MIN_CHANNELS 1
#define UX500_PLATFORM_MAX_CHANNELS 8
#define UX500_PLATFORM_MIN_PERIOD_BYTES 128

#define UX500_PLATFORM_PERIODS_QUEUED_DMA 5

extern struct snd_soc_platform ux500_soc_platform;

struct ux500_pcm_private {
	int msp_id;
	int stream_id;
	int period;
	unsigned int offset;
};

void ux500_pcm_dma_eot_handler(void *data);

#endif

/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#ifndef __AUDIOIO_HWCTRL_COMMON_H__
#define __AUDIOIO_HWCTRL_COMMON_H__

#include <linux/types.h>
#include <mach/ste_audio_io_ioctl.h>
/*
 * Defines
 */

#define MAX_GAIN				100
#define MIN_GAIN				0
#define	MAX_NO_CHANNELS				4
#define	MAX_NO_GAINS				3
#define	MAX_NO_LOOPS				1
#define	MAX_NO_LOOP_GAINS			1

struct gain_descriptor_t {
	int min_gain;
	int max_gain;
	uint gain_step;
};


/* Number of channels for each transducer */
extern const uint transducer_no_of_channels[MAX_NO_TRANSDUCERS];

/*
 * Maximum number of gains in each transducer path
 * all channels of a specific transducer have same max no of gains
 */
extern const uint transducer_no_of_gains[MAX_NO_TRANSDUCERS];

/* Maximum number of supported loops for each transducer */
extern const uint transducer_no_Of_supported_loop_indexes[MAX_NO_TRANSDUCERS];
extern const uint transducer_max_no_Of_supported_loops[MAX_NO_TRANSDUCERS];
extern const uint max_no_of_loop_gains[MAX_NO_TRANSDUCERS];
extern const int hs_analog_gain_table[16] ;

extern struct gain_descriptor_t gain_descriptor[MAX_NO_TRANSDUCERS]\
				[MAX_NO_CHANNELS][MAX_NO_GAINS];

#endif

/* End of audio_io_hwctrl_common.h */

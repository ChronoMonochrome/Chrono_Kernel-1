/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#ifndef _AUDIOIO_DEV_H_
#define _AUDIOIO_DEV_H_

#include <mach/ste_audio_io_ioctl.h>
#include "ste_audio_io_core.h"

union audioio_cmd_data_t {
	struct audioio_burst_ctrl_t  audioio_burst_ctrl;
	struct audioio_fade_ctrl_t  audioio_fade_ctrl;
	struct audioio_mute_trnsdr_t  audioio_mute_trnsdr;
	struct audioio_gain_ctrl_trnsdr_t  audioio_gain_ctrl_trnsdr;
	struct audioio_gain_desc_trnsdr_t  audioio_gain_desc_trnsdr;
	struct audioio_support_loop_t  audioio_support_loop;
	struct audioio_gain_loop_t  audioio_gain_loop;
	struct audioio_get_gain_t  audioio_get_gain;
	struct audioio_loop_ctrl_t  audioio_loop_ctrl;
	struct audioio_pwr_ctrl_t  audioio_pwr_ctrl;
	struct audioio_data_t  audioio_data;
	struct audioio_fsbitclk_ctrl_t audioio_fsbitclk_ctrl;
	struct audioio_acodec_pwr_ctrl_t audioio_acodec_pwr_ctrl;
	struct audioio_pseudoburst_ctrl_t audioio_pseudoburst_ctrl;
};


#endif /* _AUDIOIO_DEV_H_ */


/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#ifndef _AUDIOIO_CORE_H_
#define _AUDIOIO_CORE_H_

#include <mach/ste_audio_io_ioctl.h>
#include "ste_audio_io_func.h"
#include "ste_audio_io_hwctrl_common.h"

#define MAX_NO_CHANNELS			4

#define STE_AUDIOIO_POWER_AUDIO		1
#define STE_AUDIOIO_POWER_VIBRA		2

struct transducer_context_t {
	/* public variables */
	int gain[MAX_NO_CHANNELS];
	int is_muted[MAX_NO_CHANNELS];
	int is_power_up[MAX_NO_CHANNELS];
	/* public funcs */
	int (*pwr_up_func)(enum AUDIOIO_CH_INDEX, struct device *);
	int (*pwr_down_func)(enum AUDIOIO_CH_INDEX, struct device *);
	int (*pwr_state_func)(struct device *);
	int (*set_gain_func)(enum AUDIOIO_CH_INDEX, u16, int, u32,
						struct device *);
	int (*get_gain_func)(int *, int *, u16, struct device *);
	int (*mute_func)(enum AUDIOIO_CH_INDEX, struct device *);
	int (*unmute_func)(enum AUDIOIO_CH_INDEX, int *, struct device *);
	int (*mute_state_func)(struct device *);
	int (*enable_fade_func)(struct device *);
	int (*disable_fade_func)(struct device *);
	int (*switch_to_burst_func)(int, struct device *);
	int (*switch_to_normal_func)(struct device *);
	int (*enable_loop)(enum AUDIOIO_CH_INDEX, enum AUDIOIO_HAL_HW_LOOPS,
				int, struct device *, void *);
	int (*disable_loop)(enum AUDIOIO_CH_INDEX, enum AUDIOIO_HAL_HW_LOOPS,
						struct device *, void *);
};

struct audiocodec_context_t {
	int	audio_codec_powerup;
	int	is_audio_clk_enabled;
	enum	AUDIOIO_CLK_TYPE clk_type;
	int	power_client;
	int	vibra_client;
	struct mutex audio_io_mutex;
	struct mutex vibrator_mutex;
	struct transducer_context_t *transducer[MAX_NO_TRANSDUCERS];
	struct device *dev;
	int (*gpio_altf_init) (void);
	int (*gpio_altf_exit) (void);
};


int ste_audio_io_core_api_access_read(struct audioio_data_t *dev_data);

int ste_audio_io_core_api_access_write(struct audioio_data_t *dev_data);

int ste_audio_io_core_api_power_control_transducer(
			struct audioio_pwr_ctrl_t *pwr_ctrl);

int ste_audio_io_core_api_power_status_transducer(
					struct audioio_pwr_ctrl_t *pwr_ctrl);

int ste_audio_io_core_api_loop_control(struct audioio_loop_ctrl_t *loop_ctrl);

int ste_audio_io_core_api_loop_status(struct audioio_loop_ctrl_t *loop_ctrl);

int ste_audio_io_core_api_get_transducer_gain_capability(
		struct audioio_get_gain_t *get_gain);

int ste_audio_io_core_api_gain_capabilities_loop(
				struct audioio_gain_loop_t *gain_loop);

int ste_audio_io_core_api_supported_loops(
				struct audioio_support_loop_t *support_loop);

int ste_audio_io_core_api_gain_descriptor_transducer(
	struct audioio_gain_desc_trnsdr_t *gdesc_trnsdr);

int ste_audio_io_core_api_gain_control_transducer(
	struct audioio_gain_ctrl_trnsdr_t *gctrl_trnsdr);

int ste_audio_io_core_api_gain_query_transducer(
	struct audioio_gain_ctrl_trnsdr_t *gctrl_trnsdr);

int ste_audio_io_core_api_mute_control_transducer(
	struct audioio_mute_trnsdr_t *mute_trnsdr);

int ste_audio_io_core_api_mute_status_transducer(
	struct audioio_mute_trnsdr_t *mute_trnsdr);

int ste_audio_io_core_api_fading_control(struct audioio_fade_ctrl_t *fade_ctrl);

int ste_audio_io_core_api_burstmode_control(
				struct audioio_burst_ctrl_t *burst_ctrl);

int ste_audio_io_core_api_powerup_audiocodec(int power_client);

int ste_audio_io_core_api_powerdown_audiocodec(int power_client);

int ste_audio_io_core_api_init_data(struct platform_device *pdev);

bool ste_audio_io_core_is_ready_for_suspend(void);
void ste_audio_io_core_api_free_data(void);

int ste_audio_io_core_api_fsbitclk_control(
				struct audioio_fsbitclk_ctrl_t *fsbitclk_ctrl);
int ste_audio_io_core_api_pseudoburst_control(
		struct audioio_pseudoburst_ctrl_t *pseudoburst_ctrl);

void ste_audio_io_core_api_store_data(enum AUDIOIO_CH_INDEX channel_index,
							int *ptr, int value);

int ste_audioio_vibrator_alloc(int client, int mask);

void ste_audioio_vibrator_release(int client);

enum AUDIOIO_COMMON_SWITCH ste_audio_io_core_api_get_status(
				enum AUDIOIO_CH_INDEX channel_index, int *ptr);

int ste_audio_io_core_api_acodec_power_control(struct audioio_acodec_pwr_ctrl_t
							*audio_acodec_pwr_ctrl);

int ste_audio_io_core_api_fir_coeffs_control(struct audioio_fir_coefficients_t
						*fir_coeffs);

int ste_audio_io_core_clk_select_control(struct audioio_clk_select_t
						*clk_type);

int ste_audio_io_core_debug(int x);

#endif /* _AUDIOIO_CORE_H_ */


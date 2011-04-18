/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#ifndef _AUDIOIO_FUNC_H_
#define _AUDIOIO_FUNC_H_

#include <linux/string.h>
#include <linux/platform_device.h>
#include <mach/ste_audio_io_ioctl.h>
#include <linux/mfd/ab8500.h>
#include <linux/mfd/abx500.h>

#define AB8500_REV_10		0x10
#define AB8500_REV_11		0x11
#define AB8500_REV_20		0x20

#define AB8500_CTRL3_REG		0x00000200
#define AB8500_GPIO_DIR4_REG		0x00001013
#define AB8500_GPIO_DIR5_REG		0x00001014
#define AB8500_GPIO_OUT5_REG		0x00001024

extern struct platform_device *ste_audio_io_device;
extern struct regulator *regulator_avsource;

int dump_acodec_registers(const char *, struct device *dev);
int debug_audioio(int  x);

#define AB8500_BLOCK_ADDR(address) ((address >> 8) & 0xff)
#define AB8500_OFFSET_ADDR(address) (address & 0xff)

static inline unsigned char HW_REG_READ(unsigned short reg)
{
	unsigned char ret;
	int err;

	err = abx500_get_register_interruptible(&ste_audio_io_device->dev,
					  AB8500_BLOCK_ADDR(reg),
					  AB8500_OFFSET_ADDR(reg),
					  &ret);
	if (err < 0)
		return err;
	else
		return ret;
}

static inline int HW_REG_WRITE(unsigned short reg, unsigned char data)
{
	return abx500_set_register_interruptible(&ste_audio_io_device->dev,
						 AB8500_BLOCK_ADDR(reg),
						 AB8500_OFFSET_ADDR(reg),
						 data);
}

unsigned int ab8500_acodec_modify_write(unsigned int reg, u8 mask_set,
					u8 mask_clear);

#define HW_ACODEC_MODIFY_WRITE(reg, mask_set, mask_clear)\
	ab8500_acodec_modify_write(reg, mask_set, mask_clear)

unsigned int ab8500_modify_write(unsigned int reg, u8 mask_set, u8 mask_clear);

int ste_audio_io_power_up_headset(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_headset(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_headset_query(struct device *dev);
int ste_audio_io_set_headset_gain(enum AUDIOIO_CH_INDEX chnl_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_headset_gain(int *, int *, u16,
					struct device *dev);
int ste_audio_io_mute_headset(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_headset(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_headset_state(struct device *dev);
int ste_audio_io_enable_fade_headset(struct device *dev);
int ste_audio_io_disable_fade_headset(struct device *dev);
int ste_audio_io_switch_to_burst_mode_headset(int burst_fifo_switch_frame,
					struct device *dev);
int ste_audio_io_switch_to_normal_mode_headset(
					struct device *dev);

int ste_audio_io_power_up_earpiece(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_earpiece(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_earpiece_query(struct device *dev);
int ste_audio_io_set_earpiece_gain(enum AUDIOIO_CH_INDEX chnl_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_earpiece_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_earpiece(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_earpiece(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_earpiece_state(struct device *dev);
int ste_audio_io_enable_fade_earpiece(struct device *dev);
int ste_audio_io_disable_fade_earpiece(struct device *dev);

int ste_audio_io_power_up_ihf(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_ihf(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_ihf_query(struct device *dev);
int ste_audio_io_set_ihf_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_ihf_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_ihf(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_ihf(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_ihf_state(struct device *dev);
int ste_audio_io_enable_fade_ihf(struct device *dev);
int ste_audio_io_disable_fade_ihf(struct device *dev);

int ste_audio_io_power_up_vibl(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_vibl(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_vibl_query(struct device *dev);
int ste_audio_io_set_vibl_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_vibl_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_vibl(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_vibl(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_vibl_state(struct device *dev);
int ste_audio_io_enable_fade_vibl(struct device *dev);
int ste_audio_io_disable_fade_vibl(struct device *dev);

int ste_audio_io_power_up_vibr(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_vibr(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_vibr_query(struct device *dev);
int ste_audio_io_set_vibr_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_vibr_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_vibr(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_vibr(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_vibr_state(struct device *dev);
int ste_audio_io_enable_fade_vibr(struct device *dev);
int ste_audio_io_disable_fade_vibr(struct device *dev);

int ste_audio_io_power_up_mic1a(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_mic1a(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_mic1a_query(struct device *dev);
int ste_audio_io_set_mic1a_gain(enum AUDIOIO_CH_INDEX chnl_index,
	u16 gain_index, int gain_value, u32 linear, struct device *dev);
int ste_audio_io_get_mic1a_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_mic1a(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_mic1a(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_mic1a_state(struct device *dev);
int ste_audio_io_enable_fade_mic1a(struct device *dev);
int ste_audio_io_disable_fade_mic1a(struct device *dev);

/*
 *** Mic1b ***
 */
int ste_audio_io_power_up_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_mic1b_query(struct device *dev);
int ste_audio_io_set_mic1b_gain(enum AUDIOIO_CH_INDEX chnl_index,
		u16 gain_index, int gain_value, u32 linear, struct device *dev);
int ste_audio_io_get_mic1b_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_mic1b(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_mic1b_state(struct device *dev);
int ste_audio_io_enable_fade_mic1b(struct device *dev);
int ste_audio_io_disable_fade_mic1b(struct device *dev);
int ste_audio_io_enable_loop_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS,
					int loop_gain, struct device *dev,
					void *cookie);
int ste_audio_io_disable_loop_mic1b(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS hw_loop,
					struct device *dev, void *cookie);
/*
 *** Mic2 ***
 */
int ste_audio_io_power_up_mic2(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_mic2(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_mic2_query(struct device *dev);
int ste_audio_io_set_mic2_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_mic2_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_mic2(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_mic2(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_mic2_state(struct device *dev);
int ste_audio_io_enable_fade_mic2(struct device *dev);
int ste_audio_io_disable_fade_mic2(struct device *dev);

int ste_audio_io_power_up_lin(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_lin(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_lin_query(struct device *dev);
int ste_audio_io_set_lin_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_lin_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_lin(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_lin(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_lin_state(struct device *dev);
int ste_audio_io_enable_fade_lin(struct device *dev);
int ste_audio_io_disable_fade_lin(struct device *dev);

int ste_audio_io_power_up_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_dmic12_query(struct device *dev);
int ste_audio_io_set_dmic12_gain(enum AUDIOIO_CH_INDEX chnl_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_dmic12_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_dmic12(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_dmic12_state(struct device *dev);
int ste_audio_io_enable_fade_dmic12(struct device *dev);
int ste_audio_io_disable_fade_dmic12(struct device *dev);
int ste_audio_io_enable_loop_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS,
					int loop_gain, struct device *dev,
					void *cookie);
int ste_audio_io_disable_loop_dmic12(enum AUDIOIO_CH_INDEX chnl_index,
					enum AUDIOIO_HAL_HW_LOOPS hw_loop,
					struct device *dev, void *cookie);

int ste_audio_io_power_up_dmic34(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_dmic34(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_dmic34_query(struct device *dev);
int ste_audio_io_set_dmic34_gain(enum AUDIOIO_CH_INDEX chnl_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_dmic34_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_dmic34(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_dmic34(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_dmic34_state(struct device *dev);
int ste_audio_io_enable_fade_dmic34(struct device *dev);
int ste_audio_io_disable_fade_dmic34(struct device *dev);

int ste_audio_io_power_up_dmic56(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_dmic56(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_dmic56_query(struct device *dev);
int ste_audio_io_set_dmic56_gain(enum AUDIOIO_CH_INDEX chnl_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_dmic56_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_dmic56(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_dmic56(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_dmic56_state(struct device *dev);
int ste_audio_io_enable_fade_dmic56(struct device *dev);
int ste_audio_io_disable_fade_dmic56(struct device *dev);

int ste_audio_io_power_up_fmrx(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_fmrx(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_fmrx_query(struct device *dev);
int ste_audio_io_set_fmrx_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_fmrx_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_fmrx(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_fmrx(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_fmrx_state(struct device *dev);
int ste_audio_io_enable_fade_fmrx(struct device *dev);
int ste_audio_io_disable_fade_fmrx(struct device *dev);

int ste_audio_io_power_up_fmtx(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_fmtx(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_fmtx_query(struct device *dev);
int ste_audio_io_set_fmtx_gain(enum AUDIOIO_CH_INDEX chnl_index, u16 gain_index,
					int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_fmtx_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_fmtx(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_fmtx(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_fmtx_state(struct device *dev);
int ste_audio_io_enable_fade_fmtx(struct device *dev);
int ste_audio_io_disable_fade_fmtx(struct device *dev);

int ste_audio_io_power_up_bluetooth(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_down_bluetooth(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_power_state_bluetooth_query(struct device *dev);
int ste_audio_io_set_bluetooth_gain(enum AUDIOIO_CH_INDEX chnl_index,
			u16 gain_index, int gain_value, u32 linear,
					struct device *dev);
int ste_audio_io_get_bluetooth_gain(int*, int*, u16,
					struct device *dev);
int ste_audio_io_mute_bluetooth(enum AUDIOIO_CH_INDEX chnl_index,
					struct device *dev);
int ste_audio_io_unmute_bluetooth(enum AUDIOIO_CH_INDEX chnl_index, int *gain,
					struct device *dev);
int ste_audio_io_mute_bluetooth_state(struct device *dev);
int ste_audio_io_enable_fade_bluetooth(struct device *dev);
int ste_audio_io_disable_fade_bluetooth(struct device *dev);


#endif


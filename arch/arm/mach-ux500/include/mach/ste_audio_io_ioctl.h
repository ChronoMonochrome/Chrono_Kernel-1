/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Deepak KARDA/ deepak.karda@stericsson.com for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2.
 */

#ifndef _AUDIOIO_IOCTL_H_
#define _AUDIOIO_IOCTL_H_


#define AUDIOIO_IOC_MAGIC            'N'
#define AUDIOIO_READ_REGISTER        _IOWR(AUDIOIO_IOC_MAGIC, 1,\
						struct audioio_data_t)
#define AUDIOIO_WRITE_REGISTER       _IOW(AUDIOIO_IOC_MAGIC, 2,\
						struct audioio_data_t)
#define AUDIOIO_PWR_CTRL_TRNSDR      _IOW(AUDIOIO_IOC_MAGIC, 3,\
						struct audioio_pwr_ctrl_t)
#define AUDIOIO_PWR_STS_TRNSDR       _IOR(AUDIOIO_IOC_MAGIC, 4,\
						struct audioio_pwr_ctrl_t)
#define AUDIOIO_LOOP_CTRL            _IOW(AUDIOIO_IOC_MAGIC, 5,\
						struct audioio_loop_ctrl_t)
#define AUDIOIO_LOOP_STS             _IOR(AUDIOIO_IOC_MAGIC, 6,\
						struct audioio_loop_ctrl_t)
#define AUDIOIO_GET_TRNSDR_GAIN_CAPABILITY      _IOR(AUDIOIO_IOC_MAGIC, 7,\
						struct audioio_get_gain_t)
#define AUDIOIO_GAIN_CAP_LOOP        _IOR(AUDIOIO_IOC_MAGIC, 8,\
						struct audioio_gain_loop_t)
#define AUDIOIO_SUPPORT_LOOP         _IOR(AUDIOIO_IOC_MAGIC, 9,\
						struct audioio_support_loop_t)
#define AUDIOIO_GAIN_DESC_TRNSDR     _IOR(AUDIOIO_IOC_MAGIC, 10,\
					struct audioio_gain_desc_trnsdr_t)
#define AUDIOIO_GAIN_CTRL_TRNSDR     _IOW(AUDIOIO_IOC_MAGIC, 11,\
					struct audioio_gain_ctrl_trnsdr_t)
#define AUDIOIO_GAIN_QUERY_TRNSDR    _IOR(AUDIOIO_IOC_MAGIC, 12,\
					struct audioio_gain_ctrl_trnsdr_t)
#define AUDIOIO_MUTE_CTRL_TRNSDR     _IOW(AUDIOIO_IOC_MAGIC, 13,\
						struct audioio_mute_trnsdr_t)
#define AUDIOIO_MUTE_STS_TRNSDR      _IOR(AUDIOIO_IOC_MAGIC, 14,\
						struct audioio_mute_trnsdr_t)
#define AUDIOIO_FADE_CTRL		_IOW(AUDIOIO_IOC_MAGIC, 15,\
						struct audioio_fade_ctrl_t)
#define AUDIOIO_BURST_CTRL            _IOW(AUDIOIO_IOC_MAGIC, 16,\
						struct audioio_burst_ctrl_t)
#define AUDIOIO_READ_ALL_ACODEC_REGS_CTRL     _IOW(AUDIOIO_IOC_MAGIC, 17,\
				struct audioio_read_all_acodec_reg_ctrl_t)
#define AUDIOIO_FSBITCLK_CTRL            _IOW(AUDIOIO_IOC_MAGIC, 18,\
						struct audioio_fsbitclk_ctrl_t)
#define AUDIOIO_PSEUDOBURST_CTRL	_IOW(AUDIOIO_IOC_MAGIC, 19,\
					struct audioio_pseudoburst_ctrl_t)
#define AUDIOIO_AUDIOCODEC_PWR_CTRL	_IOW(AUDIOIO_IOC_MAGIC, 20, \
					struct audioio_acodec_pwr_ctrl_t)
#define AUDIOIO_FIR_COEFFS_CTRL		_IOW(AUDIOIO_IOC_MAGIC, 21, \
					struct audioio_fir_coefficients_t)
#define AUDIOIO_LOOP_GAIN_DESC_TRNSDR		_IOR(AUDIOIO_IOC_MAGIC, 22,\
					struct audioio_gain_desc_trnsdr_t)
#define AUDIOIO_CLK_SELECT_CTRL		_IOR(AUDIOIO_IOC_MAGIC, 23,\
					struct	audioio_clk_select_t)
/* audio codec channel ids */
#define EAR_CH			0
#define HS_CH			1
#define IHF_CH			2
#define VIBL_CH			3
#define VIBR_CH			4
#define MIC1A_CH		5
#define MIC1B_CH		6
#define MIC2_CH			7
#define LIN_CH			8
#define DMIC12_CH		9
#define DMIC34_CH		10
#define DMIC56_CH		11
#define MULTI_MIC_CH		12
#define FMRX_CH			13
#define FMTX_CH			14
#define BLUETOOTH_CH	15

#define FIRST_CH		EAR_CH
#define LAST_CH			BLUETOOTH_CH

#define MAX_NO_TRANSDUCERS	16
#define STE_AUDIOIO_MAX_COEFFICIENTS	128
#define MAX_NO_OF_LOOPS			19

#define AUDIOIO_TRUE		1
#define AUDIOIO_FALSE		0

enum AUDIOIO_CLK_TYPE {
	AUDIOIO_ULP_CLK,
	AUDIOIO_SYS_CLK
};

enum AUDIOIO_COMMON_SWITCH {
	AUDIOIO_COMMON_OFF = 0,
	AUDIOIO_COMMON_ON,
	AUDIOIO_COMMON_ALLCHANNEL_UNSUPPORTED = 0xFFFF
};

enum AUDIOIO_HAL_HW_LOOPS {
	AUDIOIO_NO_LOOP = 0x0,
	AUDIOIO_SIDETONE_LOOP = 0x01,
	AUDIOIO_MIC1B_TO_HFL = 0x02,
	AUDIOIO_MIC1B_TO_HFR = 0x04,
	AUDIOIO_MIC1B_TO_EAR = 0x08,
	AUDIOIO_MIC1A_TO_HSL = 0x10,
	AUDIOIO_MIC1A_TO_HSR = 0x20,
	AUDIOIO_MIC1A_TO_HSR_HSL = 0x40,
	AUDIOIO_LINEIN_TO_HF = 0x80,
	AUDIOIO_DMIC12_TO_HSR_HSL = 0x100,
	AUDIOIO_DIC34_TO_HSR_HSL = 0x200,
	AUDIOIO_DIC56_TO_HSR_HSL = 0x400,
	AUDIOIO_DMIC12_TO_ST = 0x800,
	AUDIOIO_DMIC34_TO_ST = 0x1000,
	AUDIOIO_DMIC56_TO_ST = 0x2000,
	AUDIOIO_ANC_LOOP = 0x4000,
	AUDIOIO_LININ_HS = 0x8000,
	AUDIOIO_LININL_HSL = 0x10000,
	AUDIOIO_LININ_HSR = 0x20000
};


enum AUDIOIO_FADE_PERIOD {
	e_FADE_00,
	e_FADE_01,
	e_FADE_10,
	e_FADE_11
};

enum AUDIOIO_CH_INDEX {
	e_CHANNEL_1 = 0x01,
	e_CHANNEL_2 = 0x02,
	e_CHANNEL_3 = 0x04,
	e_CHANNEL_4 = 0x08,
	e_CHANNEL_ALL = 0x0f
};

struct audioio_data_t {
	unsigned char	block;
	unsigned char	addr;
	unsigned char	data;
};

struct audioio_pwr_ctrl_t {
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
	int channel_type;
	enum AUDIOIO_CH_INDEX channel_index;
};

struct audioio_acodec_pwr_ctrl_t {
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
};

struct audioio_loop_ctrl_t  {
	enum AUDIOIO_HAL_HW_LOOPS hw_loop;
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
	int channel_type;
	enum AUDIOIO_CH_INDEX channel_index;
	int loop_gain;
};

struct audioio_get_gain_t {
	unsigned int num_channels;
	unsigned short max_num_gain;
};

struct audioio_gain_loop_t {
	int channel_type;
	unsigned short num_loop;
	unsigned short max_gains;
};

struct audioio_support_loop_t {
	int channel_type;
	unsigned short spprtd_loop_index;
};

struct audioio_gain_desc_trnsdr_t {
	enum AUDIOIO_CH_INDEX channel_index;
	int channel_type;
	unsigned short gain_index;
	int min_gain;
	int max_gain;
	unsigned int gain_step;
};

struct audioio_gain_ctrl_trnsdr_t {
	enum AUDIOIO_CH_INDEX channel_index;
	int channel_type;
	unsigned short gain_index;
	int gain_value;
	unsigned int linear;
};

struct audioio_mute_trnsdr_t  {
	int channel_type;
	enum AUDIOIO_CH_INDEX channel_index;
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
};

struct audioio_fade_ctrl_t  {
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
	enum AUDIOIO_FADE_PERIOD fade_period;
	int channel_type;
	enum AUDIOIO_CH_INDEX channel_index;
};

struct audioio_burst_ctrl_t {
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
	int channel_type;
	int burst_fifo_interrupt_sample_count;
	int burst_fifo_length;/* BFIFOTx */
	int burst_fifo_switch_frame;
	int burst_fifo_sample_number;
};

struct audioio_read_all_acodec_reg_ctrl_t {
	unsigned char data[200];
};

struct audioio_fsbitclk_ctrl_t {
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
};

struct audioio_pseudoburst_ctrl_t {
	enum AUDIOIO_COMMON_SWITCH ctrl_switch;
};

struct audioio_fir_coefficients_t {
	unsigned char start_addr;
	unsigned short coefficients[STE_AUDIOIO_MAX_COEFFICIENTS];
};

struct audioio_clk_select_t {
	enum AUDIOIO_CLK_TYPE required_clk;
};
#endif

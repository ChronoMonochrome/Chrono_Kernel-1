/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Deepak Karda
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

/*-----------------------------------------------------------------------------
* Common Includes
*---------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/soundcard.h>
#include <linux/sound.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/i2s/i2s.h>
#include <mach/msp.h>
#include <linux/gpio.h>
/*#include <mach/i2c.h>*/
#include <mach/debug.h>
#include <mach/u8500_acodec_ab8500.h>
#include <mach/ab8500.h>

#ifdef CONFIG_U8500_AB8500_CUT10
#include <mach/ab8500_codec_v1_0.h>
#endif
#ifdef CONFIG_U8500_AB8500_ED
#include <mach/ab8500_codec.h>
#endif

#define ELEMENT_SIZE 0
#define FRAME_SIZE -1
#define MSP_NUM 0

/* Debugging stuff */

#define ACODEC_NAME 	  "DRIVER ACODEC"
#define DRIVER_DEBUG	        CONFIG_STM_ACODEC_DEBUG	/* enables/disables debug msgs */
#define DRIVER_DEBUG_PFX	ACODEC_NAME	/* msg header represents this module */
#define DRIVER_DBG	        KERN_ERR	/* message level */
#define NMDK_DEBUG             CONFIG_STM_ACODEC_DEBUG
extern struct driver_debug_st DBG_ST;

#if NMDK_DEBUG > 0
t_ab8500_codec_error dump_acodec_registers(void);
t_ab8500_codec_error dump_msp_registers(void);
#endif

#ifdef CONFIG_U8500_ACODEC_DMA
static void u8500_digital_lpbk_tx_dma_start(void);
static void u8500_digital_lpbk_rx_dma_start(void);
#endif

int second_config;
/*----------------------------------------------------------------------------
* global declarations
*---------------------------------------------------------------------------*/
t_u8500_codec_system_context g_codec_system_context;

int u8500_acodec_rates[MAX_NO_OF_RATES] = { 48000 };

char *codec_dest_texts[NUMBER_OUTPUT_DEVICE] = {
	"CODEC_DEST_HEADSET", "CODEC_DEST_EARPIECE", "CODEC_DEST_HANDSFREE",
	"CODEC_DEST_VIBRATOR1", "CODEC_DEST_VIBRATOR2"
};

char *codec_in_texts[NUMBER_INPUT_DEVICE] = {
	"CODEC_SRC_LINEIN", "CODEC_SRC_MICROPHONE_1A",
	"CODEC_SRC_MICROPHONE_1B",
	"CODEC_SRC_MICROPHONE_2", "CODEC_SRC_D_MICROPHONE_1",
	"CODEC_SRC_D_MICROPHONE_2",
	"CODEC_SRC_D_MICROPHONE_3", "CODEC_SRC_D_MICROPHONE_4",
	"CODEC_SRC_D_MICROPHONE_5",
	"CODEC_SRC_D_MICROPHONE_6", "CODEC_SRC_D_MICROPHONE_12",
	"CODEC_SRC_D_MICROPHONE_34",
	"CODEC_SRC_D_MICROPHONE_56"
};

char *lpbk_state_in_texts[NUMBER_LOOPBACK_STATE] = { "DISABLE", "ENABLE" };
char *switch_state_in_texts[NUMBER_SWITCH_STATE] = { "DISABLE", "ENABLE" };
char *power_state_in_texts[NUMBER_POWER_STATE] = { "DISABLE", "ENABLE" };
char *tdm_mode_state_in_texts[NUMBER_POWER_STATE] = { "DISABLE", "ENABLE" };
char *direct_rendering_state_in_texts[NUMBER_DIRECT_RENDERING_STATE] =
    { "DISABLE", "ENABLE" };
char *pcm_rendering_state_in_texts[NUMBER_PCM_RENDERING_STATE] =
    { "DISABLE", "ENABLE", "PENDING" };

EXPORT_SYMBOL(codec_dest_texts);
EXPORT_SYMBOL(codec_in_texts);

static void ab8500_codec_power_init(void);
static int check_device_id();
t_ab8500_codec_error perform_src_routing(t_ab8500_codec_src input_device);
t_ab8500_codec_error
    u8500_acodec_allocate_all_mono_slots
    (t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line1);
t_ab8500_codec_error
    u8500_acodec_allocate_all_stereo_slots
    (t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line1,
     t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line2);

#if 0				//from Arnaud
/* For Codec in Master mode for recording*/
struct msp_protocol_desc protocol_desc_tdm_mode = {
	MSP_DATA_TRANSFER_WIDTH_HALFWORD,	/*rx_data_transfer_width */
	MSP_DATA_TRANSFER_WIDTH_HALFWORD,	/*tx_data_transfer_width */
	MSP_SINGLE_PHASE,	/*rx_phase_mode                 */
	MSP_SINGLE_PHASE,	/*tx_phase_mode                 */
	MSP_PHASE2_START_MODE_IMEDIATE,	/*rx_phase2_start_mode  */
	MSP_PHASE2_START_MODE_IMEDIATE,	/*tx_phase2_start_mode  */
	MSP_BTF_MS_BIT_FIRST,	/*rx_endianess                  */
	MSP_BTF_MS_BIT_FIRST,	/*tx_endianess                  */
	MSP_FRAME_LENGTH_2,	/*rx_frame_length_1             */
	MSP_FRAME_LENGTH_2,	/*rx_frame_length_2             */
	MSP_FRAME_LENGTH_2,	/*tx_frame_length_1             */
	MSP_FRAME_LENGTH_2,	/*tx_frame_length_2             */
	MSP_ELEM_LENGTH_16,	/*rx_element_length_1   */
	MSP_ELEM_LENGTH_16,	/*rx_element_length_2   */
	MSP_ELEM_LENGTH_16,	/*tx_element_length_1   */
	MSP_ELEM_LENGTH_16,	/*tx_element_length_2   */
	MSP_DELAY_0,		/*rx_data_delay                 */
	MSP_DELAY_0,		/*tx_data_delay                 */
	MSP_FALLING_EDGE,	/*rx_clock_pol                  */
	MSP_RISING_EDGE,	/*tx_clock_pol                  */
	MSP_FRAME_SYNC_POL_ACTIVE_HIGH,	/*rx_msp_frame_pol              */
	MSP_FRAME_SYNC_POL_ACTIVE_HIGH,	/*tx_msp_frame_pol              */
	MSP_HWS_NO_SWAP,	/*rx_half_word_swap             */
	MSP_HWS_NO_SWAP,	/*tx_half_word_swap             */
	MSP_COMPRESS_MODE_LINEAR,	/*compression_mode              */
	MSP_EXPAND_MODE_LINEAR,	/*expansion_mode                */
	MSP_SPI_CLOCK_MODE_NON_SPI,	/*spi_clk_mode                  */
	MSP_SPI_BURST_MODE_DISABLE,	/*spi_burst_mode                */
	63,			/*frame_period                  */
	31,			/*frame_width                   */
	64,			/*total_clocks_for_one_frame            */
};
#endif

#if 0				//from HCL
/* For Codec in Master mode for recording*/
struct msp_protocol_desc protocol_desc_tdm_mode = {
	MSP_DATA_TRANSFER_WIDTH_WORD,	/*rx_data_transfer_width */
	MSP_DATA_TRANSFER_WIDTH_WORD,	/*tx_data_transfer_width */
	MSP_DUAL_PHASE,		/*rx_phase_mode                 */
	MSP_DUAL_PHASE,		/*tx_phase_mode                 */
	MSP_PHASE2_START_MODE_FRAME_SYNC,	/*rx_phase2_start_mode  */
	MSP_PHASE2_START_MODE_FRAME_SYNC,	/*tx_phase2_start_mode  */
	MSP_BTF_MS_BIT_FIRST,	/*rx_endianess                  */
	MSP_BTF_MS_BIT_FIRST,	/*tx_endianess                  */
	MSP_FRAME_LENGTH_1,	/*rx_frame_length_1             */
	MSP_FRAME_LENGTH_1,	/*rx_frame_length_2             */
	MSP_FRAME_LENGTH_1,	/*tx_frame_length_1             */
	MSP_FRAME_LENGTH_1,	/*tx_frame_length_2             */
	MSP_ELEM_LENGTH_16,	/*rx_element_length_1   */
	MSP_ELEM_LENGTH_16,	/*rx_element_length_2   */
	MSP_ELEM_LENGTH_16,	/*tx_element_length_1   */
	MSP_ELEM_LENGTH_16,	/*tx_element_length_2   */
	MSP_DELAY_0,		/*rx_data_delay                 */
	MSP_DELAY_0,		/*tx_data_delay                 */
	MSP_RISING_EDGE,	/*rx_clock_pol                  */
	MSP_RISING_EDGE,	/*tx_clock_pol                  */
	MSP_FRAME_SYNC_POL_ACTIVE_HIGH,	/*rx_msp_frame_pol              */
	MSP_FRAME_SYNC_POL_ACTIVE_HIGH,	/*tx_msp_frame_pol              */
	MSP_HWS_NO_SWAP,	/*rx_half_word_swap             */
	MSP_HWS_NO_SWAP,	/*tx_half_word_swap             */
	MSP_COMPRESS_MODE_LINEAR,	/*compression_mode              */
	MSP_EXPAND_MODE_LINEAR,	/*expansion_mode                */
	MSP_SPI_CLOCK_MODE_NON_SPI,	/*spi_clk_mode                  */
	MSP_SPI_BURST_MODE_DISABLE,	/*spi_burst_mode                */
	255,			/*frame_period                  */
	0,			/*frame_width                   */
	256,			/*total_clocks_for_one_frame            */
};

#endif

#if 0				//from STS
struct msp_protocol_desc protocol_desc_tdm_mode = {
	MSP_DATA_TRANSFER_WIDTH_HALFWORD,	/*rx_data_transfer_width */
	MSP_DATA_TRANSFER_WIDTH_HALFWORD,	/*tx_data_transfer_width */
	MSP_SINGLE_PHASE,	/*rx_phase_mode                 */
	MSP_SINGLE_PHASE,	/*tx_phase_mode                 */
	MSP_PHASE2_START_MODE_IMEDIATE,	/*rx_phase2_start_mode  */
	MSP_PHASE2_START_MODE_IMEDIATE,	/*tx_phase2_start_mode  */
	MSP_BTF_MS_BIT_FIRST,	/*rx_endianess                  */
	MSP_BTF_MS_BIT_FIRST,	/*tx_endianess                  */
	MSP_FRAME_LENGTH_2,	/*rx_frame_length_1             */
	MSP_FRAME_LENGTH_1,	/*rx_frame_length_2             */
	MSP_FRAME_LENGTH_2,	/*tx_frame_length_1             */
	MSP_FRAME_LENGTH_1,	/*tx_frame_length_2             */
	MSP_ELEM_LENGTH_16,	/*rx_element_length_1   */
	MSP_ELEM_LENGTH_16,	/*rx_element_length_2   */
	MSP_ELEM_LENGTH_16,	/*tx_element_length_1   */
	MSP_ELEM_LENGTH_16,	/*tx_element_length_2   */
	MSP_DELAY_0,		/*rx_data_delay                 */
	MSP_DELAY_0,		/*tx_data_delay                 */
	MSP_FALLING_EDGE,	/*rx_clock_pol                  */
	MSP_RISING_EDGE,	/*tx_clock_pol                  */
	MSP_FRAME_SYNC_POL_ACTIVE_HIGH,	/*rx_msp_frame_pol              */
	MSP_FRAME_SYNC_POL_ACTIVE_HIGH,	/*tx_msp_frame_pol              */
	MSP_HWS_NO_SWAP,	/*rx_half_word_swap             */
	MSP_HWS_NO_SWAP,	/*tx_half_word_swap             */
	MSP_COMPRESS_MODE_LINEAR,	/*compression_mode              */
	MSP_EXPAND_MODE_LINEAR,	/*expansion_mode                */
	MSP_SPI_CLOCK_MODE_NON_SPI,	/*spi_clk_mode                  */
	MSP_SPI_BURST_MODE_DISABLE,	/*spi_burst_mode                */
	25,			/*frame_period                  */
	32,			/*frame_width                   */
	32,			/*total_clocks_for_one_frame            */
};
#endif

#define DIGITAL_LPBK_MAX_BIFFERS	3

#ifdef CONFIG_U8500_AB8500_CUT10
#define NB_OF_CHANNEL_USED	 		8
#else
#define NB_OF_CHANNEL_USED	 		6
#endif

#define MONO_SRC	1
#define STEREO_SRC	2

typedef struct {
	unsigned char *area;	/* virtual pointer */
	dma_addr_t addr;	/* physical address */
} t_dma_buffer;

typedef struct {
	struct completion tx_dma_com;
	struct completion rx_dma_com;
	volatile int rx_active;
	volatile int tx_active;
	t_dma_buffer buffer;
	int data_size;
	int rx_index;
	int tx_index;
} t_digital_lpbk_cnxt;

const int play_flag = 1;
const int capture_flag = 2;

t_digital_lpbk_cnxt digital_lpbk_cnxt;

void u8500_set_defaults()
{
	int i;

	for (i = 0; i < NUMBER_INPUT_DEVICE; i++) {
		g_codec_system_context.input_config[i].left_volume = 0;
		g_codec_system_context.input_config[i].right_volume = 0;
		g_codec_system_context.input_config[i].mute_state = DISABLE;
		g_codec_system_context.input_config[i].power_state = DISABLE;
	}

	for (i = 0; i < NUMBER_OUTPUT_DEVICE; i++) {
		g_codec_system_context.output_config[i].left_volume = 0;
		g_codec_system_context.output_config[i].right_volume = 0;
		g_codec_system_context.output_config[i].mute_state = DISABLE;
		g_codec_system_context.output_config[i].power_state = DISABLE;
	}

}				//END OF FUNCTION

struct i2sdrv_data *i2sdrv[MAX_I2S_CLIENTS];

t_ab8500_codec_error u8500_acodec_open(int client_id, int stream_id)
{
	struct i2sdrv_data *p_i2sdrv_data = NULL;
	struct i2s_device *i2s;

	p_i2sdrv_data = i2sdrv[client_id];

	if (!p_i2sdrv_data)
		return (-1);

	i2s = p_i2sdrv_data->i2s;

	if (stream_id == 0)	//PLAYBACK
	{
		if (p_i2sdrv_data->tx_status)
			return -1;
		else {
			p_i2sdrv_data->tx_status = 1;
		}
	} else if (stream_id == 1)	//CAPTURE
	{
		if (p_i2sdrv_data->rx_status)
			return -1;
		else {
			p_i2sdrv_data->rx_status = 1;
		}
	}

	p_i2sdrv_data->flag = 0;

	return 0;
}

t_ab8500_codec_error u8500_acodec_send_data(int client_id, void *data,
					    size_t bytes, int dma_flag)
{
	struct i2sdrv_data *p_i2sdrv_data = NULL;
	struct i2s_device *i2s_dev = NULL;
	int bytes_transmit;
	struct i2s_message message;

	p_i2sdrv_data = i2sdrv[client_id];

	if (!p_i2sdrv_data)
		return (-1);

	i2s_dev = p_i2sdrv_data->i2s;

	if (p_i2sdrv_data->flag) {
		stm_dbg(DBG_ST.acodec, " I2S controller not available\n");
		return -1;
	}

	message.i2s_transfer_mode = I2S_TRANSFER_MODE_SINGLE_DMA;
	message.i2s_direction = I2S_DIRECTION_TX;
	message.txbytes = bytes;
	message.txdata = data;
	message.dma_flag = dma_flag;

	bytes_transmit = i2s_transfer(i2s_dev->controller, &message);

	if (bytes_transmit < 0) {
		printk("error in transfer\n");
		return -1;
	}
	return bytes_transmit;

}

t_ab8500_codec_error u8500_acodec_loopback_configure(int client_id, void *data,
						     size_t bytes, int dma_flag)
{
	struct i2sdrv_data *p_i2sdrv_data = NULL;
	struct i2s_device *i2s_dev = NULL;
	int bytes_receive;
	struct i2s_message message;

	p_i2sdrv_data = i2sdrv[client_id];

	if (!p_i2sdrv_data)
		return (-1);

	i2s_dev = p_i2sdrv_data->i2s;

	if (p_i2sdrv_data->flag) {
		stm_dbg(DBG_ST.acodec, " I2S controller not available\n");
		return -1;
	}

	message.i2s_transfer_mode = I2S_TRANSFER_MODE_INF_LOOPBACK;
	message.rxbytes = bytes;
	message.rxdata = data;
	message.txbytes = bytes;
	message.txdata = data;
	message.dma_flag = dma_flag;

	bytes_receive = i2s_transfer(i2s_dev->controller, &message);

	if (bytes_receive < 0) {
		printk(" not get\n");
		return -1;
	}
	return bytes_receive;

}

t_ab8500_codec_error u8500_acodec_receive_data(int client_id, void *data,
					       size_t bytes, int dma_flag)
{
	struct i2sdrv_data *p_i2sdrv_data = NULL;
	struct i2s_device *i2s_dev = NULL;
	int bytes_receive;
	struct i2s_message message;

	p_i2sdrv_data = i2sdrv[client_id];

	if (!p_i2sdrv_data)
		return (-1);

	i2s_dev = p_i2sdrv_data->i2s;

	if (p_i2sdrv_data->flag) {
		stm_dbg(DBG_ST.acodec, " I2S controller not available\n");
		return -1;
	}

	message.i2s_transfer_mode = I2S_TRANSFER_MODE_SINGLE_DMA;
	message.i2s_direction = I2S_DIRECTION_RX;
	message.rxbytes = bytes;
	message.rxdata = data;
	message.dma_flag = dma_flag;

	bytes_receive = i2s_transfer(i2s_dev->controller, &message);

	if (bytes_receive < 0) {
		printk(" not get\n");
		return -1;
	}
	return bytes_receive;

}

t_ab8500_codec_error u8500_acodec_close(int client_id, t_acodec_disable flag)
{
	struct i2sdrv_data *p_i2sdrv_data = NULL;
	struct i2s_device *i2s_dev = NULL;
	int status = 0;

	p_i2sdrv_data = i2sdrv[client_id];

	if (!p_i2sdrv_data)
		return (-1);

	i2s_dev = p_i2sdrv_data->i2s;

	if (p_i2sdrv_data->flag) {
		stm_dbg(DBG_ST.acodec, " I2S controller not available\n");
		return -1;
	}

	if (flag == DISABLE_ALL) {
		p_i2sdrv_data->flag = -1;
		p_i2sdrv_data->tx_status = 0;
		p_i2sdrv_data->rx_status = 0;
	} else if (flag == DISABLE_TRANSMIT) {
		p_i2sdrv_data->tx_status = 0;
	} else if (flag == DISABLE_RECEIVE) {
		p_i2sdrv_data->rx_status = 0;
	}
	status = i2s_cleanup(i2s_dev->controller, flag);
	if (status) {
		return -1;
	}

	return 0;
}

/**
* u8500_acodec_enable_audio_mode
*
* @direction - direction of data flow (from/to) audiocode
* @mspClockSel - clock for MSP
* @mspInClockFreq - input clock for MSP
* @channels - number of channel, 1 for mono and 2 for stereo
*
* It configures the audiocodec in audio mode. In this case,the I2S
* protocol is used for data exchanges.
*/

t_ab8500_codec_error u8500_acodec_enable_audio_mode(struct acodec_configuration
						    * acodec_config)
{
	struct i2s_device *i2s_dev = NULL;
	t_ab8500_codec_error error_status = AB8500_CODEC_OK;
	struct msp_config msp_config;
	t_ab8500_codec_error codec_error;
	t_ab8500_codec_mode codec_in_mode = AB8500_CODEC_MODE_MANUAL_SETTING;
	t_ab8500_codec_mode codec_out_mode = AB8500_CODEC_MODE_MANUAL_SETTING;
	t_ab8500_codec_direction codec_direction;
/*#ifdef CONFIG_U8500_AB8500_CUT10*/
#if 1
	t_ab8500_codec_tdm_config tdm_config;
#endif

	memset(&msp_config, 0, sizeof(msp_config));

	FUNC_ENTER();
	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_enable_audio_mode()\n");

	if (i2sdrv[I2S_CLIENT_MSP1]->flag) {
		stm_dbg(DBG_ST.acodec, " I2S controller not available\n");
		return -1;
	}

	i2s_dev = i2sdrv[I2S_CLIENT_MSP1]->i2s;

	if (g_codec_system_context.cur_user == NO_USER) {
		stm_error("Audiocodec not yet configured by any user\n");
		return (AB8500_CODEC_ERROR);
	} else if (g_codec_system_context.cur_user != acodec_config->user) {
		stm_error
		    (" Trying to acces audiocodec already in use by user %d\n",
		     g_codec_system_context.cur_user);
		return (AB8500_CODEC_ERROR);
	}

	switch (acodec_config->direction) {
	case AB8500_CODEC_DIRECTION_INOUT:
		codec_direction = AB8500_CODEC_DIRECTION_INOUT;
		codec_in_mode = AB8500_CODEC_MODE_VOICE;	//HIFI
		codec_out_mode = AB8500_CODEC_MODE_VOICE;	//VOICE
		break;
	case AB8500_CODEC_DIRECTION_IN:
		codec_direction = AB8500_CODEC_DIRECTION_IN;
		codec_in_mode = AB8500_CODEC_MODE_VOICE;	//HIFI
		break;
	case AB8500_CODEC_DIRECTION_OUT:
		codec_direction = AB8500_CODEC_DIRECTION_OUT;
		codec_out_mode = AB8500_CODEC_MODE_VOICE;	//HIFI
		break;
	default:
		stm_error("Invalid direction\n");
		return AB8500_CODEC_ERROR;
	}

	/* MSP configuration  */

	msp_config.tx_clock_sel = 0;	//TX_CLK_SEL_SRG;
	msp_config.rx_clock_sel = 0;	//RX_CLK_SEL_SRG;

	msp_config.tx_frame_sync_sel = 0;	//0x00000400; Frame synchronization signal is provided by an external source. MSPTFS is an input pin
	msp_config.rx_frame_sync_sel = 0;	//0: Rx Frame synchronization signal is provided by an external source. MSPRFS is an input pin

	msp_config.input_clock_freq = MSP_INPUT_FREQ_48MHZ;

	msp_config.srg_clock_sel = 0;	//0x000C0000

	//msp_config.rx_endianess = MSP_BIG_ENDIAN;
	//msp_config.tx_endianess = MSP_BIG_ENDIAN;

	msp_config.rx_frame_sync_pol = RX_FIFO_SYNC_HI;
	msp_config.tx_frame_sync_pol = TX_FIFO_SYNC_HI;

	//msp_config.rx_unexpect_frame_sync = MSP_UNEXPECTED_FS_IGNORE;
	//msp_config.tx_unexpect_frame_sync = MSP_UNEXPECTED_FS_IGNORE;

	msp_config.rx_fifo_config = RX_FIFO_ENABLE;
	msp_config.tx_fifo_config = TX_FIFO_ENABLE;

	msp_config.spi_clk_mode = SPI_CLK_MODE_NORMAL;
	msp_config.spi_burst_mode = 0;

	msp_config.handler = acodec_config->handler;
	msp_config.tx_callback_data = acodec_config->tx_callback_data;
	msp_config.tx_data_enable = 0;
	msp_config.rx_callback_data = acodec_config->rx_callback_data;

	msp_config.loopback_enable = 0;
	msp_config.multichannel_configured = 0;

	msp_config.def_elem_len = 0;
	//msp_config.loopback_enable  = g_codec_system_context.msp_loopback;

	stm_dbg(DBG_ST.acodec, "  msp_config.loopback_enable = 0x%x \n",
		msp_config.loopback_enable);

#if 0
	msp_config.default_protocol_desc = 1;
#else
	msp_config.default_protocol_desc = 0;
	msp_config.protocol_desc.rx_phase_mode = MSP_SINGLE_PHASE;
	msp_config.protocol_desc.tx_phase_mode = MSP_SINGLE_PHASE;
	msp_config.protocol_desc.rx_phase2_start_mode =
	    MSP_PHASE2_START_MODE_IMEDIATE;
	msp_config.protocol_desc.tx_phase2_start_mode =
	    MSP_PHASE2_START_MODE_IMEDIATE;
	msp_config.protocol_desc.rx_bit_transfer_format = MSP_BTF_MS_BIT_FIRST;
	msp_config.protocol_desc.tx_bit_transfer_format = MSP_BTF_MS_BIT_FIRST;
	msp_config.protocol_desc.rx_frame_length_1 = MSP_FRAME_LENGTH_1;
	msp_config.protocol_desc.rx_frame_length_2 = MSP_FRAME_LENGTH_1;
	msp_config.protocol_desc.tx_frame_length_1 = MSP_FRAME_LENGTH_1;
	msp_config.protocol_desc.tx_frame_length_2 = MSP_FRAME_LENGTH_1;
	msp_config.protocol_desc.rx_element_length_1 = MSP_ELEM_LENGTH_32;
	msp_config.protocol_desc.rx_element_length_2 = MSP_ELEM_LENGTH_32;
	msp_config.protocol_desc.tx_element_length_1 = MSP_ELEM_LENGTH_32;
	msp_config.protocol_desc.tx_element_length_2 = MSP_ELEM_LENGTH_32;
	msp_config.protocol_desc.rx_data_delay = MSP_DELAY_0;
	msp_config.protocol_desc.tx_data_delay = MSP_DELAY_0;
	msp_config.protocol_desc.rx_clock_pol = MSP_RISING_EDGE;
	msp_config.protocol_desc.tx_clock_pol = MSP_FALLING_EDGE;
	msp_config.protocol_desc.rx_frame_sync_pol =
	    MSP_FRAME_SYNC_POL_ACTIVE_HIGH;
	msp_config.protocol_desc.tx_frame_sync_pol =
	    MSP_FRAME_SYNC_POL_ACTIVE_HIGH;
	msp_config.protocol_desc.rx_half_word_swap = MSP_HWS_NO_SWAP;
	msp_config.protocol_desc.tx_half_word_swap = MSP_HWS_NO_SWAP;
	msp_config.protocol_desc.compression_mode = MSP_COMPRESS_MODE_LINEAR;
	msp_config.protocol_desc.expansion_mode = MSP_EXPAND_MODE_LINEAR;
	msp_config.protocol_desc.spi_clk_mode = MSP_SPI_CLOCK_MODE_NON_SPI;
	msp_config.protocol_desc.spi_burst_mode = MSP_SPI_BURST_MODE_DISABLE;
	msp_config.protocol_desc.frame_sync_ignore = MSP_FRAME_SYNC_IGNORE;
	msp_config.protocol_desc.frame_period = 63;
	msp_config.protocol_desc.frame_width = 31;
	msp_config.protocol_desc.total_clocks_for_one_frame = 64;

#endif

	msp_config.direction = MSP_BOTH_T_R_MODE;
	msp_config.protocol = MSP_PCM_PROTOCOL;	//MSP_I2S_PROTOCOL
	msp_config.frame_size = ELEMENT_SIZE;
	//  msp_config.frame_freq = freq; $kardad$
	msp_config.frame_freq = CODEC_SAMPLING_FREQ_48KHZ;

	/* enable msp for both tr and rx mode with dma data transfer. THIS IS NOW DONE SEPARATELY from SAA. */

	if (acodec_config->channels == 1)
		msp_config.data_size = MSP_DATA_SIZE_16BIT;
	else
		msp_config.data_size = MSP_DATA_SIZE_32BIT;

#ifdef CONFIG_U8500_ACODEC_DMA
	msp_config.work_mode = MSP_DMA_MODE;
#elif defined(CONFIG_U8500_ACODEC_POLL)
	msp_config.work_mode = MSP_POLLING_MODE;
#else
	msp_config.work_mode = MSP_INTERRUPT_MODE;
#endif

	if (DISABLE == acodec_config->direct_rendering_mode) {
		msp_config.multichannel_configured = 1;
		msp_config.multichannel_config.tx_multichannel_enable = 1;
		if (acodec_config->channels == 1) {
			msp_config.multichannel_config.tx_channel_0_enable =
			    0x0000001;
		} else {
			msp_config.multichannel_config.tx_channel_0_enable =
			    0x0000003;
		}
		msp_config.multichannel_config.tx_channel_1_enable = 0x0000000;
		msp_config.multichannel_config.tx_channel_2_enable = 0x0000000;
		msp_config.multichannel_config.tx_channel_3_enable = 0x0000000;

		msp_config.multichannel_config.rx_multichannel_enable = 1;

		if (acodec_config->channels == 1) {
			msp_config.multichannel_config.rx_channel_0_enable =
			    0x0000001;
		} else {
			msp_config.multichannel_config.rx_channel_0_enable =
			    0x0000003;
		}
		msp_config.multichannel_config.rx_channel_1_enable = 0x0000000;
		msp_config.multichannel_config.rx_channel_2_enable = 0x0000000;
		msp_config.multichannel_config.rx_channel_3_enable = 0x0000000;

		if (acodec_config->tdm8_ch_mode == ENABLE) {
			msp_config.def_elem_len = 1;

			msp_config.protocol_desc.tx_element_length_1 =
			    MSP_ELEM_LENGTH_20;
			msp_config.protocol_desc.tx_frame_length_1 =
			    MSP_FRAME_LENGTH_8;
			msp_config.protocol_desc.tx_data_delay = MSP_DELAY_1;

			msp_config.protocol_desc.tx_element_length_2 =
			    MSP_ELEM_LENGTH_8;
			msp_config.protocol_desc.tx_frame_length_2 =
			    MSP_FRAME_LENGTH_1;

			msp_config.protocol_desc.rx_element_length_1 =
			    MSP_ELEM_LENGTH_20;
			msp_config.protocol_desc.rx_frame_length_1 =
			    MSP_FRAME_LENGTH_8;
			msp_config.protocol_desc.rx_data_delay = MSP_DELAY_1;

			msp_config.protocol_desc.rx_element_length_2 =
			    MSP_ELEM_LENGTH_8;
			msp_config.protocol_desc.rx_frame_length_2 =
			    MSP_FRAME_LENGTH_1;

			msp_config.protocol_desc.frame_sync_ignore =
			    MSP_FRAME_SYNC_UNIGNORE;
			msp_config.protocol_desc.rx_clock_pol = MSP_RISING_EDGE;

			//if(acodec_config->digital_loopback == ENABLE) {
			if (1) {
				msp_config.multichannel_config.
				    tx_channel_0_enable =
				    (1 << NB_OF_CHANNEL_USED) - 1;
				msp_config.multichannel_config.
				    rx_channel_0_enable =
				    (1 << NB_OF_CHANNEL_USED) - 1;
			} else {
				msp_config.multichannel_config.
				    tx_channel_0_enable = 0x3;
				msp_config.multichannel_config.
				    rx_channel_0_enable = 0x3;
			}
		}

		if (acodec_config->tdm8_ch_mode == ENABLE) {
			/* TFSDLY = 2 delay units */
			msp_config.iodelay = 0x20;
		}

		error_status = i2s_setup(i2s_dev->controller, &msp_config);
		if (error_status < 0) {
			stm_error("error in msp enable, error_status is %d\n",
				  error_status);
			return error_status;
		}
	} else if (ENABLE == acodec_config->direct_rendering_mode) {
		writel(0x00, ((char *)(IO_ADDRESS(U8500_MSP1_BASE) + 0x04)));	//MSP_GCR
	}

	if (ACODEC_CONFIG_REQUIRED == acodec_config->acodec_config_need) {
		AB8500_CODEC_SelectInterface(AB8500_CODEC_AUDIO_INTERFACE_0);

		codec_error = AB8500_CODEC_PowerUp();
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_PowerUp failed\n");
			return AB8500_CODEC_ERROR;
		}

/*#ifdef CONFIG_U8500_AB8500_CUT10*/
#if 1
		tdm_config.cr27_if1_bitclk_osr =
		    AB8500_CODEC_CR27_IF1_BITCLK_OSR_32;
		tdm_config.cr27_if0_bitclk_osr =
		    AB8500_CODEC_CR27_IF0_BITCLK_OSR_32;
		tdm_config.cr28_if0wl = AB8500_CODEC_CR28_IF0WL_16BITS;
		tdm_config.cr30_if1wl = AB8500_CODEC_CR30_IF1WL_16BITS;

		switch (acodec_config->direction) {
		case AB8500_CODEC_DIRECTION_INOUT:
			tdm_config.cr28_bitclk0p =
			    AB8500_CODEC_CR28_BITCLK0P_FALLING_EDGE;
			tdm_config.cr28_if0del =
			    AB8500_CODEC_CR28_IF0DEL_DELAYED;
			break;
		case AB8500_CODEC_DIRECTION_IN:
			tdm_config.cr28_bitclk0p =
			    AB8500_CODEC_CR28_BITCLK0P_RISING_EDGE;
			tdm_config.cr28_if0del =
			    AB8500_CODEC_CR28_IF0DEL_NOT_DELAYED;
			break;
		case AB8500_CODEC_DIRECTION_OUT:
			tdm_config.cr28_bitclk0p =
			    AB8500_CODEC_CR28_BITCLK0P_FALLING_EDGE;
			tdm_config.cr28_if0del =
			    AB8500_CODEC_CR28_IF0DEL_DELAYED;
			break;
		default:
			stm_error("Invalid direction\n");
			return AB8500_CODEC_ERROR;
		}

		if (acodec_config->tdm8_ch_mode == ENABLE) {
			tdm_config.cr27_if0_bitclk_osr =
			    AB8500_CODEC_CR27_IF0_BITCLK_OSR_256;
			tdm_config.cr28_if0wl = AB8500_CODEC_CR28_IF0WL_20BITS;
			tdm_config.cr28_bitclk0p =
			    AB8500_CODEC_CR28_BITCLK0P_RISING_EDGE;
			tdm_config.cr28_if0del =
			    AB8500_CODEC_CR28_IF0DEL_DELAYED;
			codec_in_mode = AB8500_CODEC_MODE_VOICE;
			codec_out_mode = AB8500_CODEC_MODE_VOICE;
			acodec_config->direction = AB8500_CODEC_DIRECTION_INOUT;
		}

		codec_error =
		    AB8500_CODEC_SetModeAndDirection(acodec_config->direction,
						     codec_in_mode,
						     codec_out_mode,
						     &tdm_config);
#else
		codec_error =
		    AB8500_CODEC_SetModeAndDirection(acodec_config->direction,
						     codec_in_mode,
						     codec_out_mode);
#endif
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("set mode and direction failed\n");
			return AB8500_CODEC_ERROR;
		}

		codec_error =
		    AB8500_CODEC_SetMasterMode(AB8500_CODEC_MASTER_MODE_ENABLE);

		if (AB8500_CODEC_OK != codec_error) {
			stm_error("set mode and direction failed\n");
			return AB8500_CODEC_ERROR;
		}

		/*codec_error = trg_codec_set_sample_frequency(0); */

		/*u8500_acodec_set_volume(g_codec_system_context.in_left_volume,
		   g_codec_system_context.in_right_volume,
		   g_codec_system_context.out_left_volume,
		   g_codec_system_context.out_right_volume,
		   user); */
#if 0
		if (AB8500_CODEC_DIRECTION_IN == acodec_config->direction
		    || AB8500_CODEC_DIRECTION_INOUT ==
		    acodec_config->direction) {

			u8500_acodec_allocate_ad_slot
			    (AB8500_CODEC_SRC_D_MICROPHONE_1, TDM_8_CH_MODE);
			u8500_acodec_allocate_ad_slot
			    (AB8500_CODEC_SRC_D_MICROPHONE_2, TDM_8_CH_MODE);

			/*codec_error = AB8500_CODEC_ADSlotAllocation (AB8500_CODEC_SLOT0,
			   AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT3);
			   if (AB8500_CODEC_OK != codec_error) {
			   stm_error("ab8500_codec_adslot_allocation failed\n");
			   return AB8500_CODEC_ERROR;
			   }
			   codec_error = AB8500_CODEC_ADSlotAllocation (AB8500_CODEC_SLOT1,
			   AB8500_CODEC_CR31_TO_CR46_SLOT_IS_TRISTATE);
			   if (AB8500_CODEC_OK != codec_error) {
			   stm_error("ab8500_codec_adslot_allocation failed\n");
			   return AB8500_CODEC_ERROR;
			   } */

		}

		if (AB8500_CODEC_DIRECTION_OUT == acodec_config->direction
		    || AB8500_CODEC_DIRECTION_INOUT ==
		    acodec_config->direction) {
			u8500_acodec_allocate_da_slot(AB8500_CODEC_DEST_HEADSET,
						      TDM_8_CH_MODE);
			/*codec_error = AB8500_CODEC_DASlotAllocation (AB8500_CODEC_DA_CHANNEL_NUMBER_1,
			   AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT08);
			   if (AB8500_CODEC_OK != codec_error) {
			   stm_error
			   ("ab8500_codec_daslot_allocation failed\n");
			   return AB8500_CODEC_ERROR;
			   }
			   codec_error = AB8500_CODEC_DASlotAllocation (AB8500_CODEC_DA_CHANNEL_NUMBER_2,
			   AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT09);
			   if (AB8500_CODEC_OK != codec_error) {
			   stm_error
			   ("ab8500_codec_daslot_allocation failed\n");
			   return AB8500_CODEC_ERROR;
			   } */

		}
#endif

	}			//END of if acodec_config_need

/*#if DRIVER_DEBUG > 0
	{
		dump_msp_registers();
		dump_acodec_registers();
	}
#endif*/

	stm_dbg(DBG_ST.acodec,
		"leaving in u8500_acodec_enable_audio_mode() \n");

	FUNC_EXIT();
	return AB8500_CODEC_OK;
}

/**
* u8500_acodec_set_output_volume - configures the volume level for both speakers
* @in_left_volume - volume for left channel of mic
* @in_right_volume - volume for right channel of mic
* @out_left_volume - volume for left speaker
* @out_right_volume - volume for right speaker
*/
t_ab8500_codec_error u8500_acodec_set_output_volume(t_ab8500_codec_dest
						    dest_device,
						    int left_volume,
						    int right_volume,
						    t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_set_output_volume()\n");

	FUNC_ENTER();

	user = user;		//keep compiler happy

	g_codec_system_context.output_config[dest_device].left_volume =
	    left_volume;
	g_codec_system_context.output_config[dest_device].right_volume =
	    right_volume;

	AB8500_CODEC_SetDestVolume(dest_device, left_volume, right_volume);

	FUNC_EXIT();
	return codec_error;
}

/*u8500_acodec_get_output_volume*/

t_ab8500_codec_error u8500_acodec_get_output_volume(t_ab8500_codec_dest
						    dest_device,
						    int *p_left_volume,
						    int *p_right_volume,
						    t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_set_output_volume()\n");

	user = user;		//keep compiler happy

	*p_left_volume =
	    g_codec_system_context.output_config[dest_device].left_volume;
	*p_right_volume =
	    g_codec_system_context.output_config[dest_device].right_volume;

	return codec_error;
}

/**
* u8500_acodec_set_input_volume - configures the volume level for both speakers
* @in_left_volume - volume for left channel of mic
* @in_right_volume - volume for right channel of mic
* @out_left_volume - volume for left speaker
* @out_right_volume - volume for right speaker
*/
t_ab8500_codec_error u8500_acodec_set_input_volume(t_ab8500_codec_src
						   src_device, int left_volume,
						   int right_volume,
						   t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_set_input_volume()\n");

	user = user;		//keep compiler happy

	g_codec_system_context.input_config[src_device].left_volume =
	    left_volume;
	g_codec_system_context.input_config[src_device].right_volume =
	    right_volume;

	AB8500_CODEC_SetSrcVolume(src_device, left_volume, right_volume);

	return codec_error;
}

/**
* u8500_acodec_get_input_volume - configures the volume level for both speakers
* @in_left_volume - volume for left channel of mic
* @in_right_volume - volume for right channel of mic
* @out_left_volume - volume for left speaker
* @out_right_volume - volume for right speaker
*/
t_ab8500_codec_error u8500_acodec_get_input_volume(t_ab8500_codec_src
						   src_device,
						   int *p_left_volume,
						   int *p_right_volume,
						   t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_get_input_volume()\n");

	user = user;		//keep compiler happy

	*p_left_volume =
	    g_codec_system_context.input_config[src_device].left_volume;
	*p_right_volume =
	    g_codec_system_context.input_config[src_device].right_volume;

	return codec_error;
}

/**
* u8500_acodec_toggle_playback_mute_control - configures the mute for both speakers
* @in_left_volume - volume for left channel of mic
* @in_right_volume - volume for right channel of mic
* @out_left_volume - volume for left speaker
* @out_right_volume - volume for right speaker
*/
t_ab8500_codec_error
u8500_acodec_toggle_playback_mute_control(t_ab8500_codec_dest dest_device,
					  t_u8500_bool_state mute_state,
					  t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_toggle_playback_mute_control \n");

	user = user;		//keep compiler happy

	g_codec_system_context.output_config[dest_device].mute_state =
	    mute_state;

	if (ENABLE == mute_state) {
		AB8500_CODEC_DestPowerControl(dest_device,
					      AB8500_CODEC_SRC_STATE_ENABLE);
	} else {
		AB8500_CODEC_DestPowerControl(dest_device,
					      AB8500_CODEC_SRC_STATE_DISABLE);
	}

	return codec_error;
}

/**
* u8500_acodec_toggle_capture_mute_control - configures the mute for both speakers
* @in_left_volume - volume for left channel of mic
* @in_right_volume - volume for right channel of mic
* @out_left_volume - volume for left speaker
* @out_right_volume - volume for right speaker
*/
t_ab8500_codec_error u8500_acodec_toggle_capture_mute_control(t_ab8500_codec_src
							      src_device,
							      t_u8500_bool_state
							      mute_state,
							      t_acodec_user
							      user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering in u8500_acodec_toggle_capture_mute_control \n");

	user = user;		//keep compiler happy

	g_codec_system_context.input_config[src_device].mute_state = mute_state;

	if (ENABLE == mute_state) {
		AB8500_CODEC_SrcPowerControl(src_device,
					     AB8500_CODEC_SRC_STATE_ENABLE);
	} else {
		AB8500_CODEC_SrcPowerControl(src_device,
					     AB8500_CODEC_SRC_STATE_DISABLE);
	}

	return codec_error;
}

/**
* u8500_acodec_select_input
* @input_device: MIC or linein.
*
* This routine selects the input device mic or linein.
*/

t_ab8500_codec_error u8500_acodec_select_input(t_ab8500_codec_src
					       input_device,
					       t_acodec_user user,
					       t_u8500_mode mode)
{

	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec, "  Entering u8500_acodec_select_input\n");

	if (TDM_8_CH_MODE == mode)
		u8500_acodec_allocate_ad_slot(input_device, TDM_8_CH_MODE);
	else
		u8500_acodec_allocate_ad_slot(input_device, CLASSICAL_MODE);

	codec_error = AB8500_CODEC_SelectInput(input_device);

	stm_dbg(DBG_ST.acodec, "  leaving u8500_acodec_select_input\n");
	return codec_error;
}

/**
* u8500_acodec_select_output
* @output_device: output device HP/LSP
*
* This routine selects the output device Headphone or loud speaker
*/

t_ab8500_codec_error u8500_acodec_select_output(t_ab8500_codec_dest
						output_device,
						t_acodec_user user,
						t_u8500_mode mode)
{

	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	FUNC_ENTER();
	stm_dbg(DBG_ST.acodec, "  Entering u8500_acodec_select_output()\n");

	if (TDM_8_CH_MODE == mode)
		u8500_acodec_allocate_da_slot(output_device, TDM_8_CH_MODE);
	else
		u8500_acodec_allocate_da_slot(output_device, CLASSICAL_MODE);

	codec_error = AB8500_CODEC_SelectOutput(output_device);

	stm_dbg(DBG_ST.acodec, "  leaving u8500_acodec_select_output()\n");
	FUNC_EXIT();
	return codec_error;
}

t_ab8500_codec_error u8500_acodec_allocate_ad_slot(t_ab8500_codec_src
						   input_device,
						   t_u8500_mode mode)
{
	t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line1,
	    ad_data_line2;
	t_ab8500_codec_slot slot1, slot2;
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	slot1 = AB8500_CODEC_SLOT_UNDEFINED;
	slot2 = AB8500_CODEC_SLOT_UNDEFINED;

	ad_data_line1 =
	    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_UNDEFINED;
	ad_data_line2 =
	    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_UNDEFINED;

	switch (input_device) {
	case AB8500_CODEC_SRC_D_MICROPHONE_1:
		{
			slot1 = AB8500_CODEC_SLOT0;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT1;
		}
		break;
	case AB8500_CODEC_SRC_MICROPHONE_2:
	case AB8500_CODEC_SRC_D_MICROPHONE_2:
		{
			slot1 = AB8500_CODEC_SLOT1;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT2;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_3:
	case AB8500_CODEC_SRC_MICROPHONE_1A:
	case AB8500_CODEC_SRC_MICROPHONE_1B:
		{
			slot1 = AB8500_CODEC_SLOT2;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT3;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_4:
		{
			slot1 = AB8500_CODEC_SLOT3;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT4;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_5:
		{
			slot1 = AB8500_CODEC_SLOT4;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT5;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_6:
		{
			slot1 = AB8500_CODEC_SLOT5;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT6;
		}
		break;
	case AB8500_CODEC_SRC_LINEIN:
		{
			slot1 = AB8500_CODEC_SLOT0;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT1;

			slot2 = AB8500_CODEC_SLOT1;
			ad_data_line2 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT2;
		}
	case AB8500_CODEC_SRC_FM_RX:
		{
			slot1 = AB8500_CODEC_SLOT6;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT7;

			slot2 = AB8500_CODEC_SLOT7;
			ad_data_line2 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT8;
		}
		break;
	case AB8500_CODEC_SRC_ALL:
		break;
	}

	if ((AB8500_CODEC_SLOT_UNDEFINED != slot1)
	    && (AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_UNDEFINED !=
		ad_data_line1)) {
		if (CLASSICAL_MODE == mode) {
			slot1 = AB8500_CODEC_SLOT0;
		}
		codec_error =
		    AB8500_CODEC_ADSlotAllocation(slot1, ad_data_line1);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	if ((AB8500_CODEC_SLOT_UNDEFINED != slot2)
	    && (AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_UNDEFINED !=
		ad_data_line2)) {
		if (CLASSICAL_MODE == mode) {
			slot2 = AB8500_CODEC_SLOT1;
		}
		codec_error =
		    AB8500_CODEC_ADSlotAllocation(slot2, ad_data_line2);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	return AB8500_CODEC_OK;
}

t_ab8500_codec_error u8500_acodec_unallocate_ad_slot(t_ab8500_codec_src
						     input_device,
						     t_u8500_mode mode)
{
	t_ab8500_codec_slot slot1, slot2;
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	slot1 = AB8500_CODEC_SLOT_UNDEFINED;
	slot2 = AB8500_CODEC_SLOT_UNDEFINED;

	switch (input_device) {
	case AB8500_CODEC_SRC_D_MICROPHONE_1:
		{
			slot1 = AB8500_CODEC_SLOT0;
		}
		break;
	case AB8500_CODEC_SRC_MICROPHONE_2:
	case AB8500_CODEC_SRC_D_MICROPHONE_2:
		{
			slot1 = AB8500_CODEC_SLOT1;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_3:
	case AB8500_CODEC_SRC_MICROPHONE_1A:
	case AB8500_CODEC_SRC_MICROPHONE_1B:
		{
			slot1 = AB8500_CODEC_SLOT2;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_4:
		{
			slot1 = AB8500_CODEC_SLOT3;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_5:
		{
			slot1 = AB8500_CODEC_SLOT4;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_6:
		{
			slot1 = AB8500_CODEC_SLOT5;
		}
		break;
	case AB8500_CODEC_SRC_LINEIN:
		{
			slot1 = AB8500_CODEC_SLOT0;
			slot2 = AB8500_CODEC_SLOT1;
		}
		break;
	case AB8500_CODEC_SRC_ALL:
		break;
	}

	if (AB8500_CODEC_SLOT_UNDEFINED != slot1) {
		if (CLASSICAL_MODE == mode) {
			slot1 = AB8500_CODEC_SLOT0;
		}
		codec_error =
		    AB8500_CODEC_ADSlotAllocation(slot1,
						  AB8500_CODEC_CR31_TO_CR46_SLOT_IS_TRISTATE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	if (AB8500_CODEC_SLOT_UNDEFINED != slot2) {
		if (CLASSICAL_MODE == mode) {
			slot2 = AB8500_CODEC_SLOT1;
		}
		codec_error =
		    AB8500_CODEC_ADSlotAllocation(slot2,
						  AB8500_CODEC_CR31_TO_CR46_SLOT_IS_TRISTATE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	return AB8500_CODEC_OK;
}

#ifdef CONFIG_U8500_AB8500_CUT10
t_ab8500_codec_error u8500_acodec_allocate_da_slot(t_ab8500_codec_dest
						   output_device,
						   t_u8500_mode mode)
{
	t_ab8500_codec_da_channel_number da_ch_no1, da_ch_no2;
	t_ab8500_codec_cr51_to_cr58_sltoda da_slot1, da_slot2;
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED;
	da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED;

	da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT_UNDEFINED;
	da_slot2 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT_UNDEFINED;

	switch (output_device) {
	case AB8500_CODEC_DEST_HEADSET:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_1;
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT08;

			da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_2;
			da_slot2 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT09;
		}
		break;
	case AB8500_CODEC_DEST_EARPIECE:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_1;
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT08;
		}
		break;
	case AB8500_CODEC_DEST_HANDSFREE:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_3;
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT10;

			da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_4;
			da_slot2 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT11;
		}
		break;
	case AB8500_CODEC_DEST_VIBRATOR_L:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_5;
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT12;
		}
		break;
	case AB8500_CODEC_DEST_VIBRATOR_R:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_6;
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT13;
		}
		break;

	case AB8500_CODEC_DEST_FM_TX:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_7;
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT14;

			da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_8;
			da_slot2 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT15;
		}

	case AB8500_CODEC_DEST_ALL:
		break;
	}

	if ((AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED != da_ch_no1)
	    && (AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT_UNDEFINED != da_slot1)) {
		if (CLASSICAL_MODE == mode) {
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT08;
		}
		codec_error =
		    AB8500_CODEC_DASlotAllocation(da_ch_no1, da_slot1);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	if ((AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED != da_ch_no2)
	    && (AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT_UNDEFINED != da_slot2)) {
		if (CLASSICAL_MODE == mode) {
			da_slot1 = AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT09;
		}
		codec_error =
		    AB8500_CODEC_DASlotAllocation(da_ch_no2, da_slot2);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	return AB8500_CODEC_OK;
}

#else
t_ab8500_codec_error u8500_acodec_allocate_da_slot(t_ab8500_codec_dest
						   output_device,
						   t_u8500_mode mode)
{
	t_ab8500_codec_da_channel_number da_ch_no1, da_ch_no2;
	t_ab8500_codec_cr51_to_cr56_sltoda da_slot1, da_slot2;
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED;
	da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED;

	da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT_UNDEFINED;
	da_slot2 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT_UNDEFINED;

	switch (output_device) {
	case AB8500_CODEC_DEST_HEADSET:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_1;
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT08;

			da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_2;
			da_slot2 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT09;
		}
		break;
	case AB8500_CODEC_DEST_EARPIECE:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_1;
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT08;
		}
		break;
	case AB8500_CODEC_DEST_HANDSFREE:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_3;
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT10;

			da_ch_no2 = AB8500_CODEC_DA_CHANNEL_NUMBER_4;
			da_slot2 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT11;
		}
		break;
	case AB8500_CODEC_DEST_VIBRATOR_L:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_5;
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT12;
		}
		break;
	case AB8500_CODEC_DEST_VIBRATOR_R:
		{
			da_ch_no1 = AB8500_CODEC_DA_CHANNEL_NUMBER_6;
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT13;
		}
		break;

	case AB8500_CODEC_DEST_ALL:
		break;
	}

	if ((AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED != da_ch_no1)
	    && (AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT_UNDEFINED != da_slot1)) {
		if (CLASSICAL_MODE == mode) {
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT08;
		}
		codec_error =
		    AB8500_CODEC_DASlotAllocation(da_ch_no1, da_slot1);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	if ((AB8500_CODEC_DA_CHANNEL_NUMBER_UNDEFINED != da_ch_no2)
	    && (AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT_UNDEFINED != da_slot2)) {
		if (CLASSICAL_MODE == mode) {
			da_slot1 = AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT09;
		}
		codec_error =
		    AB8500_CODEC_DASlotAllocation(da_ch_no2, da_slot2);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}

	return AB8500_CODEC_OK;
}
#endif

t_ab8500_codec_error u8500_acodec_unallocate_da_slot(t_ab8500_codec_dest
						     output_device,
						     t_u8500_mode mode)
{
	return AB8500_CODEC_OK;
}

t_ab8500_codec_error u8500_acodec_set_src_power_cntrl(t_ab8500_codec_src
						      input_device,
						      t_u8500_bool_state
						      pwr_state)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	if (ENABLE == pwr_state) {
		u8500_acodec_allocate_ad_slot(input_device, TDM_8_CH_MODE);
		codec_error =
		    AB8500_CODEC_SrcPowerControl(input_device,
						 AB8500_CODEC_SRC_STATE_ENABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_SrcPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.input_config[input_device].power_state =
		    ENABLE;
	} else {
		u8500_acodec_unallocate_ad_slot(input_device, TDM_8_CH_MODE);
		codec_error =
		    AB8500_CODEC_SrcPowerControl(input_device,
						 AB8500_CODEC_SRC_STATE_DISABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_SrcPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.input_config[input_device].power_state =
		    DISABLE;
	}

	return AB8500_CODEC_OK;
}

t_u8500_bool_state u8500_acodec_get_src_power_state(t_ab8500_codec_src
						    input_device)
{
	return (g_codec_system_context.input_config[input_device].power_state);
}

t_ab8500_codec_error u8500_acodec_set_dest_power_cntrl(t_ab8500_codec_dest
						       output_device,
						       t_u8500_bool_state
						       pwr_state)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	if (ENABLE == pwr_state) {
		AB8500_CODEC_SelectInterface(AB8500_CODEC_AUDIO_INTERFACE_0);

		codec_error = AB8500_CODEC_PowerUp();
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_PowerUp failed\n");
			return AB8500_CODEC_ERROR;
		}

		u8500_acodec_allocate_da_slot(output_device, TDM_8_CH_MODE);

		codec_error =
		    AB8500_CODEC_DestPowerControl(output_device,
						  AB8500_CODEC_DEST_STATE_ENABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_DestPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.output_config[output_device].
		    power_state = ENABLE;
	} else {
		u8500_acodec_unallocate_da_slot(output_device, TDM_8_CH_MODE);
		codec_error =
		    AB8500_CODEC_DestPowerControl(output_device,
						  AB8500_CODEC_DEST_STATE_DISABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_DestPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.output_config[output_device].
		    power_state = DISABLE;
	}

	return AB8500_CODEC_OK;
}

t_u8500_bool_state u8500_acodec_get_dest_power_state(t_ab8500_codec_dest
						     output_device)
{
	return (g_codec_system_context.output_config[output_device].
		power_state);
}

/**
* u8500_acodec_toggle_analog_lpbk
* @output_device: output device HP/LSP
*
* This routine selects the output device Headphone or loud speaker
*/
t_ab8500_codec_error u8500_acodec_toggle_analog_lpbk(t_u8500_bool_state
						     lpbk_state,
						     t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	stm_dbg(DBG_ST.acodec,
		"  Entering inu8500_acodec_toggle_analog_lpbk() \n");

	user = user;		//keep compiler happy

	if (ENABLE == lpbk_state) {
		/* Reset CODEC */
		codec_error = AB8500_CODEC_Reset();

		AB8500_CODEC_SelectInterface(AB8500_CODEC_AUDIO_INTERFACE_0);

		codec_error = AB8500_CODEC_PowerUp();
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_PowerUp failed\n");
			return AB8500_CODEC_ERROR;
		}

		codec_error = AB8500_CODEC_SelectInput(AB8500_CODEC_SRC_LINEIN);

		codec_error =
		    AB8500_CODEC_SrcPowerControl(AB8500_CODEC_SRC_LINEIN,
						 AB8500_CODEC_SRC_STATE_ENABLE);

		//codec_error = AB8500_CODEC_SetSrcVolume(AB8500_CODEC_SRC_LINEIN,VOL_MAX,VOL_MAX);

		codec_error =
		    AB8500_CODEC_SelectOutput(AB8500_CODEC_DEST_HEADSET);

		codec_error =
		    AB8500_CODEC_DestPowerControl(AB8500_CODEC_DEST_HEADSET,
						  AB8500_CODEC_DEST_STATE_ENABLE);

		//codec_error = AB8500_CODEC_SetDestVolume(AB8500_CODEC_DEST_HEADSET,0,0);

		codec_error = AB8500_CODEC_SetAnalogLoopback(VOL_MAX, VOL_MAX);

		ab8500_write(AB8500_AUDIO, 0xd05, 0x30);
		ab8500_write(AB8500_AUDIO, 0xd07, 0xf3);
		ab8500_write(AB8500_AUDIO, 0xd16, 0xdd);
		ab8500_write(AB8500_AUDIO, 0xd17, 0x55);
		ab8500_write(AB8500_AUDIO, 0xd3f, 0xc0);

	} else {
		codec_error = AB8500_CODEC_RemoveAnalogLoopback();
	}

#if DRIVER_DEBUG > 0
	{
		dump_acodec_registers();
	}
#endif

	return codec_error;
}

#ifdef CONFIG_U8500_ACODEC_POLL

static int digital_lpbk_msp_rx_tx_thread(void *data)
{
	t_digital_lpbk_cnxt *p_cnxt = (t_digital_lpbk_cnxt *) data;
	unsigned int sample[8], count = 32;

	daemonize("digital_lpbk_msp_rx_tx_thread");
	allow_signal(SIGKILL);

	printk("\n Rx-Tx : digital_lpbk_msp_rx_tx_thread started \n");

	while ((!signal_pending(current)) && (p_cnxt->rx_active)) {

//              ret_val = u8500_msp_receive_data(alsa_msp_adev,p_cnxt->buffer[p_cnxt->rx_index],p_cnxt->data_size);

		//u8500_msp_transceive_data(alsa_msp_adev,p_cnxt->buffer[0], p_cnxt->data_size,p_cnxt->buffer[1], p_cnxt->data_size);

		//u8500_msp_transceive_data(alsa_msp_adev,p_cnxt->buffer[1], p_cnxt->data_size,p_cnxt->buffer[0], p_cnxt->data_size);

#if DRIVER_DEBUG > 1
		stm_dbg(DBG_ST.alsa, " Receiving \n");
#endif
		u8500_acodec_receive_data(I2S_CLIENT_MSP1, (void *)sample,
					  count, 0);

#if DRIVER_DEBUG > 1
		stm_dbg(DBG_ST.alsa, " Transmitting \n");
#endif
		u8500_acodec_send_data(I2S_CLIENT_MSP1, (void *)sample, count,
				       0);

	}
	printk("\n Rx-Tx : digital_lpbk_msp_rx_tx_thread ended \n");
	return 0;
}

#endif

#ifdef CONFIG_U8500_ACODEC_DMA

static void u8500_digital_lpbk_dma_start()
{
	u8500_acodec_loopback_configure(I2S_CLIENT_MSP1,
					(void *)digital_lpbk_cnxt.buffer.addr,
					digital_lpbk_cnxt.data_size, 1);

	stm_dbg(DBG_ST.alsa, " Rx DMA Transfer started\n");
	stm_dbg(DBG_ST.alsa, " Rx : add = %x  size=%d\n",
		(int)(digital_lpbk_cnxt.buffer.addr),
		digital_lpbk_cnxt.data_size);

}
#endif

/**
* u8500_acodec_toggle_digital_lpbk
* @output_device: output device HP/LSP
*
* This routine selects the output device Headphone or loud speaker
*/

t_ab8500_codec_error u8500_acodec_toggle_digital_lpbk(t_u8500_bool_state
						      lpbk_state,
						      t_ab8500_codec_dest
						      dest_device,
						      t_ab8500_codec_src
						      src_device,
						      t_acodec_user user,
						      t_u8500_bool_state
						      tdm8_ch_mode)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;
	struct acodec_configuration acodec_config;
	int status = 0;

	stm_dbg(DBG_ST.acodec,
		"  Entering u8500_acodec_toggle_digital_lpbk() \n");

	user = user;		//keep compiler happy

	if (ENABLE == lpbk_state) {
		//data_size = 1024*100;

		//data[0] = (unsigned char *)kmalloc(data_size, GFP_KERNEL);

		codec_error = AB8500_CODEC_Reset();

		//AB8500_CODEC_SelectInterface(AB8500_CODEC_AUDIO_INTERFACE_0);

		//codec_error = AB8500_CODEC_PowerUp();
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_PowerUp failed\n");
			return AB8500_CODEC_ERROR;
		}

		status = u8500_acodec_open(I2S_CLIENT_MSP1, 0);
		if (status) {
			printk("failed in getting acodec playback open\n");
			return -1;
		}
		status = u8500_acodec_open(I2S_CLIENT_MSP1, 1);
		if (status) {
			printk("failed in getting acdoec capture open\n");
			return -1;
		}

		u8500_acodec_setuser(USER_ALSA);

		if (ENABLE == tdm8_ch_mode) {
			printk("\n 20 bit 8 ch Digital Loopback");
			printk("\n DMIC1 -> HS-L");
			printk("\n DMIC2 -> HS-R");
			printk("\n DMIC3 -> IHF-L");
			printk("\n DMIC5 -> Vibra-L\n");
			printk("\n DMIC6 -> Vibra-R\n");
			printk("\n FM    -> FM Tx\n");
		} else {
			printk("\n 16 bit 2 ch Digital Loopback");
			printk("\n DMIC1 -> HS-L");
			printk("\n DMIC2 -> HS-R");
		}

		stm_dbg(DBG_ST.alsa, "enabling audiocodec audio mode\n");
		acodec_config.direction = AB8500_CODEC_DIRECTION_INOUT;
		acodec_config.input_frequency = T_CODEC_SAMPLING_FREQ_48KHZ;
		acodec_config.output_frequency = T_CODEC_SAMPLING_FREQ_48KHZ;
		acodec_config.mspClockSel = CODEC_MSP_APB_CLOCK;
		acodec_config.mspInClockFreq = CODEC_MSP_INPUT_FREQ_48MHZ;
		acodec_config.channels = 2;
		acodec_config.user = 2;
		acodec_config.acodec_config_need = ACODEC_CONFIG_REQUIRED;
		acodec_config.direct_rendering_mode = DISABLE;
		acodec_config.tdm8_ch_mode = tdm8_ch_mode;
		acodec_config.digital_loopback = ENABLE;
#ifdef CONFIG_U8500_ACODEC_POLL
		acodec_config.handler = NULL;
		acodec_config.tx_callback_data = NULL;
		acodec_config.rx_callback_data = NULL;
#endif
		u8500_acodec_enable_audio_mode(&acodec_config);

		/*turn on src devices */

		perform_src_routing(src_device);

/* 		u8500_acodec_set_src_power_cntrl(src_device,ENABLE);
		u8500_acodec_set_input_volume(src_device,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_D_MICROPHONE_2,ENABLE);
		u8500_acodec_set_input_volume(AB8500_CODEC_SRC_D_MICROPHONE_2,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_D_MICROPHONE_3,ENABLE);
		u8500_acodec_set_input_volume(AB8500_CODEC_SRC_D_MICROPHONE_3,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_D_MICROPHONE_4,ENABLE);
		u8500_acodec_set_input_volume(AB8500_CODEC_SRC_D_MICROPHONE_4,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_D_MICROPHONE_5,ENABLE);
		u8500_acodec_set_input_volume(AB8500_CODEC_SRC_D_MICROPHONE_5,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_D_MICROPHONE_6,ENABLE);
		u8500_acodec_set_input_volume(AB8500_CODEC_SRC_D_MICROPHONE_6,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_D_MICROPHONE_6,ENABLE);
		u8500_acodec_set_input_volume(AB8500_CODEC_SRC_D_MICROPHONE_6,50,50,USER_ALSA);

		u8500_acodec_set_src_power_cntrl(AB8500_CODEC_SRC_FM_RX,ENABLE); */

		/*turn on dest devices */

		//u8500_acodec_set_dest_power_cntrl(dest_device,ENABLE);
		u8500_acodec_allocate_da_slot(dest_device, TDM_8_CH_MODE);
		codec_error = AB8500_CODEC_SelectOutput(dest_device);
		u8500_acodec_set_output_volume(dest_device, 100, 100,
					       USER_ALSA);

		/*u8500_acodec_set_dest_power_cntrl(AB8500_CODEC_DEST_HEADSET,ENABLE);
		   u8500_acodec_set_output_volume(AB8500_CODEC_DEST_HEADSET,100,100,USER_ALSA); */

		/*u8500_acodec_set_dest_power_cntrl(AB8500_CODEC_DEST_HANDSFREE,ENABLE);
		   u8500_acodec_set_output_volume(AB8500_CODEC_DEST_HANDSFREE,100,100,USER_ALSA); */

#ifdef CONFIG_U8500_AB8500_CUT10
		codec_error =
		    AB8500_CODEC_DASlotAllocation
		    (AB8500_CODEC_DA_CHANNEL_NUMBER_5,
		     AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT12);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}

		codec_error =
		    AB8500_CODEC_DASlotAllocation
		    (AB8500_CODEC_DA_CHANNEL_NUMBER_6,
		     AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT13);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}

		codec_error =
		    AB8500_CODEC_DASlotAllocation
		    (AB8500_CODEC_DA_CHANNEL_NUMBER_7,
		     AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT14);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}

		codec_error =
		    AB8500_CODEC_DASlotAllocation
		    (AB8500_CODEC_DA_CHANNEL_NUMBER_8,
		     AB8500_CODEC_CR51_TO_CR58_SLTODA_SLOT15);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
#else
		codec_error =
		    AB8500_CODEC_DASlotAllocation
		    (AB8500_CODEC_DA_CHANNEL_NUMBER_5,
		     AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT12);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}

		codec_error =
		    AB8500_CODEC_DASlotAllocation
		    (AB8500_CODEC_DA_CHANNEL_NUMBER_6,
		     AB8500_CODEC_CR51_TO_CR56_SLTODA_SLOT13);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_daslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
#endif

		digital_lpbk_cnxt.data_size = 2048;

		digital_lpbk_cnxt.rx_active = 1;
		digital_lpbk_cnxt.tx_active = 1;

		digital_lpbk_cnxt.rx_index = 0;
		digital_lpbk_cnxt.tx_index = 2;

		digital_lpbk_cnxt.buffer.area =
		    dma_alloc_coherent(NULL, digital_lpbk_cnxt.data_size,
				       &digital_lpbk_cnxt.buffer.addr,
				       GFP_KERNEL);
		if (NULL == digital_lpbk_cnxt.buffer.area) {
			printk("\n dma_alloc_coherent failed \n");
		}
#if DRIVER_DEBUG > 0
		{
			dump_msp_registers();
			dump_acodec_registers();
		}
#endif

#ifdef CONFIG_U8500_ACODEC_POLL
		{
			pid_t pid_rx_tx;
			pid_rx_tx =
			    kernel_thread(digital_lpbk_msp_rx_tx_thread,
					  &digital_lpbk_cnxt,
					  CLONE_FS | CLONE_SIGHAND);
		}
#elif defined(CONFIG_U8500_ACODEC_DMA)
		{
			u8500_digital_lpbk_dma_start();
		}
#endif
	} else			//lpbk is disable
	{

		digital_lpbk_cnxt.rx_active = 0;
		digital_lpbk_cnxt.tx_active = 0;

		dma_free_coherent(NULL, digital_lpbk_cnxt.data_size,
				  digital_lpbk_cnxt.buffer.area,
				  digital_lpbk_cnxt.buffer.addr);

		u8500_acodec_set_src_power_cntrl
		    (AB8500_CODEC_SRC_D_MICROPHONE_1, DISABLE);
		u8500_acodec_set_src_power_cntrl
		    (AB8500_CODEC_SRC_D_MICROPHONE_2, DISABLE);
		u8500_acodec_set_src_power_cntrl
		    (AB8500_CODEC_SRC_D_MICROPHONE_3, DISABLE);
		u8500_acodec_set_src_power_cntrl
		    (AB8500_CODEC_SRC_D_MICROPHONE_4, DISABLE);

		u8500_acodec_set_dest_power_cntrl(AB8500_CODEC_DEST_HEADSET,
						  DISABLE);
		u8500_acodec_set_dest_power_cntrl(AB8500_CODEC_DEST_HANDSFREE,
						  DISABLE);

		u8500_acodec_unsetuser(USER_ALSA);
		u8500_acodec_close(I2S_CLIENT_MSP1, ACODEC_DISABLE_ALL);
	}
	return codec_error;
}

t_ab8500_codec_error perform_src_routing(t_ab8500_codec_src input_device)
{
	int src_type = 0;
	t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line1;
	t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line2;
	t_ab8500_codec_src input_device1;
	t_ab8500_codec_src input_device2;
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	switch (input_device) {
	case AB8500_CODEC_SRC_D_MICROPHONE_1:
		{
			src_type = MONO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT1;
		}
		break;
	case AB8500_CODEC_SRC_MICROPHONE_2:
	case AB8500_CODEC_SRC_D_MICROPHONE_2:
		{
			src_type = MONO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT2;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_3:
	case AB8500_CODEC_SRC_MICROPHONE_1A:
	case AB8500_CODEC_SRC_MICROPHONE_1B:
		{
			src_type = MONO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT3;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_4:
		{
			src_type = MONO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT4;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_5:
		{
			src_type = MONO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT5;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_6:
		{
			src_type = MONO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT6;
		}
		break;
	case AB8500_CODEC_SRC_LINEIN:
		{
			src_type = STEREO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT1;
			ad_data_line2 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT2;
			input_device1 = AB8500_CODEC_SRC_LINEIN;
			input_device2 = AB8500_CODEC_SRC_LINEIN;
		}
		break;
#ifdef CONFIG_U8500_AB8500_CUT10
	case AB8500_CODEC_SRC_D_MICROPHONE_12:
		{
			src_type = STEREO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT1;
			ad_data_line2 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT2;
			input_device1 = AB8500_CODEC_SRC_D_MICROPHONE_1;
			input_device2 = AB8500_CODEC_SRC_D_MICROPHONE_2;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_34:
		{
			src_type = STEREO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT3;
			ad_data_line2 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT4;
			input_device1 = AB8500_CODEC_SRC_D_MICROPHONE_3;
			input_device2 = AB8500_CODEC_SRC_D_MICROPHONE_4;
		}
		break;
	case AB8500_CODEC_SRC_D_MICROPHONE_56:
		{
			src_type = STEREO_SRC;
			ad_data_line1 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT5;
			ad_data_line2 =
			    AB8500_CODEC_CR31_TO_CR46_SLOT_OUTPUTS_DATA_FROM_AD_OUT6;
			input_device1 = AB8500_CODEC_SRC_D_MICROPHONE_5;
			input_device2 = AB8500_CODEC_SRC_D_MICROPHONE_6;
		}
		break;
#endif	/* #ifdef CONFIG_U8500_AB8500_CUT10 */
	}
	if (STEREO_SRC == src_type) {
		u8500_acodec_allocate_all_stereo_slots(ad_data_line1,
						       ad_data_line2);
		codec_error =
		    AB8500_CODEC_SrcPowerControl(input_device1,
						 AB8500_CODEC_SRC_STATE_ENABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_SrcPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.input_config[input_device1].power_state =
		    ENABLE;

		u8500_acodec_set_input_volume(input_device1, 50, 50, USER_ALSA);

		codec_error =
		    AB8500_CODEC_SrcPowerControl(input_device2,
						 AB8500_CODEC_SRC_STATE_ENABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_SrcPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.input_config[input_device2].power_state =
		    ENABLE;

		u8500_acodec_set_input_volume(input_device2, 50, 50, USER_ALSA);
	} else {
		u8500_acodec_allocate_all_mono_slots(ad_data_line1);
		codec_error =
		    AB8500_CODEC_SrcPowerControl(input_device,
						 AB8500_CODEC_SRC_STATE_ENABLE);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("AB8500_CODEC_SrcPowerControl failed\n");
			return AB8500_CODEC_ERROR;
		}
		g_codec_system_context.input_config[input_device].power_state =
		    ENABLE;

		u8500_acodec_set_input_volume(input_device, 50, 50, USER_ALSA);
	}
	return AB8500_CODEC_OK;
}

t_ab8500_codec_error
    u8500_acodec_allocate_all_mono_slots
    (t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line1) {
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;
	int i;

	for (i = AB8500_CODEC_SLOT0; i <= AB8500_CODEC_SLOT7; i++) {
		codec_error = AB8500_CODEC_ADSlotAllocation(i, ad_data_line1);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}
	return AB8500_CODEC_OK;
}

t_ab8500_codec_error
    u8500_acodec_allocate_all_stereo_slots
    (t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line1,
     t_ab8500_codec_cr31_to_cr46_ad_data_allocation ad_data_line2) {
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;
	int i;

	for (i = AB8500_CODEC_SLOT0; i <= AB8500_CODEC_SLOT7; i += 2) {
		codec_error = AB8500_CODEC_ADSlotAllocation(i, ad_data_line1);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
		codec_error =
		    AB8500_CODEC_ADSlotAllocation(i + 1, ad_data_line2);
		if (AB8500_CODEC_OK != codec_error) {
			stm_error("ab8500_codec_adslot_allocation failed\n");
			return AB8500_CODEC_ERROR;
		}
	}
	return AB8500_CODEC_OK;
}

#ifdef CONFIG_U8500_AB8500_CUT10
t_ab8500_codec_error
u8500_acodec_set_burst_mode_fifo(t_u8500_pmc_rendering_state fifo_state)
{

}
#else
t_ab8500_codec_error
u8500_acodec_set_burst_mode_fifo(t_u8500_pmc_rendering_state fifo_state)
{
	t_ab8500_codec_error ab8500_codec_error;
	t_ab8500_codec_burst_fifo_config burst_fifo_config;

	if (RENDERING_ENABLE == fifo_state) {
		burst_fifo_config.cr104_bfifoint = 0x1;
		burst_fifo_config.cr105_bfifotx = 0xC0;
		burst_fifo_config.cr106_bfifofsext =
		    AB8500_CODEC_CR106_BFIFOFSEXT_6SLOT_EXTRA_CLK;
		burst_fifo_config.cr106_bfifomsk =
		    AB8500_CODEC_CR106_BFIFOMSK_AD_DATA0_UNMASKED;
		burst_fifo_config.cr106_bfifomstr =
		    AB8500_CODEC_CR106_BFIFOMSTR_MASTER_MODE;
		burst_fifo_config.cr106_bfifostrt =
		    AB8500_CODEC_CR106_BFIFOSTRT_RUNNING;
		burst_fifo_config.cr107_bfifosampnr = 0x100;
		burst_fifo_config.cr108_bfifowakeup = 0x1;

		ab8500_codec_error =
		    AB8500_CODEC_ConfigureBurstFifo(&burst_fifo_config);
		if (AB8500_CODEC_OK != ab8500_codec_error) {
			return ab8500_codec_error;
		}

		ab8500_codec_error = AB8500_CODEC_EnableBurstFifo();
		if (AB8500_CODEC_OK != ab8500_codec_error) {
			return ab8500_codec_error;
		}

		printk("\n Burst mode activated\n");
	} else if (RENDERING_DISABLE == fifo_state) {
		ab8500_codec_error = AB8500_CODEC_DisableBurstFifo();
		if (AB8500_CODEC_OK != ab8500_codec_error) {
			return ab8500_codec_error;
		}
		printk("\n Burst mode deactivated\n");
	}
	return AB8500_CODEC_OK;
}
#endif
/**
* u8500_acodec_set_user
*
* Set the current user for acodec.
*/

t_ab8500_codec_error u8500_acodec_setuser(t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	FUNC_ENTER();

	if ((g_codec_system_context.cur_user == NO_USER)
	    || (g_codec_system_context.cur_user == user))
		g_codec_system_context.cur_user = user;
	else {
		stm_error
		    (" Trying to acces audiocodec already in use by user %d\n",
		     g_codec_system_context.cur_user);
		return AB8500_CODEC_ERROR;
	}
	FUNC_EXIT();
	return (codec_error);
}

/**
* u8500_acodec_unset_user
*
* Unset the current user for acodec.
*/

t_ab8500_codec_error u8500_acodec_unsetuser(t_acodec_user user)
{
	t_ab8500_codec_error codec_error = AB8500_CODEC_OK;

	if (g_codec_system_context.cur_user != user) {
		stm_error
		    (" Trying to free audiocodec already in use by other user %d\n",
		     g_codec_system_context.cur_user);
		return AB8500_CODEC_ERROR;
	} else
		g_codec_system_context.cur_user = NO_USER;

	return (codec_error);
}

#if DRIVER_DEBUG > 0
t_ab8500_codec_error dump_acodec_registers()
{
	u8 i;

	for (i = 0; i <= 0x6D; i++)
		stm_dbg(DBG_ST.acodec, "block=0x0D, adr=%x = %x\n", i,
			ab8500_read(AB8500_AUDIO, i));

	/*for (i = 0; i < 0x5e; i++)
	   stm_dbg(DBG_ST.acodec,"\n block 1,reg =%d val %x", i, ab8500_read(AB8500_AUDIO, i));
	 */
	return 0;
}

t_ab8500_codec_error dump_msp_registers()
{
	int i;

	stm_dbg(DBG_ST.acodec, "\nMSP_1 base add = 0x%x\n",
		(unsigned int)U8500_MSP1_BASE);

	for (i = 0; i < 0x40; i += 4)
		stm_dbg(DBG_ST.acodec, "msp[0x%x]=0x%x\n", i,
			readl((char *)(IO_ADDRESS(U8500_MSP1_BASE) + i)));

	return 0;
}

EXPORT_SYMBOL(dump_msp_registers);
EXPORT_SYMBOL(dump_acodec_registers);
#endif

/**
* u8500_acodec_powerdown
*
* This function power off the audio codec.
*/
void u8500_acodec_powerdown()
{
	AB8500_CODEC_PowerDown();
}

/**
* u8500_acodec_init
*
* This is the init function for STW5098 audiocodec driver.
*/

static int i2sdrv_probe(struct i2s_device *i2s)
{

	/* Allocate driver data */
	try_module_get(i2s->controller->dev.parent->driver->owner);

	/* Allocate memory to i2sdrv structure */
	i2sdrv[i2s->chip_select] =
	    kzalloc(sizeof(*i2sdrv[i2s->chip_select]), GFP_KERNEL);
	if (!i2sdrv[i2s->chip_select])
		return -ENOMEM;

	/* Initialize the driver data */
	i2sdrv[i2s->chip_select]->i2s = i2s;
	i2sdrv[i2s->chip_select]->flag = -1;
	i2sdrv[i2s->chip_select]->tx_status = 0;
	i2sdrv[i2s->chip_select]->rx_status = 0;
	spin_lock_init(&i2sdrv[i2s->chip_select]->i2s_lock);

	i2s_set_drvdata(i2s, (void *)i2sdrv[i2s->chip_select]);
	return 0;
}

static int i2sdrv_remove(struct i2s_device *i2s)
{
	struct i2sdrv_data *i2sdrv = i2s_get_drvdata(i2s);

	spin_lock_irq(&i2sdrv->i2s_lock);
	i2sdrv->i2s = NULL;
	i2s_set_drvdata(i2s, NULL);
	spin_unlock_irq(&i2sdrv->i2s_lock);

	stm_dbg(DBG_ST.acodec, "Entering AUDIOTRG_CODEC_DeIni\n");
	stm_dbg(DBG_ST.acodec, "leaving AUDIOTRG_CODEC_DeIni\n");
	module_put(i2s->controller->dev.parent->driver->owner);
	printk("Remove of I2S gets called\n");
	return 0;
}
static const struct i2s_device_id acodec_id_table[] = {
	{"i2s_device.2", 0, 0},
	{"i2s_device.1", 0, 0},
	{},
};

MODULE_DEVICE_TABLE(i2s, acodec_id_table);

static struct i2s_driver i2sdrv_i2s = {
	.driver = {
		   .name = "u8500_acodec",
		   .owner = THIS_MODULE,
		   },
	.probe = i2sdrv_probe,
	.remove = __devexit_p(i2sdrv_remove),
	.id_table = acodec_id_table,

};

static void ab8500_codec_power_init(void)
{
	__u8 data, old_data;

	old_data =
	    ab8500_read(AB8500_SYS_CTRL2_BLOCK, (AB8500_CTRL3_REG & 0xFF));

	data = 0xFE & old_data;
	ab8500_write(AB8500_SYS_CTRL2_BLOCK, (AB8500_CTRL3_REG & 0xFF), data);	//0x0200

	data = 0x02 | old_data;
	ab8500_write(AB8500_SYS_CTRL2_BLOCK, (AB8500_CTRL3_REG & 0xFF), data);	//0x0200

	old_data =
	    ab8500_read(AB8500_SYS_CTRL2_BLOCK,
			(AB8500_SYSULPCLK_CTRL1_REG & 0xFF));
#ifdef CONFIG_U8500_AB8500_CUT10
	data = 0x18 | old_data;
#else
	data = 0x10 | old_data;
#endif
	ab8500_write(AB8500_SYS_CTRL2_BLOCK, (AB8500_SYSULPCLK_CTRL1_REG & 0xFF), data);	//0x020B

	old_data =
	    ab8500_read(AB8500_REGU_CTRL1, (AB8500_REGU_MISC1_REG & 0xFF));
	data = 0x04 | old_data;
	ab8500_write(AB8500_REGU_CTRL1, (AB8500_REGU_MISC1_REG & 0xFF), data);	//0x380

	old_data =
	    ab8500_read(AB8500_REGU_CTRL1,
			(AB8500_REGU_VAUDIO_SUPPLY_REG & 0xFF));
	data = 0x5E | old_data;
	ab8500_write(AB8500_REGU_CTRL1, (AB8500_REGU_VAUDIO_SUPPLY_REG & 0xFF), data);	//0x0383

#ifdef CONFIG_U8500_AB8500_CUT10
	old_data = ab8500_read(AB8500_MISC, (AB8500_GPIO_DIR4_REG & 0xFF));
	data = 0x54 | old_data;
	ab8500_write(AB8500_MISC, (AB8500_GPIO_DIR4_REG & 0xFF), data);	//0x1013
#endif
}

/**
* u8500_acodec_deinit
*
* exit function for STW5098 audiocodec driver.
*/
static int check_device_id()
{
	__u8 data;

	data = ab8500_read(AB8500_MISC, (0x80 & 0xFF));
	if (((data & 0xF0) == 0x10) || ((data & 0xF0) == 0x11)) {
		/* V1 version */
#ifndef CONFIG_U8500_AB8500_CUT10
		printk("ERROR: AB8500 hardware detected is CUT1x\n");
		return -ENODEV;
#endif
	} else {
#ifndef CONFIG_U8500_AB8500_ED
		/* ED version */
		printk("ERROR: AB8500 hardware detected is EarlyDrop\n");
		return -ENODEV;
#endif
	}
	return 0;
}

static int __init u8500_acodec_init(void)
{
	int status, ret_val;
	t_ab8500_codec_error error;

	ret_val = check_device_id();
	if (0 != ret_val)
		return ret_val;

	status = i2s_register_driver(&i2sdrv_i2s);
	if (status < 0) {
		printk("Unable to register i2s driver\n");
		return status;
	}

	/*Initialize Audiocodec */

	ab8500_codec_power_init();

	AB8500_CODEC_Init(TRG_CODEC_ADDRESS_ON_SPI_BUS);

	/* Reset CODEC */
	error = AB8500_CODEC_Reset();
	if (AB8500_CODEC_OK != error) {
		stm_error("Error in AB8500_CODEC_Reset\n");
		return -1;
	}

	stm_dbg(DBG_ST.acodec, " leaving u8500_acodec_init() \n");
	return 0;
}

static void __exit u8500_acodec_deinit(void)
{
	stm_dbg(DBG_ST.acodec, "Entering AUDIOTRG_CODEC_DeIni\n");
	stm_dbg(DBG_ST.acodec, "leaving AUDIOTRG_CODEC_DeIni\n");
	i2s_unregister_driver(&i2sdrv_i2s);
}

module_init(u8500_acodec_init);
module_exit(u8500_acodec_deinit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AB8500 stw5098 audiocodec driver");

/* exported function by audiocodec	to be used by SAA driver and ALSA driver */

EXPORT_SYMBOL(u8500_acodec_open);
EXPORT_SYMBOL(u8500_acodec_close);
EXPORT_SYMBOL(u8500_acodec_send_data);
EXPORT_SYMBOL(u8500_acodec_receive_data);
EXPORT_SYMBOL(u8500_acodec_rates);
EXPORT_SYMBOL(u8500_acodec_powerdown);
EXPORT_SYMBOL(u8500_acodec_setuser);
EXPORT_SYMBOL(u8500_acodec_unsetuser);
EXPORT_SYMBOL(u8500_acodec_enable_audio_mode);
//EXPORT_SYMBOL(u8500_acodec_enable_voice_mode);
EXPORT_SYMBOL(u8500_acodec_get_output_volume);
EXPORT_SYMBOL(u8500_acodec_get_input_volume);
EXPORT_SYMBOL(u8500_acodec_set_output_volume);
EXPORT_SYMBOL(u8500_acodec_set_input_volume);
EXPORT_SYMBOL(u8500_acodec_select_input);
EXPORT_SYMBOL(u8500_acodec_select_output);

t_ab8500_codec_error AB8500_CODEC_Write(IN t_uint8 register_offset,
					IN t_uint8 count, IN t_uint8 * ptr_data)
{
	int i;
	u32 address;

	for (i = 0; i < count; i++) {
		address = (AB8500_AUDIO << 8) | (register_offset + i);
		ab8500_write(AB8500_AUDIO, address, ptr_data[i]);
	}
	return AB8500_CODEC_OK;
}

t_ab8500_codec_error AB8500_CODEC_Read(IN t_uint8 register_offset,
				       IN t_uint8 count,
				       IN t_uint8 * dummy_data,
				       IN t_uint8 * ptr_data)
{
	int i;
	u32 address;

	dummy_data = dummy_data;	/*keep compiler happy */

	for (i = 0; i < count; i++) {
		address = (AB8500_AUDIO << 8) | (register_offset + i);
		ptr_data[i] = ab8500_read(AB8500_AUDIO, address);
	}

	return AB8500_CODEC_OK;
}

EXPORT_SYMBOL(u8500_acodec_set_src_power_cntrl);
EXPORT_SYMBOL(u8500_acodec_set_burst_mode_fifo);
EXPORT_SYMBOL(u8500_acodec_get_dest_power_state);
EXPORT_SYMBOL(lpbk_state_in_texts);
EXPORT_SYMBOL(u8500_acodec_toggle_playback_mute_control);
EXPORT_SYMBOL(u8500_acodec_set_dest_power_cntrl);
EXPORT_SYMBOL(power_state_in_texts);
EXPORT_SYMBOL(u8500_acodec_toggle_capture_mute_control);
EXPORT_SYMBOL(u8500_acodec_toggle_analog_lpbk);
EXPORT_SYMBOL(u8500_acodec_toggle_digital_lpbk);
EXPORT_SYMBOL(tdm_mode_state_in_texts);
EXPORT_SYMBOL(switch_state_in_texts);
EXPORT_SYMBOL(pcm_rendering_state_in_texts);
EXPORT_SYMBOL(direct_rendering_state_in_texts);
EXPORT_SYMBOL(u8500_acodec_get_src_power_state);
EXPORT_SYMBOL(i2sdrv);
EXPORT_SYMBOL(second_config);

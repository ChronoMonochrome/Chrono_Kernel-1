/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ola Lilja (ola.o.lilja@stericsson.com),
 *         Roger Nilsson (roger.xr.nilsson@stericsson.com)
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#include <linux/slab.h>
#include <asm/dma.h>
#include <linux/bitops.h>

#include <mach/msp.h>
#include <linux/i2s/i2s.h>

#include <sound/soc.h>
#include <sound/soc-dai.h>
#include "ux500_msp_dai.h"
#include "ux500_pcm.h"

static struct ux500_msp_dai_private ux500_msp_dai_private[UX500_NBR_OF_DAI] = {
	{
		.i2s = NULL,
		.fmt = 0,
		.slots = 1,
		.tx_mask = 0x01,
		.rx_mask = 0x01,
		.slot_width = 16,
	},
	{
		.i2s = NULL,
		.fmt = 0,
		.slots = 1,
		.tx_mask = 0x01,
		.rx_mask = 0x01,
		.slot_width = 16,
	},
	{
		.i2s = NULL,
		.fmt = 0,
		.slots = 1,
		.tx_mask = 0x01,
		.rx_mask = 0x01,
		.slot_width = 16,
	},
};

static int ux500_msp_dai_i2s_probe(struct i2s_device *i2s)
{
	pr_info("%s: Enter (chip_select = %d, i2s = %d).\n",
		__func__,
		(int)i2s->chip_select, (int)(i2s));

	ux500_msp_dai_private[i2s->chip_select].i2s = i2s;

	try_module_get(i2s->controller->dev.parent->driver->owner);
	i2s_set_drvdata(
		i2s,
		(void *)&ux500_msp_dai_private[i2s->chip_select]);

	return 0;
}

static int ux500_msp_dai_i2s_remove(struct i2s_device *i2s)
{
	struct ux500_msp_dai_private *ux500_msp_dai_private =
		i2s_get_drvdata(i2s);

	pr_debug("%s: Enter (chip_select = %d).\n",
		__func__,
		(int)i2s->chip_select);

	ux500_msp_dai_private->i2s = NULL;
	i2s_set_drvdata(i2s, NULL);

	pr_debug("%s: Calling module_put.\n",
		__func__);
	module_put(i2s->controller->dev.parent->driver->owner);

	return 0;
}

static const struct i2s_device_id dev_id_table[] = {
	{ "i2s_device.0", 0, 0 },
	{ "i2s_device.1", 0, 0 },
	{ "i2s_device.2", 0, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2s, dev_id_table);

static struct i2s_driver i2sdrv_i2s = {
	.driver = {
		.name = "ux500_asoc_i2s",
		.owner = THIS_MODULE,
	},
	.probe = ux500_msp_dai_i2s_probe,
	.remove = __devexit_p(ux500_msp_dai_i2s_remove),
	.id_table = dev_id_table,
};

bool ux500_msp_dai_i2s_get_underrun_status(int dai_idx)
{
	struct ux500_msp_dai_private *dai_private = &ux500_msp_dai_private[dai_idx];
	int status = i2s_hw_status(dai_private->i2s->controller);
	return (bool)(status & TRANSMIT_UNDERRUN_ERR_INT);
}

dma_addr_t ux500_msp_dai_i2s_get_pointer(int dai_idx, int stream_id)
{
	struct ux500_msp_dai_private *dai_private = &ux500_msp_dai_private[dai_idx];
	return i2s_get_pointer(dai_private->i2s->controller,
			(stream_id == SNDRV_PCM_STREAM_PLAYBACK) ?
				I2S_DIRECTION_TX :
				I2S_DIRECTION_RX);
}

int ux500_msp_dai_i2s_configure_sg(dma_addr_t dma_addr,
				int sg_len,
				int sg_size,
				int dai_idx,
				int stream_id)
{
	struct ux500_msp_dai_private *dai_private = &ux500_msp_dai_private[dai_idx];
	struct i2s_message message;
	struct i2s_device *i2s_dev;
	int i;
	int ret = 0;
	struct scatterlist *sg;

	pr_debug("%s: Enter (MSP Index: %u, SG-length: %u, SG-size: %u).\n",
		__func__,
		dai_idx,
		sg_len,
		sg_size);

	if (!ux500_msp_dai[dai_idx].playback.active) {
		pr_err("%s: The I2S controller is not available."
			"MSP index:%d\n",
			__func__,
			dai_idx);
		return ret;
	}

	i2s_dev = dai_private->i2s;

	sg = kzalloc(sizeof(struct scatterlist) * sg_len, GFP_ATOMIC);
	sg_init_table(sg, sg_len);
	for (i = 0; i < sg_len; i++) {
		sg_dma_address(&sg[i]) = dma_addr + i * sg_size;
		sg_dma_len(&sg[i]) = sg_size;
	}

	message.i2s_transfer_mode = I2S_TRANSFER_MODE_CYCLIC_DMA;
	message.i2s_direction = (stream_id == SNDRV_PCM_STREAM_PLAYBACK) ?
					I2S_DIRECTION_TX :
					I2S_DIRECTION_RX;
	message.sg = sg;
	message.sg_len = sg_len;

	ret = i2s_transfer(i2s_dev->controller, &message);
	if (ret < 0) {
		pr_err("%s: Error: i2s_transfer failed. MSP index: %d\n",
			__func__,
			dai_idx);
	}

	kfree(sg);

	return ret;
}

int ux500_msp_dai_i2s_send_data(void *data,
				size_t bytes,
				int dai_idx)
{
	struct ux500_msp_dai_private *dai_private =
		&ux500_msp_dai_private[dai_idx];
	struct i2s_message message;
	struct i2s_device *i2s_dev;
	int ret = 0;

	pr_debug("%s: Enter. (MSP-index: %d, bytes = %d).\n",
		__func__,
		dai_idx,
		(int)bytes);

	i2s_dev = dai_private->i2s;

	if (!ux500_msp_dai[dai_idx].playback.active) {
		pr_err("%s: The I2S controller is not available."
			"MSP index:%d\n",
			__func__,
			dai_idx);
		return ret;
	}

	message.i2s_transfer_mode = I2S_TRANSFER_MODE_SINGLE_DMA;
	message.i2s_direction = I2S_DIRECTION_TX;
	message.txbytes = bytes;
	message.txdata = data;

	ret = i2s_transfer(i2s_dev->controller, &message);
	if (ret < 0) {
		pr_err("%s: Error: i2s_transfer failed. MSP index: %d\n",
			__func__,
			dai_idx);
	}

	return ret;
}

int ux500_msp_dai_i2s_receive_data(void *data,
				size_t bytes,
				int dai_idx)
{
	struct ux500_msp_dai_private *dai_private =
		&ux500_msp_dai_private[dai_idx];
	struct i2s_message message;
	struct i2s_device *i2s_dev;
	int ret = 0;

	pr_debug("%s: Enter. (MSP-index: %d, Bytes: %d).\n",
		__func__,
		dai_idx,
		(int)bytes);

	i2s_dev = dai_private->i2s;

	if (!ux500_msp_dai[dai_idx].capture.active) {
		pr_err("%s: MSP controller is not available. (MSP-index: %d)\n",
			__func__,
			dai_idx);
		return ret;
	}

	message.i2s_transfer_mode = I2S_TRANSFER_MODE_SINGLE_DMA;
	message.i2s_direction = I2S_DIRECTION_RX;
	message.rxbytes = bytes;
	message.rxdata = data;
	message.dma_flag = 1;

	ret = i2s_transfer(i2s_dev->controller, &message);
	if (ret < 0) {
		pr_err("%s: i2s_transfer failed (%d)! (MSP-index: %d)\n",
			__func__,
			ret,
			dai_idx);
	}

	return ret;
}

static void ux500_msp_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *msp_dai)
{
	struct ux500_msp_dai_private *dai_private = msp_dai->private_data;

	pr_info("%s: Enter (stream = %s).\n",
		__func__,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		"SNDRV_PCM_STREAM_PLAYBACK" : "SNDRV_PCM_STREAM_CAPTURE");
	if (dai_private == NULL)
		return;

	pr_debug("%s: chip_select = %d.\n",
		__func__,
		(int)dai_private->i2s->chip_select);

	if (i2s_cleanup(dai_private->i2s->controller,
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			DISABLE_TRANSMIT : DISABLE_RECEIVE)) {

			pr_err("%s: Error closing i2s for %s.\n",
			       __func__,
			       substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			       "playback" : "capture");
	}
	return;
}

static int ux500_msp_dai_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *msp_dai)
{
	struct ux500_msp_dai_private *dai_private =
		&ux500_msp_dai_private[msp_dai->id];

	pr_info("%s: MSP Index: %d.\n",
		__func__,
		msp_dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		msp_dai->playback.active : msp_dai->capture.active) {
		pr_err("%s: A %s stream is already active.\n",
			__func__,
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			"PLAYBACK" : "CAPTURE");
		return -EBUSY;
	}

	msp_dai->private_data = dai_private;

	if (dai_private->i2s == NULL) {
		pr_err("%s: MSP index: %d"
			"i2sdrv.i2s == NULL\n",
			__func__,
			msp_dai->id);
		return -1;
	}

	if (dai_private->i2s->controller == NULL) {
		pr_err("%s: MSP index: %d"
			"i2sdrv.i2s->controller == NULL.\n",
			__func__,
			msp_dai->id);
		return -1;
	}

	return 0;
}

static void ux500_msp_dai_setup_multichannel(
	struct ux500_msp_dai_private *private,
	struct msp_config *msp_config)
{
	struct msp_multichannel_config *multi =
		&msp_config->multichannel_config;

	if (private->slots > 1) {
		msp_config->multichannel_configured = 1;

		multi->tx_multichannel_enable = true;
		multi->rx_multichannel_enable = true;
		multi->rx_comparison_enable_mode = MSP_COMPARISON_DISABLED;

		multi->tx_channel_0_enable = private->tx_mask;
		multi->tx_channel_1_enable = 0;
		multi->tx_channel_2_enable = 0;
		multi->tx_channel_3_enable = 0;

		multi->rx_channel_0_enable = private->rx_mask;
		multi->rx_channel_1_enable = 0;
		multi->rx_channel_2_enable = 0;
		multi->rx_channel_3_enable = 0;

		pr_debug("%s: Multichannel enabled."
			"Slots: %d TX: %u RX: %u\n",
			__func__,
			private->slots,
			multi->tx_channel_0_enable,
			multi->rx_channel_0_enable);
	}
}

static void ux500_msp_dai_setup_frameper(
	struct ux500_msp_dai_private *private,
	unsigned int rate,
	struct msp_protocol_desc *prot_desc)
{
	switch (private->slots) {
	default:
	case 1:
		switch (rate) {
		case 8000:
			prot_desc->frame_period =
				FRAME_PER_SINGLE_SLOT_8_KHZ;
			break;
		case 16000:
			prot_desc->frame_period =
				FRAME_PER_SINGLE_SLOT_16_KHZ;
			break;
		case 44100:
			prot_desc->frame_period =
				FRAME_PER_SINGLE_SLOT_44_1_KHZ;
			break;
		case 48000:
		default:
			prot_desc->frame_period =
				FRAME_PER_SINGLE_SLOT_48_KHZ;
			break;
		}
		break;

	case 2:
		prot_desc->frame_period = FRAME_PER_2_SLOTS;
		break;

	case 8:
		prot_desc->frame_period =
			FRAME_PER_8_SLOTS;
		break;

	case 16:
		prot_desc->frame_period =
			FRAME_PER_16_SLOTS;
		break;
	}

	prot_desc->total_clocks_for_one_frame =
			prot_desc->frame_period+1;

	pr_debug("%s: Total clocks per frame: %u\n",
		__func__,
		prot_desc->total_clocks_for_one_frame);
}

static void ux500_msp_dai_setup_framing_pcm(
	struct ux500_msp_dai_private *private,
	unsigned int rate,
	struct msp_protocol_desc *prot_desc)
{
	u32 frame_length = MSP_FRAME_LENGTH_1;
	prot_desc->frame_width = 0;

	switch (private->slots) {
	default:
	case 1:
		frame_length = MSP_FRAME_LENGTH_1;
		break;

	case 2:
		frame_length = MSP_FRAME_LENGTH_2;
		break;

	case 8:
		frame_length = MSP_FRAME_LENGTH_8;
		break;

	case 16:
		frame_length = MSP_FRAME_LENGTH_16;
		break;
	}

	prot_desc->tx_frame_length_1 = frame_length;
	prot_desc->rx_frame_length_1 = frame_length;
	prot_desc->tx_frame_length_2 = frame_length;
	prot_desc->rx_frame_length_2 = frame_length;

	prot_desc->tx_element_length_1 = MSP_ELEM_LENGTH_16;
	prot_desc->rx_element_length_1 = MSP_ELEM_LENGTH_16;
	prot_desc->tx_element_length_2 = MSP_ELEM_LENGTH_16;
	prot_desc->rx_element_length_2 = MSP_ELEM_LENGTH_16;

	ux500_msp_dai_setup_frameper(private, rate, prot_desc);
}

static void ux500_msp_dai_setup_clocking(
	unsigned int fmt,
	struct msp_config *msp_config)
{

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	default:
	case SND_SOC_DAIFMT_NB_NF:
		msp_config->tx_frame_sync_pol =
			MSP_FRAME_SYNC_POL(MSP_FRAME_SYNC_POL_ACTIVE_HIGH);
		msp_config->rx_frame_sync_pol =
			MSP_FRAME_SYNC_POL_ACTIVE_HIGH << RFSPOL_SHIFT;
		break;

	case SND_SOC_DAIFMT_NB_IF:
		msp_config->tx_frame_sync_pol =
			MSP_FRAME_SYNC_POL(MSP_FRAME_SYNC_POL_ACTIVE_LOW);
		msp_config->rx_frame_sync_pol =
			MSP_FRAME_SYNC_POL_ACTIVE_LOW << RFSPOL_SHIFT;
		break;
	}

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) {
		pr_debug("%s: Codec is MASTER.\n",
			__func__);

		msp_config->rx_frame_sync_sel = 0;
		msp_config->tx_frame_sync_sel = 1 << TFSSEL_SHIFT;
		msp_config->tx_clock_sel = 0;
		msp_config->rx_clock_sel = 0;
		msp_config->srg_clock_sel = 0x2 << SCKSEL_SHIFT;
	} else {
		pr_debug("%s: Codec is SLAVE.\n",
			__func__);

		msp_config->tx_clock_sel = TX_CLK_SEL_SRG;
		msp_config->tx_frame_sync_sel = TX_SYNC_SRG_PROG;
		msp_config->rx_clock_sel = RX_CLK_SEL_SRG;
		msp_config->rx_frame_sync_sel = RX_SYNC_SRG;
		msp_config->srg_clock_sel = 1 << SCKSEL_SHIFT;
	}
}

static void ux500_msp_dai_compile_prot_desc_pcm(
	unsigned int fmt,
	struct msp_protocol_desc *prot_desc)
{
	prot_desc->rx_phase_mode = MSP_SINGLE_PHASE;
	prot_desc->tx_phase_mode = MSP_SINGLE_PHASE;
	prot_desc->rx_phase2_start_mode = MSP_PHASE2_START_MODE_IMEDIATE;
	prot_desc->tx_phase2_start_mode = MSP_PHASE2_START_MODE_IMEDIATE;
	prot_desc->rx_bit_transfer_format = MSP_BTF_MS_BIT_FIRST;
	prot_desc->tx_bit_transfer_format = MSP_BTF_MS_BIT_FIRST;
	prot_desc->rx_data_delay = MSP_DELAY_0;
	prot_desc->tx_data_delay = MSP_DELAY_0;

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A) {
		pr_debug("%s: DSP_A.\n",
			__func__);
		prot_desc->tx_clock_pol = MSP_FALLING_EDGE;
		prot_desc->rx_clock_pol = MSP_FALLING_EDGE;
	} else {
		pr_debug("%s: DSP_B.\n",
			__func__);
		prot_desc->tx_clock_pol = MSP_RISING_EDGE;
		prot_desc->rx_clock_pol = MSP_RISING_EDGE;
	}

	prot_desc->rx_half_word_swap = MSP_HWS_NO_SWAP;
	prot_desc->tx_half_word_swap = MSP_HWS_NO_SWAP;
	prot_desc->compression_mode = MSP_COMPRESS_MODE_LINEAR;
	prot_desc->expansion_mode = MSP_EXPAND_MODE_LINEAR;
	prot_desc->spi_clk_mode = MSP_SPI_CLOCK_MODE_NON_SPI;
	prot_desc->spi_burst_mode = MSP_SPI_BURST_MODE_DISABLE;
	prot_desc->frame_sync_ignore = MSP_FRAME_SYNC_IGNORE;
}

static void ux500_msp_dai_compile_prot_desc_i2s(
	struct msp_protocol_desc *prot_desc)
{
	prot_desc->rx_phase_mode = MSP_DUAL_PHASE;
	prot_desc->tx_phase_mode = MSP_DUAL_PHASE;
	prot_desc->rx_phase2_start_mode =
		MSP_PHASE2_START_MODE_FRAME_SYNC;
	prot_desc->tx_phase2_start_mode =
		MSP_PHASE2_START_MODE_FRAME_SYNC;
	prot_desc->rx_bit_transfer_format = MSP_BTF_MS_BIT_FIRST;
	prot_desc->tx_bit_transfer_format = MSP_BTF_MS_BIT_FIRST;
	prot_desc->rx_data_delay = MSP_DELAY_0;
	prot_desc->tx_data_delay = MSP_DELAY_0;

	prot_desc->rx_frame_length_1 = MSP_FRAME_LENGTH_1;
	prot_desc->rx_frame_length_2 = MSP_FRAME_LENGTH_1;
	prot_desc->tx_frame_length_1 = MSP_FRAME_LENGTH_1;
	prot_desc->tx_frame_length_2 = MSP_FRAME_LENGTH_1;
	prot_desc->rx_element_length_1 = MSP_ELEM_LENGTH_16;
	prot_desc->rx_element_length_2 = MSP_ELEM_LENGTH_16;
	prot_desc->tx_element_length_1 = MSP_ELEM_LENGTH_16;
	prot_desc->tx_element_length_2 = MSP_ELEM_LENGTH_16;

	prot_desc->rx_clock_pol = MSP_RISING_EDGE;
	prot_desc->tx_clock_pol = MSP_RISING_EDGE;

	prot_desc->tx_half_word_swap = MSP_HWS_NO_SWAP;
	prot_desc->rx_half_word_swap = MSP_HWS_NO_SWAP;
	prot_desc->compression_mode = MSP_COMPRESS_MODE_LINEAR;
	prot_desc->expansion_mode = MSP_EXPAND_MODE_LINEAR;
	prot_desc->spi_clk_mode = MSP_SPI_CLOCK_MODE_NON_SPI;
	prot_desc->spi_burst_mode = MSP_SPI_BURST_MODE_DISABLE;
	prot_desc->frame_sync_ignore = MSP_FRAME_SYNC_IGNORE;
}

static void ux500_msp_dai_compile_msp_config(
	struct snd_pcm_substream *substream,
	struct ux500_msp_dai_private *private,
	unsigned int rate,
	struct msp_config *msp_config)
{
	struct msp_protocol_desc *prot_desc = &msp_config->protocol_desc;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int fmt = private->fmt;

	memset(msp_config, 0, sizeof(*msp_config));

	msp_config->input_clock_freq = UX500_MSP_INTERNAL_CLOCK_FREQ;
	msp_config->tx_fifo_config = TX_FIFO_ENABLE;
	msp_config->rx_fifo_config = RX_FIFO_ENABLE;
	msp_config->spi_clk_mode = SPI_CLK_MODE_NORMAL;
	msp_config->spi_burst_mode = 0;
	msp_config->handler = ux500_pcm_dma_eot_handler;
	msp_config->tx_callback_data =
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		substream : NULL;
	msp_config->rx_callback_data =
		substream->stream == SNDRV_PCM_STREAM_CAPTURE ?
		substream : NULL;
	msp_config->def_elem_len = 1;
	msp_config->direction =
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		MSP_TRANSMIT_MODE : MSP_RECEIVE_MODE;
	msp_config->data_size = MSP_DATA_BITS_32;
	msp_config->work_mode = MSP_DMA_MODE;
	msp_config->frame_freq = rate;

	/* To avoid division by zero in I2S-driver (i2s_setup) */
	prot_desc->total_clocks_for_one_frame = 1;

	pr_debug("%s: rate: %u channels: %d.\n",
			__func__,
			rate,
			runtime->channels);
	switch (fmt &
		(SND_SOC_DAIFMT_FORMAT_MASK | SND_SOC_DAIFMT_MASTER_MASK)) {

	case SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS:
		pr_debug("%s: SND_SOC_DAIFMT_I2S.\n",
			__func__);

		msp_config->default_protocol_desc = 1;
		msp_config->protocol = MSP_I2S_PROTOCOL;
		break;

	default:
	case SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM:
		pr_debug("%s: SND_SOC_DAIFMT_I2S.\n",
			__func__);

		msp_config->data_size = MSP_DATA_BITS_16;
		msp_config->protocol = MSP_I2S_PROTOCOL;

		ux500_msp_dai_compile_prot_desc_i2s(prot_desc);
		break;

	case SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS:
	case SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_CBS_CFS:
	case SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_CBM_CFM:
		pr_debug("%s: PCM format.\n",
			__func__);
		msp_config->data_size = MSP_DATA_BITS_16;
		msp_config->protocol = MSP_PCM_PROTOCOL;

		ux500_msp_dai_compile_prot_desc_pcm(fmt, prot_desc);
		ux500_msp_dai_setup_multichannel(private, msp_config);
		ux500_msp_dai_setup_framing_pcm(private, rate, prot_desc);
		break;
	}

	ux500_msp_dai_setup_clocking(fmt, msp_config);
}

static int ux500_msp_dai_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *msp_dai)
{
	int ret = 0;
	struct ux500_msp_dai_private *dai_private = msp_dai->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msp_config msp_config;

	pr_debug("%s: Enter (stream = %p - %s, chip_select = %d).\n",
		__func__,
		substream,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		"SNDRV_PCM_STREAM_PLAYBACK" : "SNDRV_PCM_STREAM_CAPTURE",
		(int)dai_private->i2s->chip_select);

	pr_debug("%s: Setting up dai with rate %u.\n",
		__func__,
		runtime->rate);

	ux500_msp_dai_compile_msp_config(substream, dai_private,
		runtime->rate, &msp_config);

	ret = i2s_setup(dai_private->i2s->controller, &msp_config);
	if (ret < 0) {
		pr_err("u8500_msp_dai_prepare: i2s_setup failed! "
		"ret = %d\n", ret);
		goto cleanup;
	}

cleanup:
	return ret;
}

static int ux500_msp_dai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *msp_dai)
{
	unsigned int mask, slots_active;
	struct ux500_msp_dai_private *private = msp_dai->private_data;

	pr_debug("%s: Enter stream: %s, MSP index: %d.\n",
			__func__,
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
				"SNDRV_PCM_STREAM_PLAYBACK" :
				"SNDRV_PCM_STREAM_CAPTURE",
			(int)private->i2s->chip_select);

	switch (private->fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if (params_channels(params) != 2) {
			pr_err("%s: The I2S requires "
				"that the channel count of the substream "
				"is two. Substream channels: %d.\n",
				__func__,
				params_channels(params));
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_DSP_B:
	case SND_SOC_DAIFMT_DSP_A:

		mask = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			private->tx_mask :
			private->rx_mask;

		slots_active = hweight32(mask);

		pr_debug("TDM slots active: %d", slots_active);

		if (params_channels(params) != slots_active) {
			pr_err("%s: The PCM format requires "
				"that the channel count of the substream "
				"matches the number of active slots.\n"
				"Number of active slots: %d\n"
				"Substream channels: %d.\n",
				__func__,
				slots_active,
				params_channels(params));
			return -EINVAL;
		}
		break;

	default:
		break;
	}

	return 0;
}

static int ux500_msp_dai_set_dai_fmt(struct snd_soc_dai *msp_dai,
				unsigned int fmt)
{
	struct ux500_msp_dai_private *dai_private =
		msp_dai->private_data;

	pr_debug("%s: MSP index: %d: Enter.\n",
		__func__,
		(int)dai_private->i2s->chip_select);

	switch (fmt &
		(SND_SOC_DAIFMT_FORMAT_MASK | SND_SOC_DAIFMT_MASTER_MASK)) {
	case SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS:
	case SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_CBS_CFS:
	case SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS:
	case SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBM_CFM:
		break;

	default:
		pr_err("Unsupported DAI format (0x%x)!\n",
			fmt);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
	case SND_SOC_DAIFMT_NB_IF:
		break;

	default:
		pr_err("Unsupported DAI format (0x%x)!\n",
			fmt);
		return -EINVAL;
	}

	dai_private->fmt = fmt;
	return 0;
}

static int ux500_msp_dai_set_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask,
	unsigned int rx_mask,
	int slots,
	int slot_width)
{
	struct ux500_msp_dai_private *private =
		dai->private_data;
	unsigned int cap;

	if (!(slots == 1 || slots == 2 || slots == 8 || slots == 16)) {
		pr_err("%s - error: slots %d  Supported values are 1/2/8/16.\n",
			__func__,
			slots);
		return -EINVAL;
	}
	private->slots = slots;

	if (!(slot_width == 16)) {
		pr_err("%s - error: slot_width %d  Supported value is 16.\n",
			__func__,
			slot_width);
		return -EINVAL;
	}
	private->slot_width = slot_width;

	switch (slots) {
	default:
	case 1:
		cap = 0x01;
		break;
	case 2:
		cap = 0x03;
		break;
	case 8:
		cap = 0xFF;
		break;
	case 16:
		cap = 0xFFFF;
		break;
	}

	private->tx_mask = tx_mask & cap;
	private->rx_mask = rx_mask & cap;

	return 0;
}

static int ux500_msp_dai_trigger(struct snd_pcm_substream *substream,
				int cmd,
				struct snd_soc_dai *msp_dai)
{
	int ret = 0;
	struct ux500_msp_dai_private *dai_private =
		msp_dai->private_data;

	pr_debug("%s: Enter (stream = %p - %s,"
		" chip_select = %d, cmd = %d).\n",
		__func__,
		substream,
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			"SNDRV_PCM_STREAM_PLAYBACK" :
			"SNDRV_PCM_STREAM_CAPTURE",
		(int)dai_private->i2s->chip_select,
		cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = 0;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = 0;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = 0;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct snd_soc_dai ux500_msp_dai[UX500_NBR_OF_DAI] = {
	{
		.name = "ux500_i2s-0",
		.id = 0,
		.suspend = NULL,
		.resume = NULL,
		.playback = {
			.channels_min = UX500_MSP_MIN_CHANNELS,
			.channels_max = UX500_MSP_MAX_CHANNELS,
			.rates = UX500_I2S_RATES,
			.formats = UX500_I2S_FORMATS,
		},
		.capture = {
			.channels_min = UX500_MSP_MIN_CHANNELS,
			.channels_max = UX500_MSP_MAX_CHANNELS,
			.rates = UX500_I2S_RATES,
			.formats = UX500_I2S_FORMATS,
		},
		.ops = (struct snd_soc_dai_ops[]) {
			{
			.set_sysclk = NULL,
			.set_fmt = ux500_msp_dai_set_dai_fmt,
			.set_tdm_slot = ux500_msp_dai_set_tdm_slot,
			.startup = ux500_msp_dai_startup,
			.shutdown = ux500_msp_dai_shutdown,
			.prepare = ux500_msp_dai_prepare,
			.trigger = ux500_msp_dai_trigger,
			.hw_params = ux500_msp_dai_hw_params,
			}
		},
		.private_data = &ux500_msp_dai_private[0],
	},
	{
		.name = "ux500_i2s-1",
		.id = 1,
		.suspend = NULL,
		.resume = NULL,
		.playback = {
			.channels_min = UX500_MSP_MIN_CHANNELS,
			.channels_max = UX500_MSP_MAX_CHANNELS,
			.rates = UX500_I2S_RATES,
			.formats = UX500_I2S_FORMATS,
		},
		.capture = {
			.channels_min = UX500_MSP_MIN_CHANNELS,
			.channels_max = UX500_MSP_MAX_CHANNELS,
			.rates = UX500_I2S_RATES,
			.formats = UX500_I2S_FORMATS,
		},
		.ops = (struct snd_soc_dai_ops[]) {
			{
			.set_sysclk = NULL,
			.set_fmt = ux500_msp_dai_set_dai_fmt,
			.set_tdm_slot = ux500_msp_dai_set_tdm_slot,
			.startup = ux500_msp_dai_startup,
			.shutdown = ux500_msp_dai_shutdown,
			.prepare = ux500_msp_dai_prepare,
			.trigger = ux500_msp_dai_trigger,
			.hw_params = ux500_msp_dai_hw_params,
			}
		},
		.private_data = &ux500_msp_dai_private[1],
	},
	{
		.name = "ux500_i2s-2",
		.id = 2,
		.suspend = NULL,
		.resume = NULL,
		.playback = {
			.channels_min = UX500_MSP_MIN_CHANNELS,
			.channels_max = UX500_MSP_MAX_CHANNELS,
			.rates = UX500_I2S_RATES,
			.formats = UX500_I2S_FORMATS,
		},
		.capture = {
			.channels_min = UX500_MSP_MIN_CHANNELS,
			.channels_max = UX500_MSP_MAX_CHANNELS,
			.rates = UX500_I2S_RATES,
			.formats = UX500_I2S_FORMATS,
		},
		.ops = (struct snd_soc_dai_ops[]) {
			{
			.set_sysclk = NULL,
			.set_fmt = ux500_msp_dai_set_dai_fmt,
			.set_tdm_slot = ux500_msp_dai_set_tdm_slot,
			.startup = ux500_msp_dai_startup,
			.shutdown = ux500_msp_dai_shutdown,
			.prepare = ux500_msp_dai_prepare,
			.trigger = ux500_msp_dai_trigger,
			.hw_params = ux500_msp_dai_hw_params,
			}
		},
		.private_data = &ux500_msp_dai_private[2],
	},
};
EXPORT_SYMBOL(ux500_msp_dai);

static int __init ux500_msp_dai_init(void)
{
	int i;
	int ret = 0;

	ret = i2s_register_driver(&i2sdrv_i2s);
	if (ret < 0) {
		pr_err("%s: Unable to register as a I2S driver.\n",
			__func__);
		return ret;
	}

	for (i = 0; i < UX500_NBR_OF_DAI; i++) {
		pr_debug("%s: Register MSP dai %d.\n",
			__func__,
			i);
		ret = snd_soc_register_dai(&ux500_msp_dai[i]);
		if (ret < 0) {
			pr_err("Error: Failed to register MSP dai %d.\n",
				i);
			return ret;
		}
	}

	return ret;
}

static void __exit ux500_msp_dai_exit(void)
{
	int i;

	pr_debug("%s: Enter.\n", __func__);

	i2s_unregister_driver(&i2sdrv_i2s);

	for (i = 0; i < UX500_NBR_OF_DAI; i++)
		snd_soc_unregister_dai(&ux500_msp_dai[i]);
}

module_init(ux500_msp_dai_init);
module_exit(ux500_msp_dai_exit);

MODULE_LICENSE("GPL v2");

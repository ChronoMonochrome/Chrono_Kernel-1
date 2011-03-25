/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Roger Nilsson roger.xr.nilsson@stericsson.com
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
 #include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/bitops.h>
#include <linux/mfd/cg2900_audio.h>

#include "cg29xx.h"

#define CG29XX_NBR_OF_DAI	2
#define CG29XX_SUPPORTED_RATE_PCM (SNDRV_PCM_RATE_8000 | \
	SNDRV_PCM_RATE_16000)

#define CG29XX_SUPPORTED_RATE (SNDRV_PCM_RATE_8000 | \
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define CG29XX_SUPPORTED_FMT (SNDRV_PCM_FMTBIT_S16_LE)

enum cg29xx_dai_direction {
	CG29XX_DAI_DIRECTION_TX,
	CG29XX_DAI_DIRECTION_RX
};

static int cg29xx_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai);

static int cg29xx_dai_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai);

static int cg29xx_dai_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params,
	struct snd_soc_dai *dai);

static void cg29xx_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai);

static int cg29xx_set_dai_sysclk(
	struct snd_soc_dai *codec_dai,
	int clk_id,
	unsigned int freq, int dir);

static int cg29xx_set_dai_fmt(
	struct snd_soc_dai *codec_dai,
	unsigned int fmt);

static int cg29xx_set_tdm_slot(
	struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask,
	int slots,
	int slot_width);

static struct cg29xx_codec codec_private = {
	.session = 0,
};

static struct snd_soc_codec *cg29xx_codec;

static struct cg29xx_dai
	cg29xx_dai_private[CG29XX_NBR_OF_DAI] = {
	{
		.tx_active = 0,
		.rx_active = 0,
		.input_select = 0,
		.output_select = 0,
		.config = {
			.port = PORT_0_I2S,
			.conf.i2s.mode = DAI_MODE_SLAVE,
			.conf.i2s.half_period = HALF_PER_DUR_16,
			.conf.i2s.channel_sel = CHANNEL_SELECTION_BOTH,
			.conf.i2s.sample_rate = SAMPLE_RATE_48,
			.conf.i2s.word_width = WORD_WIDTH_32
		}
	},
	{
		.tx_active = 0,
		.rx_active = 0,
		.input_select = 0,
		.output_select = 0,
		.config = {
			.port = PORT_1_I2S_PCM,
			.conf.i2s_pcm.mode = DAI_MODE_SLAVE,
			.conf.i2s_pcm.slot_0_dir = DAI_DIR_B_RX_A_TX,
			.conf.i2s_pcm.slot_1_dir = DAI_DIR_B_TX_A_RX,
			.conf.i2s_pcm.slot_2_dir = DAI_DIR_B_RX_A_TX,
			.conf.i2s_pcm.slot_3_dir = DAI_DIR_B_RX_A_TX,
			.conf.i2s_pcm.slot_0_used = true,
			.conf.i2s_pcm.slot_1_used = false,
			.conf.i2s_pcm.slot_2_used = false,
			.conf.i2s_pcm.slot_3_used = false,
			.conf.i2s_pcm.slot_0_start = 0,
			.conf.i2s_pcm.slot_1_start = 16,
			.conf.i2s_pcm.slot_2_start = 32,
			.conf.i2s_pcm.slot_3_start = 48,
			.conf.i2s_pcm.protocol = PORT_PROTOCOL_PCM,
			.conf.i2s_pcm.ratio = STREAM_RATIO_FM48_VOICE16,
			.conf.i2s_pcm.duration = SYNC_DURATION_32,
			.conf.i2s_pcm.clk = BIT_CLK_512,
			.conf.i2s_pcm.sample_rate = SAMPLE_RATE_16,
		}
	},
};

static struct snd_soc_dai_ops cg29xx_dai_ops = {
	.startup = cg29xx_dai_startup,
	.prepare = cg29xx_dai_prepare,
	.hw_params = cg29xx_dai_hw_params,
    .shutdown = cg29xx_dai_shutdown,
	.set_sysclk = cg29xx_set_dai_sysclk,
	.set_fmt = cg29xx_set_dai_fmt,
	.set_tdm_slot = cg29xx_set_tdm_slot
};

struct snd_soc_dai cg29xx_codec_dai[] = {
	{
		.name = "cg29xx_0",
		.id = 0,
		.playback = {
			.stream_name = "cg29xx_0_pb",
			.channels_min = 2,
			.channels_max = 2,
			.rates = CG29XX_SUPPORTED_RATE,
			.formats = CG29XX_SUPPORTED_FMT,
		},
		.capture = {
			.stream_name = "cg29xx_0_cap",
			.channels_min = 2,
			.channels_max = 2,
			.rates = CG29XX_SUPPORTED_RATE,
			.formats = CG29XX_SUPPORTED_FMT,
		},
		.ops = &cg29xx_dai_ops,
		.symmetric_rates = 1,
		.private_data = &cg29xx_dai_private[0]
	},
	{
		.name = "cg29xx_1",
		.id = 1,
		.playback = {
			.stream_name = "cg29xx_1_pb",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CG29XX_SUPPORTED_RATE_PCM,
			.formats = CG29XX_SUPPORTED_FMT,
		},
		.capture = {
			.stream_name = "cg29xx_1_cap",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CG29XX_SUPPORTED_RATE_PCM,
			.formats = CG29XX_SUPPORTED_FMT,
		},
		.ops = &cg29xx_dai_ops,
		.symmetric_rates = 1,
		.private_data = &cg29xx_dai_private[1]
	}
};
EXPORT_SYMBOL_GPL(cg29xx_codec_dai);

static const char *enum_ifs_input_select[] = {
	"BT_SCO", "FM_RX"
};

static const char *enum_ifs_output_select[] = {
	"BT_SCO", "FM_TX"
};

/* If0 Input Select */
static struct soc_enum if0_input_select =
	SOC_ENUM_SINGLE(INTERFACE0_INPUT_SELECT, 0,
		ARRAY_SIZE(enum_ifs_input_select),
		enum_ifs_input_select);

/* If1 Input Select */
static struct soc_enum if1_input_select =
	SOC_ENUM_SINGLE(INTERFACE1_INPUT_SELECT, 0,
		ARRAY_SIZE(enum_ifs_input_select),
		enum_ifs_input_select);

/* If0 Output Select */
static struct soc_enum if0_output_select =
	SOC_ENUM_SINGLE(INTERFACE0_OUTPUT_SELECT, 0,
		ARRAY_SIZE(enum_ifs_output_select),
		enum_ifs_output_select);

/* If1 Output Select */
static struct soc_enum if1_output_select =
	SOC_ENUM_SINGLE(INTERFACE1_OUTPUT_SELECT, 4,
		ARRAY_SIZE(enum_ifs_output_select),
		enum_ifs_output_select);

static struct snd_kcontrol_new cg29xx_snd_controls[] = {
	SOC_ENUM("If0 Input Select",	if0_input_select),
	SOC_ENUM("If1 Input Select",	if1_input_select),
	SOC_ENUM("If0 Output Select",	if0_output_select),
	SOC_ENUM("If1 Output Select",	if1_output_select),
};

static int cg29xx_set_dai_sysclk(
	struct snd_soc_dai *codec_dai,
	int clk_id,
	unsigned int freq, int dir)
{
	return 0;
}

static int cg29xx_set_dai_fmt(
	struct snd_soc_dai *codec_dai,
	unsigned int fmt)
{
	struct cg29xx_dai *private =
		codec_dai->private_data;
	unsigned int prot;
	unsigned int msel;

	prot = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
	msel = fmt & SND_SOC_DAIFMT_MASTER_MASK;

	switch (prot) {
	case SND_SOC_DAIFMT_I2S:
		if (private->config.port != PORT_0_I2S) {
			pr_err("cg29xx_dai: unsupported DAI format 0x%x\n",
				fmt);
			return -EINVAL;
		}

		if (msel == SND_SOC_DAIFMT_CBM_CFM)
			private->config.conf.i2s.mode = DAI_MODE_MASTER;
		else
			private->config.conf.i2s.mode = DAI_MODE_SLAVE;
		break;

	case SND_SOC_DAIFMT_DSP_A:
		if (private->config.port != PORT_1_I2S_PCM ||
			msel == SND_SOC_DAIFMT_CBM_CFM) {
			pr_err("cg29xx_dai: unsupported DAI format 0x%x\n",
				fmt);
			return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int cg29xx_set_tdm_slot(
	struct snd_soc_dai *dai,
	unsigned int tx_mask,
	unsigned int rx_mask,
	int slots,
	int slot_width)
{
	struct cg29xx_dai *private =
		dai->private_data;

	if (private->config.port != PORT_1_I2S_PCM)
		return -EINVAL;

	private->config.conf.i2s_pcm.slot_0_used =
		(tx_mask | rx_mask) & (1<<CG29XX_DAI_SLOT0_SHIFT) ?
		true : false;
	private->config.conf.i2s_pcm.slot_1_used =
		(tx_mask | rx_mask) & (1<<CG29XX_DAI_SLOT1_SHIFT) ?
		true : false;
	private->config.conf.i2s_pcm.slot_2_used =
		(tx_mask | rx_mask) & (1<<CG29XX_DAI_SLOT2_SHIFT) ?
		true : false;
	private->config.conf.i2s_pcm.slot_3_used =
		(tx_mask | rx_mask) & (1<<CG29XX_DAI_SLOT3_SHIFT) ?
		true : false;

	private->config.conf.i2s_pcm.slot_0_start = 0;
	private->config.conf.i2s_pcm.slot_1_start = slot_width;
	private->config.conf.i2s_pcm.slot_2_start = 2 * slot_width;
	private->config.conf.i2s_pcm.slot_3_start = 3 * slot_width;

	return 0;
}

static int cg29xx_configure_endp(
	struct cg29xx_dai *dai,
	enum cg2900_audio_endpoint_id endpid)
{
	struct cg2900_endpoint_config config;
	int err;
	enum cg2900_dai_sample_rate dai_sr;
	enum cg2900_endpoint_sample_rate endp_sr;

	switch (dai->config.port) {
	default:
	case PORT_0_I2S:
		dai_sr = dai->config.conf.i2s.sample_rate;
		break;

	case PORT_1_I2S_PCM:
		dai_sr = dai->config.conf.i2s_pcm.sample_rate;
		break;
	}

	switch (dai_sr) {
	default:
	case SAMPLE_RATE_8:
		endp_sr = ENDPOINT_SAMPLE_RATE_8_KHZ;
		break;
	case SAMPLE_RATE_16:
		endp_sr = ENDPOINT_SAMPLE_RATE_16_KHZ;
		break;
	case SAMPLE_RATE_44_1:
		endp_sr = ENDPOINT_SAMPLE_RATE_44_1_KHZ;
		break;
	case SAMPLE_RATE_48:
		endp_sr = ENDPOINT_SAMPLE_RATE_48_KHZ;
		break;
	}

	config.endpoint_id = endpid;

	switch (endpid) {
	default:
	case ENDPOINT_BT_SCO_INOUT:
		config.config.sco.sample_rate = endp_sr;
		break;

	case ENDPOINT_FM_TX:
	case ENDPOINT_FM_RX:
		config.config.fm.sample_rate = endp_sr;
		break;
	}

	err = cg2900_audio_config_endpoint(codec_private.session, &config);

	return err;
}

static int cg29xx_stop_if(
	struct cg29xx_dai *dai,
	enum cg29xx_dai_direction direction)
{
	int err = 0;
	unsigned int *stream;

	if (direction == CG29XX_DAI_DIRECTION_TX)
		stream = &dai->tx_active;
	else
		stream = &dai->rx_active;

	if (*stream) {
		err = cg2900_audio_stop_stream(
			codec_private.session,
			*stream);
		if (!err) {
			*stream = 0;
		} else {
			pr_err("asoc cg29xx - %s - Failed to stop stream on interface %d.\n",
				__func__,
				dai->config.port);
		}
	}

	return err;
}

static int cg29xx_start_if(
	struct cg29xx_dai *dai,
	enum cg29xx_dai_direction direction)
{
	enum cg2900_audio_endpoint_id if_endpid;
	enum cg2900_audio_endpoint_id endpid;
	unsigned int *stream;
	int err;

	if (dai->config.port == PORT_0_I2S)
		if_endpid = ENDPOINT_PORT_0_I2S;
	else
		if_endpid = ENDPOINT_PORT_1_I2S_PCM;

	if (direction == CG29XX_DAI_DIRECTION_RX) {
		switch (dai->output_select) {
		default:
		case 0:
			endpid = ENDPOINT_BT_SCO_INOUT;
			break;
		case 1:
			endpid = ENDPOINT_FM_TX;
		}
		stream = &dai->rx_active;
	} else {
		switch (dai->input_select) {
		default:
		case 0:
			endpid = ENDPOINT_BT_SCO_INOUT;
			break;
		case 1:
			endpid = ENDPOINT_FM_RX;
		}

		stream = &dai->tx_active;
	}

	if (*stream) {
		pr_debug("asoc cg29xx - %s - The interface has already been started.\n",
			__func__);
		return 0;
	}

	pr_debug("asoc cg29xx - %s - direction: %d, if_id: %d endpid: %d\n",
			__func__,
			direction,
			if_endpid,
			endpid);

	err = cg29xx_configure_endp(dai, endpid);

	if (err) {
		pr_err("asoc cg29xx - %s - Configure endpoint id: %d failed.\n",
			__func__,
			endpid);

		return err;
	}

	err = cg2900_audio_start_stream(
		codec_private.session,
		if_endpid,
		endpid,
		stream);

	return err;
}

static int cg29xx_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int err = 0;

	if (!codec_private.session)
		err = cg2900_audio_open(&codec_private.session);

	return err;
}

static int cg29xx_dai_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int err = 0;
	enum cg29xx_dai_direction direction;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		direction = CG29XX_DAI_DIRECTION_RX;
	else
		direction = CG29XX_DAI_DIRECTION_TX;

	err = cg29xx_start_if(dai->private_data, direction);

	return err;
}

static void cg29xx_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	enum cg29xx_dai_direction direction;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		direction = CG29XX_DAI_DIRECTION_RX;
	else
		direction = CG29XX_DAI_DIRECTION_TX;

	(void) cg29xx_stop_if(dai->private_data, direction);
}

static int cg29xx_dai_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params,
	struct snd_soc_dai *dai)
{
	struct cg29xx_dai *private = dai->private_data;
	enum cg2900_dai_fs_duration duration = SYNC_DURATION_32;
	enum cg2900_dai_bit_clk bclk = BIT_CLK_512;
	int sr;
	int err = 0;
	enum cg2900_dai_stream_ratio ratio = STREAM_RATIO_FM48_VOICE16;

	pr_debug("cg29xx asoc - %s called. Port: %d.\n",
		__func__,
		private->config.port);

	switch (params_rate(hw_params)) {
	case 8000:
		sr = SAMPLE_RATE_8;
		bclk = BIT_CLK_512;
		duration = SYNC_DURATION_32;
		ratio = STREAM_RATIO_FM48_VOICE8;
		break;
	case 16000:
		sr = SAMPLE_RATE_16;
		bclk = BIT_CLK_512;
		duration = SYNC_DURATION_32;
		ratio = STREAM_RATIO_FM48_VOICE16;
		break;
	case 44100:
		sr = SAMPLE_RATE_44_1;
		break;
	case 48000:
		sr = SAMPLE_RATE_48;
		break;
	default:
		return -EINVAL;
	}

	if (private->config.port == PORT_0_I2S) {
		private->config.conf.i2s.sample_rate = sr;
	} else {
		private->config.conf.i2s_pcm.sample_rate = sr;
		private->config.conf.i2s_pcm.duration = duration;
		private->config.conf.i2s_pcm.clk = bclk;
		private->config.conf.i2s_pcm.ratio = ratio;
	}

	if (!(private->tx_active | private->rx_active)) {
		err = cg2900_audio_set_dai_config(
			codec_private.session,
			&private->config);

		pr_debug("asoc cg29xx: cg2900_audio_set_dai_config"
			"on port %d completed with result: %d.\n",
			private->config.port,
			err);
	}

	return err;
}

static unsigned int cg29xx_codec_read(
	struct snd_soc_codec *codec,
	unsigned int reg)
{

	switch (reg) {

	case INTERFACE0_INPUT_SELECT:
		return cg29xx_dai_private[0].input_select;

	case INTERFACE1_INPUT_SELECT:
		return cg29xx_dai_private[1].input_select;

	case INTERFACE0_OUTPUT_SELECT:
		return cg29xx_dai_private[0].output_select;

	case INTERFACE1_OUTPUT_SELECT:
		return cg29xx_dai_private[1].output_select;

	default:
		return 0;
	}

	return 0;
}

static int cg29xx_codec_write(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value)
{
	int old_value;
	struct cg29xx_dai *dai;
	enum cg29xx_dai_direction direction;
	bool restart_if = false;

	switch (reg) {

	case INTERFACE0_INPUT_SELECT:
		dai = &cg29xx_dai_private[0];
		direction = CG29XX_DAI_DIRECTION_TX;

		old_value = dai->input_select;
		dai->input_select = value;

		if ((old_value ^ value) && dai->tx_active)
			restart_if = true;
		break;

	case INTERFACE1_INPUT_SELECT:
		dai = &cg29xx_dai_private[1];
		direction = CG29XX_DAI_DIRECTION_TX;

		old_value = dai->input_select;
		dai->input_select = value;

		if ((old_value ^ value) && dai->tx_active)
			restart_if = true;
		break;

	case INTERFACE0_OUTPUT_SELECT:
		dai = &cg29xx_dai_private[0];
		direction = CG29XX_DAI_DIRECTION_RX;

		old_value = dai->output_select;
		dai->output_select = value;

		if ((old_value ^ value) && dai->rx_active)
			restart_if = true;
		break;

	case INTERFACE1_OUTPUT_SELECT:
		dai = &cg29xx_dai_private[1];
		direction = CG29XX_DAI_DIRECTION_RX;

		old_value = dai->output_select;
		dai->output_select = value;

		if ((old_value ^ value) && dai->rx_active)
			restart_if = true;
		break;

	default:
		return -EINVAL;
	}

	if (restart_if) {
		(void) cg29xx_stop_if(dai, direction);
		(void) cg29xx_start_if(dai, direction);
	}

	return 0;
}

static int cg29xx_soc_probe(struct platform_device *pdev)
{
	int err;
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	socdev->card->codec = cg29xx_codec;

	err = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (err < 0) {
		pr_err("cg29xx asoc - snd_soc_new_pcms failed with error: %d\n",
			err);
		goto err1;
	}

	snd_soc_add_controls(
		cg29xx_codec,
		cg29xx_snd_controls,
		ARRAY_SIZE(cg29xx_snd_controls));

	return 0;

	snd_soc_free_pcms(socdev);
err1:
	return err;
}

static int cg29xx_soc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	snd_soc_free_pcms(socdev);

	return 0;
}

static int cg29xx_soc_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	return 0;
}

static int cg29xx_soc_resume(struct platform_device *pdev)
{
	return 0;
}

struct snd_soc_codec_device soc_codec_dev_cg29xx = {
	.probe =	cg29xx_soc_probe,
	.remove =	cg29xx_soc_remove,
	.suspend =	cg29xx_soc_suspend,
	.resume =	cg29xx_soc_resume
};
EXPORT_SYMBOL_GPL(soc_codec_dev_cg29xx);

static int __init cg29xx_init(void)
{
	int err;
	int i;

	cg29xx_codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);

	if (!cg29xx_codec)
		return -ENOMEM;

	cg29xx_codec->name = "CG29XX";
	cg29xx_codec->owner = THIS_MODULE;
	cg29xx_codec->dai = cg29xx_codec_dai;
	cg29xx_codec->num_dai = CG29XX_NBR_OF_DAI;
	cg29xx_codec->read = cg29xx_codec_read;
	cg29xx_codec->write = cg29xx_codec_write;
	INIT_LIST_HEAD(&cg29xx_codec->dapm_widgets);
	INIT_LIST_HEAD(&cg29xx_codec->dapm_paths);
	mutex_init(&cg29xx_codec->mutex);

	err = snd_soc_register_codec(cg29xx_codec);

	if (err) {
		pr_err(
			"asoc cg29xx - snd_soc_register_codec"
			" failed with error: %d.\n",
			err);

		return err;
	}

	for (i = 0; i < CG29XX_NBR_OF_DAI; i++) {
		mutex_init(&cg29xx_dai_private[i].mutex);

		err = snd_soc_register_dai(&cg29xx_codec_dai[i]);

		if (err) {
			pr_err(
				"asoc cg29xx - snd_soc_register_dai"
				" failed with error: %d.\n",
				err);
			return err;
		}
	}

	return err;
}
module_init(cg29xx_init);

static void __exit cg29xx_exit(void)
{
	int i;

	(void) cg2900_audio_close(&codec_private.session);

	if (cg29xx_codec) {
		snd_soc_unregister_codec(cg29xx_codec);
		kfree(cg29xx_codec);
		cg29xx_codec = NULL;
	}

	for (i = 0; i < CG29XX_NBR_OF_DAI; i++)
		snd_soc_unregister_dai(&cg29xx_codec_dai[i]);
}
module_exit(cg29xx_exit);

MODULE_DESCRIPTION("CG29xx codec driver");
MODULE_LICENSE("GPL v2");

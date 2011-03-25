/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ola Lilja (ola.o.lilja@stericsson.com)
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
#include <video/av8100.h>
#include <video/hdmi.h>

#include "av8100_audio.h"

#define AV8100_SUPPORTED_RATE (SNDRV_PCM_RATE_48000)
#define AV8100_SUPPORTED_FMT (SNDRV_PCM_FMTBIT_S16_LE)

static int setupAV8100_stereo(void)
{
	union av8100_configuration config;
	struct av8100_status status;
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	/* Startup AV8100 if it is not already started */
	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		pr_info("%s: Powering up AV8100.", __func__);
		ret = av8100_powerup();
		if (ret != 0) {
			pr_err("%s: Power up AV8100 failed "
				"(av8100_powerup returned %d)!\n",
				__func__,
				ret);
			return -EINVAL;
		}
	}
	if (status.av8100_state < AV8100_OPMODE_INIT) {
		ret = av8100_download_firmware(NULL, 0, I2C_INTERFACE);
		if (ret != 0) {
			pr_err("%s: Download firmware failed "
				"(av8100_download_firmware returned %d)!\n",
				__func__,
				ret);
			return -EINVAL;
		}
	}

	/* Set the HDMI format of AV8100 */
	pr_info("%s: Setting hdmi_format.", __func__);
	config.hdmi_format.hdmi_mode = AV8100_HDMI_ON;
	config.hdmi_format.hdmi_format = AV8100_HDMI;
	ret = av8100_conf_prep(AV8100_COMMAND_AUDIO_INPUT_FORMAT, &config);
	if (ret != 0) {
		pr_err("%s: Setting hdmi_format failed "
			"(av8100_conf_prep returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}
	ret = av8100_conf_w(AV8100_COMMAND_AUDIO_INPUT_FORMAT,
			NULL,
			NULL,
			I2C_INTERFACE);
	if (ret != 0) {
		pr_err("%s: Setting hdmi_format failed "
			"(av8100_conf_w returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}

	/* Set the audio input format of AV8100 */
	pr_info("%s: Setting audio_input_format.", __func__);
	config.audio_input_format.audio_input_if_format	= AV8100_AUDIO_I2SDELAYED_MODE;
	config.audio_input_format.i2s_input_nb = 1;
	config.audio_input_format.sample_audio_freq = AV8100_AUDIO_FREQ_48KHZ;
	config.audio_input_format.audio_word_lg	= AV8100_AUDIO_16BITS;
	config.audio_input_format.audio_format = AV8100_AUDIO_LPCM_MODE;
	config.audio_input_format.audio_if_mode = AV8100_AUDIO_MASTER;
	config.audio_input_format.audio_mute = AV8100_AUDIO_MUTE_DISABLE;
	ret = av8100_conf_prep(AV8100_COMMAND_AUDIO_INPUT_FORMAT, &config);
	if (ret != 0) {
		pr_err("%s: Setting audio_input_format failed "
			"(av8100_conf_prep returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}
	if (av8100_conf_w(AV8100_COMMAND_AUDIO_INPUT_FORMAT,
		NULL, NULL, I2C_INTERFACE) != 0) {
		pr_err("%s: Setting audio_input_format failed "
			"(av8100_conf_w returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}

	return 0;
}

static int av8100_codec_pcm_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	pr_debug("%s: Enter.\n", __func__);

	return 0;
}

static int av8100_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *hw_params,
				struct snd_soc_dai *dai)
{
	int ret;
	int channels;

	pr_debug("%s: Enter.\n", __func__);

	channels = params_channels(hw_params);
	switch (channels) {
	case 1:
		goto error_channels;
	case 2:
		ret = setupAV8100_stereo();
		break;
	case 6:
		goto error_channels;
	default:
		goto error_channels;
	}

	return ret;

error_channels:
	pr_err("%s: Unsupported number of channels (%d)!\n", __func__, channels);

	return -1;
}

static void av8100_codec_pcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	pr_debug("%s: Enter.\n", __func__);
}

static int av8100_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id,
				unsigned int freq, int dir)
{
	pr_debug("%s: Enter.\n", __func__);

	return 0;
}

static int av8100_codec_set_dai_fmt(struct snd_soc_dai *codec_dai,
				unsigned int fmt)
{
	pr_debug("%s: Enter.\n", __func__);

	return 0;
}

struct snd_soc_dai av8100_codec_dai[] = {
	{
		.name = "av8100_0",
		.playback = {
			.stream_name = "av8100_0",
			.channels_min = 2,
			.channels_max = 6,
			.rates = AV8100_SUPPORTED_RATE,
			.formats = AV8100_SUPPORTED_FMT,
		},
		.capture = {
			.stream_name = "av8100_0",
			.channels_min = 2,
			.channels_max = 6,
			.rates = AV8100_SUPPORTED_RATE,
			.formats = AV8100_SUPPORTED_FMT,
		},
		.ops = (struct snd_soc_dai_ops[]) {
			{
			.prepare = av8100_codec_pcm_prepare,
			.hw_params = av8100_codec_pcm_hw_params,
			.shutdown = av8100_codec_pcm_shutdown,
			.set_sysclk = av8100_codec_set_dai_sysclk,
			.set_fmt = av8100_codec_set_dai_fmt,
			}
		},
	}
};
EXPORT_SYMBOL_GPL(av8100_codec_dai);

static unsigned int av8100_codec_read(struct snd_soc_codec *codec,
				unsigned int ctl)
{
	pr_debug("%s: Enter.\n", __func__);

	return 0;
}

static int av8100_codec_write(struct snd_soc_codec *codec,
			unsigned int ctl,
			unsigned int value)
{
	pr_debug("%s: Enter.\n", __func__);

	return 0;
}

static int av8100_codec_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret;

	pr_info("%s: Enter (pdev = %p).\n", __func__, pdev);

	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;
	codec->name = "AV8100";
	codec->owner = THIS_MODULE;
	codec->dai = &av8100_codec_dai[0];
	codec->num_dai = 1;
	codec->read = av8100_codec_read;
	codec->write = av8100_codec_write;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	mutex_init(&codec->mutex);

	socdev->card->codec = codec;

	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		pr_err("%s: Error: to create new PCMs. error %d\n",
			__func__,
			ret);
		goto err;
	}

	return 0;

err:
	kfree(codec);
	return ret;
}

static int av8100_codec_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	pr_debug("%s: Enter (pdev = %p).\n", __func__, pdev);

	if (!codec)
		return 0;

	snd_soc_free_pcms(socdev);
	kfree(socdev->card->codec);

	return 0;
}

static int av8100_codec_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	pr_debug("%s: Enter (pdev = %p).\n", __func__, pdev);

	return 0;
}

static int av8100_codec_resume(struct platform_device *pdev)
{
	pr_debug("%s: Enter (pdev = %p).\n", __func__, pdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_av8100 = {
	.probe = av8100_codec_probe,
	.remove = av8100_codec_remove,
	.suspend = av8100_codec_suspend,
	.resume = av8100_codec_resume
};
EXPORT_SYMBOL_GPL(soc_codec_dev_av8100);

static int __devinit av8100_codec_init(void)
{
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	pr_info("%s: Register codec-dai.\n", __func__);
	ret = snd_soc_register_dai(&av8100_codec_dai[0]);
	if (ret < 0) {
		pr_debug("%s: Error: Failed to register codec-dai (ret = %d).\n",
			__func__,
			ret);
	}

	return ret;
}

static void av8100_codec_exit(void)
{
	pr_debug("%s: Enter.\n", __func__);

	snd_soc_unregister_dai(&av8100_codec_dai[0]);
}

module_init(av8100_codec_init);
module_exit(av8100_codec_exit);

MODULE_DESCRIPTION("AV8100 ASoC codec driver");
MODULE_AUTHOR("www.stericsson.com");
MODULE_LICENSE("GPL");

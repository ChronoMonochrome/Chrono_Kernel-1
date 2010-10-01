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

/* codec private data */
struct av8100_codec_private_data {
	struct hdmi_audio_settings as;
};

static int av8100_codec_powerup(void)
{
	struct av8100_status status;
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	status = av8100_status_get();
	if (status.av8100_state < AV8100_OPMODE_STANDBY) {
		pr_debug("%s: Powering up AV8100.", __func__);
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

	return 0;
}

static int av8100_codec_setup_hdmi_format(void)
{
	union av8100_configuration config;
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	pr_debug("%s: hdmi_mode = AV8100_HDMI_ON.", __func__);
	pr_debug("%s: hdmi_format = AV8100_HDMI.", __func__);
	config.hdmi_format.hdmi_mode = AV8100_HDMI_ON;
	config.hdmi_format.hdmi_format = AV8100_HDMI;
	ret = av8100_conf_prep(AV8100_COMMAND_HDMI, &config);
	if (ret != 0) {
		pr_err("%s: Setting hdmi_format failed "
			"(av8100_conf_prep returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}
	ret = av8100_conf_w(AV8100_COMMAND_HDMI,
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

	return 0;
}

static int av8100_codec_pcm_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	pr_debug("%s: Enter.\n", __func__);

	return 0;
}

static int av8100_codec_send_audio_infoframe(struct hdmi_audio_settings *as)
{
	union av8100_configuration config;
	struct av8100_infoframes_format_cmd info_fr;
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	pr_debug("%s: HDMI-settings:\n", __func__);
	pr_debug("%s:	audio_coding_type = %d\n", __func__, as->audio_coding_type);
	pr_debug("%s:	audio_channel_count = %d\n", __func__, as->audio_channel_count);
	pr_debug("%s:	sampling_frequency = %d\n", __func__, as->sampling_frequency);
	pr_debug("%s:	sample_size = %d\n", __func__, as->sample_size);
	pr_debug("%s:	channel_allocation = %d\n", __func__, as->channel_allocation);
	pr_debug("%s:	level_shift_value = %d\n", __func__, as->level_shift_value);
	pr_debug("%s:	downmix_inhibit = %d\n", __func__, as->downmix_inhibit);

	/* Prepare the infoframe from the hdmi_audio_settings struct */
	pr_info("%s: Preparing audio info-frame.", __func__);
	info_fr.type = 0x84;
	info_fr.version = 0x01;
	info_fr.length = 0x0a;
	info_fr.data[0] = (as->audio_coding_type << 4) | as->audio_channel_count;
	info_fr.data[1] = (as->sampling_frequency << 2) | as->sample_size;
	info_fr.data[2] = 0;
	info_fr.data[3] = as->channel_allocation;
	info_fr.data[4] = ((int)as->downmix_inhibit << 7) |
			(as->level_shift_value << 3);
	info_fr.data[5] = 0;
	info_fr.data[6] = 0;
	info_fr.data[7] = 0;
	info_fr.data[8] = 0;
	info_fr.data[9] = 0;
	info_fr.crc = info_fr.version +
		info_fr.length +
		info_fr.data[0] +
		info_fr.data[1] +
		info_fr.data[3] +
		info_fr.data[4];
	config.infoframes_format.type = info_fr.type;
	config.infoframes_format.version = info_fr.version;
	config.infoframes_format.crc = info_fr.crc;
	config.infoframes_format.length = info_fr.length;
	memcpy(&config.infoframes_format.data, info_fr.data, info_fr.length);

	/* Send audio info-frame */
	pr_info("%s: Sending audio info-frame.", __func__);
	ret = av8100_conf_prep(AV8100_COMMAND_INFOFRAMES, &config);
	if (ret != 0) {
		pr_err("%s: Sending audio info-frame failed "
			"(av8100_conf_prep returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}
	ret = av8100_conf_w(AV8100_COMMAND_INFOFRAMES,
			NULL,
			NULL,
			I2C_INTERFACE);
	if (ret != 0) {
		pr_err("%s: Sending audio info-frame failed "
			"(av8100_conf_w returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}

	return 0;
}

int av8100_codec_change_hdmi_audio_settings(struct snd_pcm_substream *substream,
					struct hdmi_audio_settings *as)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->card->codec;
	struct av8100_codec_private_data *av8100_codec_priv = codec->private_data;

	pr_debug("%s: Enter.\n", __func__);

	av8100_codec_priv->as.audio_coding_type = as->audio_coding_type;
	av8100_codec_priv->as.audio_channel_count = as->audio_channel_count;
	av8100_codec_priv->as.sampling_frequency = as->sampling_frequency;
	av8100_codec_priv->as.sample_size = as->sample_size;
	av8100_codec_priv->as.channel_allocation = as->channel_allocation;
	av8100_codec_priv->as.level_shift_value = as->level_shift_value;
	av8100_codec_priv->as.downmix_inhibit = as->downmix_inhibit;

	return 0;
}

static int av8100_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *hw_params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->card->codec;
	struct av8100_codec_private_data *av8100_codec_priv = codec->private_data;

	pr_debug("%s: Enter.\n", __func__);

	av8100_codec_send_audio_infoframe(&av8100_codec_priv->as);

	return 0;
}

static int av8100_codec_pcm_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	/* Startup AV8100 if it is not already started */
	ret = av8100_codec_powerup();
	if (ret != 0) {
		pr_err("%s: Startup of AV8100 failed "
			"(av8100_codec_powerupAV8100 returned %d)!\n",
			__func__,
			ret);
		return -EINVAL;
	}

	return 0;
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
	union av8100_configuration config;
	int ret;

	pr_debug("%s: Enter.\n", __func__);

	/* Set the HDMI format of AV8100 */
	ret = av8100_codec_setup_hdmi_format();
	if (ret != 0)
		return ret;

	/* Set the audio input format of AV8100 */
	config.audio_input_format.audio_input_if_format	=
		((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_B) ?
		AV8100_AUDIO_TDM_MODE : AV8100_AUDIO_I2SDELAYED_MODE;
	config.audio_input_format.audio_if_mode	=
		((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) ?
		AV8100_AUDIO_MASTER : AV8100_AUDIO_SLAVE;
	pr_info("%s: Setting audio_input_format "
		"(if_format = %d, if_mode = %d).",
		__func__,
		config.audio_input_format.audio_input_if_format,
		config.audio_input_format.audio_if_mode);
	config.audio_input_format.i2s_input_nb = 1;
	config.audio_input_format.sample_audio_freq = AV8100_AUDIO_FREQ_48KHZ;
	config.audio_input_format.audio_word_lg	= AV8100_AUDIO_16BITS;
	config.audio_input_format.audio_format = AV8100_AUDIO_LPCM_MODE;
	config.audio_input_format.audio_mute = AV8100_AUDIO_MUTE_DISABLE;
	ret = av8100_conf_prep(AV8100_COMMAND_AUDIO_INPUT_FORMAT, &config);
	if (ret != 0) {
		pr_err("%s: Setting audio_input_format failed "
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
		pr_err("%s: Setting audio_input_format failed "
			"(av8100_conf_w returned %d)!\n",
			__func__,
			ret);
			return -EINVAL;
	}

	return 0;
}

struct snd_soc_dai av8100_codec_dai[] = {
	{
		.name = "av8100_0",
		.playback = {
			.stream_name = "av8100_0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = AV8100_SUPPORTED_RATE,
			.formats = AV8100_SUPPORTED_FMT,
		},
		.ops = (struct snd_soc_dai_ops[]) {
			{
			.prepare = av8100_codec_pcm_prepare,
			.hw_params = av8100_codec_pcm_hw_params,
			.startup = av8100_codec_pcm_startup,
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
	struct av8100_codec_private_data *av8100_codec_priv;
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

	pr_info("%s: Init codec private data..\n", __func__);
	av8100_codec_priv = kzalloc(sizeof(struct av8100_codec_private_data), GFP_KERNEL);
	if (av8100_codec_priv == NULL)
		return -ENOMEM;

	/* Setup hdmi_audio_settings default values */
	av8100_codec_priv->as.audio_coding_type = AV8100_CODEC_CT_IEC60958_PCM;
	av8100_codec_priv->as.audio_channel_count = AV8100_CODEC_CC_2CH;
	av8100_codec_priv->as.sampling_frequency = AV8100_CODEC_SF_48KHZ;
	av8100_codec_priv->as.sample_size = AV8100_CODEC_SS_16BIT;
	av8100_codec_priv->as.channel_allocation = AV8100_CODEC_CA_FL_FR;
	av8100_codec_priv->as.level_shift_value = AV8100_CODEC_LSV_0DB;
	av8100_codec_priv->as.downmix_inhibit = false;

	codec->private_data = &av8100_codec_priv;
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

MODULE_LICENSE("GPL v2");

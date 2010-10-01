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

#include <linux/io.h>
#include <sound/soc.h>

#include "ux500_pcm.h"
#include "ux500_msp_dai.h"

#include <linux/spi/spi.h>
#include <sound/initval.h>

#include "../codecs/av8100_audio.h"

static struct platform_device *ux500_av8100_platform_device;

static int ux500_av8100_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;

	unsigned int tx_mask, fmt;
	enum hdmi_channel_allocation hdmi_ca;
	enum hdmi_audio_channel_count hdmi_cc;
	struct hdmi_audio_settings as;

	int channels = params_channels(params);
	int ret = 0;

	pr_debug("%s: Enter (TDM-mode, channels = %d, name = %s, number = %d).\n",
		__func__,
		channels,
		substream->name,
		substream->number);

	pr_debug("%s: substream->pcm->name = %s.\n", __func__, substream->pcm->name);
	pr_debug("%s: substream->pcm->id = %s.\n", __func__, substream->pcm->id);

	switch (channels) {
	case 1:
		hdmi_cc = AV8100_CODEC_CC_2CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR; /* Stereo-setup */
		tx_mask = AV8100_CODEC_MASK_MONO;
		break;
	case 2:
		hdmi_cc = AV8100_CODEC_CC_2CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR; /* Stereo */
		tx_mask = AV8100_CODEC_MASK_STEREO;
		break;
	case 3:
		hdmi_cc = AV8100_CODEC_CC_6CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR_LFE_FC_RL_RR; /* 5.1-setup */
		tx_mask = AV8100_CODEC_MASK_2DOT1;
		break;
	case 4:
		hdmi_cc = AV8100_CODEC_CC_6CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR_LFE_FC_RL_RR; /* 5.1-setup */
		tx_mask = AV8100_CODEC_MASK_QUAD;
		break;
	case 5:
		hdmi_cc = AV8100_CODEC_CC_6CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR_LFE_FC_RL_RR; /* 5.1-setup */
		tx_mask = AV8100_CODEC_MASK_5DOT0;
		break;
	case 6:
		hdmi_cc = AV8100_CODEC_CC_6CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR_LFE_FC_RL_RR; /* 5.1 */
		tx_mask = AV8100_CODEC_MASK_5DOT1;
		break;
	case 8:
		hdmi_cc = AV8100_CODEC_CC_6CH;
		hdmi_ca = AV8100_CODEC_CA_FL_FR_LFE_FC_RL_RR_FLC_FRC; /* 7.1 */
		tx_mask = AV8100_CODEC_MASK_7DOT1;
		break;
	default:
		pr_err("%s: Unsupported number of channels (channels = %d)!\n",
			__func__,
			channels);
		return -EINVAL;
	}

	/* Change HDMI audio-settings for codec-DAI. */
	pr_debug("%s: Change HDMI audio-settings for codec-DAI.\n", __func__);
	as.audio_coding_type = AV8100_CODEC_CT_IEC60958_PCM;
	as.audio_channel_count = hdmi_cc;
	as.sampling_frequency = AV8100_CODEC_SF_48KHZ;
	as.sample_size = AV8100_CODEC_SS_16BIT;
	as.channel_allocation = hdmi_ca;
	as.level_shift_value = AV8100_CODEC_LSV_0DB;
	as.downmix_inhibit = false;
	ret = av8100_codec_change_hdmi_audio_settings(substream, &as);
	if (ret < 0) {
		pr_err("%s: Unable to change HDMI audio-settings for codec-DAI "
			"(av8100_codec_change_hdmi_audio_settings returned %d)!\n",
			__func__,
			ret);
		return ret;
	}

	/* Set format for codec-DAI */
	fmt = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_CBM_CFM;
	pr_debug("%s: Setting format for codec-DAI (fmt = %d).\n",
		__func__,
		fmt);
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("%s: Unable to set format for codec-DAI "
			"(snd_soc_dai_set_tdm_slot returned %d)!\n",
			__func__,
			ret);
		return ret;
	}

	/* Set TDM-slot for CPU-DAI */
	pr_debug("%s: Setting TDM-slot for codec-DAI (tx_mask = %d).\n",
		__func__,
		tx_mask);
	ret = snd_soc_dai_set_tdm_slot(cpu_dai, tx_mask, 0, 16, 16);
	if (ret < 0) {
		pr_err("%s: Unable to set TDM-slot for codec-DAI "
			"(snd_soc_dai_set_tdm_slot returned %d)!\n",
			__func__,
			ret);
		return ret;
	}

	/* Set format for CPU-DAI */
	fmt = SND_SOC_DAIFMT_DSP_B |
		SND_SOC_DAIFMT_CBM_CFM |
		SND_SOC_DAIFMT_NB_IF;
	pr_debug("%s: Setting DAI-format for Ux500-platform (fmt = %d).\n",
		__func__,
		fmt);
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret < 0) {
		pr_err("%s: Unable to set DAI-format for Ux500-platform "
			"(snd_soc_dai_set_fmt returned %d).\n",
			__func__,
			ret);
		return ret;
	}

	return ret;
}



struct snd_soc_dai_link ux500_av8100_dai_links[] = {
	{
	.name = "hdmi",
	.stream_name = "hdmi",
	.cpu_dai = &ux500_msp_dai[2],
	.codec_dai = &av8100_codec_dai[0],
	.init = NULL,
	.ops = (struct snd_soc_ops[]) {
		{
		.hw_params = ux500_av8100_hw_params,
		}
	}
	},
};

static struct snd_soc_card ux500_av8100 = {
	.name = "hdmi",
	.probe = NULL,
	.dai_link = ux500_av8100_dai_links,
	.num_links = ARRAY_SIZE(ux500_av8100_dai_links),
	.platform = &ux500_soc_platform,
};

struct snd_soc_device ux500_av8100_drvdata = {
	.card = &ux500_av8100,
	.codec_dev = &soc_codec_dev_av8100,
};

static int __init ux500_av8100_soc_init(void)
{
	int ret = 0;

	pr_debug("%s: Enter.\n", __func__);

	pr_info("%s: Card name: %s\n",
		__func__,
		ux500_av8100_drvdata.card->name);

	pr_debug("%s: DAI-link 0, name: %s\n",
		__func__,
		ux500_av8100_drvdata.card->dai_link[0].name);
	pr_debug("%s: DAI-link 0, stream_name: %s\n",
		__func__,
		ux500_av8100_drvdata.card->dai_link[0].stream_name);

	pr_debug("%s: Allocate platform device (%s).\n",
		__func__,
		ux500_av8100_drvdata.card->name);
	ux500_av8100_platform_device = platform_device_alloc("soc-audio", -1);
	if (!ux500_av8100_platform_device)
		return -ENOMEM;

	pr_debug("%s: Set platform drvdata (%s).\n",
		__func__,
		ux500_av8100_drvdata.card->name);
	platform_set_drvdata(
		ux500_av8100_platform_device,
		&ux500_av8100_drvdata);
	ux500_av8100_drvdata.dev = &ux500_av8100_platform_device->dev;

	pr_debug("%s: Add platform device (%s).\n",
		__func__,
		ux500_av8100_drvdata.card->name);
	ret = platform_device_add(ux500_av8100_platform_device);
	if (ret) {
		pr_err("%s: Error: Failed to add platform device (%s).\n",
			__func__,
			ux500_av8100_drvdata.card->name);
		platform_device_put(ux500_av8100_platform_device);
	}

	return ret;
}

static void __exit ux500_av8100_soc_exit(void)
{
	pr_debug("%s: Enter.\n", __func__);

	pr_debug("%s: Unregister platform device (%s).\n",
		__func__,
		ux500_av8100_drvdata.card->name);
	platform_device_unregister(ux500_av8100_platform_device);
}

module_init(ux500_av8100_soc_init);
module_exit(ux500_av8100_soc_exit);

MODULE_LICENSE("GPL v2");

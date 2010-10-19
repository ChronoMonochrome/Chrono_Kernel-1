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

static int ux500_av8100_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	pr_debug("%s: Enter.\n", __func__);

	pr_debug("%s: substream->pcm->name = %s.\n", __func__, substream->pcm->name);
	pr_debug("%s: substream->pcm->id = %s.\n", __func__, substream->pcm->id);
	pr_debug("%s: substream->name = %s.\n", __func__, substream->name);
	pr_debug("%s: substream->number = %d.\n", __func__, substream->number);

	if (cpu_dai->ops->set_fmt) {
		dev_dbg(&ux500_av8100_platform_device->dev,
			"%s: Setting format on codec_dai: "
			"SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM.",
			__func__);
		ret = snd_soc_dai_set_fmt(
			codec_dai,
			SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM);
		if (ret < 0) {
			dev_dbg(&ux500_av8100_platform_device->dev,
				"%s: snd_soc_dai_set_fmt failed with %d.\n",
				__func__,
				ret);
			return ret;
		}

		dev_dbg(&ux500_av8100_platform_device->dev,
			"%s: Setting format on cpu_dai: "
			"SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM.",
			__func__);
		ret = snd_soc_dai_set_fmt(
			cpu_dai,
			SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM);
		if (ret < 0) {
			dev_dbg(&ux500_av8100_platform_device->dev,
				"%s: snd_soc_dai_set_fmt failed with %d.\n",
				__func__,
				ret);
			return ret;
		}
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

MODULE_LICENSE("GPL");

/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ola Lilja ola.o.lilja@stericsson.com,
 *         Roger Nilsson roger.xr.nilsson@stericsson.com
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
#include <linux/device.h>
#include <linux/io.h>
#include <sound/soc.h>

#include "ux500_pcm.h"
#include "ux500_msp_dai.h"
#include "mach/hardware.h"
#include "../codecs/ab3550.h"

static struct platform_device *ux500_ab3550_platform_device;

#define AB3550_DAI_FMT_I2S_M (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM)
#define AB3550_DAI_FMT_I2S_S (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS)
#define AB3550_DAI_FMT AB3550_DAI_FMT_I2S_S

static int ux500_ab3550_startup(struct snd_pcm_substream *substream)
{
	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Enter\n",
		__func__);
	return 0;
}

static void ux500_ab3550_shutdown(struct snd_pcm_substream *substream)
{
	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Enter\n",
		__func__);
}

static int ux500_ab3550_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ifid, ret = 0;

	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Enter\n",
		__func__);

	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: substream->pcm->name = %s\n"
		"substream->pcm->id = %s.\n"
		"substream->name = %s.\n"
		"substream->number = %d.\n",
		__func__,
		substream->pcm->name,
		substream->pcm->id,
		substream->name,
		substream->number);

	for (ifid = 0; ifid < ARRAY_SIZE(ab3550_codec_dai); ifid++) {
		if (strcmp(codec_dai->name, ab3550_codec_dai[ifid].name) == 0)
			break;
	}

	if (codec_dai->ops->set_fmt) {
		ret = snd_soc_dai_set_fmt(codec_dai, AB3550_DAI_FMT);
		if (ret < 0) {
			dev_dbg(&ux500_ab3550_platform_device->dev,
				"%s: snd_soc_dai_set_fmt failed with %d.\n",
				__func__,
				ret);
			return ret;
		}

		ret = snd_soc_dai_set_fmt(cpu_dai, AB3550_DAI_FMT);

		if (ret < 0) {
			dev_dbg(&ux500_ab3550_platform_device->dev,
				"%s: snd_soc_dai_set_fmt"
				" failed with %d.\n", __func__, ret);
			return ret;
		}
	}

	return ret;
}

static struct snd_soc_ops ux500_ab3550_ops = {
	.startup = ux500_ab3550_startup,
	.shutdown = ux500_ab3550_shutdown,
	.hw_params = ux500_ab3550_hw_params,
};

struct snd_soc_dai_link ux500_ab3550_dai_links[] = {
	{
		.name = "ab3550_0",
		.stream_name = "ab3550_0",
		.cpu_dai = &ux500_msp_dai[0],
		.codec_dai = &ab3550_codec_dai[0],
		.init = NULL,
		.ops = &ux500_ab3550_ops,
	},
	{
		.name = "ab3550_1",
		.stream_name = "ab3550_1",
		.cpu_dai = &ux500_msp_dai[1],
		.codec_dai = &ab3550_codec_dai[1],
		.init = NULL,
		.ops = &ux500_ab3550_ops,
	},
};

static struct snd_soc_card ux500_ab3550 = {
	.name = "ab3550",
	.probe = NULL,
	.dai_link = ux500_ab3550_dai_links,
	.num_links = ARRAY_SIZE(ux500_ab3550_dai_links),
	.platform = &ux500_soc_platform,
};

struct snd_soc_device ux500_ab3550_drvdata = {
	.card = &ux500_ab3550,
	.codec_dev = &soc_codec_dev_ab3550,
};

static int __init mop500_ab3550_soc_init(void)
{
	int i;
	int ret = 0;

	pr_debug("%s: Enter\n",
		__func__);
	pr_debug("%s: Card name: %s\n",
		__func__,
		ux500_ab3550_drvdata.card->name);

	for (i = 0; i < ARRAY_SIZE(ux500_ab3550_dai_links); i++) {
		pr_debug("%s: DAI-link %d, name: %s\n",
			__func__,
			i,
			ux500_ab3550_drvdata.card->dai_link[i].name);
		pr_debug("%s: DAI-link %d, stream_name: %s\n",
			__func__,
			i,
			ux500_ab3550_drvdata.card->dai_link[i].stream_name);
	}

	pr_debug("%s: Allocate platform device (%s)\n",
		__func__,
		ux500_ab3550_drvdata.card->name);
	ux500_ab3550_platform_device = platform_device_alloc("soc-audio", -1);
	if (!ux500_ab3550_platform_device)
		return -ENOMEM;

	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Set platform drvdata (%s)\n",
		__func__,
		ux500_ab3550_drvdata.card->name);
	platform_set_drvdata(
		ux500_ab3550_platform_device,
		&ux500_ab3550_drvdata);

	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Add platform device (%s)\n",
		__func__,
		ux500_ab3550_drvdata.card->name);
	ux500_ab3550_drvdata.dev = &ux500_ab3550_platform_device->dev;

	ret = platform_device_add(ux500_ab3550_platform_device);
	if (ret) {
		dev_dbg(&ux500_ab3550_platform_device->dev,
			"%s: Error: Failed to add platform device (%s)\n",
			__func__,
			ux500_ab3550_drvdata.card->name);
		platform_device_put(ux500_ab3550_platform_device);
	}

	return ret;
}
module_init(mop500_ab3550_soc_init);

static void __exit mop500_ab3550_soc_exit(void)
{
	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Enter.\n",
		__func__);

	dev_dbg(&ux500_ab3550_platform_device->dev,
		"%s: Un-register platform device (%s)\n",
		__func__,
		ux500_ab3550_drvdata.card->name);
	platform_device_unregister(ux500_ab3550_platform_device);
}
module_exit(mop500_ab3550_soc_exit);

MODULE_LICENSE("GPL");

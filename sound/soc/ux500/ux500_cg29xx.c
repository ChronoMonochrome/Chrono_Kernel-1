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
#include <linux/device.h>
#include <linux/io.h>
#include <sound/soc.h>

#include "ux500_pcm.h"
#include "ux500_msp_dai.h"
#include "../codecs/cg29xx.h"

#define UX500_CG29XX_DAI_SLOT_WIDTH	16
#define UX500_CG29XX_DAI_SLOTS	2
#define UX500_CG29XX_DAI_ACTIVE_SLOTS	0x01

static struct platform_device *ux500_cg29xx_platform_device;

static int ux500_cg29xx_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;

	int err;

	pr_debug("%s: substream->pcm->name = %s.\n"
		"substream->pcm->id = %s.\n"
		"substream->name = %s.\n"
		"substream->number = %d.\n",
		__func__,
		substream->pcm->name,
		substream->pcm->id,
		substream->name,
		substream->number);

	err = snd_soc_dai_set_fmt(
			codec_dai,
			SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS);

	if (err) {
		pr_err("%s: snd_soc_dai_set_fmt(codec)"
				" failed with %d.\n",
				__func__,
			err);
		goto out_err;
	}

	err = snd_soc_dai_set_tdm_slot(
		codec_dai,
		1 << CG29XX_DAI_SLOT0_SHIFT,
		1 << CG29XX_DAI_SLOT0_SHIFT,
		UX500_CG29XX_DAI_SLOTS,
		UX500_CG29XX_DAI_SLOT_WIDTH);

	if (err) {
		pr_err("%s: cg29xx_set_tdm_slot(codec)"
				" failed with %d.\n",
				__func__,
				err);
		goto out_err;
	}

	err = snd_soc_dai_set_fmt(
			cpu_dai,
			SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS |
			SND_SOC_DAIFMT_NB_NF);

	if (err) {
		pr_err("%s: snd_soc_dai_set_fmt(dai)"
				" failed with %d.\n",
				__func__,
			err);
		goto out_err;
	}

	err = snd_soc_dai_set_tdm_slot(cpu_dai,
		UX500_CG29XX_DAI_ACTIVE_SLOTS,
		UX500_CG29XX_DAI_ACTIVE_SLOTS,
		UX500_CG29XX_DAI_SLOTS,
		UX500_CG29XX_DAI_SLOT_WIDTH);

	if (err) {
		pr_err("%s: cg29xx_set_tdm_slot(dai)"
				" failed with %d.\n",
				__func__,
				err);
		goto out_err;
	}

out_err:
	return err;
}

static struct snd_soc_ops ux500_cg29xx_ops = {
	.hw_params = ux500_cg29xx_hw_params,
};

struct snd_soc_dai_link ux500_cg29xx_dai_links[] = {
	{
		.name = "cg29xx_0",
		.stream_name = "cg29xx_0",
		.cpu_dai = &ux500_msp_dai[0],
		.codec_dai = &cg29xx_codec_dai[1],
		.init = NULL,
		.ops = &ux500_cg29xx_ops,
	},
};

static struct snd_soc_card ux500_cg29xx = {
	.name = "cg29xx",
	.probe = NULL,
	.dai_link = ux500_cg29xx_dai_links,
	.num_links = ARRAY_SIZE(ux500_cg29xx_dai_links),
	.platform = &ux500_soc_platform,
};

struct snd_soc_device ux500_cg29xx_drvdata = {
	.card = &ux500_cg29xx,
	.codec_dev = &soc_codec_dev_cg29xx,
};

static int __init ux500_cg29xx_soc_init(void)
{
	int err;
	int i;

	for (i = 0; i < ARRAY_SIZE(ux500_cg29xx_dai_links); i++) {
		pr_debug("%s: DAI-link %d, name: %s\n",
			__func__,
			i,
			ux500_cg29xx_drvdata.card->dai_link[i].name);
	}

	ux500_cg29xx_platform_device =
		platform_device_alloc("soc-audio", -1);
	if (!ux500_cg29xx_platform_device)
		return -ENOMEM;

	platform_set_drvdata(
		ux500_cg29xx_platform_device,
		&ux500_cg29xx_drvdata);

	ux500_cg29xx_drvdata.dev = &ux500_cg29xx_platform_device->dev;

	err = platform_device_add(ux500_cg29xx_platform_device);
	if (err) {
		pr_err("%s: Error: Failed to add platform device (%s).\n",
			__func__,
			ux500_cg29xx_drvdata.card->name);
		platform_device_put(ux500_cg29xx_platform_device);
	}

	return err;
}
module_init(ux500_cg29xx_soc_init);

static void __exit ux500_cg29xx_soc_exit(void)
{
	platform_device_unregister(ux500_cg29xx_platform_device);
}
module_exit(ux500_cg29xx_soc_exit);

MODULE_LICENSE("GPL v2");

/*
 * Copyright (C) ST-Ericsson SA 2011
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

#include <sound/soc.h>
#include "../codecs/ab5500.h"
int ux500_ab5500_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

void ux500_ab5500_shutdown(struct snd_pcm_substream *substream)
{
	printk(KERN_DEBUG "%s: Enter.\n", __func__);
}

int ux500_ab5500_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	int channels = params_channels(params);

	printk(KERN_DEBUG "%s: Enter.\n", __func__);
	printk(KERN_DEBUG "%s: substream->pcm->name = %s.\n", __func__, substream->pcm->name);
	printk(KERN_DEBUG "%s: substream->pcm->id = %s.\n", __func__, substream->pcm->id);
	printk(KERN_DEBUG "%s: substream->name = %s.\n", __func__, substream->name);
	printk(KERN_DEBUG "%s: substream->number = %d.\n", __func__, substream->number);
	printk(KERN_DEBUG "%s: channels = %d.\n", __func__, channels);
	printk(KERN_DEBUG "%s: DAI-index (Codec): %d\n", __func__, codec_dai->id);
	printk(KERN_DEBUG "%s: DAI-index (Platform): %d\n", __func__, cpu_dai->id);

	ret = snd_soc_dai_set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S |  SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	return ret;
}

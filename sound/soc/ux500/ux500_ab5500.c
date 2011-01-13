/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ola Lilja <ola.o.lilja@stericsson.com>,
 *         Roger Nilsson <roger.xr.nilsson@stericsson.com>
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
	printk(KERN_DEBUG "%s: Enter.\n", __func__);

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

	ret = snd_soc_dai_set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_DEBUG "%s: snd_soc_dai_set_fmt failed with %d.\n",
			__func__,
			ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_DEBUG "%s: snd_soc_dai_set_fmt failed with %d.\n",
			__func__,
			ret);
		return ret;
	}

	return ret;
}

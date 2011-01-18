/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Mikko J. Lehto <mikko.lehto@symbio.com>,
 *         Mikko Sarmanne <mikko.sarmanne@symbio.com>,
 *         Jarmo K. Kuronen <jarmo.kuronen@symbio.com>
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
#include <linux/regulator/consumer.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <mach/hardware.h>
#include "ux500_pcm.h"
#include "ux500_msp_dai.h"
#include "../codecs/ab8500.h"
#include "ux500_ab8500_accessory.h"

#define AB8500_DAIFMT_TDM_MASTER \
		(SND_SOC_DAIFMT_DSP_B | \
		SND_SOC_DAIFMT_CBM_CFM | \
		SND_SOC_DAIFMT_NB_NF | \
		SND_SOC_DAIFMT_CONT)

#define TX_SLOT_MONO	0x0008
#define TX_SLOT_STEREO	0x000a
#define RX_SLOT_MONO	0x0001
#define RX_SLOT_STEREO	0x0003

#define DEF_TX_SLOTS	TX_SLOT_STEREO
#define DEF_RX_SLOTS	RX_SLOT_MONO

/* Slot configuration */
static unsigned int tx_slots = DEF_TX_SLOTS;
static unsigned int rx_slots = DEF_RX_SLOTS;

/* List the regulators that are to be controlled.. */
static struct regulator_bulk_data ab8500_regus[5] = {
	{	.supply = "v-dmic"	},
	{	.supply = "v-audio"	},
	{	.supply = "v-amic1"	},
	{	.supply = "v-amic2"	},
	{	.supply = "vcc-N2158"	}
};

static int create_regulators(struct device *dev)
{
	int i, status = 0;

	pr_debug("%s: Enter.\n", __func__);

	for (i = 0; i < ARRAY_SIZE(ab8500_regus); ++i)
		ab8500_regus[i].consumer = NULL;

	for (i = 0; i < ARRAY_SIZE(ab8500_regus); ++i) {
		ab8500_regus[i].consumer = regulator_get(
			dev, ab8500_regus[i].supply);
		if (IS_ERR(ab8500_regus[i].consumer)) {
			status = PTR_ERR(ab8500_regus[i].consumer);
			pr_err("%s: Failed to get supply '%s' (%d)\n",
				__func__, ab8500_regus[i].supply, status);
			ab8500_regus[i].consumer = NULL;
			goto err_get;
		}
	}

	return 0;

err_get:

	for (i = 0; i < ARRAY_SIZE(ab8500_regus); ++i) {
		if (ab8500_regus[i].consumer) {
			regulator_put(ab8500_regus[i].consumer);
			ab8500_regus[i].consumer = NULL;
		}
	}

	return status;
}

int enable_regulator(const char *name)
{
	int i, status;

	for (i = 0; i < ARRAY_SIZE(ab8500_regus); ++i) {
		if (strcmp(name, ab8500_regus[i].supply) != 0)
			continue;

		status = regulator_enable(ab8500_regus[i].consumer);

		if (status != 0) {
			pr_err("%s: Failure with regulator %s (%d)\n",
				__func__, name, status);
			return status;
		} else {
			pr_debug("%s: Enabled regulator %s.\n",
				__func__, name);
			return 0;
		}
	}

	return -EINVAL;
}

void disable_regulator(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ab8500_regus); ++i) {
		if (strcmp(name, ab8500_regus[i].supply) == 0) {
			regulator_disable(ab8500_regus[i].consumer);
			pr_debug("%s: Disabled regulator %s.\n",
				__func__, name);
			return;
		}
	}
}

int ux500_ab8500_startup(struct snd_pcm_substream *substream)
{
	pr_info("%s: Enter\n", __func__);

	return 0;
}

void ux500_ab8500_shutdown(struct snd_pcm_substream *substream)
{
	pr_info("%s: Enter\n", __func__);

	/* Reset slots configuration to default(s) */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		tx_slots = DEF_TX_SLOTS;
	else
		rx_slots = DEF_RX_SLOTS;
}

int ux500_ab8500_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int channels, ret = 0;

	pr_info("%s: Enter\n", __func__);

	pr_info("%s: substream->pcm->name = %s\n"
		"substream->pcm->id = %s.\n"
		"substream->name = %s.\n"
		"substream->number = %d.\n",
		__func__,
		substream->pcm->name,
		substream->pcm->id,
		substream->name,
		substream->number);

	ret = snd_soc_dai_set_fmt(codec_dai, AB8500_DAIFMT_TDM_MASTER);
	if (ret < 0) {
		pr_err("%s: snd_soc_dai_set_fmt failed codec_dai %d.\n",
			__func__, ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, AB8500_DAIFMT_TDM_MASTER);
	if (ret < 0) {
		pr_err("%s: snd_soc_dai_set_fmt cpu_dai %d.\n",
				__func__, ret);
		return ret;
	}

	channels = params_channels(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (channels == 1)
			tx_slots = TX_SLOT_MONO;
		else if (channels == 2)
			tx_slots = TX_SLOT_STEREO;
		else
			return -EINVAL;
	} else {
		if (channels == 1)
			rx_slots = RX_SLOT_MONO;
		else if (channels == 2)
			rx_slots = RX_SLOT_STEREO;
		else
			return -EINVAL;
	}

	pr_info("%s: CPU-DAI TDM: TX=0x%04X RX=0x%04x\n",
		__func__, tx_slots, rx_slots);
	ret = snd_soc_dai_set_tdm_slot(cpu_dai,
					tx_slots, rx_slots,
					16, 16);

	pr_info("%s: CODEC-DAI TDM: TX=0x%04X RX=0x%04x\n",
		__func__, tx_slots, rx_slots);
	ret += snd_soc_dai_set_tdm_slot(codec_dai,
					tx_slots, rx_slots,
					16, 16);

	return ret;
}

static int regulator_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		enable_regulator(w->name);
	else
		disable_regulator(w->name);
	return 0;
}

static const struct snd_soc_dapm_widget dapm_widgets[] = {
	SND_SOC_DAPM_MIC("v-dmic", regulator_event),
	SND_SOC_DAPM_MIC("v-amic1", regulator_event),
	SND_SOC_DAPM_MIC("v-amic2", regulator_event),
	SND_SOC_DAPM_MIC("vcc-N2158", regulator_event),
};

static const struct snd_soc_dapm_route dapm_routes[] = {
	{"MIC1A", NULL, "v-amic1"},

	{"MIC1B", NULL, "vcc-N2158"},
	{"vcc-N2158", NULL, "v-amic1"},

	{"MIC2", NULL, "v-amic2"},

	{"DMIC1", NULL, "v-dmic"},
	{"DMIC2", NULL, "v-dmic"},
	{"DMIC3", NULL, "v-dmic"},
	{"DMIC4", NULL, "v-dmic"},
	{"DMIC5", NULL, "v-dmic"},
	{"DMIC6", NULL, "v-dmic"},
};

int ux500_ab8500_machine_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int status = 0;

	pr_info("%s: Enter.\n", __func__);

	status = create_regulators(codec->dev);
	if (status < 0) {
		pr_err("%s: Failed to instantiate regulators (%d).\n",
			__func__, status);
		return status;
	}

	/*
	status = ab8500_accessory_init(rtd->codec);
	if (status < 0) {
		pr_err("%s: Failed to initialize accessories (%d).\n",
				__func__, status);
		return status;
	}
	*/

	snd_soc_dapm_new_controls(codec, dapm_widgets,
			ARRAY_SIZE(dapm_widgets));
	snd_soc_dapm_add_routes(codec, dapm_routes,
			ARRAY_SIZE(dapm_routes));
	snd_soc_dapm_sync(codec);

	return status;
}

void ux500_ab8500_soc_machine_drv_cleanup(void)
{
	pr_info("%s: Enter.\n", __func__);

	/*
	ab8500_accessory_cleanup();
	*/

	regulator_bulk_free(ARRAY_SIZE(ab8500_regus), ab8500_regus);
}

struct snd_soc_ops ux500_ab8500_ops[] = {
	{
	.hw_params = ux500_ab8500_hw_params,
	.startup = ux500_ab8500_startup,
	.shutdown = ux500_ab8500_shutdown,
	}
};

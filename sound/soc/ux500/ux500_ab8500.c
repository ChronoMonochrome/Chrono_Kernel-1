/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Mikko J. Lehto <mikko.lehto@symbio.com>,
 *         Mikko Sarmanne <mikko.sarmanne@symbio.com>,
 *         Jarmo K. Kuronen <jarmo.kuronen@symbio.com>.
 *         Ola Lilja <ola.o.lilja@stericsson.com>
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
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <mach/hardware.h>
#include "ux500_pcm.h"
#include "ux500_msp_dai.h"
#include "../codecs/ab8500_audio.h"

#define TX_SLOT_MONO	0x0008
#define TX_SLOT_STEREO	0x000a
#define RX_SLOT_MONO	0x0001
#define RX_SLOT_STEREO	0x0003
#define TX_SLOT_8CH	0x00FF
#define RX_SLOT_8CH	0x00FF

#define DEF_TX_SLOTS	TX_SLOT_STEREO
#define DEF_RX_SLOTS	RX_SLOT_MONO

#define DRIVERMODE_NORMAL	0
#define DRIVERMODE_CODEC_ONLY	1

static struct snd_soc_jack jack;

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

int ux500_ab8500_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int fmt, fmt_if1;
	int channels, ret = 0, slots, slot_width, driver_mode;
	bool streamIsPlayback;

	pr_debug("%s: Enter\n", __func__);

	pr_debug("%s: substream->pcm->name = %s\n"
		"substream->pcm->id = %s.\n"
		"substream->name = %s.\n"
		"substream->number = %d.\n",
		__func__,
		substream->pcm->name,
		substream->pcm->id,
		substream->name,
		substream->number);

	channels = params_channels(params);

	/* Setup codec depending on driver-mode */
	driver_mode = (channels == 8) ?
		DRIVERMODE_CODEC_ONLY : DRIVERMODE_NORMAL;
	pr_debug("%s: Driver-mode: %s.\n",
		__func__,
		(driver_mode == DRIVERMODE_NORMAL) ? "NORMAL" : "CODEC_ONLY");
	if (driver_mode == DRIVERMODE_NORMAL) {
		ab8500_audio_set_bit_delay(codec_dai, 0);
		ab8500_audio_set_word_length(codec_dai, 16);
		fmt = SND_SOC_DAIFMT_DSP_B |
			SND_SOC_DAIFMT_CBM_CFM |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CONT;
	} else {
		ab8500_audio_set_bit_delay(codec_dai, 1);
		ab8500_audio_set_word_length(codec_dai, 20);
		fmt = SND_SOC_DAIFMT_DSP_B |
			SND_SOC_DAIFMT_CBM_CFM |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_GATED;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("%s: snd_soc_dai_set_fmt failed for codec_dai (ret = %d).\n",
			__func__,
			ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret < 0) {
		pr_err("%s: snd_soc_dai_set_fmt for cpu_dai (ret = %d).\n",
			__func__,
			ret);
		return ret;
	}

	/* Setup TDM-slots */

	streamIsPlayback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
	switch (channels) {
	case 1:
		slots = 16;
		slot_width = 16;
		tx_slots = (streamIsPlayback) ? TX_SLOT_MONO : 0;
		rx_slots = (streamIsPlayback) ? 0 : RX_SLOT_MONO;
		break;
	case 2:
		slots = 16;
		slot_width = 16;
		tx_slots = (streamIsPlayback) ? TX_SLOT_STEREO : 0;
		rx_slots = (streamIsPlayback) ? 0 : RX_SLOT_STEREO;
		break;
	case 8:
		slots = 16;
		slot_width = 16;
		tx_slots = (streamIsPlayback) ? TX_SLOT_8CH : 0;
		rx_slots = (streamIsPlayback) ? 0 : RX_SLOT_8CH;
		break;
	default:
		return -EINVAL;
	}

	pr_debug("%s: CPU-DAI TDM: TX=0x%04X RX=0x%04x\n",
		__func__, tx_slots, rx_slots);
	ret = snd_soc_dai_set_tdm_slot(cpu_dai, tx_slots, rx_slots, slots, slot_width);
	if (ret)
		return ret;

	pr_debug("%s: CODEC-DAI TDM: TX=0x%04X RX=0x%04x\n",
		__func__, tx_slots, rx_slots);
	ret = snd_soc_dai_set_tdm_slot(codec_dai, tx_slots, rx_slots, slots, slot_width);
	if (ret)
		return ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: Setup IF1 for FM-radio.\n", __func__);
		fmt_if1 = SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_I2S;
		ret = ab8500_audio_setup_if1(codec_dai->codec, fmt_if1, 16, 1);
		if (ret)
			return ret;
	}

	return 0;
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

static int create_jack(struct snd_soc_codec *codec)
{
	return snd_soc_jack_new(codec,
	"AB8500 Hs Status",
	SND_JACK_HEADPHONE     |
	SND_JACK_MICROPHONE    |
	SND_JACK_HEADSET       |
	SND_JACK_LINEOUT       |
	SND_JACK_MECHANICAL    |
	SND_JACK_VIDEOOUT,
	&jack);
}

void ux500_ab8500_jack_report(int value)
{
	if (jack.jack)
		snd_soc_jack_report(&jack, value, 0xFF);
}
EXPORT_SYMBOL_GPL(ux500_ab8500_jack_report);

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

	status = create_jack(codec);
	if (status < 0) {
		pr_err("%s: Failed to create Jack (%d).\n", __func__, status);
		return status;
	}

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


	regulator_bulk_free(ARRAY_SIZE(ab8500_regus), ab8500_regus);
}

struct snd_soc_ops ux500_ab8500_ops[] = {
	{
	.hw_params = ux500_ab8500_hw_params,
	.startup = ux500_ab8500_startup,
	.shutdown = ux500_ab8500_shutdown,
	}
};

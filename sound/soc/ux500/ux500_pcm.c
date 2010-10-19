/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ola Lilja (ola.o.lilja@stericsson.com),
 *         Roger Nilsson (roger.xr.nilsson@stericsson.com)
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <asm/page.h>

#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "ux500_pcm.h"
#include "ux500_msp_dai.h"

static struct snd_pcm_hardware ux500_pcm_hw_playback = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_PAUSE),
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE |
			SNDRV_PCM_FMTBIT_S16_BE | SNDRV_PCM_FMTBIT_U16_BE,
	.rates = SNDRV_PCM_RATE_KNOT,
	.rate_min = UX500_PLATFORM_MIN_RATE_PLAYBACK,
	.rate_max = UX500_PLATFORM_MAX_RATE_PLAYBACK,
	.channels_min = UX500_PLATFORM_MIN_CHANNELS,
	.channels_max = UX500_PLATFORM_MAX_CHANNELS,
	.buffer_bytes_max = UX500_PLATFORM_BUFFER_SIZE,
	.period_bytes_min = UX500_PLATFORM_MIN_PERIOD_BYTES,
	.period_bytes_max = PAGE_SIZE,
	.periods_min = UX500_PLATFORM_BUFFER_SIZE / PAGE_SIZE,
	.periods_max = UX500_PLATFORM_BUFFER_SIZE /
			UX500_PLATFORM_MIN_PERIOD_BYTES
};

static struct snd_pcm_hardware ux500_pcm_hw_capture = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_PAUSE),
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE |
			SNDRV_PCM_FMTBIT_S16_BE | SNDRV_PCM_FMTBIT_U16_BE,
	.rates = SNDRV_PCM_RATE_KNOT,
	.rate_min = UX500_PLATFORM_MIN_RATE_CAPTURE,
	.rate_max = UX500_PLATFORM_MAX_RATE_CAPTURE,
	.channels_min = UX500_PLATFORM_MIN_CHANNELS,
	.channels_max = UX500_PLATFORM_MAX_CHANNELS,
	.buffer_bytes_max = UX500_PLATFORM_BUFFER_SIZE,
	.period_bytes_min = UX500_PLATFORM_MIN_PERIOD_BYTES,
	.period_bytes_max = PAGE_SIZE,
	.periods_min = UX500_PLATFORM_BUFFER_SIZE / PAGE_SIZE,
	.periods_max = UX500_PLATFORM_BUFFER_SIZE /
			UX500_PLATFORM_MIN_PERIOD_BYTES
};

static void ux500_pcm_dma_enqueue(struct snd_pcm_substream *substream)
{
	unsigned int dma_size;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ux500_pcm_private *private = substream->runtime->private_data;

	pr_debug("%s: Enter MSP Index: %d.\n",
			__func__,
			private->msp_id);

	dma_size = frames_to_bytes(runtime, runtime->period_size);
	if (substream->pstr->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ux500_msp_dai_i2s_send_data(
			(void *)(runtime->dma_addr + private->offset),
			dma_size,
			private->msp_id);
	} else{
		ux500_msp_dai_i2s_receive_data(
			(void *)(runtime->dma_addr + private->offset),
			dma_size,
			private->msp_id);
	}

	private->period++;
	private->period %= runtime->periods;
	private->offset =
		frames_to_bytes(runtime, runtime->period_size) *
		private->period;
}

static void ux500_pcm_dma_hw_free(struct device *dev,
		struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *buf = runtime->dma_buffer_p;

	if (runtime->dma_area == NULL)
		return;

	if (buf != &substream->dma_buffer) {
		dma_free_coherent(
			buf->dev.dev,
			buf->bytes,
			buf->area,
			buf->addr);
		kfree(runtime->dma_buffer_p);
	}

	snd_pcm_set_runtime_buffer(substream, NULL);
}

void ux500_pcm_dma_eot_handler(void *data)
{
	struct snd_pcm_substream *substream = data;
	struct snd_pcm_runtime *runtime;
	struct ux500_pcm_private *private;

	pr_debug("%s: Enter\n", __func__);

	if (substream) {
		runtime = substream->runtime;
		private = substream->runtime->private_data;

		snd_pcm_period_elapsed(substream);

		if (runtime->status->state == SNDRV_PCM_STATE_RUNNING)
			ux500_pcm_dma_enqueue(substream);
	}
}
EXPORT_SYMBOL(ux500_pcm_dma_eot_handler);

static int ux500_pcm_open(struct snd_pcm_substream *substream)
{
	int stream_id = substream->pstr->stream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ux500_pcm_private *private;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	pr_info("%s: Enter\n", __func__);

	private = kzalloc(sizeof(struct ux500_pcm_private), GFP_KERNEL);
	if (private == NULL)
		return -ENOMEM;

	private->msp_id = rtd->dai->cpu_dai->id;
	runtime->private_data = private;

	pr_debug("%s: Setting HW-config\n", __func__);
	runtime->hw = (stream_id == SNDRV_PCM_STREAM_PLAYBACK) ?
		ux500_pcm_hw_playback : ux500_pcm_hw_capture;

	return 0;
}

static int ux500_pcm_close(struct snd_pcm_substream *substream)
{
	struct ux500_pcm_private *private = substream->runtime->private_data;

	pr_info("%s: Enter\n", __func__);

	kfree(private);

	return 0;
}

static int ux500_pcm_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *buf = runtime->dma_buffer_p;
	int ret = 0;
	int size;

	pr_info("%s: Enter\n", __func__);

	size = params_buffer_bytes(hw_params);

	if (buf) {
		if (buf->bytes >= size)
			goto out;
		ux500_pcm_dma_hw_free(NULL, substream);
	}

	if (substream->dma_buffer.area != NULL &&
		substream->dma_buffer.bytes >= size) {
		buf = &substream->dma_buffer;
	} else {
		buf = kmalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
		if (!buf)
			goto nomem;

		buf->dev.type = SNDRV_DMA_TYPE_DEV;
		buf->dev.dev = NULL;
		buf->area = dma_alloc_coherent(
			NULL,
			size,
			&buf->addr,
			GFP_KERNEL);
		buf->bytes = size;
		buf->private_data = NULL;

		if (!buf->area)
			goto free;
	}
	snd_pcm_set_runtime_buffer(substream, buf);
	ret = 1;
 out:
	runtime->dma_bytes = size;
	return ret;

 free:
	kfree(buf);
 nomem:
	return -ENOMEM;
}

static int ux500_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s: Enter\n", __func__);

	ux500_pcm_dma_hw_free(NULL, substream);

	return 0;
}

static int ux500_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("%s: Enter\n", __func__);

	return 0;
}

static int ux500_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int i;
	struct ux500_pcm_private *private = substream->runtime->private_data;

	pr_debug("%s: Enter\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for (i = 0; i < UX500_PLATFORM_PERIODS_QUEUED_DMA; i++)
			ux500_pcm_dma_enqueue(substream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("%s: SNDRV_PCM_TRIGGER_STOP\n", __func__);
		private->period = 0;
		break;

	default:
		pr_err("%s: Invalid command in pcm trigger\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_uframes_t ux500_pcm_pointer(struct snd_pcm_substream *substream)
{
	unsigned int offset;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ux500_pcm_private *private = substream->runtime->private_data;

	pr_debug("%s: Enter\n", __func__);

	offset = bytes_to_frames(runtime, private->offset);
	if (offset < 0 || private->offset < 0)
		pr_debug("%s: Offset=%i %i\n",
			__func__,
			offset,
			private->offset);

	return offset;
}

static int ux500_pcm_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s: Enter.\n", __func__);

	return dma_mmap_coherent(
		NULL,
		vma,
		runtime->dma_area,
		runtime->dma_addr,
		runtime->dma_bytes);
}

static struct snd_pcm_ops ux500_pcm_ops = {
	.open		= ux500_pcm_open,
	.close		= ux500_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= ux500_pcm_hw_params,
	.hw_free	= ux500_pcm_hw_free,
	.prepare	= ux500_pcm_prepare,
	.trigger	= ux500_pcm_trigger,
	.pointer	= ux500_pcm_pointer,
	.mmap		= ux500_pcm_mmap
};

int ux500_pcm_new(struct snd_card *card,
		struct snd_soc_dai *dai,
		struct snd_pcm *pcm)
{
	pr_debug("%s: pcm = %d\n", __func__, (int)pcm);

	pcm->info_flags = 0;
	strcpy(pcm->name, "UX500_PCM");

	pr_debug("%s: pcm->name = %s.\n", __func__, pcm->name);

	return 0;
}

static void ux500_pcm_free(struct snd_pcm *pcm)
{
	pr_debug("%s: Enter\n", __func__);
}

static int ux500_pcm_suspend(struct snd_soc_dai *dai)
{
	pr_debug("%s: Enter\n", __func__);

	return 0;
}

static int ux500_pcm_resume(struct snd_soc_dai *dai)
{
	pr_debug("%s: Enter\n", __func__);

	return 0;
}

struct snd_soc_platform ux500_soc_platform = {
	.name           = "ux500-audio",
	.pcm_ops        = &ux500_pcm_ops,
	.pcm_new        = ux500_pcm_new,
	.pcm_free       = ux500_pcm_free,
	.suspend        = ux500_pcm_suspend,
	.resume         = ux500_pcm_resume,
};
EXPORT_SYMBOL(ux500_soc_platform);

static int __init ux500_pcm_init(void)
{
	int ret;

	pr_debug("%s: Register platform.\n", __func__);
	ret = snd_soc_register_platform(&ux500_soc_platform);
	if (ret < 0)
		pr_debug("%s: Error: Failed to register platform!\n",
			__func__);

	return 0;
}

static void __exit ux500_pcm_exit(void)
{
	snd_soc_unregister_platform(&ux500_soc_platform);
}

module_init(ux500_pcm_init);
module_exit(ux500_pcm_exit);

MODULE_LICENSE("GPL");

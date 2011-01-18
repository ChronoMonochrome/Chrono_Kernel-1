/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Jarmo K. Kuronen <jarmo.kuronen@symbio.com>
 *         for ST-Ericsson.
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef UX500_AB8500_H
#define UX500_AB8500_H

extern struct snd_soc_ops ux500_ab8500_ops[];

int ux500_ab8500_machine_codec_init(struct snd_soc_pcm_runtime *runtime);

void ux500_ab8500_soc_machine_drv_cleanup(void);

int enable_regulator(const char *name);
void disable_regulator(const char *name);

#endif

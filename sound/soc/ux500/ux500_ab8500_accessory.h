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

#ifndef UX500_AB8500_ACCESSORY_H__
#define UX500_AB8500_ACCESSORY_H__

struct snd_soc_codec;

extern int ab8500_accessory_init(struct snd_soc_codec *codec);
extern void ab8500_accessory_cleanup(void);

#endif

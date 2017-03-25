/*
 * ab8500-ponkey.h - API for AB8500 PowerOn Key
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_AB8500_PONKEY_H
#define __LINUX_AB8500_PONKEY_H

void ab8500_ponkey_emulator(unsigned long keycode, bool press);
void abb_ponkey_unmap_all_keys(unsigned long *keys, unsigned int array_len);
void abb_ponkey_remap_power_key(unsigned long old_keycode, unsigned long new_keycode);
void abb_ponkey_unmap_power_key(unsigned long old_keycode);

#endif /* __LINUX_AB8500_PONKEY_H */

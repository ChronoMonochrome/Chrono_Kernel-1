/*
 * This file is part of wl1271
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/crc7.h>
#include <linux/spi/spi.h>

<<<<<<< HEAD:drivers/net/wireless/wl12xx/io.c
#include "wl12xx.h"
=======
#include "wlcore.h"
#include "debug.h"
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/io.c
#include "wl12xx_80211.h"
#include "io.h"
#include "tx.h"

<<<<<<< HEAD:drivers/net/wireless/wl12xx/io.c
#define OCP_CMD_LOOP  32

#define OCP_CMD_WRITE 0x1
#define OCP_CMD_READ  0x2

#define OCP_READY_MASK  BIT(18)
#define OCP_STATUS_MASK (BIT(16) | BIT(17))

#define OCP_STATUS_NO_RESP    0x00000
#define OCP_STATUS_OK         0x10000
#define OCP_STATUS_REQ_FAILED 0x20000
#define OCP_STATUS_RESP_ERROR 0x30000

=======
>>>>>>> lk-3.5:drivers/net/wireless/ti/wlcore/io.c
bool wl1271_set_block_size(struct wl1271 *wl)
{
	if (wl->if_ops->set_block_size) {
		wl->if_ops->set_block_size(wl, WL12XX_BUS_BLOCK_SIZE);
		return true;
	}

	return false;
}

void wlcore_disable_interrupts(struct wl1271 *wl)
{
	wl->if_ops->disable_irq(wl);
}
EXPORT_SYMBOL_GPL(wlcore_disable_interrupts);

void wlcore_enable_interrupts(struct wl1271 *wl)
{
	wl->if_ops->enable_irq(wl);
}
EXPORT_SYMBOL_GPL(wlcore_enable_interrupts);

int wlcore_translate_addr(struct wl1271 *wl, int addr)
{
	struct wlcore_partition_set *part = &wl->curr_part;

	/*
	 * To translate, first check to which window of addresses the
	 * particular address belongs. Then subtract the starting address
	 * of that window from the address. Then, add offset of the
	 * translated region.
	 *
	 * The translated regions occur next to each other in physical device
	 * memory, so just add the sizes of the preceding address regions to
	 * get the offset to the new region.
	 */
	if ((addr >= part->mem.start) &&
	    (addr < part->mem.start + part->mem.size))
		return addr - part->mem.start;
	else if ((addr >= part->reg.start) &&
		 (addr < part->reg.start + part->reg.size))
		return addr - part->reg.start + part->mem.size;
	else if ((addr >= part->mem2.start) &&
		 (addr < part->mem2.start + part->mem2.size))
		return addr - part->mem2.start + part->mem.size +
			part->reg.size;
	else if ((addr >= part->mem3.start) &&
		 (addr < part->mem3.start + part->mem3.size))
		return addr - part->mem3.start + part->mem.size +
			part->reg.size + part->mem2.size;

	WARN(1, "HW address 0x%x out of range", addr);
	return 0;
}
EXPORT_SYMBOL_GPL(wlcore_translate_addr);

/* Set the partitions to access the chip addresses
 *
 * To simplify driver code, a fixed (virtual) memory map is defined for
 * register and memory addresses. Because in the chipset, in different stages
 * of operation, those addresses will move around, an address translation
 * mechanism is required.
 *
 * There are four partitions (three memory and one register partition),
 * which are mapped to two different areas of the hardware memory.
 *
 *                                Virtual address
 *                                     space
 *
 *                                    |    |
 *                                 ...+----+--> mem.start
 *          Physical address    ...   |    |
 *               space       ...      |    | [PART_0]
 *                        ...         |    |
 *  00000000  <--+----+...         ...+----+--> mem.start + mem.size
 *               |    |         ...   |    |
 *               |MEM |      ...      |    |
 *               |    |   ...         |    |
 *  mem.size  <--+----+...            |    | {unused area)
 *               |    |   ...         |    |
 *               |REG |      ...      |    |
 *  mem.size     |    |         ...   |    |
 *      +     <--+----+...         ...+----+--> reg.start
 *  reg.size     |    |   ...         |    |
 *               |MEM2|      ...      |    | [PART_1]
 *               |    |         ...   |    |
 *                                 ...+----+--> reg.start + reg.size
 *                                    |    |
 *
 */
void wlcore_set_partition(struct wl1271 *wl,
			  const struct wlcore_partition_set *p)
{
	/* copy partition info */
	memcpy(&wl->curr_part, p, sizeof(*p));

	wl1271_debug(DEBUG_IO, "mem_start %08X mem_size %08X",
		     p->mem.start, p->mem.size);
	wl1271_debug(DEBUG_IO, "reg_start %08X reg_size %08X",
		     p->reg.start, p->reg.size);
	wl1271_debug(DEBUG_IO, "mem2_start %08X mem2_size %08X",
		     p->mem2.start, p->mem2.size);
	wl1271_debug(DEBUG_IO, "mem3_start %08X mem3_size %08X",
		     p->mem3.start, p->mem3.size);

	wl1271_raw_write32(wl, HW_PART0_START_ADDR, p->mem.start);
	wl1271_raw_write32(wl, HW_PART0_SIZE_ADDR, p->mem.size);
	wl1271_raw_write32(wl, HW_PART1_START_ADDR, p->reg.start);
	wl1271_raw_write32(wl, HW_PART1_SIZE_ADDR, p->reg.size);
	wl1271_raw_write32(wl, HW_PART2_START_ADDR, p->mem2.start);
	wl1271_raw_write32(wl, HW_PART2_SIZE_ADDR, p->mem2.size);
	/*
	 * We don't need the size of the last partition, as it is
	 * automatically calculated based on the total memory size and
	 * the sizes of the previous partitions.
	 */
	wl1271_raw_write32(wl, HW_PART3_START_ADDR, p->mem3.start);
}
EXPORT_SYMBOL_GPL(wlcore_set_partition);

void wlcore_select_partition(struct wl1271 *wl, u8 part)
{
	wl1271_debug(DEBUG_IO, "setting partition %d", part);

	wlcore_set_partition(wl, &wl->ptable[part]);
}
EXPORT_SYMBOL_GPL(wlcore_select_partition);

void wl1271_io_reset(struct wl1271 *wl)
{
	wl->if_ops->reset(wl);
}

void wl1271_io_init(struct wl1271 *wl)
{
	wl->if_ops->init(wl);
}

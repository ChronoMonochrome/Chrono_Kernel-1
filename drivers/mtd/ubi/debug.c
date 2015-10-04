/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Artem Bityutskiy (Битюцкий Артём)
 */

/*
 * Here we keep all the UBI debugging stuff which should normally be disabled
 * and compiled-out, but it is extremely helpful when hunting bugs or doing big
 * changes.
 */

#ifdef CONFIG_MTD_UBI_DEBUG

#include "ubi.h"
#include <linux/module.h>
#include <linux/moduleparam.h>

unsigned int ubi_chk_flags;
unsigned int ubi_tst_flags;

module_param_named(debug_chks, ubi_chk_flags, uint, S_IRUGO | S_IWUSR);
module_param_named(debug_tsts, ubi_chk_flags, uint, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(debug_chks, "Debug check flags");
MODULE_PARM_DESC(debug_tsts, "Debug special test flags");

/**
 * ubi_dbg_dump_ec_hdr - dump an erase counter header.
 * @ec_hdr: the erase counter header to dump
 */
void ubi_dbg_dump_ec_hdr(const struct ubi_ec_hdr *ec_hdr)
{
;
//	printk(KERN_DEBUG "\tmagic          %#08x\n",
;
;
//	printk(KERN_DEBUG "\tec             %llu\n",
;
//	printk(KERN_DEBUG "\tvid_hdr_offset %d\n",
;
//	printk(KERN_DEBUG "\tdata_offset    %d\n",
;
//	printk(KERN_DEBUG "\timage_seq      %d\n",
;
//	printk(KERN_DEBUG "\thdr_crc        %#08x\n",
;
;
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
		       ec_hdr, UBI_EC_HDR_SIZE, 1);
}

/**
 * ubi_dbg_dump_vid_hdr - dump a volume identifier header.
 * @vid_hdr: the volume identifier header to dump
 */
void ubi_dbg_dump_vid_hdr(const struct ubi_vid_hdr *vid_hdr)
{
;
;
;
;
;
;
;
;
;
;
;
//	printk(KERN_DEBUG "\tsqnum     %llu\n",
;
;
;
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
		       vid_hdr, UBI_VID_HDR_SIZE, 1);
}

/**
 * ubi_dbg_dump_vol_info- dump volume information.
 * @vol: UBI volume description object
 */
void ubi_dbg_dump_vol_info(const struct ubi_volume *vol)
{
;
;
;
;
;
;
;
;
;
;
;
;
;

	if (vol->name_len <= UBI_VOL_NAME_MAX &&
	    strnlen(vol->name, vol->name_len + 1) == vol->name_len) {
;
	} else {
//		printk(KERN_DEBUG "\t1st 5 characters of name: %c%c%c%c%c\n",
//		       vol->name[0], vol->name[1], vol->name[2],
;
	}
}

/**
 * ubi_dbg_dump_vtbl_record - dump a &struct ubi_vtbl_record object.
 * @r: the object to dump
 * @idx: volume table index
 */
void ubi_dbg_dump_vtbl_record(const struct ubi_vtbl_record *r, int idx)
{
	int name_len = be16_to_cpu(r->name_len);

;
//	printk(KERN_DEBUG "\treserved_pebs   %d\n",
;
;
;
;
;
;

	if (r->name[0] == '\0') {
;
		return;
	}

	if (name_len <= UBI_VOL_NAME_MAX &&
	    strnlen(&r->name[0], name_len + 1) == name_len) {
;
	} else {
//		printk(KERN_DEBUG "\t1st 5 characters of name: %c%c%c%c%c\n",
//			r->name[0], r->name[1], r->name[2], r->name[3],
;
	}
;
}

/**
 * ubi_dbg_dump_sv - dump a &struct ubi_scan_volume object.
 * @sv: the object to dump
 */
void ubi_dbg_dump_sv(const struct ubi_scan_volume *sv)
{
;
;
;
;
;
;
;
;
;
}

/**
 * ubi_dbg_dump_seb - dump a &struct ubi_scan_leb object.
 * @seb: the object to dump
 * @type: object type: 0 - not corrupted, 1 - corrupted
 */
void ubi_dbg_dump_seb(const struct ubi_scan_leb *seb, int type)
{
;
;
;
	if (type == 0) {
;
;
;
	}
}

/**
 * ubi_dbg_dump_mkvol_req - dump a &struct ubi_mkvol_req object.
 * @req: the object to dump
 */
void ubi_dbg_dump_mkvol_req(const struct ubi_mkvol_req *req)
{
	char nm[17];

;
;
;
;
;
;

	memcpy(nm, req->name, 16);
	nm[16] = 0;
;
}

/**
 * ubi_dbg_dump_flash - dump a region of flash.
 * @ubi: UBI device description object
 * @pnum: the physical eraseblock number to dump
 * @offset: the starting offset within the physical eraseblock to dump
 * @len: the length of the region to dump
 */
void ubi_dbg_dump_flash(struct ubi_device *ubi, int pnum, int offset, int len)
{
	int err;
	size_t read;
	void *buf;
	loff_t addr = (loff_t)pnum * ubi->peb_size + offset;

	buf = vmalloc(len);
	if (!buf)
		return;
	err = ubi->mtd->read(ubi->mtd, addr, len, &read, buf);
	if (err && err != -EUCLEAN) {
		ubi_err("error %d while reading %d bytes from PEB %d:%d, "
			"read %zd bytes", err, len, pnum, offset, read);
		goto out;
	}

	dbg_msg("dumping %d bytes of data from PEB %d, offset %d",
		len, pnum, offset);
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1, buf, len, 1);
out:
	vfree(buf);
	return;
}

#endif /* CONFIG_MTD_UBI_DEBUG */

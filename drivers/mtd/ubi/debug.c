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
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Erase counter header dump:\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tmagic          %#08x\n",
	       be32_to_cpu(ec_hdr->magic));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tversion        %d\n", (int)ec_hdr->version);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tec             %llu\n",
	       (long long)be64_to_cpu(ec_hdr->ec));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvid_hdr_offset %d\n",
	       be32_to_cpu(ec_hdr->vid_hdr_offset));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tdata_offset    %d\n",
	       be32_to_cpu(ec_hdr->data_offset));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\timage_seq      %d\n",
	       be32_to_cpu(ec_hdr->image_seq));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\thdr_crc        %#08x\n",
	       be32_to_cpu(ec_hdr->hdr_crc));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "erase counter header hexdump:\n");
#else
	;
#endif
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
		       ec_hdr, UBI_EC_HDR_SIZE, 1);
}

/**
 * ubi_dbg_dump_vid_hdr - dump a volume identifier header.
 * @vid_hdr: the volume identifier header to dump
 */
void ubi_dbg_dump_vid_hdr(const struct ubi_vid_hdr *vid_hdr)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Volume identifier header dump:\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tmagic     %08x\n", be32_to_cpu(vid_hdr->magic));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tversion   %d\n",  (int)vid_hdr->version);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_type  %d\n",  (int)vid_hdr->vol_type);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tcopy_flag %d\n",  (int)vid_hdr->copy_flag);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tcompat    %d\n",  (int)vid_hdr->compat);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_id    %d\n",  be32_to_cpu(vid_hdr->vol_id));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tlnum      %d\n",  be32_to_cpu(vid_hdr->lnum));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tdata_size %d\n",  be32_to_cpu(vid_hdr->data_size));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tused_ebs  %d\n",  be32_to_cpu(vid_hdr->used_ebs));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tdata_pad  %d\n",  be32_to_cpu(vid_hdr->data_pad));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tsqnum     %llu\n",
		(unsigned long long)be64_to_cpu(vid_hdr->sqnum));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\thdr_crc   %08x\n", be32_to_cpu(vid_hdr->hdr_crc));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Volume identifier header hexdump:\n");
#else
	;
#endif
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
		       vid_hdr, UBI_VID_HDR_SIZE, 1);
}

/**
 * ubi_dbg_dump_vol_info- dump volume information.
 * @vol: UBI volume description object
 */
void ubi_dbg_dump_vol_info(const struct ubi_volume *vol)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Volume information dump:\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_id          %d\n", vol->vol_id);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\treserved_pebs   %d\n", vol->reserved_pebs);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\talignment       %d\n", vol->alignment);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tdata_pad        %d\n", vol->data_pad);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_type        %d\n", vol->vol_type);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tname_len        %d\n", vol->name_len);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tusable_leb_size %d\n", vol->usable_leb_size);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tused_ebs        %d\n", vol->used_ebs);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tused_bytes      %lld\n", vol->used_bytes);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tlast_eb_bytes   %d\n", vol->last_eb_bytes);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tcorrupted       %d\n", vol->corrupted);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tupd_marker      %d\n", vol->upd_marker);
#else
	;
#endif

	if (vol->name_len <= UBI_VOL_NAME_MAX &&
	    strnlen(vol->name, vol->name_len + 1) == vol->name_len) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\tname            %s\n", vol->name);
#else
		;
#endif
	} else {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\t1st 5 characters of name: %c%c%c%c%c\n",
		       vol->name[0], vol->name[1], vol->name[2],
		       vol->name[3], vol->name[4]);
#else
		;
#endif
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

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Volume table record %d dump:\n", idx);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\treserved_pebs   %d\n",
	       be32_to_cpu(r->reserved_pebs));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\talignment       %d\n", be32_to_cpu(r->alignment));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tdata_pad        %d\n", be32_to_cpu(r->data_pad));
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_type        %d\n", (int)r->vol_type);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tupd_marker      %d\n", (int)r->upd_marker);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tname_len        %d\n", name_len);
#else
	;
#endif

	if (r->name[0] == '\0') {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\tname            NULL\n");
#else
		;
#endif
		return;
	}

	if (name_len <= UBI_VOL_NAME_MAX &&
	    strnlen(&r->name[0], name_len + 1) == name_len) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\tname            %s\n", &r->name[0]);
#else
		;
#endif
	} else {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\t1st 5 characters of name: %c%c%c%c%c\n",
			r->name[0], r->name[1], r->name[2], r->name[3],
			r->name[4]);
#else
		;
#endif
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tcrc             %#08x\n", be32_to_cpu(r->crc));
#else
	;
#endif
}

/**
 * ubi_dbg_dump_sv - dump a &struct ubi_scan_volume object.
 * @sv: the object to dump
 */
void ubi_dbg_dump_sv(const struct ubi_scan_volume *sv)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Volume scanning information dump:\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_id         %d\n", sv->vol_id);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\thighest_lnum   %d\n", sv->highest_lnum);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tleb_count      %d\n", sv->leb_count);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tcompat         %d\n", sv->compat);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_type       %d\n", sv->vol_type);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tused_ebs       %d\n", sv->used_ebs);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tlast_data_size %d\n", sv->last_data_size);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tdata_pad       %d\n", sv->data_pad);
#else
	;
#endif
}

/**
 * ubi_dbg_dump_seb - dump a &struct ubi_scan_leb object.
 * @seb: the object to dump
 * @type: object type: 0 - not corrupted, 1 - corrupted
 */
void ubi_dbg_dump_seb(const struct ubi_scan_leb *seb, int type)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "eraseblock scanning information dump:\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tec       %d\n", seb->ec);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tpnum     %d\n", seb->pnum);
#else
	;
#endif
	if (type == 0) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\tlnum     %d\n", seb->lnum);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\tscrub    %d\n", seb->scrub);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "\tsqnum    %llu\n", seb->sqnum);
#else
		;
#endif
	}
}

/**
 * ubi_dbg_dump_mkvol_req - dump a &struct ubi_mkvol_req object.
 * @req: the object to dump
 */
void ubi_dbg_dump_mkvol_req(const struct ubi_mkvol_req *req)
{
	char nm[17];

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "Volume creation request dump:\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_id    %d\n",   req->vol_id);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\talignment %d\n",   req->alignment);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tbytes     %lld\n", (long long)req->bytes);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tvol_type  %d\n",   req->vol_type);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\tname_len  %d\n",   req->name_len);
#else
	;
#endif

	memcpy(nm, req->name, 16);
	nm[16] = 0;
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "\t1st 16 characters of name: %s\n", nm);
#else
	;
#endif
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

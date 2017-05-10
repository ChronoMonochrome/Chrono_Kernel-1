/*******************************************************************************
 * Filename:  target_core_rd.c
 *
 * This file contains the Storage Engine <-> Ramdisk transport
 * specific functions.
 *
 * (c) Copyright 2003-2012 RisingTide Systems LLC.
 *
 * Nicholas A. Bellinger <nab@kernel.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ******************************************************************************/

#include <linux/string.h>
#include <linux/parser.h>
#include <linux/timer.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>

#include <target/target_core_base.h>
#include <target/target_core_backend.h>

#include "target_core_rd.h"

static inline struct rd_dev *RD_DEV(struct se_device *dev)
{
	return container_of(dev, struct rd_dev, dev);
}

/*	rd_attach_hba(): (Part of se_subsystem_api_t template)
 *
 *
 */
static int rd_attach_hba(struct se_hba *hba, u32 host_id)
{
	struct rd_host *rd_host;

	rd_host = kzalloc(sizeof(struct rd_host), GFP_KERNEL);
	if (!rd_host) {
		pr_err("Unable to allocate memory for struct rd_host\n");
		return -ENOMEM;
	}

	rd_host->rd_host_id = host_id;

	hba->hba_ptr = rd_host;

<<<<<<< HEAD
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "CORE_HBA[%d] - TCM Ramdisk HBA Driver %s on"
		" Generic Target Core Stack %s\n", hba->hba_id,
		RD_HBA_VERSION, TARGET_CORE_MOD_VERSION);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "CORE_HBA[%d] - Attached Ramdisk HBA: %u to Generic"
		" Target Core TCQ Depth: %d MaxSectors: %u\n", hba->hba_id,
		rd_host->rd_host_id, atomic_read(&hba->max_queue_depth),
		RD_MAX_SECTORS);
#else
	;
#endif
=======
	pr_debug("CORE_HBA[%d] - TCM Ramdisk HBA Driver %s on"
		" Generic Target Core Stack %s\n", hba->hba_id,
		RD_HBA_VERSION, TARGET_CORE_MOD_VERSION);
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD

	return 0;
}

static void rd_detach_hba(struct se_hba *hba)
{
	struct rd_host *rd_host = hba->hba_ptr;

<<<<<<< HEAD
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "CORE_HBA[%d] - Detached Ramdisk HBA: %u from"
=======
	pr_debug("CORE_HBA[%d] - Detached Ramdisk HBA: %u from"
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD
		" Generic Target Core\n", hba->hba_id, rd_host->rd_host_id);
#else
	;
#endif

	kfree(rd_host);
	hba->hba_ptr = NULL;
}

/*	rd_release_device_space():
 *
 *
 */
static void rd_release_device_space(struct rd_dev *rd_dev)
{
	u32 i, j, page_count = 0, sg_per_table;
	struct rd_dev_sg_table *sg_table;
	struct page *pg;
	struct scatterlist *sg;

	if (!rd_dev->sg_table_array || !rd_dev->sg_table_count)
		return;

	sg_table = rd_dev->sg_table_array;

	for (i = 0; i < rd_dev->sg_table_count; i++) {
		sg = sg_table[i].sg_table;
		sg_per_table = sg_table[i].rd_sg_count;

		for (j = 0; j < sg_per_table; j++) {
			pg = sg_page(&sg[j]);
			if (pg) {
				__free_page(pg);
				page_count++;
			}
		}

		kfree(sg);
	}

<<<<<<< HEAD
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "CORE_RD[%u] - Released device space for Ramdisk"
=======
	pr_debug("CORE_RD[%u] - Released device space for Ramdisk"
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD
		" Device ID: %u, pages %u in %u tables total bytes %lu\n",
		rd_dev->rd_host->rd_host_id, rd_dev->rd_dev_id, page_count,
		rd_dev->sg_table_count, (unsigned long)page_count * PAGE_SIZE);
#else
	;
#endif

	kfree(sg_table);
	rd_dev->sg_table_array = NULL;
	rd_dev->sg_table_count = 0;
}


/*	rd_build_device_space():
 *
 *
 */
static int rd_build_device_space(struct rd_dev *rd_dev)
{
	u32 i = 0, j, page_offset = 0, sg_per_table, sg_tables, total_sg_needed;
	u32 max_sg_per_table = (RD_MAX_ALLOCATION_SIZE /
				sizeof(struct scatterlist));
	struct rd_dev_sg_table *sg_table;
	struct page *pg;
	struct scatterlist *sg;

	if (rd_dev->rd_page_count <= 0) {
		pr_err("Illegal page count: %u for Ramdisk device\n",
			rd_dev->rd_page_count);
		return -EINVAL;
	}
	total_sg_needed = rd_dev->rd_page_count;

	sg_tables = (total_sg_needed / max_sg_per_table) + 1;

	sg_table = kzalloc(sg_tables * sizeof(struct rd_dev_sg_table), GFP_KERNEL);
	if (!sg_table) {
		pr_err("Unable to allocate memory for Ramdisk"
			" scatterlist tables\n");
		return -ENOMEM;
	}

	rd_dev->sg_table_array = sg_table;
	rd_dev->sg_table_count = sg_tables;

	while (total_sg_needed) {
		sg_per_table = (total_sg_needed > max_sg_per_table) ?
			max_sg_per_table : total_sg_needed;

		sg = kzalloc(sg_per_table * sizeof(struct scatterlist),
				GFP_KERNEL);
		if (!sg) {
			pr_err("Unable to allocate scatterlist array"
				" for struct rd_dev\n");
			return -ENOMEM;
		}

		sg_init_table(sg, sg_per_table);

		sg_table[i].sg_table = sg;
		sg_table[i].rd_sg_count = sg_per_table;
		sg_table[i].page_start_offset = page_offset;
		sg_table[i++].page_end_offset = (page_offset + sg_per_table)
						- 1;

		for (j = 0; j < sg_per_table; j++) {
			pg = alloc_pages(GFP_KERNEL, 0);
			if (!pg) {
				pr_err("Unable to allocate scatterlist"
					" pages for struct rd_dev_sg_table\n");
				return -ENOMEM;
			}
			sg_assign_page(&sg[j], pg);
			sg[j].length = PAGE_SIZE;
		}

		page_offset += sg_per_table;
		total_sg_needed -= sg_per_table;
	}

<<<<<<< HEAD
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "CORE_RD[%u] - Built Ramdisk Device ID: %u space of"
=======
	pr_debug("CORE_RD[%u] - Built Ramdisk Device ID: %u space of"
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD
		" %u pages in %u tables\n", rd_dev->rd_host->rd_host_id,
		rd_dev->rd_dev_id, rd_dev->rd_page_count,
		rd_dev->sg_table_count);
#else
	;
#endif

	return 0;
}

static struct se_device *rd_alloc_device(struct se_hba *hba, const char *name)
{
	struct rd_dev *rd_dev;
	struct rd_host *rd_host = hba->hba_ptr;

	rd_dev = kzalloc(sizeof(struct rd_dev), GFP_KERNEL);
	if (!rd_dev) {
		pr_err("Unable to allocate memory for struct rd_dev\n");
		return NULL;
	}

	rd_dev->rd_host = rd_host;

	return &rd_dev->dev;
}

static int rd_configure_device(struct se_device *dev)
{
	struct rd_dev *rd_dev = RD_DEV(dev);
	struct rd_host *rd_host = dev->se_hba->hba_ptr;
	int ret;

	if (!(rd_dev->rd_flags & RDF_HAS_PAGE_COUNT)) {
		pr_debug("Missing rd_pages= parameter\n");
		return -EINVAL;
	}

	ret = rd_build_device_space(rd_dev);
	if (ret < 0)
		goto fail;

	dev->dev_attrib.hw_block_size = RD_BLOCKSIZE;
	dev->dev_attrib.hw_max_sectors = UINT_MAX;
	dev->dev_attrib.hw_queue_depth = RD_MAX_DEVICE_QUEUE_DEPTH;

	rd_dev->rd_dev_id = rd_host->rd_host_dev_id_count++;

<<<<<<< HEAD
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "CORE_RD[%u] - Added TCM %s Ramdisk Device ID: %u of"
=======
	pr_debug("CORE_RD[%u] - Added TCM MEMCPY Ramdisk Device ID: %u of"
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD
		" %u pages in %u tables, %lu total bytes\n",
		rd_host->rd_host_id, rd_dev->rd_dev_id, rd_dev->rd_page_count,
		rd_dev->sg_table_count,
		(unsigned long)(rd_dev->rd_page_count * PAGE_SIZE));
#else
	;
#endif

	return 0;

fail:
	rd_release_device_space(rd_dev);
	return ret;
}

static void rd_free_device(struct se_device *dev)
{
	struct rd_dev *rd_dev = RD_DEV(dev);

	rd_release_device_space(rd_dev);
	kfree(rd_dev);
}

static struct rd_dev_sg_table *rd_get_sg_table(struct rd_dev *rd_dev, u32 page)
{
	struct rd_dev_sg_table *sg_table;
	u32 i, sg_per_table = (RD_MAX_ALLOCATION_SIZE /
				sizeof(struct scatterlist));

	i = page / sg_per_table;
	if (i < rd_dev->sg_table_count) {
		sg_table = &rd_dev->sg_table_array[i];
		if ((sg_table->page_start_offset <= page) &&
		    (sg_table->page_end_offset >= page))
			return sg_table;
	}

	pr_err("Unable to locate struct rd_dev_sg_table for page: %u\n",
			page);

	return NULL;
}

static sense_reason_t
rd_execute_rw(struct se_cmd *cmd)
{
	struct scatterlist *sgl = cmd->t_data_sg;
	u32 sgl_nents = cmd->t_data_nents;
	enum dma_data_direction data_direction = cmd->data_direction;
	struct se_device *se_dev = cmd->se_dev;
	struct rd_dev *dev = RD_DEV(se_dev);
	struct rd_dev_sg_table *table;
<<<<<<< HEAD
	struct scatterlist *sg_d, *sg_s;
	void *dst, *src;
	u32 i = 0, j = 0, dst_offset = 0, src_offset = 0;
	u32 length, page_end = 0, table_sg_end;
	u32 rd_offset = req->rd_offset;

	table = rd_get_sg_table(dev, req->rd_page);
	if (!(table))
		return -1;

	table_sg_end = (table->page_end_offset - req->rd_page);
	sg_d = task->task_sg;
	sg_s = &table->sg_table[req->rd_page - table->page_start_offset];
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "RD[%u]: Read LBA: %llu, Size: %u Page: %u, Offset:"
		" %u\n", dev->rd_dev_id, task->task_lba, req->rd_size,
		req->rd_page, req->rd_offset);
#else
	;
#endif
#endif
	src_offset = rd_offset;

	while (req->rd_size) {
		if ((sg_d[i].length - dst_offset) <
		    (sg_s[j].length - src_offset)) {
			length = (sg_d[i].length - dst_offset);
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 1 - sg_d[%d]: %p length: %d"
				" offset: %u sg_s[%d].length: %u\n", i,
				&sg_d[i], sg_d[i].length, sg_d[i].offset, j,
				sg_s[j].length);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 1 - length: %u dst_offset: %u"
				" src_offset: %u\n", length, dst_offset,
				src_offset);
#else
			;
#endif
#endif
			if (length > req->rd_size)
				length = req->rd_size;

			dst = sg_virt(&sg_d[i++]) + dst_offset;
			if (!dst)
				BUG();

			src = sg_virt(&sg_s[j]) + src_offset;
			if (!src)
				BUG();

			dst_offset = 0;
			src_offset = length;
			page_end = 0;
		} else {
			length = (sg_s[j].length - src_offset);
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 2 - sg_d[%d]: %p length: %d"
				" offset: %u sg_s[%d].length: %u\n", i,
				&sg_d[i], sg_d[i].length, sg_d[i].offset,
				j, sg_s[j].length);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 2 - length: %u dst_offset: %u"
				" src_offset: %u\n", length, dst_offset,
				src_offset);
#else
			;
#endif
#endif
			if (length > req->rd_size)
				length = req->rd_size;

			dst = sg_virt(&sg_d[i]) + dst_offset;
			if (!dst)
				BUG();

			if (sg_d[i].length == length) {
				i++;
				dst_offset = 0;
			} else
				dst_offset = length;

			src = sg_virt(&sg_s[j++]) + src_offset;
			if (!src)
				BUG();

			src_offset = 0;
			page_end = 1;
		}

		memcpy(dst, src, length);

#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "page: %u, remaining size: %u, length: %u,"
			" i: %u, j: %u\n", req->rd_page,
			(req->rd_size - length), length, i, j);
#else
		;
#endif
#endif
		req->rd_size -= length;
		if (!(req->rd_size))
			return 0;

		if (!page_end)
			continue;

		if (++req->rd_page <= table->page_end_offset) {
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "page: %u in same page table\n",
				req->rd_page);
#else
			;
#endif
#endif
			continue;
		}
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "getting new page table for page: %u\n",
				req->rd_page);
#else
		;
#endif
#endif
		table = rd_get_sg_table(dev, req->rd_page);
		if (!(table))
			return -1;

		sg_s = &table->sg_table[j = 0];
	}

	return 0;
}

/*	rd_MEMCPY_write():
 *
 *
 */
static int rd_MEMCPY_write(struct rd_request *req)
{
	struct se_task *task = &req->rd_task;
	struct rd_dev *dev = req->rd_dev;
	struct rd_dev_sg_table *table;
	struct scatterlist *sg_d, *sg_s;
	void *dst, *src;
	u32 i = 0, j = 0, dst_offset = 0, src_offset = 0;
	u32 length, page_end = 0, table_sg_end;
	u32 rd_offset = req->rd_offset;

	table = rd_get_sg_table(dev, req->rd_page);
	if (!(table))
		return -1;

	table_sg_end = (table->page_end_offset - req->rd_page);
	sg_d = &table->sg_table[req->rd_page - table->page_start_offset];
	sg_s = task->task_sg;
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "RD[%d] Write LBA: %llu, Size: %u, Page: %u,"
		" Offset: %u\n", dev->rd_dev_id, task->task_lba, req->rd_size,
		req->rd_page, req->rd_offset);
#else
	;
#endif
#endif
	dst_offset = rd_offset;

	while (req->rd_size) {
		if ((sg_s[i].length - src_offset) <
		    (sg_d[j].length - dst_offset)) {
			length = (sg_s[i].length - src_offset);
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 1 - sg_s[%d]: %p length: %d"
				" offset: %d sg_d[%d].length: %u\n", i,
				&sg_s[i], sg_s[i].length, sg_s[i].offset,
				j, sg_d[j].length);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 1 - length: %u src_offset: %u"
				" dst_offset: %u\n", length, src_offset,
				dst_offset);
#else
			;
#endif
#endif
			if (length > req->rd_size)
				length = req->rd_size;

			src = sg_virt(&sg_s[i++]) + src_offset;
			if (!src)
				BUG();

			dst = sg_virt(&sg_d[j]) + dst_offset;
			if (!dst)
				BUG();

			src_offset = 0;
			dst_offset = length;
			page_end = 0;
		} else {
			length = (sg_d[j].length - dst_offset);
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 2 - sg_s[%d]: %p length: %d"
				" offset: %d sg_d[%d].length: %u\n", i,
				&sg_s[i], sg_s[i].length, sg_s[i].offset,
				j, sg_d[j].length);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "Step 2 - length: %u src_offset: %u"
				" dst_offset: %u\n", length, src_offset,
				dst_offset);
#else
			;
#endif
#endif
			if (length > req->rd_size)
				length = req->rd_size;

			src = sg_virt(&sg_s[i]) + src_offset;
			if (!src)
				BUG();

			if (sg_s[i].length == length) {
				i++;
				src_offset = 0;
			} else
				src_offset = length;

			dst = sg_virt(&sg_d[j++]) + dst_offset;
			if (!dst)
				BUG();

			dst_offset = 0;
			page_end = 1;
		}
=======
	struct scatterlist *rd_sg;
	struct sg_mapping_iter m;
	u32 rd_offset;
	u32 rd_size;
	u32 rd_page;
	u32 src_len;
	u64 tmp;

	tmp = cmd->t_task_lba * se_dev->dev_attrib.block_size;
	rd_offset = do_div(tmp, PAGE_SIZE);
	rd_page = tmp;
	rd_size = cmd->data_length;

	table = rd_get_sg_table(dev, rd_page);
	if (!table)
		return TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE;

	rd_sg = &table->sg_table[rd_page - table->page_start_offset];

	pr_debug("RD[%u]: %s LBA: %llu, Size: %u Page: %u, Offset: %u\n",
			dev->rd_dev_id,
			data_direction == DMA_FROM_DEVICE ? "Read" : "Write",
			cmd->t_task_lba, rd_size, rd_page, rd_offset);

	src_len = PAGE_SIZE - rd_offset;
	sg_miter_start(&m, sgl, sgl_nents,
			data_direction == DMA_FROM_DEVICE ?
				SG_MITER_TO_SG : SG_MITER_FROM_SG);
	while (rd_size) {
		u32 len;
		void *rd_addr;

		sg_miter_next(&m);
		if (!(u32)m.length) {
			pr_debug("RD[%u]: invalid sgl %p len %zu\n",
				 dev->rd_dev_id, m.addr, m.length);
			sg_miter_stop(&m);
			return TCM_INCORRECT_AMOUNT_OF_DATA;
		}
		len = min((u32)m.length, src_len);
		if (len > rd_size) {
			pr_debug("RD[%u]: size underrun page %d offset %d "
				 "size %d\n", dev->rd_dev_id,
				 rd_page, rd_offset, rd_size);
			len = rd_size;
		}
		m.consumed = len;

		rd_addr = sg_virt(rd_sg) + rd_offset;
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD

		if (data_direction == DMA_FROM_DEVICE)
			memcpy(m.addr, rd_addr, len);
		else
			memcpy(rd_addr, m.addr, len);

<<<<<<< HEAD
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "page: %u, remaining size: %u, length: %u,"
			" i: %u, j: %u\n", req->rd_page,
			(req->rd_size - length), length, i, j);
#else
		;
#endif
#endif
		req->rd_size -= length;
		if (!(req->rd_size))
			return 0;

		if (!page_end)
			continue;

		if (++req->rd_page <= table->page_end_offset) {
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "page: %u in same page table\n",
				req->rd_page);
#else
			;
#endif
#endif
			continue;
		}
#ifdef DEBUG_RAMDISK_MCP
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "getting new page table for page: %u\n",
				req->rd_page);
#else
		;
#endif
#endif
		table = rd_get_sg_table(dev, req->rd_page);
		if (!(table))
			return -1;

		sg_d = &table->sg_table[j = 0];
	}

	return 0;
}

/*	rd_MEMCPY_do_task(): (Part of se_subsystem_api_t template)
 *
 *
 */
static int rd_MEMCPY_do_task(struct se_task *task)
{
	struct se_device *dev = task->se_dev;
	struct rd_request *req = RD_REQ(task);
	unsigned long long lba;
	int ret;

	req->rd_page = (task->task_lba * DEV_ATTRIB(dev)->block_size) / PAGE_SIZE;
	lba = task->task_lba;
	req->rd_offset = (do_div(lba,
			  (PAGE_SIZE / DEV_ATTRIB(dev)->block_size))) *
			   DEV_ATTRIB(dev)->block_size;
	req->rd_size = task->task_size;

	if (task->task_data_direction == DMA_FROM_DEVICE)
		ret = rd_MEMCPY_read(req);
	else
		ret = rd_MEMCPY_write(req);

	if (ret != 0)
		return ret;

	task->task_scsi_status = GOOD;
	transport_complete_task(task, 1);

	return PYX_TRANSPORT_SENT_TO_TRANSPORT;
}

/*	rd_DIRECT_with_offset():
 *
 *
 */
static int rd_DIRECT_with_offset(
	struct se_task *task,
	struct list_head *se_mem_list,
	u32 *se_mem_cnt,
	u32 *task_offset)
{
	struct rd_request *req = RD_REQ(task);
	struct rd_dev *dev = req->rd_dev;
	struct rd_dev_sg_table *table;
	struct se_mem *se_mem;
	struct scatterlist *sg_s;
	u32 j = 0, set_offset = 1;
	u32 get_next_table = 0, offset_length, table_sg_end;

	table = rd_get_sg_table(dev, req->rd_page);
	if (!(table))
		return -1;

	table_sg_end = (table->page_end_offset - req->rd_page);
	sg_s = &table->sg_table[req->rd_page - table->page_start_offset];
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "%s DIRECT LBA: %llu, Size: %u Page: %u, Offset: %u\n",
		(task->task_data_direction == DMA_TO_DEVICE) ?
			"Write" : "Read",
		task->task_lba, req->rd_size, req->rd_page, req->rd_offset);
#else
	;
#endif
#endif
	while (req->rd_size) {
		se_mem = kmem_cache_zalloc(se_mem_cache, GFP_KERNEL);
		if (!(se_mem)) {
			printk(KERN_ERR "Unable to allocate struct se_mem\n");
			return -1;
		}
		INIT_LIST_HEAD(&se_mem->se_list);

		if (set_offset) {
			offset_length = sg_s[j].length - req->rd_offset;
			if (offset_length > req->rd_size)
				offset_length = req->rd_size;

			se_mem->se_page = sg_page(&sg_s[j++]);
			se_mem->se_off = req->rd_offset;
			se_mem->se_len = offset_length;

			set_offset = 0;
			get_next_table = (j > table_sg_end);
			goto check_eot;
		}

		offset_length = (req->rd_size < req->rd_offset) ?
			req->rd_size : req->rd_offset;

		se_mem->se_page = sg_page(&sg_s[j]);
		se_mem->se_len = offset_length;

		set_offset = 1;

check_eot:
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "page: %u, size: %u, offset_length: %u, j: %u"
			" se_mem: %p, se_page: %p se_off: %u se_len: %u\n",
			req->rd_page, req->rd_size, offset_length, j, se_mem,
			se_mem->se_page, se_mem->se_off, se_mem->se_len);
#else
		;
#endif
#endif
		list_add_tail(&se_mem->se_list, se_mem_list);
		(*se_mem_cnt)++;

		req->rd_size -= offset_length;
		if (!(req->rd_size))
			goto out;

		if (!set_offset && !get_next_table)
			continue;

		if (++req->rd_page <= table->page_end_offset) {
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "page: %u in same page table\n",
					req->rd_page);
#else
			;
#endif
#endif
			continue;
		}
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "getting new page table for page: %u\n",
				req->rd_page);
#else
		;
#endif
#endif
		table = rd_get_sg_table(dev, req->rd_page);
		if (!(table))
			return -1;

		sg_s = &table->sg_table[j = 0];
	}

out:
	T_TASK(task->task_se_cmd)->t_tasks_se_num += *se_mem_cnt;
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "RD_DR - Allocated %u struct se_mem segments for task\n",
			*se_mem_cnt);
#else
	;
#endif
#endif
	return 0;
}

/*	rd_DIRECT_without_offset():
 *
 *
 */
static int rd_DIRECT_without_offset(
	struct se_task *task,
	struct list_head *se_mem_list,
	u32 *se_mem_cnt,
	u32 *task_offset)
{
	struct rd_request *req = RD_REQ(task);
	struct rd_dev *dev = req->rd_dev;
	struct rd_dev_sg_table *table;
	struct se_mem *se_mem;
	struct scatterlist *sg_s;
	u32 length, j = 0;

	table = rd_get_sg_table(dev, req->rd_page);
	if (!(table))
		return -1;

	sg_s = &table->sg_table[req->rd_page - table->page_start_offset];
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "%s DIRECT LBA: %llu, Size: %u, Page: %u\n",
		(task->task_data_direction == DMA_TO_DEVICE) ?
			"Write" : "Read",
		task->task_lba, req->rd_size, req->rd_page);
#else
	;
#endif
#endif
	while (req->rd_size) {
		se_mem = kmem_cache_zalloc(se_mem_cache, GFP_KERNEL);
		if (!(se_mem)) {
			printk(KERN_ERR "Unable to allocate struct se_mem\n");
			return -1;
		}
		INIT_LIST_HEAD(&se_mem->se_list);

		length = (req->rd_size < sg_s[j].length) ?
			req->rd_size : sg_s[j].length;

		se_mem->se_page = sg_page(&sg_s[j++]);
		se_mem->se_len = length;

#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "page: %u, size: %u, j: %u se_mem: %p,"
			" se_page: %p se_off: %u se_len: %u\n", req->rd_page,
			req->rd_size, j, se_mem, se_mem->se_page,
			se_mem->se_off, se_mem->se_len);
#else
		;
#endif
#endif
		list_add_tail(&se_mem->se_list, se_mem_list);
		(*se_mem_cnt)++;

		req->rd_size -= length;
		if (!(req->rd_size))
			goto out;

		if (++req->rd_page <= table->page_end_offset) {
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
			printk("page: %u in same page table\n",
				req->rd_page);
#else
			;
#endif
#endif
			continue;
		}
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "getting new page table for page: %u\n",
				req->rd_page);
#else
		;
#endif
#endif
		table = rd_get_sg_table(dev, req->rd_page);
		if (!(table))
			return -1;

		sg_s = &table->sg_table[j = 0];
	}

out:
	T_TASK(task->task_se_cmd)->t_tasks_se_num += *se_mem_cnt;
#ifdef DEBUG_RAMDISK_DR
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "RD_DR - Allocated %u struct se_mem segments for task\n",
			*se_mem_cnt);
#else
	;
#endif
#endif
	return 0;
}

/*	rd_DIRECT_do_se_mem_map():
 *
 *
 */
static int rd_DIRECT_do_se_mem_map(
	struct se_task *task,
	struct list_head *se_mem_list,
	void *in_mem,
	struct se_mem *in_se_mem,
	struct se_mem **out_se_mem,
	u32 *se_mem_cnt,
	u32 *task_offset_in)
{
	struct se_cmd *cmd = task->task_se_cmd;
	struct rd_request *req = RD_REQ(task);
	u32 task_offset = *task_offset_in;
	unsigned long long lba;
	int ret;

	req->rd_page = ((task->task_lba * DEV_ATTRIB(task->se_dev)->block_size) /
			PAGE_SIZE);
	lba = task->task_lba;
	req->rd_offset = (do_div(lba,
			  (PAGE_SIZE / DEV_ATTRIB(task->se_dev)->block_size))) *
			   DEV_ATTRIB(task->se_dev)->block_size;
	req->rd_size = task->task_size;

	if (req->rd_offset)
		ret = rd_DIRECT_with_offset(task, se_mem_list, se_mem_cnt,
				task_offset_in);
	else
		ret = rd_DIRECT_without_offset(task, se_mem_list, se_mem_cnt,
				task_offset_in);

	if (ret < 0)
		return ret;

	if (CMD_TFO(cmd)->task_sg_chaining == 0)
		return 0;
	/*
	 * Currently prevent writers from multiple HW fabrics doing
	 * pci_map_sg() to RD_DR's internal scatterlist memory.
	 */
	if (cmd->data_direction == DMA_TO_DEVICE) {
		printk(KERN_ERR "DMA_TO_DEVICE not supported for"
				" RAMDISK_DR with task_sg_chaining=1\n");
		return -1;
=======
		rd_size -= len;
		if (!rd_size)
			continue;

		src_len -= len;
		if (src_len) {
			rd_offset += len;
			continue;
		}

		/* rd page completed, next one please */
		rd_page++;
		rd_offset = 0;
		src_len = PAGE_SIZE;
		if (rd_page <= table->page_end_offset) {
			rd_sg++;
			continue;
		}

		table = rd_get_sg_table(dev, rd_page);
		if (!table) {
			sg_miter_stop(&m);
			return TCM_LOGICAL_UNIT_COMMUNICATION_FAILURE;
		}

		/* since we increment, the first sg entry is correct */
		rd_sg = table->sg_table;
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD
	}
	sg_miter_stop(&m);

	target_complete_cmd(cmd, SAM_STAT_GOOD);
	return 0;
}

enum {
	Opt_rd_pages, Opt_err
};

static match_table_t tokens = {
	{Opt_rd_pages, "rd_pages=%d"},
	{Opt_err, NULL}
};

static ssize_t rd_set_configfs_dev_params(struct se_device *dev,
		const char *page, ssize_t count)
{
	struct rd_dev *rd_dev = RD_DEV(dev);
	char *orig, *ptr, *opts;
	substring_t args[MAX_OPT_ARGS];
	int ret = 0, arg, token;

	opts = kstrdup(page, GFP_KERNEL);
	if (!opts)
		return -ENOMEM;

	orig = opts;

	while ((ptr = strsep(&opts, ",\n")) != NULL) {
		if (!*ptr)
			continue;

		token = match_token(ptr, tokens, args);
		switch (token) {
		case Opt_rd_pages:
			match_int(args, &arg);
			rd_dev->rd_page_count = arg;
<<<<<<< HEAD
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "RAMDISK: Referencing Page"
=======
			pr_debug("RAMDISK: Referencing Page"
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD
				" Count: %u\n", rd_dev->rd_page_count);
#else
			;
#endif
			rd_dev->rd_flags |= RDF_HAS_PAGE_COUNT;
			break;
		default:
			break;
		}
	}

	kfree(orig);
	return (!ret) ? count : ret;
}

static ssize_t rd_show_configfs_dev_params(struct se_device *dev, char *b)
{
<<<<<<< HEAD
	struct rd_dev *rd_dev = se_dev->se_dev_su_ptr;

	if (!(rd_dev->rd_flags & RDF_HAS_PAGE_COUNT)) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "Missing rd_pages= parameter\n");
#else
		;
#endif
		return -1;
	}
=======
	struct rd_dev *rd_dev = RD_DEV(dev);
>>>>>>> 90aeaae... Merge branch 'lk-3.9' into HEAD

	ssize_t bl = sprintf(b, "TCM RamDisk ID: %u  RamDisk Makeup: rd_mcp\n",
			rd_dev->rd_dev_id);
	bl += sprintf(b + bl, "        PAGES/PAGE_SIZE: %u*%lu"
			"  SG_table_count: %u\n", rd_dev->rd_page_count,
			PAGE_SIZE, rd_dev->sg_table_count);
	return bl;
}

static sector_t rd_get_blocks(struct se_device *dev)
{
	struct rd_dev *rd_dev = RD_DEV(dev);

	unsigned long long blocks_long = ((rd_dev->rd_page_count * PAGE_SIZE) /
			dev->dev_attrib.block_size) - 1;

	return blocks_long;
}

static struct sbc_ops rd_sbc_ops = {
	.execute_rw		= rd_execute_rw,
};

static sense_reason_t
rd_parse_cdb(struct se_cmd *cmd)
{
	return sbc_parse_cdb(cmd, &rd_sbc_ops);
}

static struct se_subsystem_api rd_mcp_template = {
	.name			= "rd_mcp",
	.inquiry_prod		= "RAMDISK-MCP",
	.inquiry_rev		= RD_MCP_VERSION,
	.transport_type		= TRANSPORT_PLUGIN_VHBA_VDEV,
	.attach_hba		= rd_attach_hba,
	.detach_hba		= rd_detach_hba,
	.alloc_device		= rd_alloc_device,
	.configure_device	= rd_configure_device,
	.free_device		= rd_free_device,
	.parse_cdb		= rd_parse_cdb,
	.set_configfs_dev_params = rd_set_configfs_dev_params,
	.show_configfs_dev_params = rd_show_configfs_dev_params,
	.get_device_type	= sbc_get_device_type,
	.get_blocks		= rd_get_blocks,
};

int __init rd_module_init(void)
{
	int ret;

	ret = transport_subsystem_register(&rd_mcp_template);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

void rd_module_exit(void)
{
	transport_subsystem_release(&rd_mcp_template);
}

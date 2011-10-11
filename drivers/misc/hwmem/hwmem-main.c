/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Hardware memory driver, hwmem
 *
 * Author: Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>,
 * Johan Mossberg <johan.xx.mossberg@stericsson.com> for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/list.h>
#include <linux/hwmem.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <asm/sizes.h>
#include "cache_handler.h"

struct hwmem_alloc_threadg_info {
	struct list_head list;

	struct pid *threadg_pid; /* Ref counted */

	enum hwmem_access access;
};

struct hwmem_alloc {
	struct list_head list;

	atomic_t ref_cnt;
	enum hwmem_alloc_flags flags;
	u32 paddr;
	void *kaddr;
	u32 size;
	s32 name;

	/* Access control */
	enum hwmem_access default_access;
	struct list_head threadg_info_list;

	/* Cache handling */
	struct cach_buf cach_buf;
};

static struct platform_device *hwdev;

static u32 hwmem_paddr;
static u32 hwmem_size;

static LIST_HEAD(alloc_list);
static DEFINE_IDR(global_idr);
static DEFINE_MUTEX(lock);

static void vm_open(struct vm_area_struct *vma);
static void vm_close(struct vm_area_struct *vma);
static struct vm_operations_struct vm_ops = {
	.open = vm_open,
	.close = vm_close,
};

#ifdef CONFIG_DEBUG_FS

static int debugfs_allocs_read(struct file *filp, char __user *buf,
						size_t count, loff_t *f_pos);
static const struct file_operations debugfs_allocs_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_allocs_read,
};

#endif /* #ifdef CONFIG_DEBUG_FS */

static void clean_alloc_list(void);
static void kunmap_alloc(struct hwmem_alloc *alloc);

/* Helpers */

static u32 get_alloc_offset(struct hwmem_alloc *alloc)
{
	return alloc->paddr - hwmem_paddr;
}

static void destroy_hwmem_alloc_threadg_info(
		struct hwmem_alloc_threadg_info *info)
{
	if (info->threadg_pid)
		put_pid(info->threadg_pid);

	kfree(info);
}

static void clean_hwmem_alloc_threadg_info_list(struct hwmem_alloc *alloc)
{
	struct hwmem_alloc_threadg_info *info;
	struct hwmem_alloc_threadg_info *tmp;

	list_for_each_entry_safe(info, tmp, &(alloc->threadg_info_list), list) {
		list_del(&info->list);
		destroy_hwmem_alloc_threadg_info(info);
	}
}

static enum hwmem_access get_access(struct hwmem_alloc *alloc)
{
	struct hwmem_alloc_threadg_info *info;
	struct pid *my_pid;
	bool found = false;

	my_pid = find_get_pid(task_tgid_nr(current));
	if (!my_pid)
		return 0;

	list_for_each_entry(info, &(alloc->threadg_info_list), list) {
		if (info->threadg_pid == my_pid) {
			found = true;
			break;
		}
	}

	put_pid(my_pid);

	if (found)
		return info->access;
	else
		return alloc->default_access;
}

static void clear_alloc_mem(struct hwmem_alloc *alloc)
{
	cach_set_domain(&alloc->cach_buf, HWMEM_ACCESS_WRITE,
						HWMEM_DOMAIN_CPU, NULL);

	memset(alloc->kaddr, 0, alloc->size);
}

static void clean_alloc(struct hwmem_alloc *alloc)
{
	if (alloc->name) {
		idr_remove(&global_idr, alloc->name);
		alloc->name = 0;
	}

	alloc->flags = 0;

	clean_hwmem_alloc_threadg_info_list(alloc);

	kunmap_alloc(alloc);
}

static void destroy_alloc(struct hwmem_alloc *alloc)
{
	clean_alloc(alloc);

	kfree(alloc);
}

static void __hwmem_release(struct hwmem_alloc *alloc)
{
	struct hwmem_alloc *other;

	clean_alloc(alloc);

	other = list_entry(alloc->list.prev, struct hwmem_alloc, list);
	if ((alloc->list.prev != &alloc_list) &&
			atomic_read(&other->ref_cnt) == 0) {
		other->size += alloc->size;
		list_del(&alloc->list);
		destroy_alloc(alloc);
		alloc = other;
	}
	other = list_entry(alloc->list.next, struct hwmem_alloc, list);
	if ((alloc->list.next != &alloc_list) &&
			atomic_read(&other->ref_cnt) == 0) {
		alloc->size += other->size;
		list_del(&other->list);
		destroy_alloc(other);
	}
}

static struct hwmem_alloc *find_free_alloc_bestfit(u32 size)
{
	u32 best_diff = ~0;
	struct hwmem_alloc *alloc = NULL, *i;

	list_for_each_entry(i, &alloc_list, list) {
		u32 diff = i->size - size;
		if (atomic_read(&i->ref_cnt) > 0 || i->size < size)
			continue;
		if (diff < best_diff) {
			alloc = i;
			best_diff = diff;
		}
	}

	return alloc != NULL ? alloc : ERR_PTR(-ENOMEM);
}

static struct hwmem_alloc *split_allocation(struct hwmem_alloc *alloc,
							u32 new_alloc_size)
{
	struct hwmem_alloc *new_alloc;

	new_alloc = kzalloc(sizeof(struct hwmem_alloc), GFP_KERNEL);
	if (new_alloc == NULL)
		return ERR_PTR(-ENOMEM);

	atomic_inc(&new_alloc->ref_cnt);
	INIT_LIST_HEAD(&new_alloc->threadg_info_list);
	new_alloc->paddr = alloc->paddr;
	new_alloc->size = new_alloc_size;
	alloc->size -= new_alloc_size;
	alloc->paddr += new_alloc_size;

	list_add_tail(&new_alloc->list, &alloc->list);

	return new_alloc;
}

static int init_alloc_list(void)
{
	/*
	 * Hack to not get any allocs that cross a 64MiB boundary as B2R2 can't
	 * handle that.
	 */
	int ret;
	u32 curr_pos = hwmem_paddr;
	u32 hwmem_end = hwmem_paddr + hwmem_size;
	u32 next_64mib_boundary = (curr_pos + SZ_64M) & ~(SZ_64M - 1);
	struct hwmem_alloc *alloc;

	if (PAGE_SIZE >= SZ_64M) {
		dev_err(&hwdev->dev, "PAGE_SIZE >= SZ_64M\n");
		return -ENOMSG;
	}

	while (next_64mib_boundary < hwmem_end) {
		if (next_64mib_boundary - curr_pos > PAGE_SIZE) {
			alloc = kzalloc(sizeof(struct hwmem_alloc), GFP_KERNEL);
			if (alloc == NULL) {
				ret = -ENOMEM;
				goto error;
			}
			alloc->paddr = curr_pos;
			alloc->size = next_64mib_boundary - curr_pos -
								PAGE_SIZE;
			INIT_LIST_HEAD(&alloc->threadg_info_list);
			list_add_tail(&alloc->list, &alloc_list);
			curr_pos = alloc->paddr + alloc->size;
		}

		alloc = kzalloc(sizeof(struct hwmem_alloc), GFP_KERNEL);
		if (alloc == NULL) {
			ret = -ENOMEM;
			goto error;
		}
		alloc->paddr = curr_pos;
		alloc->size = PAGE_SIZE;
		atomic_inc(&alloc->ref_cnt);
		INIT_LIST_HEAD(&alloc->threadg_info_list);
		list_add_tail(&alloc->list, &alloc_list);
		curr_pos = alloc->paddr + alloc->size;

		next_64mib_boundary += SZ_64M;
	}

	alloc = kzalloc(sizeof(struct hwmem_alloc), GFP_KERNEL);
	if (alloc == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	alloc->paddr = curr_pos;
	alloc->size = hwmem_end - curr_pos;
	INIT_LIST_HEAD(&alloc->threadg_info_list);
	list_add_tail(&alloc->list, &alloc_list);

	return 0;

error:
	clean_alloc_list();

	return ret;
}

static void clean_alloc_list(void)
{
	while (list_empty(&alloc_list) == 0) {
		struct hwmem_alloc *i = list_first_entry(&alloc_list,
						struct hwmem_alloc, list);

		list_del(&i->list);

		destroy_alloc(i);
	}
}

static int kmap_alloc(struct hwmem_alloc *alloc)
{
	int ret;
	pgprot_t pgprot;

	struct vm_struct *area = get_vm_area(alloc->size, VM_IOREMAP);
	if (area == NULL) {
		dev_info(&hwdev->dev, "Failed to allocate %u bytes virtual"
						" memory", alloc->size);
		return -ENOMSG;
	}

	pgprot = PAGE_KERNEL;
	cach_set_pgprot_cache_options(&alloc->cach_buf, &pgprot);

	ret = ioremap_page_range((unsigned long)area->addr,
		(unsigned long)area->addr + alloc->size, alloc->paddr, pgprot);
	if (ret < 0) {
		dev_info(&hwdev->dev, "Failed to map %#x - %#x", alloc->paddr,
						alloc->paddr + alloc->size);
		goto failed_to_map;
	}

	alloc->kaddr = area->addr;

	return 0;

failed_to_map:
	area = remove_vm_area(area->addr);
	if (area == NULL)
		dev_err(&hwdev->dev,
				"Failed to unmap alloc, resource leak!\n");

	kfree(area);

	return ret;
}

static void kunmap_alloc(struct hwmem_alloc *alloc)
{
	struct vm_struct *area;

	if (alloc->kaddr == NULL)
		return;

	area = remove_vm_area(alloc->kaddr);
	if (area == NULL) {
		dev_err(&hwdev->dev,
				"Failed to unmap alloc, resource leak!\n");
		return;
	}

	kfree(area);

	alloc->kaddr = NULL;
}

/* HWMEM API */

struct hwmem_alloc *hwmem_alloc(u32 size, enum hwmem_alloc_flags flags,
		enum hwmem_access def_access, enum hwmem_mem_type mem_type)
{
	struct hwmem_alloc *alloc;
	int ret;

	if (!hwdev) {
		printk(KERN_ERR "hwmem: Badly configured\n");
		return ERR_PTR(-EINVAL);
	}

	if (size == 0)
		return ERR_PTR(-EINVAL);

	mutex_lock(&lock);

	size = PAGE_ALIGN(size);

	alloc = find_free_alloc_bestfit(size);
	if (IS_ERR(alloc)) {
		dev_info(&hwdev->dev, "Allocation failed, no free slot\n");
		goto no_slot;
	}

	if (size < alloc->size) {
		alloc = split_allocation(alloc, size);
		if (IS_ERR(alloc))
			goto split_alloc_failed;
	} else {
		atomic_inc(&alloc->ref_cnt);
	}

	alloc->flags = flags;
	alloc->default_access = def_access;
	cach_init_buf(&alloc->cach_buf, alloc->flags, alloc->size);
	ret = kmap_alloc(alloc);
	if (ret < 0)
		goto kmap_alloc_failed;
	cach_set_buf_addrs(&alloc->cach_buf, alloc->kaddr, alloc->paddr);

	clear_alloc_mem(alloc);

	goto out;

kmap_alloc_failed:
	__hwmem_release(alloc);
	alloc = ERR_PTR(ret);
split_alloc_failed:
no_slot:

out:
	mutex_unlock(&lock);

	return alloc;
}
EXPORT_SYMBOL(hwmem_alloc);

void hwmem_release(struct hwmem_alloc *alloc)
{
	mutex_lock(&lock);

	if (atomic_dec_and_test(&alloc->ref_cnt))
		__hwmem_release(alloc);

	mutex_unlock(&lock);
}
EXPORT_SYMBOL(hwmem_release);

int hwmem_set_domain(struct hwmem_alloc *alloc, enum hwmem_access access,
		enum hwmem_domain domain, struct hwmem_region *region)
{
	mutex_lock(&lock);

	cach_set_domain(&alloc->cach_buf, access, domain, region);

	mutex_unlock(&lock);

	return 0;
}
EXPORT_SYMBOL(hwmem_set_domain);

int hwmem_pin(struct hwmem_alloc *alloc, struct hwmem_mem_chunk *mem_chunks,
						u32 *mem_chunks_length)
{
	if (*mem_chunks_length < 1) {
		*mem_chunks_length = 1;
		return -ENOSPC;
	}

	mutex_lock(&lock);

	mem_chunks[0].paddr = alloc->paddr;
	mem_chunks[0].size = alloc->size;
	*mem_chunks_length = 1;

	mutex_unlock(&lock);

	return 0;
}
EXPORT_SYMBOL(hwmem_pin);

void hwmem_unpin(struct hwmem_alloc *alloc)
{
}
EXPORT_SYMBOL(hwmem_unpin);

static void vm_open(struct vm_area_struct *vma)
{
	atomic_inc(&((struct hwmem_alloc *)vma->vm_private_data)->ref_cnt);
}

static void vm_close(struct vm_area_struct *vma)
{
	hwmem_release((struct hwmem_alloc *)vma->vm_private_data);
}

int hwmem_mmap(struct hwmem_alloc *alloc, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long vma_size = vma->vm_end - vma->vm_start;
	enum hwmem_access access;
	mutex_lock(&lock);

	access = get_access(alloc);

	/* Check permissions */
	if ((!(access & HWMEM_ACCESS_WRITE) &&
				(vma->vm_flags & VM_WRITE)) ||
			(!(access & HWMEM_ACCESS_READ) &&
				(vma->vm_flags & VM_READ))) {
		ret = -EPERM;
		goto illegal_access;
	}

	if (vma_size > alloc->size) {
		ret = -EINVAL;
		goto illegal_size;
	}

	/*
	 * We don't want Linux to do anything (merging etc) with our VMAs as
	 * the offset is not necessarily valid
	 */
	vma->vm_flags |= VM_SPECIAL;
	cach_set_pgprot_cache_options(&alloc->cach_buf, &vma->vm_page_prot);
	vma->vm_private_data = (void *)alloc;
	atomic_inc(&alloc->ref_cnt);
	vma->vm_ops = &vm_ops;

	ret = remap_pfn_range(vma, vma->vm_start, alloc->paddr >> PAGE_SHIFT,
		min(vma_size, (unsigned long)alloc->size), vma->vm_page_prot);
	if (ret < 0)
		goto map_failed;

	goto out;

map_failed:
	atomic_dec(&alloc->ref_cnt);
illegal_size:
illegal_access:

out:
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(hwmem_mmap);

void *hwmem_kmap(struct hwmem_alloc *alloc)
{
	void *ret;

	mutex_lock(&lock);

	ret = alloc->kaddr;

	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(hwmem_kmap);

void hwmem_kunmap(struct hwmem_alloc *alloc)
{
}
EXPORT_SYMBOL(hwmem_kunmap);

int hwmem_set_access(struct hwmem_alloc *alloc,
		enum hwmem_access access, pid_t pid_nr)
{
	int ret;
	struct hwmem_alloc_threadg_info *info;
	struct pid *pid;
	bool found = false;

	pid = find_get_pid(pid_nr);
	if (!pid) {
		ret = -EINVAL;
		goto error_get_pid;
	}

	list_for_each_entry(info, &(alloc->threadg_info_list), list) {
		if (info->threadg_pid == pid) {
			found = true;
			break;
		}
	}

	if (!found) {
		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info) {
			ret = -ENOMEM;
			goto error_alloc_info;
		}

		info->threadg_pid = pid;
		info->access = access;

		list_add_tail(&(info->list), &(alloc->threadg_info_list));
	} else {
		info->access = access;
	}

	return 0;

error_alloc_info:
	put_pid(pid);
error_get_pid:
	return ret;
}
EXPORT_SYMBOL(hwmem_set_access);

void hwmem_get_info(struct hwmem_alloc *alloc, u32 *size,
	enum hwmem_mem_type *mem_type, enum hwmem_access *access)
{
	mutex_lock(&lock);

	if (size != NULL)
		*size = alloc->size;
	if (mem_type != NULL)
		*mem_type = HWMEM_MEM_CONTIGUOUS_SYS;
	if (access != NULL)
		*access = get_access(alloc);

	mutex_unlock(&lock);
}
EXPORT_SYMBOL(hwmem_get_info);

int hwmem_get_name(struct hwmem_alloc *alloc)
{
	int ret = 0, name;

	mutex_lock(&lock);

	if (alloc->name != 0) {
		ret = alloc->name;
		goto out;
	}

	while (true) {
		if (idr_pre_get(&global_idr, GFP_KERNEL) == 0) {
			ret = -ENOMEM;
			goto pre_get_id_failed;
		}

		ret = idr_get_new_above(&global_idr, alloc, 1, &name);
		if (ret == 0)
			break;
		else if (ret != -EAGAIN)
			goto get_id_failed;
	}

	alloc->name = name;

	ret = name;
	goto out;

get_id_failed:
pre_get_id_failed:

out:
	mutex_unlock(&lock);

	return ret;
}
EXPORT_SYMBOL(hwmem_get_name);

struct hwmem_alloc *hwmem_resolve_by_name(s32 name)
{
	struct hwmem_alloc *alloc;

	mutex_lock(&lock);

	alloc = idr_find(&global_idr, name);
	if (alloc == NULL) {
		alloc = ERR_PTR(-EINVAL);
		goto find_failed;
	}
	atomic_inc(&alloc->ref_cnt);

	goto out;

find_failed:

out:
	mutex_unlock(&lock);

	return alloc;
}
EXPORT_SYMBOL(hwmem_resolve_by_name);

/* Debug */

static int print_alloc(struct hwmem_alloc *alloc, char **buf, size_t buf_size)
{
	int ret;

	if (buf_size < 134)
		return -EINVAL;

	ret = sprintf(*buf, "paddr: %#10x\tsize: %10u\tref cnt: %2i\t"
				"name: %#10x\tflags: %#4x\t$ settings: %#4x\t"
				"def acc: %#3x\n", alloc->paddr, alloc->size,
				atomic_read(&alloc->ref_cnt), alloc->name,
				alloc->flags, alloc->cach_buf.cache_settings,
							alloc->default_access);
	if (ret < 0)
		return -ENOMSG;

	*buf += ret;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int debugfs_allocs_read(struct file *file, char __user *buf,
						size_t count, loff_t *f_pos)
{
	/*
	 * We assume the supplied buffer and PAGE_SIZE is large enough to hold
	 * information about at least one alloc, if not no data will be
	 * returned.
	 */

	int ret;
	struct hwmem_alloc *curr_alloc;
	char *local_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	char *local_buf_pos = local_buf;
	size_t available_space = min((size_t)PAGE_SIZE, count);
	/* private_data is intialized to NULL in open which I assume is 0. */
	u32 *curr_pos = (u32 *)&file->private_data;
	size_t bytes_read;

	if (local_buf == NULL)
		return -ENOMEM;

	mutex_lock(&lock);

	list_for_each_entry(curr_alloc, &alloc_list, list) {
		u32 alloc_offset = get_alloc_offset(curr_alloc);

		if (alloc_offset < *curr_pos)
			continue;

		ret = print_alloc(curr_alloc, &local_buf_pos, available_space -
					(size_t)(local_buf_pos - local_buf));
		if (ret == -EINVAL) /* No more room */
			break;
		else if (ret < 0)
			goto out;

		*curr_pos = alloc_offset + 1;
	}

	bytes_read = (size_t)(local_buf_pos - local_buf);

	ret = copy_to_user(buf, local_buf, bytes_read);
	if (ret < 0)
		goto out;

	ret = bytes_read;

out:
	kfree(local_buf);

	mutex_unlock(&lock);

	return ret;
}

static void init_debugfs(void)
{
	/* Hwmem is never unloaded so dropping the dentrys is ok. */
	struct dentry *debugfs_root_dir = debugfs_create_dir("hwmem", NULL);
	(void)debugfs_create_file("allocs", 0444, debugfs_root_dir, 0,
							&debugfs_allocs_fops);
}

#endif /* #ifdef CONFIG_DEBUG_FS */

/* Module */

extern int hwmem_ioctl_init(void);
extern void hwmem_ioctl_exit(void);

static int __devinit hwmem_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hwmem_platform_data *platform_data = pdev->dev.platform_data;

	if (sizeof(int) != 4 || sizeof(phys_addr_t) < 4 ||
				sizeof(void *) < 4 || sizeof(size_t) != 4) {
		dev_err(&pdev->dev, "sizeof(int) != 4 || sizeof(phys_addr_t)"
			" < 4 || sizeof(void *) < 4 || sizeof(size_t) !="
								" 4\n");
		return -ENOMSG;
	}

	if (hwdev || platform_data->size == 0 ||
		platform_data->start != PAGE_ALIGN(platform_data->start) ||
		platform_data->size != PAGE_ALIGN(platform_data->size)) {
		dev_err(&pdev->dev, "hwdev || platform_data->size == 0 ||"
					"platform_data->start !="
					" PAGE_ALIGN(platform_data->start) ||"
					"platform_data->size !="
					" PAGE_ALIGN(platform_data->size)\n");
		return -EINVAL;
	}

	hwdev = pdev;
	hwmem_paddr = platform_data->start;
	hwmem_size = platform_data->size;

	/*
	 * No need to flush the caches here. If we can keep track of the cache
	 * content then none of our memory will be in the caches, if we can't
	 * keep track of the cache content we always assume all our memory is
	 * in the caches.
	 */

	ret = init_alloc_list();
	if (ret < 0)
		goto init_alloc_list_failed;

	ret = hwmem_ioctl_init();
	if (ret)
		goto ioctl_init_failed;

#ifdef CONFIG_DEBUG_FS
	init_debugfs();
#endif

	dev_info(&pdev->dev, "Hwmem probed, device contains %#x bytes\n",
			hwmem_size);

	goto out;

ioctl_init_failed:
	clean_alloc_list();
init_alloc_list_failed:
	hwdev = NULL;

out:
	return ret;
}

static struct platform_driver hwmem_driver = {
	.probe	= hwmem_probe,
	.driver = {
		.name	= "hwmem",
	},
};

static int __init hwmem_init(void)
{
	return platform_driver_register(&hwmem_driver);
}
subsys_initcall(hwmem_init);

MODULE_AUTHOR("Marcus Lorentzon <marcus.xm.lorentzon@stericsson.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hardware memory driver");


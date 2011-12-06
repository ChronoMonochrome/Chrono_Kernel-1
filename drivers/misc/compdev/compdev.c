/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Display overlay compositer device driver
 *
 * Author: Anders Bauer <anders.bauer@stericsson.com>
 * for ST-Ericsson.
 *
 * Modified: Per-Daniel Olsson <per-daniel.olsson@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>

#include <linux/compdev.h>
#include <linux/hwmem.h>
#include <video/mcde_dss.h>

static LIST_HEAD(dev_list);
static DEFINE_MUTEX(dev_list_lock);

struct compdev_buffer {
	struct hwmem_alloc *alloc;
	enum compdev_ptr_type type;
	u32 size;
	u32 paddr; /* if pinned */
};

struct compdev {
	bool open;
	struct mutex lock;
	struct miscdevice mdev;
	struct list_head list;
	struct mcde_display_device *ddev;
	struct mcde_overlay *ovly[NUM_COMPDEV_BUFS];
	struct compdev_buffer ovly_buffer[NUM_COMPDEV_BUFS];
	struct compdev_size phy_size;
};

static int compdev_open(struct inode *inode, struct file *file)
{
	struct compdev *cd = NULL;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(cd, &dev_list, list)
		if (cd->mdev.minor == iminor(inode))
			break;

	if (&cd->list == &dev_list) {
		mutex_unlock(&dev_list_lock);
		return -ENODEV;
	}

	if (cd->open) {
		mutex_unlock(&dev_list_lock);
		return -EBUSY;
	}

	cd->open = true;

	mutex_unlock(&dev_list_lock);

	file->private_data = cd;

	return 0;
}

static int disable_overlay(struct mcde_overlay *ovly)
{
	struct mcde_overlay_info info;

	mcde_dss_get_overlay_info(ovly, &info);
	if (info.paddr != 0) {
		/* Set the pointer to zero to disable the overlay */
		info.paddr = 0;
		mcde_dss_apply_overlay(ovly, &info);
	}
	return 0;
}

static int compdev_release(struct inode *inode, struct file *file)
{
	struct compdev *cd = NULL;
	int i;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(cd, &dev_list, list)
		if (cd->mdev.minor == iminor(inode))
			break;
	mutex_unlock(&dev_list_lock);

	if (&cd->list == &dev_list)
		return -ENODEV;

	for (i = 0; i < NUM_COMPDEV_BUFS; i++) {
		disable_overlay(cd->ovly[i]);
		if (cd->ovly_buffer[i].paddr &&
				cd->ovly_buffer[i].type ==
				COMPDEV_PTR_HWMEM_BUF_NAME_OFFSET)
			hwmem_unpin(cd->ovly_buffer[i].alloc);

		cd->ovly_buffer[i].alloc = NULL;
		cd->ovly_buffer[i].size = 0;
		cd->ovly_buffer[i].paddr = 0;
	}

	cd->open = false;
	return 0;
}

static enum mcde_ovly_pix_fmt get_ovly_fmt(enum compdev_fmt fmt)
{
	switch (fmt) {
	default:
	case COMPDEV_FMT_RGB565:
		return MCDE_OVLYPIXFMT_RGB565;
	case COMPDEV_FMT_RGB888:
		return MCDE_OVLYPIXFMT_RGB888;
	case COMPDEV_FMT_RGBA8888:
		return MCDE_OVLYPIXFMT_RGBA8888;
	case COMPDEV_FMT_RGBX8888:
		return MCDE_OVLYPIXFMT_RGBX8888;
	case COMPDEV_FMT_YUV422:
		return MCDE_OVLYPIXFMT_YCbCr422;
	}
}

static int compdev_setup_ovly(struct compdev_img *img,
		struct compdev_buffer *buffer,
		struct mcde_overlay *ovly,
		int z_order,
		struct compdev *cd)
{
	int ret = 0;
	enum hwmem_mem_type memtype;
	enum hwmem_access access;
	struct hwmem_mem_chunk mem_chunk;
	size_t mem_chunk_length = 1;
	struct hwmem_region rgn = { .offset = 0, .count = 1, .start = 0 };
	struct mcde_overlay_info info;

	if (img->buf.type == COMPDEV_PTR_HWMEM_BUF_NAME_OFFSET) {
		buffer->type = COMPDEV_PTR_HWMEM_BUF_NAME_OFFSET;
		buffer->alloc = hwmem_resolve_by_name(img->buf.hwmem_buf_name);
		if (IS_ERR(buffer->alloc)) {
			ret = PTR_ERR(buffer->alloc);
			dev_warn(cd->mdev.this_device,
				"HWMEM resolve failed, %d\n", ret);
			goto resolve_failed;
		}

		hwmem_get_info(buffer->alloc, &buffer->size, &memtype,
				&access);

		if (!(access & HWMEM_ACCESS_READ) ||
				memtype != HWMEM_MEM_CONTIGUOUS_SYS) {
			ret = -EACCES;
			dev_warn(cd->mdev.this_device,
				"Invalid_mem overlay, %d\n", ret);
			goto invalid_mem;
		}
		ret = hwmem_pin(buffer->alloc, &mem_chunk, &mem_chunk_length);
		if (ret) {
			dev_warn(cd->mdev.this_device,
				"Pin failed, %d\n", ret);
			goto pin_failed;
		}

		rgn.size = rgn.end = buffer->size;
		ret = hwmem_set_domain(buffer->alloc, HWMEM_ACCESS_READ,
			HWMEM_DOMAIN_SYNC, &rgn);
		if (ret)
			dev_warn(cd->mdev.this_device,
				"Set domain failed, %d\n", ret);

		buffer->paddr = mem_chunk.paddr;
	} else if (img->buf.type == COMPDEV_PTR_PHYSICAL) {
		buffer->type = COMPDEV_PTR_PHYSICAL;
		buffer->alloc = NULL;
		buffer->size = img->buf.len;
		buffer->paddr = img->buf.offset;
	}

	info.stride = img->pitch;
	info.fmt = get_ovly_fmt(img->fmt);
	info.src_x = 0;
	info.src_y = 0;
	info.dst_x = img->dst_rect.x;
	info.dst_y = img->dst_rect.y;
	info.dst_z = z_order;
	info.w = img->dst_rect.width;
	info.h = img->dst_rect.height;
	info.dirty.x = 0;
	info.dirty.y = 0;
	info.dirty.w = cd->phy_size.width;
	info.dirty.h = cd->phy_size.height;
	info.paddr = buffer->paddr;
	mcde_dss_apply_overlay(ovly, &info);
	return ret;

pin_failed:
invalid_mem:
	buffer->alloc = NULL;
	buffer->size = 0;
	buffer->paddr = 0;

resolve_failed:
	return ret;
}

static int release_prev_frame(struct compdev *cd)
{
	int ret = 0;
	int i;

	/* Handle unpin of previous buffers */
	for (i = 0; i < NUM_COMPDEV_BUFS; i++) {
		if (cd->ovly_buffer[i].type ==
				COMPDEV_PTR_HWMEM_BUF_NAME_OFFSET &&
				cd->ovly_buffer[i].paddr != 0) {
			hwmem_unpin(cd->ovly_buffer[i].alloc);
			hwmem_release(cd->ovly_buffer[i].alloc);
		}
		cd->ovly_buffer[i].alloc = NULL;
		cd->ovly_buffer[i].size = 0;
		cd->ovly_buffer[i].paddr = 0;
	}
	return ret;

}

static void check_buffer(struct compdev *cd,
		struct compdev_buffer *overlay_buffer,
		struct compdev_buf *posted_buffer)
{
	if (overlay_buffer->type == COMPDEV_PTR_PHYSICAL &&
			posted_buffer->type == COMPDEV_PTR_PHYSICAL &&
			overlay_buffer->paddr == posted_buffer->offset &&
			overlay_buffer->paddr != 0)
		dev_warn(cd->mdev.this_device, "The same FB pointer!!\n");
}

static int compdev_post_buffers(struct compdev *cd,
		struct compdev_post_buffers_req *req)
{
	int ret = 0;
	int i, j;

	for (i = 0; i < NUM_COMPDEV_BUFS; i++)
		for (j = 0; j < NUM_COMPDEV_BUFS; j++)
			check_buffer(cd, &cd->ovly_buffer[i],
				&req->img_buffers[j].buf);

	/* Unpin the previous frame */
	release_prev_frame(cd);

	/* Validate buffer count */
	if (req->buffer_count > NUM_COMPDEV_BUFS || req->buffer_count == 0)	{
		dev_warn(cd->mdev.this_device,
			"Illegal buffer count, will be clamped to %d\n",
			NUM_COMPDEV_BUFS);
		req->buffer_count = NUM_COMPDEV_BUFS;
	}

	/* Handle buffers */
	for (i = 0; i < req->buffer_count; i++) {
		int overlay_index = req->buffer_count - i - 1;
		ret = compdev_setup_ovly(&req->img_buffers[i],
			&cd->ovly_buffer[i], cd->ovly[overlay_index], i, cd);
		if (ret)
			dev_warn(cd->mdev.this_device,
				"Failed to setup overlay[%d], %d\n", i, ret);
	}

	for (i = NUM_COMPDEV_BUFS; i > req->buffer_count; i--)
		disable_overlay(cd->ovly[i-1]);

	/* Do the display update */
	if (req->buffer_count > 0)
		mcde_dss_update_overlay(cd->ovly[0], false);
	else
		dev_warn(cd->mdev.this_device, "No overlays requested\n");
	return ret;
}

static long compdev_ioctl(struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	int ret;
	struct compdev *cd = (struct compdev *)file->private_data;
	struct compdev_post_buffers_req req;

	mutex_lock(&cd->lock);

	switch (cmd) {
	case COMPDEV_GET_SIZE_IOC:
		ret = copy_to_user((void __user *)arg, &cd->phy_size,
				sizeof(cd->phy_size));
		if (ret)
			ret = -EFAULT;
		break;
	case COMPDEV_POST_BUFFERS_IOC:
		/* arg is user pointer to struct compdev_post_buffers_req */

		/* Initialize the structure */
		memset(&req, 0, sizeof(req));

		/*
		 * The user request is a sub structure of the
		 * kernel request structure.
		 */

		/* Get the user data */
		if (copy_from_user(&req, (void *)arg, sizeof(req))) {
			dev_warn(cd->mdev.this_device,
				"%s: copy_from_user failed\n",
				__func__);
			mutex_unlock(&cd->lock);
			return -EFAULT;
		}

		ret = compdev_post_buffers(cd, &req);

		break;
	default:
		ret = -ENOSYS;
	}

	mutex_unlock(&cd->lock);

	return ret;
}

static const struct file_operations compdev_fops = {
	.open = compdev_open,
	.release = compdev_release,
	.unlocked_ioctl = compdev_ioctl,
};

static void init_compdev(struct compdev *cd, struct mcde_display_device *ddev,
							const char *name)
{
	mutex_init(&cd->lock);
	INIT_LIST_HEAD(&cd->list);
	cd->ddev = ddev;
	cd->mdev.minor = MISC_DYNAMIC_MINOR;
	cd->mdev.name = name;
	cd->mdev.fops = &compdev_fops;
}

int compdev_create(struct mcde_display_device *ddev,
		struct mcde_overlay *parent_ovly)
{
	int ret = 0;
	int i;
	struct compdev *cd;
	struct mcde_video_mode vmode;
	struct mcde_overlay_info info;

	static int counter;
	char name[10];

	cd = kzalloc(sizeof(struct compdev), GFP_KERNEL);
	if (!cd)
		return -ENOMEM;

	snprintf(name, sizeof(name), "%s%d", COMPDEV_DEFAULT_DEVICE_PREFIX,
		counter++);
	init_compdev(cd, ddev, name);
	mcde_dss_get_video_mode(ddev, &vmode);

	cd->ovly[0] = parent_ovly;
	if (!cd->ovly[0]) {
		ret = -ENOMEM;
		goto fail_create_ovly;
	}

	for (i = 1; i < NUM_COMPDEV_BUFS; i++) {
		cd->ovly[i] = mcde_dss_create_overlay(ddev, &info);
		if (!cd->ovly[i]) {
			ret = -ENOMEM;
			goto fail_create_ovly;
		}
		mcde_dss_enable_overlay(cd->ovly[i]);
		disable_overlay(cd->ovly[i]);
	}

	mcde_dss_get_native_resolution(ddev, &cd->phy_size.width,
			&cd->phy_size.height);

	ret = misc_register(&cd->mdev);
	if (ret)
		goto fail_register_misc;
	mutex_lock(&dev_list_lock);
	list_add_tail(&cd->list, &dev_list);
	mutex_unlock(&dev_list_lock);

	goto out;

fail_register_misc:
fail_create_ovly:
	for (i = 0; i < NUM_COMPDEV_BUFS; i++) {
		if (cd->ovly[i])
			mcde_dss_destroy_overlay(cd->ovly[i]);
	}
	kfree(cd);
out:
	return ret;
}

void compdev_destroy(struct mcde_display_device *ddev)
{
	struct compdev *cd;
	struct compdev *tmp;
	int i;

	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(cd, tmp, &dev_list, list) {
		if (cd->ddev == ddev) {
			list_del(&cd->list);
			misc_deregister(&cd->mdev);
			for (i = 0; i < NUM_COMPDEV_BUFS; i++)
				mcde_dss_destroy_overlay(cd->ovly[i]);
			kfree(cd);
			break;
		}
	}
	mutex_unlock(&dev_list_lock);
}

static void compdev_destroy_all(void)
{
	struct compdev *cd;
	struct compdev *tmp;
	int i;

	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(cd, tmp, &dev_list, list) {
		list_del(&cd->list);
		misc_deregister(&cd->mdev);
		for (i = 0; i < NUM_COMPDEV_BUFS; i++)
			mcde_dss_destroy_overlay(cd->ovly[i]);
		kfree(cd);
	}
	mutex_unlock(&dev_list_lock);

	mutex_destroy(&dev_list_lock);
}

static int __init compdev_init(void)
{
	pr_info("%s\n", __func__);

	mutex_init(&dev_list_lock);

	return 0;
}
module_init(compdev_init);

static void __exit compdev_exit(void)
{
	compdev_destroy_all();
	pr_info("%s\n", __func__);
}
module_exit(compdev_exit);

MODULE_AUTHOR("Anders Bauer <anders.bauer@stericsson.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Display overlay device driver");


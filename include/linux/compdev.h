/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * ST-Ericsson Display overlay compositer device driver
 *
 * Author: Anders Bauer <anders.bauer@stericsson.com>
 * for ST-Ericsson.
 *
 * Modified: Per-Daniel Olsson <per-daniel.olsson@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef _COMPDEV_H_
#define _COMPDEV_H_

#if !defined(__KERNEL__) && !defined(_KERNEL)
#include <stdint.h>
#else
#include <linux/types.h>
#include <video/mcde.h>
#endif

#if defined(__KERNEL__) || defined(_KERNEL)
#include <linux/mm_types.h>
#include <linux/bitops.h>
#else
#define BIT(nr)			(1UL << (nr))
#endif

#define COMPDEV_DEFAULT_DEVICE_PREFIX "comp"
#define NUM_COMPDEV_BUFS 2

enum compdev_fmt {
	COMPDEV_FMT_RGB565,
	COMPDEV_FMT_RGB888,
	COMPDEV_FMT_RGBX8888,
	COMPDEV_FMT_RGBA8888,
	COMPDEV_FMT_YUV422,
};

struct compdev_size {
	uint16_t width;
	uint16_t height;
};

/* Display rotation */
enum compdev_rotation {
	COMPDEV_ROT_0       = 0,
	COMPDEV_ROT_90_CCW  = 90,
	COMPDEV_ROT_180_CCW = 180,
	COMPDEV_ROT_270_CCW = 270,
	COMPDEV_ROT_90_CW   = COMPDEV_ROT_270_CCW,
	COMPDEV_ROT_180_CW  = COMPDEV_ROT_180_CCW,
	COMPDEV_ROT_270_CW  = COMPDEV_ROT_90_CCW,
};

enum compdev_ptr_type {
	COMPDEV_PTR_PHYSICAL,
	COMPDEV_PTR_HWMEM_BUF_NAME_OFFSET,
};

struct compdev_rect {
	__s32  x;
	__s32  y;
	__s32  width;
	__s32  height;
};

struct compdev_buf {
	enum compdev_ptr_type  type;
	__s32                  hwmem_buf_name;
	__s32                  fd;
	__u32                  offset;
	__u32                  len;
};

struct compdev_img {
	enum compdev_fmt     fmt;
	struct compdev_buf   buf;
	__s32                width;
	__s32                height;
	__u32                pitch;
	struct compdev_rect  dst_rect;
};

struct compdev_post_buffers_req {
	enum   compdev_rotation  rotation;
	struct compdev_img       img_buffers[NUM_COMPDEV_BUFS];
	__u8                     buffer_count;
};

#define COMPDEV_GET_SIZE_IOC       _IOR('D', 1, struct compdev_size)
#define COMPDEV_POST_BUFFERS_IOC   _IOW('D', 2, struct compdev_post_buffers_req)

#ifdef __KERNEL__

int compdev_create(struct mcde_display_device *ddev,
		struct mcde_overlay *parent_ovly);
void compdev_destroy(struct mcde_display_device *ddev);

#endif /* __KERNEL__ */

#endif /* _COMPDEV_H_ */


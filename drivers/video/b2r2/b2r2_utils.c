/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson B2R2 utils
 *
 * Author: Johan Mossberg <johan.xx.mossberg@stericsson.com> for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#include "b2r2_utils.h"

#include "b2r2_debug.h"

#include <video/b2r2_blt.h>

#include <linux/kernel.h>
#include <linux/errno.h>


const s32 b2r2_s32_max = 2147483647;


void b2r2_get_img_bounding_rect(struct b2r2_blt_img *img,
					struct b2r2_blt_rect *bounding_rect)
{
	bounding_rect->x = 0;
	bounding_rect->y = 0;
	bounding_rect->width = img->width;
	bounding_rect->height = img->height;
}


bool b2r2_is_zero_area_rect(struct b2r2_blt_rect *rect)
{
	return rect->width == 0 || rect->height == 0;
}

bool b2r2_is_rect_inside_rect(struct b2r2_blt_rect *rect1,
						struct b2r2_blt_rect *rect2)
{
	return rect1->x >= rect2->x &&
		rect1->y >= rect2->y &&
		rect1->x + rect1->width <= rect2->x + rect2->width &&
		rect1->y + rect1->height <= rect2->y + rect2->height;
}

void b2r2_intersect_rects(struct b2r2_blt_rect *rect1,
	struct b2r2_blt_rect *rect2, struct b2r2_blt_rect *intersection)
{
	struct b2r2_blt_rect tmp_rect;

	tmp_rect.x = max(rect1->x, rect2->x);
	tmp_rect.y = max(rect1->y, rect2->y);
	tmp_rect.width = min(rect1->x + rect1->width, rect2->x + rect2->width)
				- tmp_rect.x;
	if (tmp_rect.width < 0)
		tmp_rect.width = 0;
	tmp_rect.height =
		min(rect1->y + rect1->height, rect2->y + rect2->height) -
				tmp_rect.y;
	if (tmp_rect.height < 0)
		tmp_rect.height = 0;

	*intersection = tmp_rect;
}


int b2r2_get_fmt_bpp(enum b2r2_blt_fmt fmt)
{
	/*
	 * Currently this function is not used that often but if that changes a
	 * lookup table could make it a lot faster.
	 */
	switch (fmt) {
	case B2R2_BLT_FMT_1_BIT_A1:
		return 1;

	case B2R2_BLT_FMT_8_BIT_A8:
		return 8;

	case B2R2_BLT_FMT_YUV420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV420_PACKED_SEMIPLANAR_MB_STE:
		return 12;

	case B2R2_BLT_FMT_16_BIT_ARGB4444:
	case B2R2_BLT_FMT_16_BIT_ARGB1555:
	case B2R2_BLT_FMT_16_BIT_RGB565:
	case B2R2_BLT_FMT_Y_CB_Y_CR:
	case B2R2_BLT_FMT_CB_Y_CR_Y:
	case B2R2_BLT_FMT_YUV422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMIPLANAR_MB_STE:
		return 16;

	case B2R2_BLT_FMT_24_BIT_RGB888:
	case B2R2_BLT_FMT_24_BIT_ARGB8565:
	case B2R2_BLT_FMT_24_BIT_YUV888:
	case B2R2_BLT_FMT_24_BIT_VUY888:
	case B2R2_BLT_FMT_YUV444_PACKED_PLANAR:
		return 24;

	case B2R2_BLT_FMT_32_BIT_ARGB8888:
	case B2R2_BLT_FMT_32_BIT_ABGR8888:
	case B2R2_BLT_FMT_32_BIT_AYUV8888:
	case B2R2_BLT_FMT_32_BIT_VUYA8888:
		return 32;

	default:
		b2r2_log_err("%s: Internal error! Format %#x not recognized.\n",
			__func__, fmt);
		return 32;
	}
}

int b2r2_get_fmt_y_bpp(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_YUV420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV420_PACKED_SEMIPLANAR_MB_STE:
	case B2R2_BLT_FMT_Y_CB_Y_CR:
	case B2R2_BLT_FMT_CB_Y_CR_Y:
	case B2R2_BLT_FMT_YUV422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMIPLANAR_MB_STE:
	case B2R2_BLT_FMT_YUV444_PACKED_PLANAR:
	case B2R2_BLT_FMT_24_BIT_YUV888:
	case B2R2_BLT_FMT_32_BIT_AYUV8888:
	case B2R2_BLT_FMT_24_BIT_VUY888:
	case B2R2_BLT_FMT_32_BIT_VUYA8888:
		return 8;

	default:
		b2r2_log_err("%s: Internal error! Non YCbCr format supplied.\n",
			__func__);
		return 8;
	}
}


bool b2r2_is_single_plane_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_1_BIT_A1:
	case B2R2_BLT_FMT_8_BIT_A8:
	case B2R2_BLT_FMT_16_BIT_ARGB4444:
	case B2R2_BLT_FMT_16_BIT_ARGB1555:
	case B2R2_BLT_FMT_16_BIT_RGB565:
	case B2R2_BLT_FMT_24_BIT_RGB888:
	case B2R2_BLT_FMT_24_BIT_ARGB8565:
	case B2R2_BLT_FMT_24_BIT_YUV888:
	case B2R2_BLT_FMT_24_BIT_VUY888:
	case B2R2_BLT_FMT_32_BIT_ARGB8888:
	case B2R2_BLT_FMT_32_BIT_ABGR8888:
	case B2R2_BLT_FMT_32_BIT_AYUV8888:
	case B2R2_BLT_FMT_32_BIT_VUYA8888:
	case B2R2_BLT_FMT_Y_CB_Y_CR:
	case B2R2_BLT_FMT_CB_Y_CR_Y:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_independent_pixel_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_1_BIT_A1:
	case B2R2_BLT_FMT_8_BIT_A8:
	case B2R2_BLT_FMT_16_BIT_ARGB4444:
	case B2R2_BLT_FMT_16_BIT_ARGB1555:
	case B2R2_BLT_FMT_16_BIT_RGB565:
	case B2R2_BLT_FMT_24_BIT_RGB888:
	case B2R2_BLT_FMT_24_BIT_ARGB8565:
	case B2R2_BLT_FMT_24_BIT_YUV888:
	case B2R2_BLT_FMT_24_BIT_VUY888:
	case B2R2_BLT_FMT_32_BIT_ARGB8888:
	case B2R2_BLT_FMT_32_BIT_ABGR8888:
	case B2R2_BLT_FMT_32_BIT_AYUV8888:
	case B2R2_BLT_FMT_32_BIT_VUYA8888:
	case B2R2_BLT_FMT_YUV444_PACKED_PLANAR:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_ycbcri_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_Y_CB_Y_CR:
	case B2R2_BLT_FMT_CB_Y_CR_Y:
	case B2R2_BLT_FMT_24_BIT_YUV888:
	case B2R2_BLT_FMT_32_BIT_AYUV8888:
	case B2R2_BLT_FMT_24_BIT_VUY888:
	case B2R2_BLT_FMT_32_BIT_VUYA8888:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_ycbcrsp_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_YUV420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_SEMI_PLANAR:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_ycbcrp_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_YUV420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV444_PACKED_PLANAR:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_ycbcr420_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_YUV420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU420_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV420_PACKED_SEMIPLANAR_MB_STE:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_ycbcr422_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_Y_CB_Y_CR:
	case B2R2_BLT_FMT_CB_Y_CR_Y:
	case B2R2_BLT_FMT_YUV422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YVU422_PACKED_SEMI_PLANAR:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMIPLANAR_MB_STE:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_ycbcr444_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_YUV444_PACKED_PLANAR:
	case B2R2_BLT_FMT_32_BIT_AYUV8888:
	case B2R2_BLT_FMT_24_BIT_YUV888:
	case B2R2_BLT_FMT_32_BIT_VUYA8888:
	case B2R2_BLT_FMT_24_BIT_VUY888:
		return true;

	default:
		return false;
	}
}

bool b2r2_is_mb_fmt(enum b2r2_blt_fmt fmt)
{
	switch (fmt) {
	case B2R2_BLT_FMT_YUV420_PACKED_SEMIPLANAR_MB_STE:
	case B2R2_BLT_FMT_YUV422_PACKED_SEMIPLANAR_MB_STE:
		return true;

	default:
		return false;
	}
}

u32 b2r2_calc_pitch_from_width(s32 width, enum b2r2_blt_fmt fmt)
{
	if (b2r2_is_single_plane_fmt(fmt)) {
		return (u32)b2r2_div_round_up(width *
				b2r2_get_fmt_bpp(fmt), 8);
	} else if (b2r2_is_ycbcrsp_fmt(fmt) || b2r2_is_ycbcrp_fmt(fmt)) {
		return (u32)b2r2_div_round_up(width *
				b2r2_get_fmt_y_bpp(fmt), 8);
	} else {
		b2r2_log_err("%s: Internal error! "
				"Pitchless format supplied.\n",
				__func__);
		return 0;
	}
}

u32 b2r2_get_img_pitch(struct b2r2_blt_img *img)
{
	if (img->pitch != 0)
		return img->pitch;
	else
		return b2r2_calc_pitch_from_width(img->width, img->fmt);
}

s32 b2r2_get_img_size(struct b2r2_blt_img *img)
{
	if (b2r2_is_single_plane_fmt(img->fmt)) {
		return (s32)b2r2_get_img_pitch(img) * img->height;
	} else if (b2r2_is_ycbcrsp_fmt(img->fmt) ||
			b2r2_is_ycbcrp_fmt(img->fmt)) {
		s32 y_plane_size;

		y_plane_size = (s32)b2r2_get_img_pitch(img) * img->height;

		if (b2r2_is_ycbcr420_fmt(img->fmt)) {
			return y_plane_size + y_plane_size / 2;
		} else if (b2r2_is_ycbcr422_fmt(img->fmt)) {
			return y_plane_size * 2;
		} else if (b2r2_is_ycbcr444_fmt(img->fmt)) {
			return y_plane_size * 3;
		} else {
			b2r2_log_err("%s: Internal error! "
					"Format %#x not recognized.\n",
					__func__, img->fmt);
			return 0;
		}
	} else if (b2r2_is_mb_fmt(img->fmt)) {
		return (img->width * img->height *
				b2r2_get_fmt_bpp(img->fmt)) / 8;
	} else {
		b2r2_log_err("%s: Internal error! "
				"Format %#x not recognized.\n",
				__func__, img->fmt);
		return 0;
	}
}


s32 b2r2_div_round_up(s32 dividend, s32 divisor)
{
	s32 quotient = dividend / divisor;
	if (dividend % divisor != 0)
		quotient++;

	return quotient;
}

bool b2r2_is_aligned(s32 value, s32 alignment)
{
	return value % alignment == 0;
}

s32 b2r2_align_up(s32 value, s32 alignment)
{
	s32 remainder = abs(value) % abs(alignment);
	s32 value_to_add;

	if (remainder > 0) {
		if (value >= 0)
			value_to_add = alignment - remainder;
		else
			value_to_add = remainder;
	} else {
		value_to_add = 0;
	}

	return value + value_to_add;
}

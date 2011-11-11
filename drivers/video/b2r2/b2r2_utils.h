/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson B2R2 utils
 *
 * Author: Johan Mossberg <johan.xx.mossberg@stericsson.com> for ST-Ericsson
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef _LINUX_DRIVERS_VIDEO_B2R2_UTILS_H_
#define _LINUX_DRIVERS_VIDEO_B2R2_UTILS_H_

#include <video/b2r2_blt.h>

#include "b2r2_internal.h"

extern const s32 b2r2_s32_max;

int calculate_scale_factor(struct b2r2_control *cont,
		u32 from, u32 to, u16 *sf_out);
void b2r2_get_img_bounding_rect(struct b2r2_blt_img *img,
		struct b2r2_blt_rect *bounding_rect);

bool b2r2_is_zero_area_rect(struct b2r2_blt_rect *rect);
bool b2r2_is_rect_inside_rect(struct b2r2_blt_rect *rect1,
		struct b2r2_blt_rect *rect2);
bool b2r2_is_rect_gte_rect(struct b2r2_blt_rect *rect1,
		struct b2r2_blt_rect *rect2);
void b2r2_intersect_rects(struct b2r2_blt_rect *rect1,
		struct b2r2_blt_rect *rect2,
		struct b2r2_blt_rect *intersection);
void b2r2_trim_rects(struct b2r2_control *cont,
			const struct b2r2_blt_req *req,
			struct b2r2_blt_rect *new_bg_rect,
			struct b2r2_blt_rect *new_dst_rect,
			struct b2r2_blt_rect *new_src_rect);

int b2r2_get_fmt_bpp(struct b2r2_control *cont, enum b2r2_blt_fmt fmt);
int b2r2_get_fmt_y_bpp(struct b2r2_control *cont, enum b2r2_blt_fmt fmt);

bool b2r2_is_single_plane_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_independent_pixel_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_ycbcri_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_ycbcrsp_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_ycbcrp_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_ycbcr420_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_ycbcr422_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_ycbcr444_fmt(enum b2r2_blt_fmt fmt);
bool b2r2_is_mb_fmt(enum b2r2_blt_fmt fmt);

/*
 * Rounds up if an invalid width causes the pitch to be non byte aligned.
 */
u32 b2r2_calc_pitch_from_width(struct b2r2_control *cont,
		s32 width, enum b2r2_blt_fmt fmt);
u32 b2r2_get_img_pitch(struct b2r2_control *cont,
		struct b2r2_blt_img *img);
s32 b2r2_get_img_size(struct b2r2_control *cont,
		struct b2r2_blt_img *img);

s32 b2r2_div_round_up(s32 dividend, s32 divisor);
bool b2r2_is_aligned(s32 value, s32 alignment);
s32 b2r2_align_up(s32 value, s32 alignment);

#endif

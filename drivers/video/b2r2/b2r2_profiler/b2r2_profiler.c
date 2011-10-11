/*
 * Copyright (C) ST-Ericsson AB 2010
 *
 * ST-Ericsson B2R2 profiler implementation
 *
 * Author: Johan Mossberg <johan.xx.mossberg@stericsson.com>
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/jiffies.h>

#include <video/b2r2_blt.h>
#include "../b2r2_profiler_api.h"


#define S32_MAX 2147483647


static int src_format_filter_on = false;
module_param(src_format_filter_on, bool, S_IRUGO | S_IWUSR);
static unsigned int src_format_filter;
module_param(src_format_filter, uint, S_IRUGO | S_IWUSR);

static int print_blts_on = 0;
module_param(print_blts_on, bool, S_IRUGO | S_IWUSR);
static int use_mpix_per_second_in_print_blts = 1;
module_param(use_mpix_per_second_in_print_blts, bool, S_IRUGO | S_IWUSR);

static int min_avg_max_mpix_per_second_on = 1;
module_param(min_avg_max_mpix_per_second_on, bool, S_IRUGO | S_IWUSR);

static const unsigned int min_avg_max_mpix_per_second_num_blts_used = 400;
static struct {
	unsigned long sampling_start_time_jiffies;

	s32 min_mpix_per_second;
	struct b2r2_blt_req min_blt_request;
	struct b2r2_blt_profiling_info min_blt_profiling_info;

	s32 max_mpix_per_second;
	struct b2r2_blt_req max_blt_request;
	struct b2r2_blt_profiling_info max_blt_profiling_info;

	s32 accumulated_num_pixels;
	s32 accumulated_num_usecs;

	u32 num_blts_done;
} min_avg_max_mpix_per_second_state;


static s32 nsec_2_usec(const s32 nsec);

static int is_scale_blt(const struct b2r2_blt_req * const request);
static s32 get_blt_mpix_per_second(const struct b2r2_blt_req * const request, const struct b2r2_blt_profiling_info * const blt_profiling_info);
static void print_blt(const struct b2r2_blt_req * const request, const struct b2r2_blt_profiling_info * const blt_profiling_info);

static s32 get_num_pixels_in_blt(const struct b2r2_blt_req * const request);
static s32 get_mpix_per_second(const s32 num_pixels, const s32 num_usecs);
static void print_min_avg_max_mpix_per_second_state(void);
static void reset_min_avg_max_mpix_per_second_state(void);
static void do_min_avg_max_mpix_per_second(const struct b2r2_blt_req * const request, const struct b2r2_blt_profiling_info * const blt_profiling_info);

static void blt_done(const struct b2r2_blt_req * const blt, const s32 request_id, const struct b2r2_blt_profiling_info * const blt_profiling_info);


static struct b2r2_profiler this = {
	.blt_done = blt_done,
};


static s32 nsec_2_usec(const s32 nsec)
{
	return nsec / 1000;
}


static int is_scale_blt(const struct b2r2_blt_req * const request)
{
	if ((request->transform & B2R2_BLT_TRANSFORM_CCW_ROT_90 &&
		(request->src_rect.width != request->dst_rect.height ||
		request->src_rect.height != request->dst_rect.width)) ||
		(!(request->transform & B2R2_BLT_TRANSFORM_CCW_ROT_90) &&
		(request->src_rect.width != request->dst_rect.width ||
		request->src_rect.height != request->dst_rect.height)))
		return 1;
	else
		return 0;
}

static s32 get_blt_mpix_per_second(const struct b2r2_blt_req * const request, const struct b2r2_blt_profiling_info * const blt_profiling_info)
{
	return get_mpix_per_second(get_num_pixels_in_blt(request),
		nsec_2_usec(blt_profiling_info->nsec_active_in_cpu + blt_profiling_info->nsec_active_in_b2r2));
}

static void print_blt(const struct b2r2_blt_req * const request, const struct b2r2_blt_profiling_info * const blt_profiling_info)
{
	char tmp_str[128];
	sprintf(tmp_str, "SF: %#10x, DF: %#10x, F: %#10x, T: %#3x, S: %1i, P: %7i",
		request->src_img.fmt,
		request->dst_img.fmt,
		request->flags,
		request->transform,
		is_scale_blt(request),
		get_num_pixels_in_blt(request));
	if (use_mpix_per_second_in_print_blts)
		printk(KERN_ALERT "%s, MPix/s: %3i\n",
			tmp_str,
			get_blt_mpix_per_second(request, blt_profiling_info));
	else
		printk(KERN_ALERT "%s, CPU: %10i, B2R2: %10i, Tot: %10i ns\n",
			tmp_str,
			blt_profiling_info->nsec_active_in_cpu,
			blt_profiling_info->nsec_active_in_b2r2,
			blt_profiling_info->total_time_nsec);
}


static s32 get_num_pixels_in_blt(const struct b2r2_blt_req * const request)
{
	s32 num_pixels_in_src = request->src_rect.width * request->src_rect.height;
	s32 num_pixels_in_dst = request->dst_rect.width * request->dst_rect.height;
	if (request->flags & (B2R2_BLT_FLAG_SOURCE_FILL |
				B2R2_BLT_FLAG_SOURCE_FILL_RAW))
		return num_pixels_in_dst;
	else
		return (num_pixels_in_src + num_pixels_in_dst) / 2;
}

static s32 get_mpix_per_second(const s32 num_pixels, const s32 num_usecs)
{
	s32 num_pixels_scale_factor = num_pixels != 0 ? S32_MAX / num_pixels : S32_MAX;
	s32 num_usecs_scale_factor = num_usecs != 0 ? S32_MAX / num_usecs : S32_MAX;
	s32 scale_factor = min(num_pixels_scale_factor, num_usecs_scale_factor);

	s32 num_pixels_scaled = num_pixels * scale_factor;
	s32 num_usecs_scaled = num_usecs * scale_factor;

	if (num_usecs_scaled < 1000000)
		return 0;

	return (num_pixels_scaled / 1000000) / (num_usecs_scaled / 1000000);
}

static void print_min_avg_max_mpix_per_second_state(void)
{
	printk(KERN_ALERT "Min: %3i, Avg: %3i, Max: %3i MPix/s\n",
		min_avg_max_mpix_per_second_state.min_mpix_per_second,
		get_mpix_per_second(min_avg_max_mpix_per_second_state.accumulated_num_pixels,
		min_avg_max_mpix_per_second_state.accumulated_num_usecs),
		min_avg_max_mpix_per_second_state.max_mpix_per_second);
	printk(KERN_ALERT "Min blit:\n");
	print_blt(&min_avg_max_mpix_per_second_state.min_blt_request,
		&min_avg_max_mpix_per_second_state.min_blt_profiling_info);
	printk(KERN_ALERT "Max blit:\n");
	print_blt(&min_avg_max_mpix_per_second_state.max_blt_request,
		&min_avg_max_mpix_per_second_state.max_blt_profiling_info);
}

static void reset_min_avg_max_mpix_per_second_state(void)
{
	min_avg_max_mpix_per_second_state.sampling_start_time_jiffies =
		jiffies;
	min_avg_max_mpix_per_second_state.min_mpix_per_second = S32_MAX;
	min_avg_max_mpix_per_second_state.max_mpix_per_second = 0;
	min_avg_max_mpix_per_second_state.accumulated_num_pixels = 0;
	min_avg_max_mpix_per_second_state.accumulated_num_usecs = 0;
	min_avg_max_mpix_per_second_state.num_blts_done = 0;
}

static void do_min_avg_max_mpix_per_second(const struct b2r2_blt_req * const request, const struct b2r2_blt_profiling_info * const blt_profiling_info)
{
	s32 num_pixels_in_blt;
	s32 num_usec_blt_took;
	s32 blt_mpix_per_second;

	if (time_before(jiffies, min_avg_max_mpix_per_second_state.sampling_start_time_jiffies))
		return;

	num_pixels_in_blt = get_num_pixels_in_blt(request);
	num_usec_blt_took = nsec_2_usec(blt_profiling_info->nsec_active_in_cpu + blt_profiling_info->nsec_active_in_b2r2);
	blt_mpix_per_second = get_mpix_per_second(num_pixels_in_blt,
		num_usec_blt_took);

	if (blt_mpix_per_second <= min_avg_max_mpix_per_second_state.min_mpix_per_second) {
		min_avg_max_mpix_per_second_state.min_mpix_per_second =
			blt_mpix_per_second;
		memcpy(&min_avg_max_mpix_per_second_state.min_blt_request,
			request, sizeof(struct b2r2_blt_req));
		memcpy(&min_avg_max_mpix_per_second_state.min_blt_profiling_info,
			blt_profiling_info, sizeof(struct b2r2_blt_profiling_info));
	}

	if (blt_mpix_per_second >= min_avg_max_mpix_per_second_state.max_mpix_per_second) {
		min_avg_max_mpix_per_second_state.max_mpix_per_second =
			blt_mpix_per_second;
		memcpy(&min_avg_max_mpix_per_second_state.max_blt_request,
			request, sizeof(struct b2r2_blt_req));
		memcpy(&min_avg_max_mpix_per_second_state.max_blt_profiling_info,
			blt_profiling_info, sizeof(struct b2r2_blt_profiling_info));
	}

	min_avg_max_mpix_per_second_state.accumulated_num_pixels +=
		num_pixels_in_blt;
	min_avg_max_mpix_per_second_state.accumulated_num_usecs +=
		num_usec_blt_took;

	min_avg_max_mpix_per_second_state.num_blts_done++;

	if (min_avg_max_mpix_per_second_state.num_blts_done >= min_avg_max_mpix_per_second_num_blts_used) {
		print_min_avg_max_mpix_per_second_state();
		reset_min_avg_max_mpix_per_second_state();
		/* The printouts initiated above can disturb the next measurement
		so we delay it two seconds to give the printouts a chance to finish. */
		min_avg_max_mpix_per_second_state.sampling_start_time_jiffies =
			jiffies + (2 * HZ);
	}
}

static void blt_done(const struct b2r2_blt_req * const request, const s32 request_id, const struct b2r2_blt_profiling_info * const blt_profiling_info)
{
	/* Filters */
	if (src_format_filter_on && request->src_img.fmt != src_format_filter)
		return;

	/* Processors */
	if (print_blts_on)
		print_blt(request, blt_profiling_info);

	if (min_avg_max_mpix_per_second_on)
		do_min_avg_max_mpix_per_second(request, blt_profiling_info);
}


static int __init b2r2_profiler_init(void)
{
	reset_min_avg_max_mpix_per_second_state();

	return b2r2_register_profiler(&this);
}
module_init(b2r2_profiler_init);

static void __exit b2r2_profiler_exit(void)
{
	b2r2_unregister_profiler(&this);
}
module_exit(b2r2_profiler_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johan Mossberg (johan.xx.mossberg@stericsson.com)");
MODULE_DESCRIPTION("B2R2 Profiler");

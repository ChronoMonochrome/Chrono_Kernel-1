/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson B2R2 Blitter module
 *
 * Author: Robert Fekete <robert.fekete@stericsson.com>
 * Author: Paul Wannback
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/fb.h>
#include <linux/uaccess.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <asm/cacheflush.h>
#include <linux/smp.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/hwmem.h>

#include "b2r2_internal.h"
#include "b2r2_node_split.h"
#include "b2r2_generic.h"
#include "b2r2_mem_alloc.h"
#include "b2r2_profiler_socket.h"
#include "b2r2_timing.h"
#include "b2r2_debug.h"
#include "b2r2_utils.h"
#include "b2r2_input_validation.h"

#define B2R2_HEAP_SIZE (4 * PAGE_SIZE)
#define MAX_TMP_BUF_SIZE (128 * PAGE_SIZE)

/*
 * TODO:
 * Implementation of query cap
 * Support for user space virtual pointer to physically consecutive memory
 * Support for user space virtual pointer to physically scattered memory
 * Callback reads lagging behind in blt_api_stress app
 * Store smaller items in the report list instead of the whole request
 * Support read of many report records at once.
 */

/**
 * b2r2_blt_dev - Our device, /dev/b2r2_blt
 */
static struct miscdevice *b2r2_blt_dev;

static struct {
	struct b2r2_work_buf buf;
	bool in_use;
} tmp_bufs[MAX_TMP_BUFS_NEEDED];

/* Statistics */

/**
 * stat_lock - Spin lock protecting the statistics
 */
static struct mutex   stat_lock;
/**
 * stat_n_jobs_added - Number of jobs added to b2r2_core
 */
static unsigned long stat_n_jobs_added;
/**
 * stat_n_jobs_released - Number of jobs released (job_release called)
 */
static unsigned long stat_n_jobs_released;
/**
 * stat_n_jobs_in_report_list - Number of jobs currently in the report list
 */
static unsigned long stat_n_jobs_in_report_list;
/**
 * stat_n_in_blt - Number of client threads currently exec inside b2r2_blt()
 */
static unsigned long stat_n_in_blt;
/**
 * stat_n_in_blt_synch - Nunmber of client threads currently waiting for synch
 */
static unsigned long stat_n_in_blt_synch;
/**
 * stat_n_in_blt_add - Number of client threads currenlty adding in b2r2_blt
 */
static unsigned long stat_n_in_blt_add;
/**
 * stat_n_in_blt_wait - Number of client threads currently waiting in b2r2_blt
 */
static unsigned long stat_n_in_blt_wait;
/**
 * stat_n_in_sync_0 - Number of client threads currently in b2r2_blt_sync
 *                    waiting for all client jobs to finish
 */
static unsigned long stat_n_in_synch_0;
/**
 * stat_n_in_sync_job - Number of client threads currently in b2r2_blt_sync
 *                      waiting specific job to finish
 */
static unsigned long stat_n_in_synch_job;
/**
 * stat_n_in_query_cap - Number of clients currently in query cap
 */
static unsigned long stat_n_in_query_cap;
/**
 * stat_n_in_open - Number of clients currently in b2r2_blt_open
 */
static unsigned long stat_n_in_open;
/**
 * stat_n_in_release - Number of clients currently in b2r2_blt_release
 */
static unsigned long stat_n_in_release;

/* Debug file system support */
#ifdef CONFIG_DEBUG_FS
/**
 * debugfs_latest_request - Copy of the latest request issued
 */
struct b2r2_blt_request debugfs_latest_request;
/**
 * debugfs_root_dir - The debugfs root directory, i.e. /debugfs/b2r2
 */
static struct dentry *debugfs_root_dir;

static int sprintf_req(struct b2r2_blt_request *request, char *buf, int size);
#endif

/* Local functions */
static void inc_stat(unsigned long *stat);
static void dec_stat(unsigned long *stat);
static int b2r2_blt_synch(struct b2r2_blt_instance *instance,
			int request_id);
static int b2r2_blt_query_cap(struct b2r2_blt_instance *instance,
			struct b2r2_blt_query_cap *query_cap);

#ifndef CONFIG_B2R2_GENERIC_ONLY
static int b2r2_blt(struct b2r2_blt_instance *instance,
		struct b2r2_blt_request *request);

static void job_callback(struct b2r2_core_job *job);
static void job_release(struct b2r2_core_job *job);
static int job_acquire_resources(struct b2r2_core_job *job, bool atomic);
static void job_release_resources(struct b2r2_core_job *job, bool atomic);
#endif

#ifdef CONFIG_B2R2_GENERIC
static int b2r2_generic_blt(struct b2r2_blt_instance *instance,
		struct b2r2_blt_request *request);

static void job_callback_gen(struct b2r2_core_job *job);
static void job_release_gen(struct b2r2_core_job *job);
static int job_acquire_resources_gen(struct b2r2_core_job *job, bool atomic);
static void job_release_resources_gen(struct b2r2_core_job *job, bool atomic);
static void tile_job_callback_gen(struct b2r2_core_job *job);
static void tile_job_release_gen(struct b2r2_core_job *job);
#endif


static int resolve_buf(struct b2r2_blt_img *img,
			struct b2r2_blt_rect *rect_2b_used,
			bool is_dst,
		struct b2r2_resolved_buf *resolved);
static void unresolve_buf(struct b2r2_blt_buf *buf,
			struct b2r2_resolved_buf *resolved);
static void sync_buf(struct b2r2_blt_img *img,
		struct b2r2_resolved_buf *resolved,
		bool is_dst,
		struct b2r2_blt_rect *rect);
static bool is_report_list_empty(struct b2r2_blt_instance *instance);
static bool is_synching(struct b2r2_blt_instance *instance);
static void get_actual_dst_rect(struct b2r2_blt_req *req,
					struct b2r2_blt_rect *actual_dst_rect);
static void set_up_hwmem_region(struct b2r2_blt_img *img,
		struct b2r2_blt_rect *rect, struct hwmem_region *region);
static int resolve_hwmem(struct b2r2_blt_img *img,
			struct b2r2_blt_rect *rect_2b_used, bool is_dst,
				struct b2r2_resolved_buf *resolved_buf);
static void unresolve_hwmem(struct b2r2_resolved_buf *resolved_buf);

/**
 * struct sync_args - Data for clean/flush
 *
 * @start: Virtual start address
 * @end: Virtual end address
 */
struct sync_args {
	unsigned long start;
	unsigned long end;
};
/**
 * flush_l1_cache_range_curr_cpu() - Cleans and invalidates L1 cache on the current CPU
 *
 * @arg: Pointer to sync_args structure
 */
static inline void flush_l1_cache_range_curr_cpu(void *arg)
{
	struct sync_args *sa = (struct sync_args *)arg;

	dmac_flush_range((void *)sa->start, (void *)sa->end);
}

#ifdef CONFIG_SMP
/**
 * inv_l1_cache_range_all_cpus() - Cleans and invalidates L1 cache on all CPU:s
 *
 * @sa: Pointer to sync_args structure
 */
static void flush_l1_cache_range_all_cpus(struct sync_args *sa)
{
	on_each_cpu(flush_l1_cache_range_curr_cpu, sa, 1);
}
#endif

/**
 * clean_l1_cache_range_curr_cpu() - Cleans L1 cache on current CPU
 *
 * Ensures that data is written out from the CPU:s L1 cache,
 * it will still be in the cache.
 *
 * @arg: Pointer to sync_args structure
 */
static inline void clean_l1_cache_range_curr_cpu(void *arg)
{
	struct sync_args *sa = (struct sync_args *)arg;

	dmac_map_area((void *)sa->start,
		(void *)sa->end - (void *)sa->start,
		DMA_TO_DEVICE);
}

#ifdef CONFIG_SMP
/**
 * clean_l1_cache_range_all_cpus() - Cleans L1 cache on all CPU:s
 *
 * Ensures that data is written out from all CPU:s L1 cache,
 * it will still be in the cache.
 *
 * @sa: Pointer to sync_args structure
 */
static void clean_l1_cache_range_all_cpus(struct sync_args *sa)
{
	on_each_cpu(clean_l1_cache_range_curr_cpu, sa, 1);
}
#endif

/**
 * b2r2_blt_open - Implements file open on the b2r2_blt device
 *
 * @inode: File system inode
 * @filp: File pointer
 *
 * A B2R2 BLT instance is created and stored in the file structure.
 */
static int b2r2_blt_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct b2r2_blt_instance *instance;

	b2r2_log_info("%s\n", __func__);

	inc_stat(&stat_n_in_open);

	/* Allocate and initialize the instance */
	instance = (struct b2r2_blt_instance *)
		kmalloc(sizeof(*instance), GFP_KERNEL);
	if (!instance) {
		b2r2_log_err("%s: Failed to alloc\n", __func__);
		goto instance_alloc_failed;
	}
	memset(instance, 0, sizeof(*instance));
	INIT_LIST_HEAD(&instance->report_list);
	mutex_init(&instance->lock);
	init_waitqueue_head(&instance->report_list_waitq);
	init_waitqueue_head(&instance->synch_done_waitq);

	/*
	 * Remember the instance so that we can retrieve it in
	 * other functions
	 */
	filp->private_data = instance;
	goto out;

instance_alloc_failed:
out:
	dec_stat(&stat_n_in_open);

	return ret;
}

/**
 * b2r2_blt_release - Implements last close on an instance of
 *                    the b2r2_blt device
 *
 * @inode: File system inode
 * @filp: File pointer
 *
 * All active jobs are finished or cancelled and allocated data
 * is released.
 */
static int b2r2_blt_release(struct inode *inode, struct file *filp)
{
	int ret;
	struct b2r2_blt_instance *instance;

	b2r2_log_info("%s\n", __func__);

	inc_stat(&stat_n_in_release);

	instance = (struct b2r2_blt_instance *) filp->private_data;

	/* Finish all outstanding requests */
	ret = b2r2_blt_synch(instance, 0);
	if (ret < 0)
		b2r2_log_warn(
			"%s: b2r2_blt_sync failed with %d\n", __func__, ret);

	/* Now cancel any remaining outstanding request */
	if (instance->no_of_active_requests) {
		struct b2r2_core_job *job;

		b2r2_log_warn("%s: %d active requests\n",
			__func__, instance->no_of_active_requests);

		/* Find and cancel all jobs belonging to us */
		job = b2r2_core_job_find_first_with_tag((int) instance);
		while (job) {
			b2r2_core_job_cancel(job);
			/* Matches addref in b2r2_core_job_find... */
			b2r2_core_job_release(job, __func__);
			job = b2r2_core_job_find_first_with_tag((int) instance);
		}

		b2r2_log_warn(
			"%s: %d active requests after cancel\n",
			__func__, instance->no_of_active_requests);
	}

	/* Release jobs in report list */
	mutex_lock(&instance->lock);
	while (!list_empty(&instance->report_list)) {
		struct b2r2_blt_request *request = list_first_entry(
			&instance->report_list,
			struct b2r2_blt_request,
			list);
		list_del_init(&request->list);
		mutex_unlock(&instance->lock);
		/*
		 * This release matches the addref when the job was put into
		 * the report list
		 */
		b2r2_core_job_release(&request->job, __func__);
		mutex_lock(&instance->lock);
	}
	mutex_unlock(&instance->lock);

	/* Release our instance */
	kfree(instance);

	dec_stat(&stat_n_in_release);

	return 0;
}

/**
 * b2r2_blt_ioctl - This routine implements b2r2_blt ioctl interface
 *
 * @file: file pointer.
 * @cmd	:ioctl command.
 * @arg: input argument for ioctl.
 *
 * Returns 0 if OK else negative error code
 */
static long b2r2_blt_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct b2r2_blt_instance *instance;

	/** Process actual ioctl */

	b2r2_log_info("%s\n", __func__);

	/* Get the instance from the file structure */
	instance = (struct b2r2_blt_instance *) file->private_data;

	switch (cmd) {
	case B2R2_BLT_IOC: {
		/* This is the "blit" command */

		/* arg is user pointer to struct b2r2_blt_request */
		struct b2r2_blt_request *request =
			kmalloc(sizeof(*request), GFP_KERNEL);
		if (!request) {
			b2r2_log_err("%s: Failed to alloc mem\n",
				__func__);
			return -ENOMEM;
		}

		/* Initialize the structure */
		memset(request, 0, sizeof(*request));
		INIT_LIST_HEAD(&request->list);
		request->instance = instance;

		/*
		 * The user request is a sub structure of the
		 * kernel request structure.
		 */

		/* Get the user data */
		if (copy_from_user(&request->user_req, (void *)arg,
				sizeof(request->user_req))) {
			b2r2_log_err(
				"%s: copy_from_user failed\n",
				__func__);
			kfree(request);
			return -EFAULT;
		}

		if (!b2r2_validate_user_req(&request->user_req)) {
			kfree(request);
			return -EINVAL;
		}

		request->profile = is_profiler_registered_approx();

		/*
		 * If the user specified a color look-up table,
		 * make a copy that the HW can use.
		 */
		if ((request->user_req.flags &
				B2R2_BLT_FLAG_CLUT_COLOR_CORRECTION) != 0) {
			request->clut = dma_alloc_coherent(b2r2_blt_device(),
				CLUT_SIZE, &(request->clut_phys_addr),
				GFP_DMA | GFP_KERNEL);
			if (request->clut == NULL) {
				b2r2_log_err("%s CLUT allocation failed.\n",
					__func__);
				kfree(request);
				return -ENOMEM;
			}

			if (copy_from_user(request->clut,
					request->user_req.clut, CLUT_SIZE)) {
				b2r2_log_err("%s: CLUT copy_from_user failed\n",
					__func__);
				dma_free_coherent(b2r2_blt_device(), CLUT_SIZE,
					request->clut, request->clut_phys_addr);
				request->clut = NULL;
				request->clut_phys_addr = 0;
				kfree(request);
				return -EFAULT;
			}
		}

		/* Perform the blit */

#ifdef CONFIG_B2R2_GENERIC_ONLY
		/* Use the generic path for all operations */
		ret = b2r2_generic_blt(instance, request);
#else
		/* Use the optimized path */
		ret = b2r2_blt(instance, request);
#endif

#ifdef CONFIG_B2R2_GENERIC_FALLBACK
		/* Fall back to generic path if operation was not supported */
		if (ret == -ENOSYS) {
			struct b2r2_blt_request *request_gen;
			b2r2_log_info("b2r2_blt=%d Going generic.\n", ret);
			request_gen = kmalloc(sizeof(*request_gen), GFP_KERNEL);
			if (!request_gen) {
				b2r2_log_err("%s: Failed to alloc mem for "
					"request_gen\n", __func__);
				return -ENOMEM;
			}

			/* Initialize the structure */
			memset(request_gen, 0, sizeof(*request_gen));
			INIT_LIST_HEAD(&request_gen->list);
			request_gen->instance = instance;

			/*
			 * The user request is a sub structure of the
			 * kernel request structure.
			 */

			/* Get the user data */
			if (copy_from_user(&request_gen->user_req, (void *)arg,
				sizeof(request_gen->user_req))) {
					b2r2_log_err(
						"%s: copy_from_user failed\n",
						__func__);
					kfree(request_gen);
					return -EFAULT;
			}

			/*
			 * If the user specified a color look-up table,
			 * make a copy that the HW can use.
			 */
			if ((request_gen->user_req.flags &
					B2R2_BLT_FLAG_CLUT_COLOR_CORRECTION)
					!= 0) {
				request_gen->clut =
					dma_alloc_coherent(b2r2_blt_device(),
					CLUT_SIZE,
					&(request_gen->clut_phys_addr),
					GFP_DMA | GFP_KERNEL);
				if (request_gen->clut == NULL) {
					b2r2_log_err("%s CLUT allocation "
						"failed.\n", __func__);
					kfree(request_gen);
					return -ENOMEM;
				}

				if (copy_from_user(request_gen->clut,
						request_gen->user_req.clut,
						CLUT_SIZE)) {
					b2r2_log_err("%s: CLUT copy_from_user "
						"failed\n", __func__);
					dma_free_coherent(b2r2_blt_device(),
						CLUT_SIZE, request_gen->clut,
						request_gen->clut_phys_addr);
					request_gen->clut = NULL;
					request_gen->clut_phys_addr = 0;
					kfree(request_gen);
					return -EFAULT;
				}
			}

			request_gen->profile = is_profiler_registered_approx();

			ret = b2r2_generic_blt(instance, request_gen);
			b2r2_log_info("\nb2r2_generic_blt=%d Generic done.\n",
				ret);
		}
#endif /* CONFIG_B2R2_GENERIC_FALLBACK */

		break;
	}

	case B2R2_BLT_SYNCH_IOC:
		/* This is the "synch" command */

		/* arg is request_id */
		ret = b2r2_blt_synch(instance, (int) arg);
		break;

	case B2R2_BLT_QUERY_CAP_IOC:
	{
		/* This is the "query capabilities" command */

		/* Arg is struct b2r2_blt_query_cap */
		struct b2r2_blt_query_cap query_cap;

		/* Get the user data */
		if (copy_from_user(&query_cap, (void *)arg,
				sizeof(query_cap))) {
			b2r2_log_err(
				"%s: copy_from_user failed\n",
				__func__);
			return -EFAULT;
		}

		/* Fill in our capabilities */
		ret = b2r2_blt_query_cap(instance, &query_cap);

		/* Return data to user */
		if (copy_to_user((void *)arg, &query_cap,
				sizeof(query_cap))) {
			b2r2_log_err("%s: copy_to_user failed\n",
				__func__);
			return -EFAULT;
		}
		break;
	}

	default:
		/* Unknown command */
		b2r2_log_err(
			"%s: Unknown cmd %d\n", __func__, cmd);
		ret = -EINVAL;
	break;

	}

	if (ret < 0)
		b2r2_log_err("EC %d OK!\n", -ret);

	return ret;
}

/**
 * b2r2_blt_poll - Support for user-space poll, select & epoll.
 *                 Used for user-space callback
 *
 * @filp: File to poll on
 * @wait: Poll table to wait on
 *
 * This function checks if there are anything to read
 */
static unsigned b2r2_blt_poll(struct file *filp, poll_table *wait)
{
	struct b2r2_blt_instance *instance;
	unsigned int mask = 0;

	b2r2_log_info("%s\n", __func__);

	/* Get the instance from the file structure */
	instance = (struct b2r2_blt_instance *) filp->private_data;

	poll_wait(filp, &instance->report_list_waitq, wait);
	mutex_lock(&instance->lock);
	if (!list_empty(&instance->report_list))
		mask |= POLLIN | POLLRDNORM;
	mutex_unlock(&instance->lock);

	return mask;
}

/**
 * b2r2_blt_read - Read report data, user for user-space callback
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes read or negative error code
 */
static ssize_t b2r2_blt_read(struct file *filp, char __user *buf, size_t count,
			loff_t *f_pos)
{
	int ret = 0;
	struct b2r2_blt_instance *instance;
	struct b2r2_blt_request *request;
	struct b2r2_blt_report report;

	b2r2_log_info("%s\n", __func__);

	/* Get the instance from the file structure */
	instance = (struct b2r2_blt_instance *) filp->private_data;

	/*
	 * We return only complete report records, one at a time.
	 * Might be more efficient to support read of many.
	 */
	count = (count / sizeof(struct b2r2_blt_report)) *
		sizeof(struct b2r2_blt_report);
	if (count > sizeof(struct b2r2_blt_report))
		count = sizeof(struct b2r2_blt_report);
	if (count == 0)
		return count;

	/*
	 * Loop and wait here until we have anything to return or
	 * until interrupted
	 */
	mutex_lock(&instance->lock);
	while (list_empty(&instance->report_list)) {
		mutex_unlock(&instance->lock);

		/* Return if non blocking read */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		b2r2_log_info("%s - Going to sleep\n", __func__);
		if (wait_event_interruptible(
			instance->report_list_waitq,
			!is_report_list_empty(instance)))
			/* signal: tell the fs layer to handle it */
			return -ERESTARTSYS;

		/* Otherwise loop, but first reaquire the lock */
		mutex_lock(&instance->lock);
	}

	/* Ok, we have something to return */

	/* Return */
	request = NULL;
	if (!list_empty(&instance->report_list))
		request = list_first_entry(
			&instance->report_list, struct b2r2_blt_request, list);

	if (request) {
		/* Remove from list to avoid reading twice */
		list_del_init(&request->list);

		report.request_id = request->request_id;
		report.report1 = request->user_req.report1;
		report.report2 = request->user_req.report2;
		report.usec_elapsed = 0; /* TBD */

		mutex_unlock(&instance->lock);
		if (copy_to_user(buf,
				&report,
				sizeof(report)))
			ret = -EFAULT;
		mutex_lock(&instance->lock);

		if (ret) {
			/* copy to user failed, re-insert into list */
			list_add(&request->list,
				&request->instance->report_list);
			request = NULL;
		}
	}
	mutex_unlock(&instance->lock);

	if (request)
		/*
		 * Release matching the addref when the job was put into
		 * the report list
		 */
		b2r2_core_job_release(&request->job, __func__);

	return count;
}

/**
 * b2r2_blt_fops - File operations for b2r2_blt
 */
static const struct file_operations b2r2_blt_fops = {
	.owner =          THIS_MODULE,
	.open =           b2r2_blt_open,
	.release =        b2r2_blt_release,
	.unlocked_ioctl = b2r2_blt_ioctl,
	.poll  =          b2r2_blt_poll,
	.read  =          b2r2_blt_read,
};

/**
 * b2r2_blt_misc_dev - Misc device config for b2r2_blt
 */
static struct miscdevice b2r2_blt_misc_dev = {
	MISC_DYNAMIC_MINOR,
	"b2r2_blt",
	&b2r2_blt_fops
};


#ifndef CONFIG_B2R2_GENERIC_ONLY
/**
 * b2r2_blt - Implementation of the B2R2 blit request
 *
 * @instance: The B2R2 BLT instance
 * @request; The request to perform
 */
static int b2r2_blt(struct b2r2_blt_instance *instance,
		struct b2r2_blt_request *request)
{
	int ret = 0;
	struct b2r2_blt_rect actual_dst_rect;
	int request_id = 0;
	struct b2r2_node *last_node = request->first_node;
	int node_count;

	u32 thread_runtime_at_start = 0;

	if (request->profile) {
		request->start_time_nsec = b2r2_get_curr_nsec();
		thread_runtime_at_start = (u32)task_sched_runtime(current);
	}

	b2r2_log_info("%s\n", __func__);

	inc_stat(&stat_n_in_blt);

	/* Debug prints of incoming request */
	b2r2_log_info(
		"src.fmt=%#010x src.buf={%d,%d,%d} "
		"src.w,h={%d,%d} src.rect={%d,%d,%d,%d}\n",
		request->user_req.src_img.fmt,
		request->user_req.src_img.buf.type,
		request->user_req.src_img.buf.fd,
		request->user_req.src_img.buf.offset,
		request->user_req.src_img.width,
		request->user_req.src_img.height,
		request->user_req.src_rect.x,
		request->user_req.src_rect.y,
		request->user_req.src_rect.width,
		request->user_req.src_rect.height);
	b2r2_log_info(
		"dst.fmt=%#010x dst.buf={%d,%d,%d} "
		"dst.w,h={%d,%d} dst.rect={%d,%d,%d,%d}\n",
		request->user_req.dst_img.fmt,
		request->user_req.dst_img.buf.type,
		request->user_req.dst_img.buf.fd,
		request->user_req.dst_img.buf.offset,
		request->user_req.dst_img.width,
		request->user_req.dst_img.height,
		request->user_req.dst_rect.x,
		request->user_req.dst_rect.y,
		request->user_req.dst_rect.width,
		request->user_req.dst_rect.height);

	inc_stat(&stat_n_in_blt_synch);

	/* Wait here if synch is ongoing */
	ret = wait_event_interruptible(instance->synch_done_waitq,
				!is_synching(instance));
	if (ret) {
		b2r2_log_warn(
			"%s: Sync wait interrupted, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		dec_stat(&stat_n_in_blt_synch);
		goto synch_interrupted;
	}

	dec_stat(&stat_n_in_blt_synch);

	/* Resolve the buffers */

	/* Source buffer */
	ret = resolve_buf(&request->user_req.src_img,
		&request->user_req.src_rect, false, &request->src_resolved);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Resolve src buf failed, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		goto resolve_src_buf_failed;
	}

	/* Source mask buffer */
	ret = resolve_buf(&request->user_req.src_mask,
			&request->user_req.src_rect, false,
			&request->src_mask_resolved);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Resolve src mask buf failed, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		goto resolve_src_mask_buf_failed;
	}

	/* Destination buffer */
	get_actual_dst_rect(&request->user_req, &actual_dst_rect);
	ret = resolve_buf(&request->user_req.dst_img, &actual_dst_rect,
			true, &request->dst_resolved);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Resolve dst buf failed, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		goto resolve_dst_buf_failed;
	}

	/* Debug prints of resolved buffers */
	b2r2_log_info("src.rbuf={%X,%p,%d} {%p,%X,%X,%d}\n",
		request->src_resolved.physical_address,
		request->src_resolved.virtual_address,
		request->src_resolved.is_pmem,
		request->src_resolved.filep,
		request->src_resolved.file_physical_start,
		request->src_resolved.file_virtual_start,
		request->src_resolved.file_len);

	b2r2_log_info("dst.rbuf={%X,%p,%d} {%p,%X,%X,%d}\n",
		request->dst_resolved.physical_address,
		request->dst_resolved.virtual_address,
		request->dst_resolved.is_pmem,
		request->dst_resolved.filep,
		request->dst_resolved.file_physical_start,
		request->dst_resolved.file_virtual_start,
		request->dst_resolved.file_len);

	/* Calculate the number of nodes (and resources) needed for this job */
	ret = b2r2_node_split_analyze(request, MAX_TMP_BUF_SIZE,
			&node_count, &request->bufs, &request->buf_count,
			&request->node_split_job);
	if (ret == -ENOSYS) {
		/* There was no optimized path for this request */
		b2r2_log_info(
			"%s: No optimized path for request\n", __func__);
		goto no_optimized_path;

	} else if (ret < 0) {
		b2r2_log_warn(
			"%s: Failed to analyze request, ret = %d\n",
			__func__, ret);
#ifdef CONFIG_DEBUG_FS
		{
			/* Failed, dump job to dmesg */
			char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

			b2r2_log_info(
				"%s: Analyze failed for:\n", __func__);
			if (Buf != NULL) {
				sprintf_req(request, Buf, sizeof(char) * 4096);
				b2r2_log_info("%s", Buf);
				kfree(Buf);
			} else {
				b2r2_log_info("Unable to print the request. "
					"Message buffer allocation failed.\n");
			}
		}
#endif
		goto generate_nodes_failed;
	}

	/* Allocate the nodes needed */
#ifdef B2R2_USE_NODE_GEN
	request->first_node = b2r2_blt_alloc_nodes(node_count);
	if (request->first_node == NULL) {
		b2r2_log_warn(
			"%s: Failed to allocate nodes, ret = %d\n",
			__func__, ret);
		goto generate_nodes_failed;
	}
#else
	ret = b2r2_node_alloc(node_count, &(request->first_node));
	if (ret < 0 || request->first_node == NULL) {
		b2r2_log_warn(
			"%s: Failed to allocate nodes, ret = %d\n",
			__func__, ret);
		goto generate_nodes_failed;
	}
#endif

	/* Build the B2R2 node list */
	ret = b2r2_node_split_configure(&request->node_split_job,
			request->first_node);

	if (ret < 0) {
		b2r2_log_warn(
			"%s: Failed to perform node split, ret = %d\n",
			__func__, ret);
		goto generate_nodes_failed;
	}

	/* Exit here if dry run */
	if (request->user_req.flags & B2R2_BLT_FLAG_DRY_RUN)
		goto exit_dry_run;

	/* Configure the request */
	last_node = request->first_node;
	while (last_node && last_node->next)
		last_node = last_node->next;

	request->job.tag = (int) instance;
	request->job.prio = request->user_req.prio;
	request->job.first_node_address =
		request->first_node->physical_address;
	request->job.last_node_address =
		last_node->physical_address;
	request->job.callback = job_callback;
	request->job.release = job_release;
	request->job.acquire_resources = job_acquire_resources;
	request->job.release_resources = job_release_resources;

	/* Synchronize memory occupied by the buffers */

	/* Source buffer */
	if (!(request->user_req.flags &
				B2R2_BLT_FLAG_SRC_NO_CACHE_FLUSH) &&
			(request->user_req.src_img.buf.type !=
				B2R2_BLT_PTR_PHYSICAL) &&
			!b2r2_is_mb_fmt(request->user_req.src_img.fmt))
			/* MB formats are never touched by SW */
		sync_buf(&request->user_req.src_img,
			&request->src_resolved,
			false, /*is_dst*/
			&request->user_req.src_rect);

	/* Source mask buffer */
	if (!(request->user_req.flags &
				B2R2_BLT_FLAG_SRC_MASK_NO_CACHE_FLUSH) &&
			(request->user_req.src_mask.buf.type !=
				B2R2_BLT_PTR_PHYSICAL) &&
			!b2r2_is_mb_fmt(request->user_req.src_mask.fmt))
			/* MB formats are never touched by SW */
		sync_buf(&request->user_req.src_mask,
			&request->src_mask_resolved,
			false, /*is_dst*/
			NULL);

	/* Destination buffer */
	if (!(request->user_req.flags &
				B2R2_BLT_FLAG_DST_NO_CACHE_FLUSH) &&
			(request->user_req.dst_img.buf.type !=
				B2R2_BLT_PTR_PHYSICAL) &&
			!b2r2_is_mb_fmt(request->user_req.dst_img.fmt))
			/* MB formats are never touched by SW */
		sync_buf(&request->user_req.dst_img,
			&request->dst_resolved,
			true, /*is_dst*/
			&request->user_req.dst_rect);

#ifdef CONFIG_DEBUG_FS
	/* Remember latest request for debugfs */
	debugfs_latest_request = *request;
#endif

	/* Submit the job */
	b2r2_log_info("%s: Submitting job\n", __func__);

	inc_stat(&stat_n_in_blt_add);

	if (request->profile)
		request->nsec_active_in_cpu =
			(s32)((u32)task_sched_runtime(current) -
					thread_runtime_at_start);

	mutex_lock(&instance->lock);

	/* Add the job to b2r2_core */
	request_id = b2r2_core_job_add(&request->job);
	request->request_id = request_id;

	dec_stat(&stat_n_in_blt_add);

	if (request_id < 0) {
		b2r2_log_warn("%s: Failed to add job, ret = %d\n",
			__func__, request_id);
		ret = request_id;
		mutex_unlock(&instance->lock);
		goto job_add_failed;
	}

	inc_stat(&stat_n_jobs_added);

	instance->no_of_active_requests++;
	mutex_unlock(&instance->lock);

	/* Wait for the job to be done if synchronous */
	if ((request->user_req.flags & B2R2_BLT_FLAG_ASYNCH) == 0) {
		b2r2_log_info("%s: Synchronous, waiting\n",
			__func__);

		inc_stat(&stat_n_in_blt_wait);

		ret = b2r2_core_job_wait(&request->job);

		dec_stat(&stat_n_in_blt_wait);

		if (ret < 0 && ret != -ENOENT)
			b2r2_log_warn(
				"%s: Failed to wait job, ret = %d\n",
				__func__, ret);
		else
			b2r2_log_info(
				"%s: Synchronous wait done\n", __func__);
		ret = 0;
	}

	/*
	 * Release matching the addref in b2r2_core_job_add,
	 * the request must not be accessed after this call
	 */
	b2r2_core_job_release(&request->job, __func__);

	dec_stat(&stat_n_in_blt);

	return ret >= 0 ? request_id : ret;

job_add_failed:
exit_dry_run:
no_optimized_path:
generate_nodes_failed:
	unresolve_buf(&request->user_req.dst_img.buf,
		&request->dst_resolved);
resolve_dst_buf_failed:
	unresolve_buf(&request->user_req.src_mask.buf,
		&request->src_mask_resolved);
resolve_src_mask_buf_failed:
	unresolve_buf(&request->user_req.src_img.buf,
		&request->src_resolved);
resolve_src_buf_failed:
synch_interrupted:
	job_release(&request->job);
	dec_stat(&stat_n_jobs_released);
	if ((request->user_req.flags & B2R2_BLT_FLAG_DRY_RUN) == 0 || ret)
		b2r2_log_warn(
			"%s returns with error %d\n", __func__, ret);

	dec_stat(&stat_n_in_blt);

	return ret;
}

/**
 * Called when job is done or cancelled
 *
 * @job: The job
 */
static void job_callback(struct b2r2_core_job *job)
{
	struct b2r2_blt_request *request =
		container_of(job, struct b2r2_blt_request, job);

	if (b2r2_blt_device())
		b2r2_log_info("%s\n", __func__);

	/* Local addref / release within this func */
	b2r2_core_job_addref(job, __func__);

	/* Unresolve the buffers */
	unresolve_buf(&request->user_req.src_img.buf,
		&request->src_resolved);
	unresolve_buf(&request->user_req.src_mask.buf,
		&request->src_mask_resolved);
	unresolve_buf(&request->user_req.dst_img.buf,
		&request->dst_resolved);

	/* Move to report list if the job shall be reported */
	/* FIXME: Use a smaller struct? */
	mutex_lock(&request->instance->lock);
	if (request->user_req.flags & B2R2_BLT_FLAG_REPORT_WHEN_DONE) {
		/* Move job to report list */
		list_add_tail(&request->list,
			&request->instance->report_list);
		inc_stat(&stat_n_jobs_in_report_list);

		/* Wake up poll */
		wake_up_interruptible(
			&request->instance->report_list_waitq);

		/* Add a reference because we put the job in the report list */
		b2r2_core_job_addref(job, __func__);
	}

	/*
	 * Decrease number of active requests and wake up
	 * synching threads if active requests reaches zero
	 */
	BUG_ON(request->instance->no_of_active_requests == 0);
	request->instance->no_of_active_requests--;
	if (request->instance->synching &&
	request->instance->no_of_active_requests == 0) {
		request->instance->synching = false;
		/* Wake up all syncing */

		wake_up_interruptible_all(
			&request->instance->synch_done_waitq);
	}
	mutex_unlock(&request->instance->lock);

#ifdef CONFIG_DEBUG_FS
	/* Dump job if cancelled */
	if (job->job_state == B2R2_CORE_JOB_CANCELED) {
		char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

		b2r2_log_info("%s: Job cancelled:\n", __func__);
		if (Buf != NULL) {
			sprintf_req(request, Buf, sizeof(char) * 4096);
			b2r2_log_info("%s", Buf);
			kfree(Buf);
		} else {
			b2r2_log_info("Unable to print the request. "
					"Message buffer allocation failed.\n");
		}
	}
#endif

	if (request->profile) {
		request->total_time_nsec =
			(s32)(b2r2_get_curr_nsec() - request->start_time_nsec);
		b2r2_call_profiler_blt_done(request);
	}

	/* Local addref / release within this func */
	b2r2_core_job_release(job, __func__);
}

/**
 * Called when job should be released (free memory etc.)
 *
 * @job: The job
 */
static void job_release(struct b2r2_core_job *job)
{
	struct b2r2_blt_request *request =
		container_of(job, struct b2r2_blt_request, job);

	inc_stat(&stat_n_jobs_released);

	b2r2_log_info("%s, first_node=%p, ref_count=%d\n",
		__func__, request->first_node, request->job.ref_count);

	b2r2_node_split_cancel(&request->node_split_job);

	if (request->first_node) {
		b2r2_debug_job_done(request->first_node);
#ifdef B2R2_USE_NODE_GEN
		b2r2_blt_free_nodes(request->first_node);
#else
		b2r2_node_free(request->first_node);
#endif
	}

	/* Release memory for the request */
	if (request->clut != NULL) {
		dma_free_coherent(b2r2_blt_device(), CLUT_SIZE, request->clut,
				request->clut_phys_addr);
		request->clut = NULL;
		request->clut_phys_addr = 0;
	}
	kfree(request);
}

/**
 * Tells the job to try to allocate the resources needed to execute the job.
 * Called just before execution of a job.
 *
 * @job: The job
 * @atomic: true if called from atomic (i.e. interrupt) context. If function
 *          can't allocate in atomic context it should return error, it
 *          will then be called later from non-atomic context.
 */
static int job_acquire_resources(struct b2r2_core_job *job, bool atomic)
{
	struct b2r2_blt_request *request =
		container_of(job, struct b2r2_blt_request, job);
	int ret;
	int i;

	b2r2_log_info("%s\n", __func__);

	if (request->buf_count == 0)
		return 0;

	if (request->buf_count > MAX_TMP_BUFS_NEEDED) {
		b2r2_log_err("%s: request->buf_count > MAX_TMP_BUFS_NEEDED\n",
								__func__);
		return -ENOMSG;
	}

	/*
	 * 1 to 1 mapping between request temp buffers and temp buffers
	 * (request temp buf 0 is always temp buf 0, request temp buf 1 is
	 * always temp buf 1 and so on) to avoid starvation of jobs that
	 * require multiple temp buffers. Not optimal in terms of memory
	 * usage but we avoid get into a situation where lower prio jobs can
	 * delay higher prio jobs that require more temp buffers.
	 */
	if (tmp_bufs[0].in_use)
		return -EAGAIN;

	for (i = 0; i < request->buf_count; i++) {
		if (tmp_bufs[i].buf.size < request->bufs[i].size) {
			b2r2_log_err("%s: tmp_bufs[i].buf.size < "
					"request->bufs[i].size\n",
								__func__);
			ret = -ENOMSG;
			goto error;
		}

		tmp_bufs[i].in_use = true;
		request->bufs[i].phys_addr = tmp_bufs[i].buf.phys_addr;
		request->bufs[i].virt_addr = tmp_bufs[i].buf.virt_addr;

		b2r2_log_info("%s: phys=%p, virt=%p\n",
			__func__, (void *)request->bufs[i].phys_addr,
			request->bufs[i].virt_addr);

		ret = b2r2_node_split_assign_buffers(&request->node_split_job,
					request->first_node, request->bufs,
					request->buf_count);
		if (ret < 0)
			goto error;
	}

	return 0;

error:
	for (i = 0; i < request->buf_count; i++)
		tmp_bufs[i].in_use = false;

	return ret;
}

/**
 * Tells the job to free the resources needed to execute the job.
 * Called after execution of a job.
 *
 * @job: The job
 * @atomic: true if called from atomic (i.e. interrupt) context. If function
 *          can't allocate in atomic context it should return error, it
 *          will then be called later from non-atomic context.
 */
static void job_release_resources(struct b2r2_core_job *job, bool atomic)
{
	struct b2r2_blt_request *request =
		container_of(job, struct b2r2_blt_request, job);
	int i;

	b2r2_log_info("%s\n", __func__);

	/* Free any temporary buffers */
	for (i = 0; i < request->buf_count; i++) {

		b2r2_log_info("%s: freeing %d bytes\n",
			__func__, request->bufs[i].size);
		tmp_bufs[i].in_use = false;
		memset(&request->bufs[i], 0, sizeof(request->bufs[i]));
	}
	request->buf_count = 0;

	/*
	 * Early release of nodes
	 * FIXME: If nodes are to be reused we don't want to release here
	 */
	if (!atomic && request->first_node) {
		b2r2_debug_job_done(request->first_node);

#ifdef B2R2_USE_NODE_GEN
		b2r2_blt_free_nodes(request->first_node);
#else
		b2r2_node_free(request->first_node);
#endif
		request->first_node = NULL;
	}
}

#endif /* !CONFIG_B2R2_GENERIC_ONLY */

#ifdef CONFIG_B2R2_GENERIC
/**
 * Called when job for one tile is done or cancelled
 * in the generic path.
 *
 * @job: The job
 */
static void tile_job_callback_gen(struct b2r2_core_job *job)
{
	if (b2r2_blt_device())
		b2r2_log_info("%s\n", __func__);

	/* Local addref / release within this func */
	b2r2_core_job_addref(job, __func__);

#ifdef CONFIG_DEBUG_FS
	/* Notify if a tile job is cancelled */
	if (job->job_state == B2R2_CORE_JOB_CANCELED)
		b2r2_log_info("%s: Tile job cancelled:\n", __func__);
#endif

	/* Local addref / release within this func */
	b2r2_core_job_release(job, __func__);
}

/**
 * Called when job is done or cancelled.
 * Used for the last tile in the generic path
 * to notify waiting clients.
 *
 * @job: The job
 */
static void job_callback_gen(struct b2r2_core_job *job)
{
	struct b2r2_blt_request *request =
		container_of(job, struct b2r2_blt_request, job);

	if (b2r2_blt_device())
		b2r2_log_info("%s\n", __func__);

	/* Local addref / release within this func */
	b2r2_core_job_addref(job, __func__);

	/* Unresolve the buffers */
	unresolve_buf(&request->user_req.src_img.buf,
		&request->src_resolved);
	unresolve_buf(&request->user_req.src_mask.buf,
		&request->src_mask_resolved);
	unresolve_buf(&request->user_req.dst_img.buf,
		&request->dst_resolved);

	/* Move to report list if the job shall be reported */
	/* FIXME: Use a smaller struct? */
	mutex_lock(&request->instance->lock);

	if (request->user_req.flags & B2R2_BLT_FLAG_REPORT_WHEN_DONE) {
		/* Move job to report list */
		list_add_tail(&request->list,
			&request->instance->report_list);
		inc_stat(&stat_n_jobs_in_report_list);

		/* Wake up poll */
		wake_up_interruptible(
			&request->instance->report_list_waitq);

		/*
		 * Add a reference because we put the
		 * job in the report list
		 */
		b2r2_core_job_addref(job, __func__);
	}

	/*
	 * Decrease number of active requests and wake up
	 * synching threads if active requests reaches zero
	 */
	BUG_ON(request->instance->no_of_active_requests == 0);
	request->instance->no_of_active_requests--;
	if (request->instance->synching &&
	request->instance->no_of_active_requests == 0) {
		request->instance->synching = false;
		/* Wake up all syncing */

		wake_up_interruptible_all(
			&request->instance->synch_done_waitq);
	}
	mutex_unlock(&request->instance->lock);

#ifdef CONFIG_DEBUG_FS
	/* Dump job if cancelled */
	if (job->job_state == B2R2_CORE_JOB_CANCELED) {
		char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

		b2r2_log_info("%s: Job cancelled:\n", __func__);
		if (Buf != NULL) {
			sprintf_req(request, Buf, sizeof(char) * 4096);
			b2r2_log_info("%s", Buf);
			kfree(Buf);
		} else {
			b2r2_log_info("Unable to print the request. "
					"Message buffer allocation failed.\n");
		}
	}
#endif

	/* Local addref / release within this func */
	b2r2_core_job_release(job, __func__);
}

/**
 * Called when tile job should be released (free memory etc.)
 * Should be used only for tile jobs. Tile jobs should only be used
 * by b2r2_core, thus making ref_count trigger their release.
 *
 * @job: The job
 */

static void tile_job_release_gen(struct b2r2_core_job *job)
{
	inc_stat(&stat_n_jobs_released);

	b2r2_log_info("%s, first_node_address=0x%.8x, ref_count=%d\n",
		__func__, job->first_node_address, job->ref_count);

	/* Release memory for the job */
	kfree(job);
}

/**
 * Called when job should be released (free memory etc.)
 *
 * @job: The job
 */

static void job_release_gen(struct b2r2_core_job *job)
{
	struct b2r2_blt_request *request =
		container_of(job, struct b2r2_blt_request, job);

	inc_stat(&stat_n_jobs_released);

	b2r2_log_info("%s, first_node=%p, ref_count=%d\n",
		__func__, request->first_node, request->job.ref_count);

	if (request->first_node) {
		b2r2_debug_job_done(request->first_node);

		/* Free nodes */
#ifdef B2R2_USE_NODE_GEN
		b2r2_blt_free_nodes(request->first_node);
#else
		b2r2_node_free(request->first_node);
#endif
	}

	/* Release memory for the request */
	if (request->clut != NULL) {
		dma_free_coherent(b2r2_blt_device(), CLUT_SIZE, request->clut,
				request->clut_phys_addr);
		request->clut = NULL;
		request->clut_phys_addr = 0;
	}
	kfree(request);
}

static int job_acquire_resources_gen(struct b2r2_core_job *job, bool atomic)
{
	/* Nothing so far. Temporary buffers are pre-allocated */
	return 0;
}
static void job_release_resources_gen(struct b2r2_core_job *job, bool atomic)
{
	/* Nothing so far. Temporary buffers are pre-allocated */
}

/**
 * b2r2_generic_blt - Generic implementation of the B2R2 blit request
 *
 * @instance: The B2R2 BLT instance
 * @request; The request to perform
 */
static int b2r2_generic_blt(struct b2r2_blt_instance *instance,
		struct b2r2_blt_request *request)
{
	int ret = 0;
	struct b2r2_blt_rect actual_dst_rect;
	int request_id = 0;
	struct b2r2_node *last_node = request->first_node;
	int node_count;
	s32 tmp_buf_width = 0;
	s32 tmp_buf_height = 0;
	u32 tmp_buf_count = 0;
	s32 x;
	s32 y;
	const struct b2r2_blt_rect *dst_rect = &(request->user_req.dst_rect);
	const s32 dst_img_width = request->user_req.dst_img.width;
	const s32 dst_img_height = request->user_req.dst_img.height;
	const enum b2r2_blt_flag flags = request->user_req.flags;
	/* Descriptors for the temporary buffers */
	struct b2r2_work_buf work_bufs[4];
	struct b2r2_blt_rect dst_rect_tile;
	int i;

	u32 thread_runtime_at_start = 0;
	s32 nsec_active_in_b2r2 = 0;

	/*
	 * Early exit if zero blt.
	 * dst_rect outside of dst_img or
	 * dst_clip_rect outside of dst_img.
	 */
	if (dst_rect->x + dst_rect->width <= 0 ||
			dst_rect->y + dst_rect->height <= 0 ||
			dst_img_width <= dst_rect->x ||
			dst_img_height <= dst_rect->y ||
			((flags & B2R2_BLT_FLAG_DESTINATION_CLIP) != 0 &&
			(dst_img_width <= request->user_req.dst_clip_rect.x ||
			dst_img_height <= request->user_req.dst_clip_rect.y ||
			request->user_req.dst_clip_rect.x +
			request->user_req.dst_clip_rect.width <= 0 ||
			request->user_req.dst_clip_rect.y +
			request->user_req.dst_clip_rect.height <= 0))) {
		goto zero_blt;
	}

	if (request->profile) {
		request->start_time_nsec = b2r2_get_curr_nsec();
		thread_runtime_at_start = (u32)task_sched_runtime(current);
	}

	memset(work_bufs, 0, sizeof(work_bufs));

	b2r2_log_info("%s\n", __func__);

	inc_stat(&stat_n_in_blt);

	/* Debug prints of incoming request */
	b2r2_log_info(
		"src.fmt=%#010x flags=0x%.8x src.buf={%d,%d,0x%.8x}\n"
		"src.w,h={%d,%d} src.rect={%d,%d,%d,%d}\n",
		request->user_req.src_img.fmt,
		request->user_req.flags,
		request->user_req.src_img.buf.type,
		request->user_req.src_img.buf.fd,
		request->user_req.src_img.buf.offset,
		request->user_req.src_img.width,
		request->user_req.src_img.height,
		request->user_req.src_rect.x,
		request->user_req.src_rect.y,
		request->user_req.src_rect.width,
		request->user_req.src_rect.height);
	b2r2_log_info(
		"dst.fmt=%#010x dst.buf={%d,%d,0x%.8x}\n"
		"dst.w,h={%d,%d} dst.rect={%d,%d,%d,%d}\n"
		"dst_clip_rect={%d,%d,%d,%d}\n",
		request->user_req.dst_img.fmt,
		request->user_req.dst_img.buf.type,
		request->user_req.dst_img.buf.fd,
		request->user_req.dst_img.buf.offset,
		request->user_req.dst_img.width,
		request->user_req.dst_img.height,
		request->user_req.dst_rect.x,
		request->user_req.dst_rect.y,
		request->user_req.dst_rect.width,
		request->user_req.dst_rect.height,
		request->user_req.dst_clip_rect.x,
		request->user_req.dst_clip_rect.y,
		request->user_req.dst_clip_rect.width,
		request->user_req.dst_clip_rect.height);

	inc_stat(&stat_n_in_blt_synch);

	/* Wait here if synch is ongoing */
	ret = wait_event_interruptible(instance->synch_done_waitq,
				!is_synching(instance));
	if (ret) {
		b2r2_log_warn(
			"%s: Sync wait interrupted, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		dec_stat(&stat_n_in_blt_synch);
		goto synch_interrupted;
	}

	dec_stat(&stat_n_in_blt_synch);

	/* Resolve the buffers */

	/* Source buffer */
	ret = resolve_buf(&request->user_req.src_img,
		&request->user_req.src_rect, false, &request->src_resolved);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Resolve src buf failed, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		goto resolve_src_buf_failed;
	}

	/* Source mask buffer */
	ret = resolve_buf(&request->user_req.src_mask,
					&request->user_req.src_rect, false,
						&request->src_mask_resolved);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Resolve src mask buf failed, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		goto resolve_src_mask_buf_failed;
	}

	/* Destination buffer */
	get_actual_dst_rect(&request->user_req, &actual_dst_rect);
	ret = resolve_buf(&request->user_req.dst_img, &actual_dst_rect,
						true, &request->dst_resolved);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Resolve dst buf failed, %d\n",
			__func__, ret);
		ret = -EAGAIN;
		goto resolve_dst_buf_failed;
	}

	/* Debug prints of resolved buffers */
	b2r2_log_info("src.rbuf={%X,%p,%d} {%p,%X,%X,%d}\n",
		request->src_resolved.physical_address,
		request->src_resolved.virtual_address,
		request->src_resolved.is_pmem,
		request->src_resolved.filep,
		request->src_resolved.file_physical_start,
		request->src_resolved.file_virtual_start,
		request->src_resolved.file_len);

	b2r2_log_info("dst.rbuf={%X,%p,%d} {%p,%X,%X,%d}\n",
		request->dst_resolved.physical_address,
		request->dst_resolved.virtual_address,
		request->dst_resolved.is_pmem,
		request->dst_resolved.filep,
		request->dst_resolved.file_physical_start,
		request->dst_resolved.file_virtual_start,
		request->dst_resolved.file_len);

	/* Calculate the number of nodes (and resources) needed for this job */
	ret = b2r2_generic_analyze(request, &tmp_buf_width,
			&tmp_buf_height, &tmp_buf_count, &node_count);
	if (ret < 0) {
		b2r2_log_warn(
			"%s: Failed to analyze request, ret = %d\n",
			__func__, ret);
#ifdef CONFIG_DEBUG_FS
		{
			/* Failed, dump job to dmesg */
			char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

			b2r2_log_info(
				"%s: Analyze failed for:\n", __func__);
			if (Buf != NULL) {
				sprintf_req(request, Buf, sizeof(char) * 4096);
				b2r2_log_info("%s", Buf);
				kfree(Buf);
			} else {
				b2r2_log_info("Unable to print the request. "
					"Message buffer allocation failed.\n");
			}
		}
#endif
		goto generate_nodes_failed;
	}

	/* Allocate the nodes needed */
#ifdef B2R2_USE_NODE_GEN
	request->first_node = b2r2_blt_alloc_nodes(node_count);
	if (request->first_node == NULL) {
		b2r2_log_warn(
			"%s: Failed to allocate nodes, ret = %d\n",
			__func__, ret);
		goto generate_nodes_failed;
	}
#else
	ret = b2r2_node_alloc(node_count, &(request->first_node));
	if (ret < 0 || request->first_node == NULL) {
		b2r2_log_warn(
			"%s: Failed to allocate nodes, ret = %d\n",
			__func__, ret);
		goto generate_nodes_failed;
	}
#endif

	/* Allocate the temporary buffers */
	for (i = 0; i < tmp_buf_count; i++) {
		void *virt;
		work_bufs[i].size = tmp_buf_width * tmp_buf_height * 4;

		virt = dma_alloc_coherent(b2r2_blt_device(),
				work_bufs[i].size,
				&(work_bufs[i].phys_addr),
				GFP_DMA | GFP_KERNEL);
		if (virt == NULL) {
			ret = -ENOMEM;
			goto alloc_work_bufs_failed;
		}

		work_bufs[i].virt_addr = virt;
		memset(work_bufs[i].virt_addr, 0xff, work_bufs[i].size);
	}
	ret = b2r2_generic_configure(request,
			request->first_node, &work_bufs[0], tmp_buf_count);

	if (ret < 0) {
		b2r2_log_warn(
			"%s: Failed to perform generic configure, ret = %d\n",
			__func__, ret);
		goto generic_conf_failed;
	}

	/* Exit here if dry run */
	if (flags & B2R2_BLT_FLAG_DRY_RUN)
		goto exit_dry_run;

	/*
	 * Configure the request and make sure
	 * that its job is run only for the LAST tile.
	 * This is when the request is complete
	 * and waiting clients should be notified.
	 */
	last_node = request->first_node;
	while (last_node && last_node->next)
		last_node = last_node->next;

	request->job.tag = (int) instance;
	request->job.prio = request->user_req.prio;
	request->job.first_node_address =
		request->first_node->physical_address;
	request->job.last_node_address =
		last_node->physical_address;
	request->job.callback = job_callback_gen;
	request->job.release = job_release_gen;
	/* Work buffers and nodes are pre-allocated */
	request->job.acquire_resources = job_acquire_resources_gen;
	request->job.release_resources = job_release_resources_gen;

	/* Flush the L1/L2 cache for the buffers */

	/* Source buffer */
	if (!(flags & B2R2_BLT_FLAG_SRC_NO_CACHE_FLUSH) &&
			(request->user_req.src_img.buf.type !=
				B2R2_BLT_PTR_PHYSICAL) &&
			!b2r2_is_mb_fmt(request->user_req.src_img.fmt))
			/* MB formats are never touched by SW */
		sync_buf(&request->user_req.src_img,
			&request->src_resolved,
			false, /*is_dst*/
			&request->user_req.src_rect);

	/* Source mask buffer */
	if (!(flags & B2R2_BLT_FLAG_SRC_MASK_NO_CACHE_FLUSH) &&
			(request->user_req.src_mask.buf.type !=
				B2R2_BLT_PTR_PHYSICAL) &&
			!b2r2_is_mb_fmt(request->user_req.src_mask.fmt))
			/* MB formats are never touched by SW */
		sync_buf(&request->user_req.src_mask,
			&request->src_mask_resolved,
			false, /*is_dst*/
			NULL);

	/* Destination buffer */
	if (!(flags & B2R2_BLT_FLAG_DST_NO_CACHE_FLUSH) &&
			(request->user_req.dst_img.buf.type !=
				B2R2_BLT_PTR_PHYSICAL) &&
			!b2r2_is_mb_fmt(request->user_req.dst_img.fmt))
			/* MB formats are never touched by SW */
		sync_buf(&request->user_req.dst_img,
			&request->dst_resolved,
			true, /*is_dst*/
			&request->user_req.dst_rect);

#ifdef CONFIG_DEBUG_FS
	/* Remember latest request */
	debugfs_latest_request = *request;
#endif

	/*
	 * Same nodes are reused for all the jobs needed to complete the blit.
	 * Nodes are NOT released together with associated job,
	 * as is the case with optimized b2r2_blt() path.
	 */
	mutex_lock(&instance->lock);
	instance->no_of_active_requests++;
	mutex_unlock(&instance->lock);
	/*
	 * Process all but the last row in the destination rectangle.
	 * Consider only the tiles that will actually end up inside
	 * the destination image.
	 * dst_rect->height - tmp_buf_height being <=0 is allright.
	 * The loop will not be entered since y will always be equal to or
	 * greater than zero.
	 * Early exit check at the beginning handles the cases when nothing
	 * at all should be processed.
	 */
	y = 0;
	if (dst_rect->y < 0)
		y = -dst_rect->y;

	for (; y < dst_rect->height - tmp_buf_height &&
			y + dst_rect->y < dst_img_height - tmp_buf_height;
			y += tmp_buf_height) {
		/* Tile in the destination rectangle being processed */
		struct b2r2_blt_rect dst_rect_tile;
		dst_rect_tile.y = y;
		dst_rect_tile.width = tmp_buf_width;
		dst_rect_tile.height = tmp_buf_height;

		x = 0;
		if (dst_rect->x < 0)
			x = -dst_rect->x;

		for (; x < dst_rect->width &&
				x + dst_rect->x < dst_img_width;
				x += tmp_buf_width) {
			/*
			 * Tile jobs are freed by the supplied release function
			 * when ref_count on a tile_job reaches zero.
			 */
			struct b2r2_core_job *tile_job =
				kmalloc(sizeof(*tile_job), GFP_KERNEL);
			if (tile_job == NULL) {
				/*
				 * Skip this tile. Do not abort,
				 * just hope for better luck
				 * with rest of the tiles.
				 * Memory might become available.
				 */
				b2r2_log_info("%s: Failed to alloc job. "
					"Skipping tile at (x, y)=(%d, %d)\n",
					__func__, x, y);
				continue;
			}
			tile_job->tag = request->job.tag;
			tile_job->prio = request->job.prio;
			tile_job->first_node_address =
					request->job.first_node_address;
			tile_job->last_node_address =
					request->job.last_node_address;
			tile_job->callback = tile_job_callback_gen;
			tile_job->release = tile_job_release_gen;
			/* Work buffers and nodes are pre-allocated */
			tile_job->acquire_resources = job_acquire_resources_gen;
			tile_job->release_resources = job_release_resources_gen;

			dst_rect_tile.x = x;
			if (x + dst_rect->x + tmp_buf_width > dst_img_width) {
				/*
				 * Only a part of the tile can be written.
				 * Limit imposed by buffer size.
				 */
				dst_rect_tile.width =
					dst_img_width - (x + dst_rect->x);
			} else if (x + tmp_buf_width > dst_rect->width) {
				/*
				 * Only a part of the tile can be written.
				 * In this case limit imposed by dst_rect size.
				 */
				dst_rect_tile.width = dst_rect->width - x;
			} else {
				/* Whole tile can be written. */
				dst_rect_tile.width = tmp_buf_width;
			}
			/*
			 * Where applicable, calculate area in src buffer
			 * that is needed to generate the specified part
			 * of destination rectangle.
			 */
			b2r2_generic_set_areas(request,
				request->first_node, &dst_rect_tile);
			/* Submit the job */
			b2r2_log_info("%s: Submitting job\n", __func__);

			inc_stat(&stat_n_in_blt_add);

			mutex_lock(&instance->lock);

			request_id = b2r2_core_job_add(tile_job);

			dec_stat(&stat_n_in_blt_add);

			if (request_id < 0) {
				b2r2_log_warn("%s: "
					"Failed to add tile job, ret = %d\n",
					__func__, request_id);
				ret = request_id;
				mutex_unlock(&instance->lock);
				goto job_add_failed;
			}

			inc_stat(&stat_n_jobs_added);

			mutex_unlock(&instance->lock);

			/* Wait for the job to be done */
			b2r2_log_info("%s: Synchronous, waiting\n",
				__func__);

			inc_stat(&stat_n_in_blt_wait);

			ret = b2r2_core_job_wait(tile_job);

			dec_stat(&stat_n_in_blt_wait);

			if (ret < 0 && ret != -ENOENT)
				b2r2_log_warn(
					"%s: Failed to wait job, ret = %d\n",
					__func__, ret);
			else {
				b2r2_log_info(
					"%s: Synchronous wait done\n",
					__func__);

				nsec_active_in_b2r2 +=
					tile_job->nsec_active_in_hw;
			}
			/* Release matching the addref in b2r2_core_job_add */
			b2r2_core_job_release(tile_job, __func__);
		}
	}

	x = 0;
	if (dst_rect->x < 0)
		x = -dst_rect->x;

	for (; x < dst_rect->width &&
			x + dst_rect->x < dst_img_width; x += tmp_buf_width) {
		struct b2r2_core_job *tile_job = NULL;
		if (x + tmp_buf_width < dst_rect->width &&
				x + dst_rect->x + tmp_buf_width <
				dst_img_width) {
			/*
			 * Tile jobs are freed by the supplied release function
			 * when ref_count on a tile_job reaches zero.
			 * Do NOT allocate a tile_job for the last tile.
			 * Send the job from the request. This way clients
			 * will be notified when the whole blit is complete
			 * and not just part of it.
			 */
			tile_job = kmalloc(sizeof(*tile_job), GFP_KERNEL);
			if (tile_job == NULL) {
				b2r2_log_info("%s: Failed to alloc job. "
					"Skipping tile at (x, y)=(%d, %d)\n",
					__func__, x, y);
				continue;
			}
			tile_job->tag = request->job.tag;
			tile_job->prio = request->job.prio;
			tile_job->first_node_address =
				request->job.first_node_address;
			tile_job->last_node_address =
				request->job.last_node_address;
			tile_job->callback = tile_job_callback_gen;
			tile_job->release = tile_job_release_gen;
			tile_job->acquire_resources = job_acquire_resources_gen;
			tile_job->release_resources = job_release_resources_gen;
		}

		dst_rect_tile.x = x;
		if (x + dst_rect->x + tmp_buf_width > dst_img_width) {
			/*
			 * Only a part of the tile can be written.
			 * Limit imposed by buffer size.
			 */
			dst_rect_tile.width = dst_img_width - (x + dst_rect->x);
		} else if (x + tmp_buf_width > dst_rect->width) {
			/*
			 * Only a part of the tile can be written.
			 * In this case limit imposed by dst_rect size.
			 */
			dst_rect_tile.width = dst_rect->width - x;
		} else {
			/* Whole tile can be written. */
			dst_rect_tile.width = tmp_buf_width;
		}
		/*
		 * y is now the last row. Either because the whole dst_rect
		 * has been processed, or because the last row that will be
		 * written to dst_img has been reached. Limits imposed in
		 * the same way as for width.
		 */
		dst_rect_tile.y = y;
		if (y + dst_rect->y + tmp_buf_height > dst_img_height)
			dst_rect_tile.height =
				dst_img_height - (y + dst_rect->y);
		else if (y + tmp_buf_height > dst_rect->height)
			dst_rect_tile.height = dst_rect->height - y;
		else
			dst_rect_tile.height = tmp_buf_height;

		b2r2_generic_set_areas(request,
			request->first_node, &dst_rect_tile);

		b2r2_log_info("%s: Submitting job\n", __func__);
		inc_stat(&stat_n_in_blt_add);

		mutex_lock(&instance->lock);
		if (x + tmp_buf_width < dst_rect->width &&
				x + dst_rect->x + tmp_buf_width <
				dst_img_width) {
			request_id = b2r2_core_job_add(tile_job);
		} else {
			/*
			 * Last tile. Send the job-struct from the request.
			 * Clients will be notified once it completes.
			 */
			request_id = b2r2_core_job_add(&request->job);
		}

		dec_stat(&stat_n_in_blt_add);

		if (request_id < 0) {
			b2r2_log_warn("%s: Failed to add tile job, ret = %d\n",
				__func__, request_id);
			ret = request_id;
			mutex_unlock(&instance->lock);
			if (tile_job != NULL)
				kfree(tile_job);
			goto job_add_failed;
		}

		inc_stat(&stat_n_jobs_added);
		mutex_unlock(&instance->lock);

		b2r2_log_info("%s: Synchronous, waiting\n",
			__func__);

		inc_stat(&stat_n_in_blt_wait);
		if (x + tmp_buf_width < dst_rect->width &&
				x + dst_rect->x + tmp_buf_width <
				dst_img_width) {
			ret = b2r2_core_job_wait(tile_job);
		} else {
			/*
			 * This is the last tile. Wait for the job-struct from
			 * the request.
			 */
			ret = b2r2_core_job_wait(&request->job);
		}
		dec_stat(&stat_n_in_blt_wait);

		if (ret < 0 && ret != -ENOENT)
			b2r2_log_warn(
				"%s: Failed to wait job, ret = %d\n",
				__func__, ret);
		else {
			b2r2_log_info(
				"%s: Synchronous wait done\n", __func__);

			if (x + tmp_buf_width < dst_rect->width &&
					x + dst_rect->x + tmp_buf_width <
					dst_img_width)
				nsec_active_in_b2r2 +=
					tile_job->nsec_active_in_hw;
			else
				nsec_active_in_b2r2 +=
					request->job.nsec_active_in_hw;
		}

		/*
		 * Release matching the addref in b2r2_core_job_add.
		 * Make sure that the correct job-struct is released
		 * when the last tile is processed.
		 */
		if (x + tmp_buf_width < dst_rect->width &&
				x + dst_rect->x + tmp_buf_width <
				dst_img_width) {
			b2r2_core_job_release(tile_job, __func__);
		} else {
			/*
			 * Update profiling information before
			 * the request is released together with
			 * its core_job.
			 */
			if (request->profile) {
				request->nsec_active_in_cpu =
					(s32)((u32)task_sched_runtime(current) -
					thread_runtime_at_start);
				request->total_time_nsec =
					(s32)(b2r2_get_curr_nsec() -
					request->start_time_nsec);
				request->job.nsec_active_in_hw =
					nsec_active_in_b2r2;

				b2r2_call_profiler_blt_done(request);
			}

			b2r2_core_job_release(&request->job, __func__);
		}
	}

	dec_stat(&stat_n_in_blt);

	for (i = 0; i < tmp_buf_count; i++) {
		dma_free_coherent(b2r2_blt_device(),
			work_bufs[i].size,
			work_bufs[i].virt_addr,
			work_bufs[i].phys_addr);
		memset(&(work_bufs[i]), 0, sizeof(work_bufs[i]));
	}

	return request_id;

job_add_failed:
exit_dry_run:
generic_conf_failed:
alloc_work_bufs_failed:
	for (i = 0; i < 4; i++) {
		if (work_bufs[i].virt_addr != 0) {
			dma_free_coherent(b2r2_blt_device(),
				work_bufs[i].size,
				work_bufs[i].virt_addr,
				work_bufs[i].phys_addr);
			memset(&(work_bufs[i]), 0, sizeof(work_bufs[i]));
		}
	}

generate_nodes_failed:
	unresolve_buf(&request->user_req.dst_img.buf,
		&request->dst_resolved);
resolve_dst_buf_failed:
	unresolve_buf(&request->user_req.src_mask.buf,
		&request->src_mask_resolved);
resolve_src_mask_buf_failed:
	unresolve_buf(&request->user_req.src_img.buf,
		&request->src_resolved);
resolve_src_buf_failed:
synch_interrupted:
zero_blt:
	job_release_gen(&request->job);
	dec_stat(&stat_n_jobs_released);
	dec_stat(&stat_n_in_blt);

	b2r2_log_info("b2r2:%s ret=%d", __func__, ret);
	return ret;
}
#endif /* CONFIG_B2R2_GENERIC */

/**
 * b2r2_blt_synch - Implements wait for all or a specified job
 *
 * @instance: The B2R2 BLT instance
 * @request_id: If 0, wait for all requests on this instance to finish.
 *              Else wait for request with given request id to finish.
 */
static int b2r2_blt_synch(struct b2r2_blt_instance *instance,
			int request_id)
{
	int ret = 0;
	b2r2_log_info("%s, request_id=%d\n", __func__, request_id);

	if (request_id == 0) {
		/* Wait for all requests */
		inc_stat(&stat_n_in_synch_0);

		/* Enter state "synching" if we have any active request */
		mutex_lock(&instance->lock);
		if (instance->no_of_active_requests)
			instance->synching = true;
		mutex_unlock(&instance->lock);

		/* Wait until no longer in state synching */
		ret = wait_event_interruptible(instance->synch_done_waitq,
					!is_synching(instance));
		dec_stat(&stat_n_in_synch_0);
	} else {
		struct b2r2_core_job *job;

		inc_stat(&stat_n_in_synch_job);

		/* Wait for specific job */
		job = b2r2_core_job_find(request_id);
		if (job) {
			/* Wait on find job */
			ret = b2r2_core_job_wait(job);
			/* Release matching the addref in b2r2_core_job_find */
			b2r2_core_job_release(job, __func__);
		}

		/* If job not found we assume that is has been run */

		dec_stat(&stat_n_in_synch_job);
	}

	b2r2_log_info(
		"%s, request_id=%d, returns %d\n", __func__, request_id, ret);

	return ret;
}

/**
 * Query B2R2 capabilities
 *
 * @instance: The B2R2 BLT instance
 * @query_cap: The structure receiving the capabilities
 */
static int b2r2_blt_query_cap(struct b2r2_blt_instance *instance,
		struct b2r2_blt_query_cap *query_cap)
{
	/* FIXME: Not implemented yet */
	return -ENOSYS;
}

static void get_actual_dst_rect(struct b2r2_blt_req *req,
					struct b2r2_blt_rect *actual_dst_rect)
{
	struct b2r2_blt_rect dst_img_bounds;

	b2r2_get_img_bounding_rect(&req->dst_img, &dst_img_bounds);

	b2r2_intersect_rects(&req->dst_rect, &dst_img_bounds, actual_dst_rect);

	if (req->flags & B2R2_BLT_FLAG_DESTINATION_CLIP)
		b2r2_intersect_rects(actual_dst_rect, &req->dst_clip_rect,
							actual_dst_rect);
}

static void set_up_hwmem_region(struct b2r2_blt_img *img,
		struct b2r2_blt_rect *rect, struct hwmem_region *region)
{
	s32 img_size;

	memset(region, 0, sizeof(*region));

	if (b2r2_is_zero_area_rect(rect))
		return;

	img_size = b2r2_get_img_size(img);

	if (b2r2_is_single_plane_fmt(img->fmt) &&
				b2r2_is_independent_pixel_fmt(img->fmt)) {
		int img_fmt_bpp = b2r2_get_fmt_bpp(img->fmt);
		u32 img_pitch = b2r2_get_img_pitch(img);

		region->offset = (u32)(img->buf.offset + (rect->y *
								img_pitch));
		region->count = (u32)rect->height;
		region->start = (u32)((rect->x * img_fmt_bpp) / 8);
		region->end = (u32)b2r2_div_round_up(
				(rect->x + rect->width) * img_fmt_bpp, 8);
		region->size = img_pitch;
	} else {
		/*
		 * TODO: Locking entire buffer as a quick safe solution. In the
		 * future we should lock less to avoid unecessary cache
		 * synching. Pixel interleaved YCbCr formats should be quite
		 * easy, just align start and stop points on 2.
		 */
		region->offset = (u32)img->buf.offset;
		region->count = 1;
		region->start = 0;
		region->end = (u32)img_size;
		region->size = (u32)img_size;
	}
}

static int resolve_hwmem(struct b2r2_blt_img *img,
		struct b2r2_blt_rect *rect_2b_used,
		bool is_dst,
		struct b2r2_resolved_buf *resolved_buf)
{
	int return_value = 0;
	enum hwmem_mem_type mem_type;
	enum hwmem_access access;
	enum hwmem_access required_access;
	struct hwmem_mem_chunk mem_chunk;
	size_t mem_chunk_length = 1;
	struct hwmem_region region;

	resolved_buf->hwmem_alloc =
			hwmem_resolve_by_name(img->buf.hwmem_buf_name);
	if (IS_ERR(resolved_buf->hwmem_alloc)) {
		return_value = PTR_ERR(resolved_buf->hwmem_alloc);
		b2r2_log_info("%s: hwmem_resolve_by_name failed, "
				"error code: %i\n", __func__, return_value);
		goto resolve_failed;
	}

	hwmem_get_info(resolved_buf->hwmem_alloc, &resolved_buf->file_len,
			&mem_type, &access);

	required_access = (is_dst ? HWMEM_ACCESS_WRITE : HWMEM_ACCESS_READ) |
							HWMEM_ACCESS_IMPORT;
	if ((required_access & access) != required_access) {
		b2r2_log_info("%s: Insufficient access to hwmem buffer.\n",
				__func__);
		return_value = -EACCES;
		goto access_check_failed;
	}

	if (mem_type != HWMEM_MEM_CONTIGUOUS_SYS) {
		b2r2_log_info("%s: Hwmem buffer is scattered.\n", __func__);
		return_value = -EINVAL;
		goto buf_scattered;
	}

	if (resolved_buf->file_len <
			img->buf.offset + (__u32)b2r2_get_img_size(img)) {
		b2r2_log_info("%s: Hwmem buffer too small.\n", __func__);
		return_value = -EINVAL;
		goto size_check_failed;
	}

	return_value = hwmem_pin(resolved_buf->hwmem_alloc, &mem_chunk,
							 &mem_chunk_length);
	if (return_value < 0) {
		b2r2_log_info("%s: hwmem_pin failed, "
				"error code: %i\n", __func__, return_value);
		goto pin_failed;
	}
	resolved_buf->file_physical_start = mem_chunk.paddr;

	set_up_hwmem_region(img, rect_2b_used, &region);
	return_value = hwmem_set_domain(resolved_buf->hwmem_alloc,
				required_access, HWMEM_DOMAIN_SYNC, &region);
	if (return_value < 0) {
		b2r2_log_info("%s: hwmem_set_domain failed, "
				"error code: %i\n", __func__, return_value);
		goto set_domain_failed;
	}

	resolved_buf->physical_address =
			resolved_buf->file_physical_start + img->buf.offset;

	goto out;

set_domain_failed:
	hwmem_unpin(resolved_buf->hwmem_alloc);
pin_failed:
size_check_failed:
buf_scattered:
access_check_failed:
	hwmem_release(resolved_buf->hwmem_alloc);
resolve_failed:

out:
	return return_value;
}

static void unresolve_hwmem(struct b2r2_resolved_buf *resolved_buf)
{
	hwmem_unpin(resolved_buf->hwmem_alloc);
	hwmem_release(resolved_buf->hwmem_alloc);
}

/**
 * unresolve_buf() - Must be called after resolve_buf
 *
 * @buf: The buffer specification as supplied from user space
 * @resolved: Gathered information about the buffer
 *
 * Returns 0 if OK else negative error code
 */
static void unresolve_buf(struct b2r2_blt_buf *buf,
			struct b2r2_resolved_buf *resolved)
{
#ifdef CONFIG_ANDROID_PMEM
	if (resolved->is_pmem && resolved->filep)
		put_pmem_file(resolved->filep);
#endif
	if (resolved->hwmem_alloc != NULL)
		unresolve_hwmem(resolved);
}

/**
 * resolve_buf() - Returns the physical & virtual addresses of a B2R2 blt buffer
 *
 * @img: The image specification as supplied from user space
 * @rect_2b_used: The part of the image b2r2 will use.
 * @usage: Specifies how the buffer will be used.
 * @resolved: Gathered information about the buffer
 *
 * Returns 0 if OK else negative error code
 */
static int resolve_buf(struct b2r2_blt_img *img,
		struct b2r2_blt_rect *rect_2b_used,
		bool is_dst,
		struct b2r2_resolved_buf *resolved)
{
	int ret = 0;

	memset(resolved, 0, sizeof(*resolved));

	switch (img->buf.type) {
	case B2R2_BLT_PTR_NONE:
		break;

	case B2R2_BLT_PTR_PHYSICAL:
		resolved->physical_address = img->buf.offset;
		resolved->file_len = img->buf.len;
		break;

		/* FD + OFFSET type */
	case B2R2_BLT_PTR_FD_OFFSET: {
		/*
		 * TODO: Do we need to check if the process is allowed to
		 * read/write (depending on if it's dst or src) to the file?
		 */
		struct file *file;
		int put_needed;
		int i;

#ifdef CONFIG_ANDROID_PMEM
		if (!get_pmem_file(
			img->buf.fd,
			(unsigned long *) &resolved->file_physical_start,
			(unsigned long *) &resolved->file_virtual_start,
			(unsigned long *) &resolved->file_len,
			&resolved->filep)) {
			resolved->physical_address =
				resolved->file_physical_start +
				img->buf.offset;
			resolved->virtual_address = (void *)
				(resolved->file_virtual_start +
				img->buf.offset);
			resolved->is_pmem = true;
		} else
#endif
		{
			/* Will be set to 0 if a matching dev is found */
			ret = -EINVAL;

			file = fget_light(img->buf.fd, &put_needed);
			if (file == NULL)
				return -EINVAL;
#ifdef CONFIG_FB
			if (MAJOR(file->f_dentry->d_inode->i_rdev) ==
					FB_MAJOR) {
				/*
				 * This is a frame buffer device, find fb_info
				 * (OK to do it like this, no locking???)
				 */

				for (i = 0; i < num_registered_fb; i++) {
					struct fb_info *info = registered_fb[i];

					if (info && info->dev &&
							MINOR(info->dev->devt) ==
						MINOR(file->f_dentry->
							d_inode->i_rdev)) {
						resolved->file_physical_start =
							info->fix.smem_start;
						resolved->file_virtual_start =
							(u32)info->screen_base;
						resolved->file_len =
							info->fix.smem_len;

						resolved->physical_address =
							resolved->file_physical_start +
							img->buf.offset;
						resolved->virtual_address =
						(void *) (resolved->
							file_virtual_start +
							img->buf.offset);

						ret = 0;
						break;
					}
				}
			}
#endif

			fput_light(file, put_needed);
		}

		/* Check bounds */
		if (ret >= 0 && img->buf.offset + img->buf.len >
				resolved->file_len) {
			ret = -ESPIPE;
			unresolve_buf(&img->buf, resolved);
		}

		break;
	}

	case B2R2_BLT_PTR_HWMEM_BUF_NAME_OFFSET:
		ret = resolve_hwmem(img, rect_2b_used, is_dst, resolved);
		break;

	default:
		b2r2_log_warn(
			"%s: Failed to resolve buf type %d\n",
			__func__, img->buf.type);

		ret = -EINVAL;
		break;

	}

	return ret;
}

/**
 * sync_buf - Synchronizes the memory occupied by an image buffer.
 *
 * @buf: User buffer specification
 * @resolved_buf: Gathered info (physical address etc.) about buffer
 * @is_dst: true if the buffer is a destination buffer, false if the buffer is a
 *          source buffer.
 * @rect: rectangle in the image buffer that should be synced.
 *        NULL if the buffer is a source mask.
 * @img_width: width of the complete image buffer
 * @fmt: buffer format
*/
static void sync_buf(struct b2r2_blt_img *img,
		struct b2r2_resolved_buf *resolved,
		bool is_dst,
		struct b2r2_blt_rect *rect)
{
	struct sync_args sa;
	u32 start_phys, end_phys;

	if (B2R2_BLT_PTR_NONE == img->buf.type ||
			B2R2_BLT_PTR_HWMEM_BUF_NAME_OFFSET == img->buf.type)
		return;

	start_phys = resolved->physical_address;
	end_phys = resolved->physical_address + img->buf.len;

	/*
	 * TODO: Very ugly. We should find out whether the memory is coherent in
	 * some generic way but cache handling will be rewritten soon so there
	 * is no use spending time on it. In the new design this will probably
	 * not be a problem.
	 */
	/* Frame buffer is coherent, at least now. */
	if (!resolved->is_pmem) {
		/*
		 * Drain the write buffers as they are not always part of the
		 * coherent concept.
		 */
		wmb();

		return;
	}

	/*
	 * src_mask does not have rect.
	 * Also flush full buffer for planar and semiplanar YUV formats
	 */
	if (rect == NULL ||
			(img->fmt == B2R2_BLT_FMT_YUV420_PACKED_PLANAR) ||
			(img->fmt == B2R2_BLT_FMT_YUV422_PACKED_PLANAR) ||
			(img->fmt == B2R2_BLT_FMT_YUV444_PACKED_PLANAR) ||
			(img->fmt == B2R2_BLT_FMT_YUV420_PACKED_SEMI_PLANAR) ||
			(img->fmt == B2R2_BLT_FMT_YUV422_PACKED_SEMI_PLANAR) ||
			(img->fmt ==
				B2R2_BLT_FMT_YUV420_PACKED_SEMIPLANAR_MB_STE) ||
			(img->fmt ==
				B2R2_BLT_FMT_YUV422_PACKED_SEMIPLANAR_MB_STE)) {
		sa.start = (unsigned long)resolved->virtual_address;
		sa.end = (unsigned long)resolved->virtual_address +
			img->buf.len;
		start_phys = resolved->physical_address;
		end_phys = resolved->physical_address + img->buf.len;
	} else {
		/*
		 * buffer is not a src_mask so make use of rect when
		 * clean & flush caches
		 */
		u32 bpp;	/* Bits per pixel */
		u32 pitch;

		switch (img->fmt) {
		case B2R2_BLT_FMT_16_BIT_ARGB4444: /* Fall through */
		case B2R2_BLT_FMT_16_BIT_ARGB1555: /* Fall through */
		case B2R2_BLT_FMT_16_BIT_RGB565:   /* Fall through */
		case B2R2_BLT_FMT_Y_CB_Y_CR:       /* Fall through */
		case B2R2_BLT_FMT_CB_Y_CR_Y:
			bpp = 16;
			break;
		case B2R2_BLT_FMT_24_BIT_RGB888:   /* Fall through */
		case B2R2_BLT_FMT_24_BIT_ARGB8565: /* Fall through */
		case B2R2_BLT_FMT_24_BIT_YUV888:
		case B2R2_BLT_FMT_24_BIT_VUY888:
			bpp =  24;
			break;
		case B2R2_BLT_FMT_32_BIT_ARGB8888: /* Fall through */
		case B2R2_BLT_FMT_32_BIT_ABGR8888: /* Fall through */
		case B2R2_BLT_FMT_32_BIT_AYUV8888:
		case B2R2_BLT_FMT_32_BIT_VUYA8888:
			bpp = 32;
			break;
		default:
			bpp = 12;
		}
		if (img->pitch == 0)
			pitch = (img->width * bpp) / 8;
		else
			pitch = img->pitch;

		/*
		 * For 422I formats 2 horizontal pixels share color data.
		 * Thus, the x position must be aligned down to closest even
		 * number and width must be aligned up.
		 */
		{
			s32 x;
			s32 width;

			switch (img->fmt) {
			case B2R2_BLT_FMT_Y_CB_Y_CR:       /* Fall through */
			case B2R2_BLT_FMT_CB_Y_CR_Y:
				x = (rect->x / 2) * 2;
				width = ((rect->width + 1) / 2) * 2;
				break;
			default:
				x = rect->x;
				width = rect->width;
				break;
			}

			sa.start = (unsigned long)resolved->virtual_address +
					rect->y * pitch + (x * bpp) / 8;
			sa.end = (unsigned long)sa.start +
					(rect->height - 1) * pitch +
					(width * bpp) / 8;

			start_phys = resolved->physical_address +
					rect->y * pitch + (x * bpp) / 8;
			end_phys = start_phys +
					(rect->height - 1) * pitch +
					(width * bpp) / 8;
		}
	}

	/*
	 * The virtual address to a pmem buffer is retrieved from ioremap, not
	 * sure if it's	ok to use such an address as a kernel virtual address.
	 * When doing it at a higher level such as dma_map_single it triggers an
	 * error but at lower levels such as dmac_clean_range it seems to work,
	 * hence the low level stuff.
	 */

	if (is_dst) {
		/*
		 * According to ARM's docs you must clean before invalidating
		 * (ie flush) to avoid loosing data.
		 */

		/* Flush L1 cache */
#ifdef CONFIG_SMP
		flush_l1_cache_range_all_cpus(&sa);
#else
		flush_l1_cache_range_curr_cpu(&sa);
#endif

		/* Flush L2 cache */
		outer_flush_range(start_phys, end_phys);
	} else {
		/* Clean L1 cache */
#ifdef CONFIG_SMP
		clean_l1_cache_range_all_cpus(&sa);
#else
		clean_l1_cache_range_curr_cpu(&sa);
#endif

		/* Clean L2 cache */
		outer_clean_range(start_phys, end_phys);
	}
}

/**
 * is_report_list_empty() - Spin lock protected check of report list
 *
 * @instance: The B2R2 BLT instance
 */
static bool is_report_list_empty(struct b2r2_blt_instance *instance)
{
	bool is_empty;

	mutex_lock(&instance->lock);
	is_empty = list_empty(&instance->report_list);
	mutex_unlock(&instance->lock);

	return is_empty;
}

/**
 * is_synching() - Spin lock protected check if synching
 *
 * @instance: The B2R2 BLT instance
 */
static bool is_synching(struct b2r2_blt_instance *instance)
{
	bool is_synching;

	mutex_lock(&instance->lock);
	is_synching = instance->synching;
	mutex_unlock(&instance->lock);

	return is_synching;
}

/**
 * b2r2_blt_devide() - Returns the B2R2 blt device for logging
 */
struct device *b2r2_blt_device(void)
{
	return b2r2_blt_dev ? b2r2_blt_dev->this_device : NULL;
}

/**
 * inc_stat() - Spin lock protected increment of statistics variable
 *
 * @stat: Pointer to statistics variable that should be incremented
 */
static void inc_stat(unsigned long *stat)
{
	mutex_lock(&stat_lock);
	(*stat)++;
	mutex_unlock(&stat_lock);
}

/**
 * inc_stat() - Spin lock protected decrement of statistics variable
 *
 * @stat: Pointer to statistics variable that should be decremented
 */
static void dec_stat(unsigned long *stat)
{
	mutex_lock(&stat_lock);
	(*stat)--;
	mutex_unlock(&stat_lock);
}


#ifdef CONFIG_DEBUG_FS
/**
 * sprintf_req() - Builds a string representing the request, for debug
 *
 * @request:Request that should be encoded into a string
 * @buf: Receiving buffer
 * @size: Size of receiving buffer
 *
 * Returns number of characters in string, excluding null terminator
 */
static int sprintf_req(struct b2r2_blt_request *request, char *buf, int size)
{
	size_t dev_size = 0;

	dev_size += sprintf(buf + dev_size,
			"instance: %p\n\n",
			request->instance);

	dev_size += sprintf(buf + dev_size,
			"size: %d bytes\n",
			request->user_req.size);
	dev_size += sprintf(buf + dev_size,
			"flags: %8lX\n",
			(unsigned long) request->user_req.flags);
	dev_size += sprintf(buf + dev_size,
			"transform: %3lX\n",
			(unsigned long) request->user_req.transform);
	dev_size += sprintf(buf + dev_size,
			"prio: %d\n",
			request->user_req.transform);
	dev_size += sprintf(buf + dev_size,
			"src_img.fmt: %#010x\n",
			request->user_req.src_img.fmt);
	dev_size += sprintf(buf + dev_size,
			"src_img.buf: {type=%d,hwmem_buf_name=%d,fd=%d,"
				"offset=%d,len=%d}\n",
			request->user_req.src_img.buf.type,
			request->user_req.src_img.buf.hwmem_buf_name,
			request->user_req.src_img.buf.fd,
			request->user_req.src_img.buf.offset,
			request->user_req.src_img.buf.len);
	dev_size += sprintf(buf + dev_size,
			"src_img.{width=%d,height=%d,pitch=%d}\n",
			request->user_req.src_img.width,
			request->user_req.src_img.height,
			request->user_req.src_img.pitch);
	dev_size += sprintf(buf + dev_size,
			"src_mask.fmt: %#010x\n",
			request->user_req.src_mask.fmt);
	dev_size += sprintf(buf + dev_size,
			"src_mask.buf: {type=%d,hwmem_buf_name=%d,fd=%d,"
				"offset=%d,len=%d}\n",
			request->user_req.src_mask.buf.type,
			request->user_req.src_mask.buf.hwmem_buf_name,
			request->user_req.src_mask.buf.fd,
			request->user_req.src_mask.buf.offset,
			request->user_req.src_mask.buf.len);
	dev_size += sprintf(buf + dev_size,
			"src_mask.{width=%d,height=%d,pitch=%d}\n",
			request->user_req.src_mask.width,
			request->user_req.src_mask.height,
			request->user_req.src_mask.pitch);
	dev_size += sprintf(buf + dev_size,
			"src_rect.{x=%d,y=%d,width=%d,height=%d}\n",
			request->user_req.src_rect.x,
			request->user_req.src_rect.y,
			request->user_req.src_rect.width,
			request->user_req.src_rect.height);
	dev_size += sprintf(buf + dev_size,
			"src_color=%08lX\n",
			(unsigned long) request->user_req.src_color);

	dev_size += sprintf(buf + dev_size,
			"dst_img.fmt: %#010x\n",
			request->user_req.dst_img.fmt);
	dev_size += sprintf(buf + dev_size,
			"dst_img.buf: {type=%d,hwmem_buf_name=%d,fd=%d,"
				"offset=%d,len=%d}\n",
			request->user_req.dst_img.buf.type,
			request->user_req.dst_img.buf.hwmem_buf_name,
			request->user_req.dst_img.buf.fd,
			request->user_req.dst_img.buf.offset,
			request->user_req.dst_img.buf.len);
	dev_size += sprintf(buf + dev_size,
			"dst_img.{width=%d,height=%d,pitch=%d}\n",
			request->user_req.dst_img.width,
			request->user_req.dst_img.height,
			request->user_req.dst_img.pitch);
	dev_size += sprintf(buf + dev_size,
			"dst_rect.{x=%d,y=%d,width=%d,height=%d}\n",
			request->user_req.dst_rect.x,
			request->user_req.dst_rect.y,
			request->user_req.dst_rect.width,
			request->user_req.dst_rect.height);
	dev_size += sprintf(buf + dev_size,
			"dst_clip_rect.{x=%d,y=%d,width=%d,height=%d}\n",
			request->user_req.dst_clip_rect.x,
			request->user_req.dst_clip_rect.y,
			request->user_req.dst_clip_rect.width,
			request->user_req.dst_clip_rect.height);
	dev_size += sprintf(buf + dev_size,
			"dst_color=%08lX\n",
			(unsigned long) request->user_req.dst_color);
	dev_size += sprintf(buf + dev_size,
			"global_alpha=%d\n",
			(int) request->user_req.global_alpha);
	dev_size += sprintf(buf + dev_size,
			"report1=%08lX\n",
			(unsigned long) request->user_req.report1);
	dev_size += sprintf(buf + dev_size,
			"report2=%08lX\n",
			(unsigned long) request->user_req.report2);

	dev_size += sprintf(buf + dev_size,
			"request_id: %d\n",
			request->request_id);

	dev_size += sprintf(buf + dev_size,
			"src_resolved.physical: %lX\n",
			(unsigned long) request->src_resolved.
			physical_address);
	dev_size += sprintf(buf + dev_size,
			"src_resolved.virtual: %p\n",
			request->src_resolved.virtual_address);
	dev_size += sprintf(buf + dev_size,
			"src_resolved.filep: %p\n",
			request->src_resolved.filep);
	dev_size += sprintf(buf + dev_size,
			"src_resolved.filep_physical_start: %lX\n",
			(unsigned long) request->src_resolved.
			file_physical_start);
	dev_size += sprintf(buf + dev_size,
			"src_resolved.filep_virtual_start: %p\n",
			(void *) request->src_resolved.file_virtual_start);
	dev_size += sprintf(buf + dev_size,
			"src_resolved.file_len: %d\n",
			request->src_resolved.file_len);

	dev_size += sprintf(buf + dev_size,
			"src_mask_resolved.physical: %lX\n",
			(unsigned long) request->src_mask_resolved.
			physical_address);
	dev_size += sprintf(buf + dev_size,
			"src_mask_resolved.virtual: %p\n",
			request->src_mask_resolved.virtual_address);
	dev_size += sprintf(buf + dev_size,
			"src_mask_resolved.filep: %p\n",
			request->src_mask_resolved.filep);
	dev_size += sprintf(buf + dev_size,
			"src_mask_resolved.filep_physical_start: %lX\n",
			(unsigned long) request->src_mask_resolved.
			file_physical_start);
	dev_size += sprintf(buf + dev_size,
			"src_mask_resolved.filep_virtual_start: %p\n",
			(void *) request->src_mask_resolved.
			file_virtual_start);
	dev_size += sprintf(buf + dev_size,
			"src_mask_resolved.file_len: %d\n",
			request->src_mask_resolved.file_len);

	dev_size += sprintf(buf + dev_size,
			"dst_resolved.physical: %lX\n",
			(unsigned long) request->dst_resolved.
			physical_address);
	dev_size += sprintf(buf + dev_size,
			"dst_resolved.virtual: %p\n",
			request->dst_resolved.virtual_address);
	dev_size += sprintf(buf + dev_size,
			"dst_resolved.filep: %p\n",
			request->dst_resolved.filep);
	dev_size += sprintf(buf + dev_size,
			"dst_resolved.filep_physical_start: %lX\n",
			(unsigned long) request->dst_resolved.
			file_physical_start);
	dev_size += sprintf(buf + dev_size,
			"dst_resolved.filep_virtual_start: %p\n",
			(void *) request->dst_resolved.file_virtual_start);
	dev_size += sprintf(buf + dev_size,
			"dst_resolved.file_len: %d\n",
			request->dst_resolved.file_len);

	return dev_size;
}

/**
 * debugfs_b2r2_blt_request_read() - Implements debugfs read for B2R2 register
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes read or negative error code
 */
static int debugfs_b2r2_blt_request_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos)
{
	size_t dev_size = 0;
	int ret = 0;
	char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

	if (Buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	dev_size = sprintf_req(&debugfs_latest_request, Buf,
		sizeof(char) * 4096);

	/* No more to read if offset != 0 */
	if (*f_pos > dev_size)
		goto out;

	if (*f_pos + count > dev_size)
		count = dev_size - *f_pos;

	if (copy_to_user(buf, Buf, count))
		ret = -EINVAL;
	*f_pos += count;
	ret = count;

out:
	if (Buf != NULL)
		kfree(Buf);
	return ret;
}

/**
 * debugfs_b2r2_blt_request_fops - File operations for B2R2 request debugfs
 */
static const struct file_operations debugfs_b2r2_blt_request_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_b2r2_blt_request_read,
};

/**
 * struct debugfs_reg - Represents a B2R2 node "register"
 *
 * @name: Register name
 * @offset: Offset within the node
 */
struct debugfs_reg {
	const char  name[30];
	u32   offset;
};

/**
 * debugfs_node_regs - Array with all the registers in a B2R2 node, for debug
 */
static const struct debugfs_reg debugfs_node_regs[] = {
	{"GROUP0.B2R2_NIP", offsetof(struct b2r2_link_list, GROUP0.B2R2_NIP)},
	{"GROUP0.B2R2_CIC", offsetof(struct b2r2_link_list, GROUP0.B2R2_CIC)},
	{"GROUP0.B2R2_INS", offsetof(struct b2r2_link_list, GROUP0.B2R2_INS)},
	{"GROUP0.B2R2_ACK", offsetof(struct b2r2_link_list, GROUP0.B2R2_ACK)},

	{"GROUP1.B2R2_TBA", offsetof(struct b2r2_link_list, GROUP1.B2R2_TBA)},
	{"GROUP1.B2R2_TTY", offsetof(struct b2r2_link_list, GROUP1.B2R2_TTY)},
	{"GROUP1.B2R2_TXY", offsetof(struct b2r2_link_list, GROUP1.B2R2_TXY)},
	{"GROUP1.B2R2_TSZ", offsetof(struct b2r2_link_list, GROUP1.B2R2_TSZ)},

	{"GROUP2.B2R2_S1CF", offsetof(struct b2r2_link_list, GROUP2.B2R2_S1CF)},
	{"GROUP2.B2R2_S2CF", offsetof(struct b2r2_link_list, GROUP2.B2R2_S2CF)},

	{"GROUP3.B2R2_SBA", offsetof(struct b2r2_link_list, GROUP3.B2R2_SBA)},
	{"GROUP3.B2R2_STY", offsetof(struct b2r2_link_list, GROUP3.B2R2_STY)},
	{"GROUP3.B2R2_SXY", offsetof(struct b2r2_link_list, GROUP3.B2R2_SXY)},
	{"GROUP3.B2R2_SSZ", offsetof(struct b2r2_link_list, GROUP3.B2R2_SSZ)},

	{"GROUP4.B2R2_SBA", offsetof(struct b2r2_link_list, GROUP4.B2R2_SBA)},
	{"GROUP4.B2R2_STY", offsetof(struct b2r2_link_list, GROUP4.B2R2_STY)},
	{"GROUP4.B2R2_SXY", offsetof(struct b2r2_link_list, GROUP4.B2R2_SXY)},
	{"GROUP4.B2R2_SSZ", offsetof(struct b2r2_link_list, GROUP4.B2R2_SSZ)},

	{"GROUP5.B2R2_SBA", offsetof(struct b2r2_link_list, GROUP5.B2R2_SBA)},
	{"GROUP5.B2R2_STY", offsetof(struct b2r2_link_list, GROUP5.B2R2_STY)},
	{"GROUP5.B2R2_SXY", offsetof(struct b2r2_link_list, GROUP5.B2R2_SXY)},
	{"GROUP5.B2R2_SSZ", offsetof(struct b2r2_link_list, GROUP5.B2R2_SSZ)},

	{"GROUP6.B2R2_CWO", offsetof(struct b2r2_link_list, GROUP6.B2R2_CWO)},
	{"GROUP6.B2R2_CWS", offsetof(struct b2r2_link_list, GROUP6.B2R2_CWS)},

	{"GROUP7.B2R2_CCO", offsetof(struct b2r2_link_list, GROUP7.B2R2_CCO)},
	{"GROUP7.B2R2_CML", offsetof(struct b2r2_link_list, GROUP7.B2R2_CML)},

	{"GROUP8.B2R2_FCTL", offsetof(struct b2r2_link_list, GROUP8.B2R2_FCTL)},
	{"GROUP8.B2R2_PMK", offsetof(struct b2r2_link_list, GROUP8.B2R2_PMK)},

	{"GROUP9.B2R2_RSF", offsetof(struct b2r2_link_list, GROUP9.B2R2_RSF)},
	{"GROUP9.B2R2_RZI", offsetof(struct b2r2_link_list, GROUP9.B2R2_RZI)},
	{"GROUP9.B2R2_HFP", offsetof(struct b2r2_link_list, GROUP9.B2R2_HFP)},
	{"GROUP9.B2R2_VFP", offsetof(struct b2r2_link_list, GROUP9.B2R2_VFP)},

	{"GROUP10.B2R2_RSF", offsetof(struct b2r2_link_list, GROUP10.B2R2_RSF)},
	{"GROUP10.B2R2_RZI", offsetof(struct b2r2_link_list, GROUP10.B2R2_RZI)},
	{"GROUP10.B2R2_HFP", offsetof(struct b2r2_link_list, GROUP10.B2R2_HFP)},
	{"GROUP10.B2R2_VFP", offsetof(struct b2r2_link_list, GROUP10.B2R2_VFP)},

	{"GROUP11.B2R2_FF0", offsetof(struct b2r2_link_list,
					GROUP11.B2R2_FF0)},
	{"GROUP11.B2R2_FF1", offsetof(struct b2r2_link_list,
					GROUP11.B2R2_FF1)},
	{"GROUP11.B2R2_FF2", offsetof(struct b2r2_link_list,
					GROUP11.B2R2_FF2)},
	{"GROUP11.B2R2_FF3", offsetof(struct b2r2_link_list,
					GROUP11.B2R2_FF3)},

	{"GROUP12.B2R2_KEY1", offsetof(struct b2r2_link_list,
				GROUP12.B2R2_KEY1)},
	{"GROUP12.B2R2_KEY2", offsetof(struct b2r2_link_list,
				GROUP12.B2R2_KEY2)},

	{"GROUP13.B2R2_XYL", offsetof(struct b2r2_link_list, GROUP13.B2R2_XYL)},
	{"GROUP13.B2R2_XYP", offsetof(struct b2r2_link_list, GROUP13.B2R2_XYP)},

	{"GROUP14.B2R2_SAR", offsetof(struct b2r2_link_list, GROUP14.B2R2_SAR)},
	{"GROUP14.B2R2_USR", offsetof(struct b2r2_link_list, GROUP14.B2R2_USR)},

	{"GROUP15.B2R2_VMX0", offsetof(struct b2r2_link_list,
				GROUP15.B2R2_VMX0)},
	{"GROUP15.B2R2_VMX1", offsetof(struct b2r2_link_list,
				GROUP15.B2R2_VMX1)},
	{"GROUP15.B2R2_VMX2", offsetof(struct b2r2_link_list,
				GROUP15.B2R2_VMX2)},
	{"GROUP15.B2R2_VMX3", offsetof(struct b2r2_link_list,
				GROUP15.B2R2_VMX3)},

	{"GROUP16.B2R2_VMX0", offsetof(struct b2r2_link_list,
				GROUP16.B2R2_VMX0)},
	{"GROUP16.B2R2_VMX1", offsetof(struct b2r2_link_list,
				GROUP16.B2R2_VMX1)},
	{"GROUP16.B2R2_VMX2", offsetof(struct b2r2_link_list,
				GROUP16.B2R2_VMX2)},
	{"GROUP16.B2R2_VMX3", offsetof(struct b2r2_link_list,
				GROUP16.B2R2_VMX3)},
};

/**
 * debugfs_b2r2_blt_stat_read() - Implements debugfs read for B2R2 BLT
 *                                statistics
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes read or negative error code
 */
static int debugfs_b2r2_blt_stat_read(struct file *filp, char __user *buf,
				size_t count, loff_t *f_pos)
{
	size_t dev_size = 0;
	int ret = 0;
	char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

	if (Buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&stat_lock);
	dev_size += sprintf(Buf + dev_size, "Added jobs: %lu\n",
			stat_n_jobs_added);
	dev_size += sprintf(Buf + dev_size, "Released jobs: %lu\n",
			stat_n_jobs_released);
	dev_size += sprintf(Buf + dev_size, "Jobs in report list: %lu\n",
			stat_n_jobs_in_report_list);
	dev_size += sprintf(Buf + dev_size, "Clients in open: %lu\n",
			stat_n_in_open);
	dev_size += sprintf(Buf + dev_size, "Clients in release: %lu\n",
			stat_n_in_release);
	dev_size += sprintf(Buf + dev_size, "Clients in blt: %lu\n",
			stat_n_in_blt);
	dev_size += sprintf(Buf + dev_size, "              synch: %lu\n",
			stat_n_in_blt_synch);
	dev_size += sprintf(Buf + dev_size, "              add: %lu\n",
			stat_n_in_blt_add);
	dev_size += sprintf(Buf + dev_size, "              wait: %lu\n",
			stat_n_in_blt_wait);
	dev_size += sprintf(Buf + dev_size, "Clients in synch 0: %lu\n",
			stat_n_in_synch_0);
	dev_size += sprintf(Buf + dev_size, "Clients in synch job: %lu\n",
			stat_n_in_synch_job);
	dev_size += sprintf(Buf + dev_size, "Clients in query_cap: %lu\n",
			stat_n_in_query_cap);
	mutex_unlock(&stat_lock);

	/* No more to read if offset != 0 */
	if (*f_pos > dev_size)
		goto out;

	if (*f_pos + count > dev_size)
		count = dev_size - *f_pos;

	if (copy_to_user(buf, Buf, count))
		ret = -EINVAL;
	*f_pos += count;
	ret = count;

out:
	if (Buf != NULL)
		kfree(Buf);
	return ret;
}

/**
 * debugfs_b2r2_blt_stat_fops() - File operations for B2R2 BLT
 *                                statistics debugfs
 */
static const struct file_operations debugfs_b2r2_blt_stat_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_b2r2_blt_stat_read,
};
#endif

static void init_tmp_bufs(void)
{
	int i = 0;

	for (i = 0; i < MAX_TMP_BUFS_NEEDED; i++) {
		tmp_bufs[i].buf.virt_addr = dma_alloc_coherent(
			b2r2_blt_device(), MAX_TMP_BUF_SIZE,
			&tmp_bufs[i].buf.phys_addr, GFP_DMA);
		if (tmp_bufs[i].buf.virt_addr != NULL)
			tmp_bufs[i].buf.size = MAX_TMP_BUF_SIZE;
		else {
			b2r2_log_err("%s: Failed to allocate temp buffer %i\n",
								__func__, i);

			tmp_bufs[i].buf.size = 0;
		}
	}
}

static void destroy_tmp_bufs(void)
{
	int i = 0;

	for (i = 0; i < MAX_TMP_BUFS_NEEDED; i++) {
		if (tmp_bufs[i].buf.size != 0) {
			dma_free_coherent(b2r2_blt_device(),
						tmp_bufs[i].buf.size,
						tmp_bufs[i].buf.virt_addr,
						tmp_bufs[i].buf.phys_addr);

			tmp_bufs[i].buf.size = 0;
		}
	}
}

/**
 * b2r2_blt_module_init() - Module init function
 *
 * Returns 0 if OK else negative error code
 */
int b2r2_blt_module_init(void)
{
	int ret;

	mutex_init(&stat_lock);

#ifdef CONFIG_B2R2_GENERIC
	/* Initialize generic path */
	b2r2_generic_init();
#endif

	/* Initialize node splitter */
	ret = b2r2_node_split_init();
	if (ret) {
		printk(KERN_WARNING "%s: node split init fails\n",
			__func__);
		goto b2r2_node_split_init_fail;
	}

	/* Register b2r2 driver */
	ret = misc_register(&b2r2_blt_misc_dev);
	if (ret) {
		printk(KERN_WARNING "%s: registering misc device fails\n",
			__func__);
		goto b2r2_misc_register_fail;
	}

	b2r2_blt_misc_dev.this_device->coherent_dma_mask = 0xFFFFFFFF;
	b2r2_blt_dev = &b2r2_blt_misc_dev;
	b2r2_log_info("%s\n", __func__);

	/*
	 * FIXME: This stuff should be done before the first requests i.e.
	 * before misc_register, but they need the device which is not
	 * available until after misc_register.
	 */
	init_tmp_bufs();

	/* Initialize memory allocator */
	ret = b2r2_mem_init(b2r2_blt_device(), B2R2_HEAP_SIZE,
			4, sizeof(struct b2r2_node));
	if (ret) {
		printk(KERN_WARNING "%s: initializing B2R2 memhandler fails\n",
			__func__);
		goto b2r2_mem_init_fail;
	}

#ifdef CONFIG_DEBUG_FS
	/* Register debug fs */
	if (!debugfs_root_dir) {
		debugfs_root_dir = debugfs_create_dir("b2r2_blt", NULL);
		debugfs_create_file("latest_request",
				0666, debugfs_root_dir,
				0,
				&debugfs_b2r2_blt_request_fops);
		debugfs_create_file("stat",
				0666, debugfs_root_dir,
				0,
				&debugfs_b2r2_blt_stat_fops);
	}
#endif
	goto out;

b2r2_misc_register_fail:
b2r2_mem_init_fail:
	b2r2_node_split_exit();

b2r2_node_split_init_fail:
#ifdef CONFIG_B2R2_GENERIC
	b2r2_generic_exit();
#endif
out:
	return ret;
}

/**
 * b2r2_module_exit() - Module exit function
 */
void b2r2_blt_module_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	if (debugfs_root_dir) {
		debugfs_remove_recursive(debugfs_root_dir);
		debugfs_root_dir = NULL;
	}
#endif
	if (b2r2_blt_dev) {
		b2r2_log_info("%s\n", __func__);
		b2r2_mem_exit();
		destroy_tmp_bufs();
		b2r2_blt_dev = NULL;
		misc_deregister(&b2r2_blt_misc_dev);
	}

	b2r2_node_split_exit();

#if defined(CONFIG_B2R2_GENERIC)
	b2r2_generic_exit();
#endif
}

MODULE_AUTHOR("Robert Fekete <robert.fekete@stericsson.com>");
MODULE_DESCRIPTION("ST-Ericsson B2R2 Blitter module");
MODULE_LICENSE("GPL");

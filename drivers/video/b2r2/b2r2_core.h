/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * ST-Ericsson B2R2 core driver
 *
 * Author: Robert Fekete <robert.fekete@stericsson.com>
 * Author: Paul Wannback
 * for ST-Ericsson.
 *
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef __B2R2_CORE_H__
#define __B2R2_CORE_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/workqueue.h>


/**
 * b2r2_core_job_add() - Adds a job to B2R2 job queues
 *
 * The job reference count will be increased after this function
 * has been called and b2r2_core_job_release() must be called to
 * release the reference. The job callback function will be always
 * be called after the job is done or cancelled.
 *
 * @control: The b2r2 control entity
 * @job: Job to be added
 *
 * Returns 0 if OK else negative error code
 *
 */
int b2r2_core_job_add(struct b2r2_control *control,
		struct b2r2_core_job *job);

/**
 * b2r2_core_job_wait() - Waits for an added job to be done.
 *
 * @job: Job to wait for
 *
 * Returns 0 if job done else negative error code
 *
 */
int b2r2_core_job_wait(struct b2r2_core_job *job);

/**
 * b2r2_core_job_cancel() - Cancel an already added job.
 *
 * @job: Job to cancel
 *
 * Returns 0 if job cancelled or done else negative error code
 *
 */
int b2r2_core_job_cancel(struct b2r2_core_job *job);

/**
 * b2r2_core_job_find() - Finds job with given job id
 *
 * Reference count will be increased for the found job
 *
 * @control: The b2r2 control entity
 * @job_id: Job id to find
 *
 * Returns job if found, else NULL
 *
 */
struct b2r2_core_job *b2r2_core_job_find(struct b2r2_control *control,
		int job_id);

/**
 * b2r2_core_job_find_first_with_tag() - Finds first job with given tag
 *
 * Reference count will be increased for the found job.
 * This function can be used to find all jobs for a client, i.e.
 * when cancelling all jobs for a client.
 *
 * @control: The b2r2 control entity
 * @tag: Tag to find
 *
 * Returns job if found, else NULL
 *
 */
struct b2r2_core_job *b2r2_core_job_find_first_with_tag(
		struct b2r2_control *control, int tag);

/**
 * b2r2_core_job_addref() - Increase the job reference count.
 *
 * @job: Job to increase reference count for.
 * @caller: The function calling this function (for debug)
 */
void b2r2_core_job_addref(struct b2r2_core_job *job, const char *caller);

/**
 * b2r2_core_job_release() - Decrease the job reference count. The
 *                           job will be released (the release() function
 *                           will be called) when the reference count
 *                           reaches zero.
 *
 * @job: Job to decrease reference count for.
 * @caller: The function calling this function (for debug)
 */
void b2r2_core_job_release(struct b2r2_core_job *job, const char *caller);

#endif /* !defined(__B2R2_CORE_JOB_H__) */

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
 * enum b2r2_core_queue - Indicates the B2R2 queue that the job belongs to
 *
 * @B2R2_CORE_QUEUE_AQ1: Application queue 1
 * @B2R2_CORE_QUEUE_AQ2: Application queue 2
 * @B2R2_CORE_QUEUE_AQ3: Application queue 3
 * @B2R2_CORE_QUEUE_AQ4: Application queue 4
 * @B2R2_CORE_QUEUE_CQ1: Composition queue 1
 * @B2R2_CORE_QUEUE_CQ2: Composition queue 2
 * @B2R2_CORE_QUEUE_NO_OF: Number of queues
 */
enum b2r2_core_queue {
	B2R2_CORE_QUEUE_AQ1 = 0,
	B2R2_CORE_QUEUE_AQ2,
	B2R2_CORE_QUEUE_AQ3,
	B2R2_CORE_QUEUE_AQ4,
	B2R2_CORE_QUEUE_CQ1,
	B2R2_CORE_QUEUE_CQ2,
	B2R2_CORE_QUEUE_NO_OF,
};

#define B2R2_NUM_APPLICATIONS_QUEUES 4

/**
 * enum b2r2_core_job_state - Indicates the current state of the job
 *
 * @B2R2_CORE_JOB_IDLE: Never queued
 * @B2R2_CORE_JOB_QUEUED: In queue but not started yet
 * @B2R2_CORE_JOB_RUNNING: Running, executed by B2R2
 * @B2R2_CORE_JOB_DONE: Completed
 * @B2R2_CORE_JOB_CANCELED: Canceled
 */
enum b2r2_core_job_state {
	B2R2_CORE_JOB_IDLE = 0,
	B2R2_CORE_JOB_QUEUED,
	B2R2_CORE_JOB_RUNNING,
	B2R2_CORE_JOB_DONE,
	B2R2_CORE_JOB_CANCELED,
};

/**
 * struct b2r2_core_job - Represents a B2R2 core job
 *
 * @start_sentinel: Memory overwrite guard
 *
 * @tag: Client value. Used by b2r2_core_job_find_first_with_tag().
 * @prio: Job priority, from -19 up to 20. Mapped to the
 *        B2R2 application queues. Filled in by the client.
 * @first_node_address: Physical address of the first node. Filled
 *                      in by the client.
 * @last_node_address: Physical address of the last node. Filled
 *                     in by the client.
 *
 * @callback: Function that will be called when the job is done.
 * @acquire_resources: Function that allocates the resources needed
 *                     to execute the job (i.e. SRAM alloc). Must not
 *                     sleep if atomic, should fail with negative error code
 *                     if resources not available.
 * @release_resources: Function that releases the resources previously
 *                     allocated by acquire_resources (i.e. SRAM alloc).
 * @release: Function that will be called when the reference count reaches
 *           zero.
 *
 * @job_id: Unique id for this job, assigned by B2R2 core
 * @job_state: The current state of the job
 * @jiffies: Number of jiffies needed for this request
 *
 * @list: List entry element for internal list management
 * @event: Wait queue event to wait for job done
 * @work: Work queue structure, for callback implementation
 *
 * @queue: The queue that this job shall be submitted to
 * @control: B2R2 Queue control
 * @pace_control: For composition queue only
 * @interrupt_context: Context for interrupt
 *
 * @end_sentinel: Memory overwrite guard
 */
struct b2r2_core_job {
	u32 start_sentinel;

	/* Data to be filled in by client */
	int tag;
	int prio;
	u32 first_node_address;
	u32 last_node_address;
	void (*callback)(struct b2r2_core_job *);
	int (*acquire_resources)(struct b2r2_core_job *,
		bool atomic);
	void (*release_resources)(struct b2r2_core_job *,
		bool atomic);
	void (*release)(struct b2r2_core_job *);

	/* Output data, do not modify */
	int  job_id;
	enum b2r2_core_job_state job_state;
	unsigned long jiffies;

	/* Data below is internal to b2r2_core, do not modify */

	/* Reference counting */
	u32 ref_count;

	/* Internal data */
	struct list_head  list;
	wait_queue_head_t event;
	struct work_struct work;

	/* B2R2 HW data */
	enum b2r2_core_queue queue;
	u32 control;
	u32 pace_control;
	u32 interrupt_context;

	/* Timing data */
	u32 hw_start_time;
	s32 nsec_active_in_hw;

	u32 end_sentinel;
};

/**
 * b2r2_core_job_add() - Adds a job to B2R2 job queues
 *
 * The job reference count will be increased after this function
 * has been called and b2r2_core_job_release() must be called to
 * release the reference. The job callback function will be always
 * be called after the job is done or cancelled.
 *
 * @job: Job to be added
 *
 * Returns 0 if OK else negative error code
 *
 */
int b2r2_core_job_add(struct b2r2_core_job *job);

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
 * @job_id: Job id to find
 *
 * Returns job if found, else NULL
 *
 */
struct b2r2_core_job *b2r2_core_job_find(int job_id);

/**
 * b2r2_core_job_find_first_with_tag() - Finds first job with given tag
 *
 * Reference count will be increased for the found job.
 * This function can be used to find all jobs for a client, i.e.
 * when cancelling all jobs for a client.
 *
 * @tag: Tag to find
 *
 * Returns job if found, else NULL
 *
 */
struct b2r2_core_job *b2r2_core_job_find_first_with_tag(int tag);

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

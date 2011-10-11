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

/*
 * TODO: Clock address from platform data
 * Platform data should have string id instead of numbers
 * b2r2_remove, some type of runtime problem when kernel hacking
 * debug features on
 *
 * Is there already a priority list in kernel?
 * Is it possible to handle clock using clock framework?
 * uTimeOut, use mdelay instead?
 * Measure performance
 *
 * Exchange our home-cooked ref count with kernel kref? See
 *    http://lwn.net/Articles/336224/
 *
 * B2R2:
 * Source fill 2 bug
 * Check with Symbian?
 */

/* include file */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>

#include "b2r2_core.h"
#include "b2r2_global.h"
#include "b2r2_structures.h"
#include "b2r2_internal.h"
#include "b2r2_profiler_api.h"
#include "b2r2_timing.h"
#include "b2r2_debug.h"

/**
 * B2R2_DRIVER_TIMEOUT_VALUE - Busy loop timeout after soft reset
 */
#define B2R2_DRIVER_TIMEOUT_VALUE (1500)

/**
 * B2R2_CLK_FLAG - Value to write into clock reg to turn clock on
 */
#define B2R2_CLK_FLAG (0x125)

/**
 * DEBUG_CHECK_ADDREF_RELEASE - Define this to enable addref / release debug
 */
#define DEBUG_CHECK_ADDREF_RELEASE 1

#ifdef CONFIG_DEBUG_FS
/**
 * HANDLE_TIMEOUTED_JOBS - Define this to check jobs for timeout and cancel them
 */
#define HANDLE_TIMEOUTED_JOBS 1
#endif

/**
 * B2R2_CLOCK_ALWAYS_ON - Define this to disable power save clock turn off
 */
/* #define B2R2_CLOCK_ALWAYS_ON 1 */

/**
 * START_SENTINEL - Watch guard to detect job overwrites
 */
#define START_SENTINEL 0xBABEDEEA

/**
 * STOP_SENTINEL - Watch guard to detect job overwrites
 */
#define END_SENTINEL 0xDADBDCDD

/**
 * B2R2_CORE_LOWEST_PRIO - Lowest prio allowed
 */
#define B2R2_CORE_LOWEST_PRIO -19
/**
 * B2R2_CORE_HIGHEST_PRIO - Highest prio allowed
 */
#define B2R2_CORE_HIGHEST_PRIO 20


/**
 * B2R2 Hardware defines below
 */

/* - BLT_AQ_CTL */
#define	B2R2_AQ_Enab			(0x80000000)
#define B2R2_AQ_PRIOR_0 		(0x0)
#define	B2R2_AQ_PRIOR_1 		(0x1)
#define	B2R2_AQ_PRIOR_2			(0x2)
#define	B2R2_AQ_PRIOR_3			(0x3)
#define B2R2_AQ_NODE_REPEAT_INT	(0x100000)
#define B2R2_AQ_STOP_INT		(0x200000)
#define B2R2_AQ_LNA_REACH_INT	(0x400000)
#define B2R2_AQ_COMPLETED_INT	(0x800000)

/* - BLT_CTL */
#define	B2R2BLT_CTLGLOBAL_soft_reset	(0x80000000)
#define	B2R2BLT_CTLStep_By_Step		(0x20000000)
#define	B2R2BLT_CTLBig_not_little	(0x10000000)
#define	B2R2BLT_CTLMask			(0xb0000000)
#define	B2R2BLT_CTLTestMask		(0xb0000000)
#define	B2R2BLT_CTLInitialValue		(0x0)
#define	B2R2BLT_CTLAccessType		(INITIAL_TEST)
#define	B2R2BLT_CTL			(0xa00)

/* - BLT_ITS */
#define	B2R2BLT_ITSRLD_ERROR		(0x80000000)
#define	B2R2BLT_ITSAQ4_Node_Notif	(0x8000000)
#define	B2R2BLT_ITSAQ4_Node_repeat	(0x4000000)
#define	B2R2BLT_ITSAQ4_Stopped		(0x2000000)
#define	B2R2BLT_ITSAQ4_LNA_Reached	(0x1000000)
#define	B2R2BLT_ITSAQ3_Node_Notif	(0x800000)
#define	B2R2BLT_ITSAQ3_Node_repeat	(0x400000)
#define	B2R2BLT_ITSAQ3_Stopped		(0x200000)
#define	B2R2BLT_ITSAQ3_LNA_Reached	(0x100000)
#define	B2R2BLT_ITSAQ2_Node_Notif	(0x80000)
#define	B2R2BLT_ITSAQ2_Node_repeat	(0x40000)
#define	B2R2BLT_ITSAQ2_Stopped		(0x20000)
#define	B2R2BLT_ITSAQ2_LNA_Reached	(0x10000)
#define	B2R2BLT_ITSAQ1_Node_Notif	(0x8000)
#define	B2R2BLT_ITSAQ1_Node_repeat	(0x4000)
#define	B2R2BLT_ITSAQ1_Stopped		(0x2000)
#define	B2R2BLT_ITSAQ1_LNA_Reached	(0x1000)
#define	B2R2BLT_ITSCQ2_Repaced		(0x80)
#define	B2R2BLT_ITSCQ2_Node_Notif	(0x40)
#define	B2R2BLT_ITSCQ2_retriggered	(0x20)
#define	B2R2BLT_ITSCQ2_completed	(0x10)
#define	B2R2BLT_ITSCQ1_Repaced		(0x8)
#define	B2R2BLT_ITSCQ1_Node_Notif	(0x4)
#define	B2R2BLT_ITSCQ1_retriggered	(0x2)
#define	B2R2BLT_ITSCQ1_completed	(0x1)
#define	B2R2BLT_ITSMask			(0x8ffff0ff)
#define	B2R2BLT_ITSTestMask		(0x8ffff0ff)
#define	B2R2BLT_ITSInitialValue		(0x0)
#define	B2R2BLT_ITSAccessType		(INITIAL_TEST)
#define	B2R2BLT_ITS			(0xa04)

/* - BLT_STA1 */
#define	B2R2BLT_STA1BDISP_IDLE		(0x1)
#define	B2R2BLT_STA1Mask		(0x1)
#define	B2R2BLT_STA1TestMask		(0x1)
#define	B2R2BLT_STA1InitialValue	(0x1)
#define	B2R2BLT_STA1AccessType		(INITIAL_TEST)
#define	B2R2BLT_STA1			(0xa08)


#ifdef DEBUG_CHECK_ADDREF_RELEASE

/**
 * struct addref_release - Represents one addref or release. Used
 *                         to debug addref / release problems
 *
 * @addref: true if this represents an addref else it represents
 *          a release.
 * @job: The job that was referenced
 * @caller: The caller of the addref or release
 * @ref_count: The job reference count after addref / release
 */
struct addref_release {
	bool addref;
	struct b2r2_core_job *job;
	const char *caller;
	int ref_count;
};

#endif

/**
 * struct b2r2_core - Administration data for B2R2 core
 *
 * @lock: Spin lock protecting the b2r2_core structure and the B2R2 HW
 * @hw: B2R2 registers memory mapped
 * @pmu_b2r2_clock: Control of B2R2 clock
 * @log_dev: Device used for logging via dev_... functions
 *
 * @prio_queue: Queue of jobs sorted in priority order
 * @active_jobs: Array containing pointer to zero or one job per queue
 * @n_active_jobs: Number of active jobs
 * @jiffies_last_active: jiffie value when adding last active job
 * @jiffies_last_irq: jiffie value when last irq occured
 * @timeout_work: Work structure for timeout work
 *
 * @next_job_id: Contains the job id that will be assigned to the next
 *               added job.
 *
 * @clock_request_count: When non-zero, clock is on
 * @clock_off_timer: Kernel timer to handle delayed turn off of clock
 *
 * @work_queue: Work queue to handle done jobs (callbacks) and timeouts in
 *              non-interrupt context.
 *
 * @stat_n_irq: Number of interrupts (statistics)
 * @stat_n_jobs_added: Number of jobs added (statistics)
 * @stat_n_jobs_removed: Number of jobs removed (statistics)
 * @stat_n_jobs_in_prio_list: Number of jobs in prio list (statistics)
 *
 * @debugfs_root_dir: Root directory for B2R2 debugfs
 *
 * @ar: Circular array of addref / release debug structs
 * @ar_write: Where next write will occur
 * @ar_read: First valid place to read. When ar_read == ar_write then
 *           the array is empty.
 */
struct b2r2_core {
	spinlock_t       lock;

	struct b2r2_memory_map *hw;

	u8 op_size;
	u8 ch_size;
	u8 pg_size;
	u8 mg_size;
	u16 min_req_time;
	int irq;

	struct device *log_dev;

	struct list_head prio_queue;

	struct b2r2_core_job *active_jobs[B2R2_CORE_QUEUE_NO_OF];
	unsigned long    n_active_jobs;

	unsigned long    jiffies_last_active;
	unsigned long    jiffies_last_irq;
#ifdef HANDLE_TIMEOUTED_JOBS
	struct delayed_work     timeout_work;
#endif
	int              next_job_id;

	unsigned long    clock_request_count;
	struct timer_list clock_off_timer;

	struct workqueue_struct *work_queue;

	/* Statistics */
	unsigned long    stat_n_irq;
	unsigned long    stat_n_jobs_added;
	unsigned long    stat_n_jobs_removed;

	unsigned long    stat_n_jobs_in_prio_list;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root_dir;
	struct dentry *debugfs_regs_dir;
#endif

#ifdef DEBUG_CHECK_ADDREF_RELEASE
	/* Tracking release bug...*/
	struct addref_release ar[100];
	int ar_write;
	int ar_read;
#endif

	/* Power management variables */
	struct mutex domain_lock;
	struct delayed_work domain_disable_work;

	/*
	 * We need to keep track of both the number of domain_enable/disable()
	 * calls and whether the power was actually turned off, since the
	 * power off is done in a delayed job.
	 */
	bool domain_enabled;
	int domain_request_count;

	struct clk *b2r2_clock;
	struct regulator *b2r2_reg;
};

/**
 * b2r2_core - Administration data for B2R2 core (singleton)
 */
static struct b2r2_core   b2r2_core;

/* Local functions */
static void check_prio_list(bool atomic);
static void  clear_interrupts(void);
static void trigger_job(struct b2r2_core_job *job);
static void exit_job_list(struct list_head *job_list);
static int get_next_job_id(void);
static void job_work_function(struct work_struct *ptr);
static void init_job(struct b2r2_core_job *job);
static void insert_into_prio_list(struct b2r2_core_job *job);
static struct b2r2_core_job *find_job_in_list(
	int job_id,
	struct list_head *list);
static struct b2r2_core_job *find_job_in_active_jobs(int job_id);
static struct b2r2_core_job *find_tag_in_list(
	int tag,
	struct list_head *list);
static struct b2r2_core_job *find_tag_in_active_jobs(int tag);

static int domain_enable(void);
static void domain_disable(void);

static void stop_queue(enum b2r2_core_queue queue);

#ifdef HANDLE_TIMEOUTED_JOBS
static void printk_regs(void);
static int hw_reset(void);
static void timeout_work_function(struct work_struct *ptr);
#endif

static void reset_hw_timer(struct b2r2_core_job *job);
static void start_hw_timer(struct b2r2_core_job *job);
static void stop_hw_timer(struct b2r2_core_job *job);

static int init_hw(void);
static void exit_hw(void);

/* Tracking release bug... */
#ifdef DEBUG_CHECK_ADDREF_RELEASE
/**
 * ar_add() - Adds an addref or a release to the array
 *
 * @job: The job that has been referenced
 * @caller: The caller of addref / release
 * @addref: true if it is an addref else false for release
 */
void ar_add(struct b2r2_core_job *job, const char *caller, bool addref)
{
	b2r2_core.ar[b2r2_core.ar_write].addref = addref;
	b2r2_core.ar[b2r2_core.ar_write].job = job;
	b2r2_core.ar[b2r2_core.ar_write].caller = caller;
	b2r2_core.ar[b2r2_core.ar_write].ref_count = job->ref_count;
	b2r2_core.ar_write = (b2r2_core.ar_write + 1) %
		ARRAY_SIZE(b2r2_core.ar);
	if (b2r2_core.ar_write == b2r2_core.ar_read)
		b2r2_core.ar_read = (b2r2_core.ar_read + 1) %
			ARRAY_SIZE(b2r2_core.ar);
}

/**
 * sprintf_ar() - Writes all addref / release to a string buffer
 *
 * @buf: Receiving character bufefr
 * @job: Which job to write or NULL for all
 *
 * NOTE! No buffer size check!!
 */
char *sprintf_ar(char *buf, struct b2r2_core_job *job)
{
	int i;
	int size = 0;

	for (i = b2r2_core.ar_read;
	     i != b2r2_core.ar_write;
	     i = (i + 1) % ARRAY_SIZE(b2r2_core.ar)) {
		struct addref_release *ar = &b2r2_core.ar[i];
		if (!job || job == ar->job)
			size += sprintf(buf + size,
					"%s on %p from %s, ref = %d\n",
					ar->addref ? "addref" : "release",
					ar->job, ar->caller, ar->ref_count);
	}

	return buf;
}

/**
 * printk_ar() - Writes all addref / release using dev_info
 *
 * @job: Which job to write or NULL for all
 */
void printk_ar(struct b2r2_core_job *job)
{
	int i;

	for (i = b2r2_core.ar_read;
	     i != b2r2_core.ar_write;
	     i = (i + 1) % ARRAY_SIZE(b2r2_core.ar)) {
		struct addref_release *ar = &b2r2_core.ar[i];
		if (!job || job == ar->job)
			b2r2_log_info("%s on %p from %s,"
				 " ref = %d\n",
				 ar->addref ? "addref" : "release",
				 ar->job, ar->caller, ar->ref_count);
	}
}
#endif

/**
 * internal_job_addref() - Increments the reference count for a job
 *
 * @job: Which job to increment reference count for
 * @caller: Name of function calling addref (for debug)
 *
 * Note that b2r2_core.lock _must_ be held
 */
static void internal_job_addref(struct b2r2_core_job *job, const char *caller)
{
	u32 ref_count;

	b2r2_log_info("%s (%p) (from %s)\n",
		__func__, job, caller);

	/* Sanity checks */
	BUG_ON(job == NULL);

	if (job->start_sentinel != START_SENTINEL ||
	    job->end_sentinel != END_SENTINEL ||
	    job->ref_count == 0 || job->ref_count > 10)	{
		b2r2_log_info(
			 "%s: (%p) start=%X end=%X ref_count=%d\n",
			 __func__, job, job->start_sentinel,
			 job->end_sentinel, job->ref_count);

	/* Something is wrong, print the addref / release array */
#ifdef DEBUG_CHECK_ADDREF_RELEASE
		printk_ar(NULL);
#endif
	}


	BUG_ON(job->start_sentinel != START_SENTINEL);
	BUG_ON(job->end_sentinel != END_SENTINEL);

	/* Do the actual reference count increment */
	ref_count = ++job->ref_count;

#ifdef DEBUG_CHECK_ADDREF_RELEASE
	/* Keep track of addref / release */
	ar_add(job, caller, true);
#endif

	b2r2_log_info("%s called from %s (%p): Ref Count is %d\n",
		__func__, caller, job, job->ref_count);
}

/**
 * internal_job_release() - Decrements the reference count for a job
 *
 * @job: Which job to decrement reference count for
 * @caller: Name of function calling release (for debug)
 *
 * Returns true if job_release should be called by caller
 * (reference count reached zero).
 *
 * Note that b2r2_core.lock _must_ be held
 */
bool internal_job_release(struct b2r2_core_job *job, const char *caller)
{
	u32 ref_count;
	bool call_release = false;

	b2r2_log_info("%s (%p) (from %s)\n",
		__func__, job, caller);

	/* Sanity checks */
	BUG_ON(job == NULL);

	if (job->start_sentinel != START_SENTINEL ||
	    job->end_sentinel != END_SENTINEL ||
	    job->ref_count == 0 || job->ref_count > 10) {
		b2r2_log_info(
			 "%s: (%p) start=%X end=%X ref_count=%d\n",
			 __func__, job, job->start_sentinel,
			 job->end_sentinel, job->ref_count);

#ifdef DEBUG_CHECK_ADDREF_RELEASE
		printk_ar(NULL);
#endif
	}


	BUG_ON(job->start_sentinel != START_SENTINEL);
	BUG_ON(job->end_sentinel != END_SENTINEL);

	BUG_ON(job->ref_count == 0 || job->ref_count > 10);

	/* Do the actual decrement */
	ref_count = --job->ref_count;
#ifdef DEBUG_CHECK_ADDREF_RELEASE
	ar_add(job, caller, false);
#endif
	b2r2_log_info("%s called from %s (%p) Ref Count is %d\n",
		__func__, caller, job, ref_count);

	if (!ref_count && job->release) {
		call_release = true;
		/* Job will now cease to exist */
		job->start_sentinel = 0xFFFFFFFF;
		job->end_sentinel = 0xFFFFFFFF;
	}
	return call_release;
}



/* Exported functions */

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
void b2r2_core_job_addref(struct b2r2_core_job *job, const char *caller)
{
	unsigned long flags;
	spin_lock_irqsave(&b2r2_core.lock, flags);
	internal_job_addref(job, caller);
	spin_unlock_irqrestore(&b2r2_core.lock, flags);
}

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
void b2r2_core_job_release(struct b2r2_core_job *job, const char *caller)
{
	unsigned long flags;
	bool call_release = false;
	spin_lock_irqsave(&b2r2_core.lock, flags);
	call_release = internal_job_release(job, caller);
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	if (call_release)
		job->release(job);
}

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
int b2r2_core_job_add(struct b2r2_core_job *job)
{
	unsigned long flags;

	b2r2_log_info("%s (%p)\n", __func__, job);

	/* Enable B2R2 */
	domain_enable();

	spin_lock_irqsave(&b2r2_core.lock, flags);
	b2r2_core.stat_n_jobs_added++;

	/* Initialise internal job data */
	init_job(job);

	/* Initial reference, should be released by caller of this function */
	job->ref_count = 1;

	/* Insert job into prio list */
	insert_into_prio_list(job);

	/* Check if we can dispatch job */
	check_prio_list(false);
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	return 0;
}

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
struct b2r2_core_job *b2r2_core_job_find(int job_id)
{
	unsigned long flags;
	struct b2r2_core_job *job;

	b2r2_log_info("%s (%d)\n", __func__, job_id);

	spin_lock_irqsave(&b2r2_core.lock, flags);
	/* Look through prio queue */
	job = find_job_in_list(job_id, &b2r2_core.prio_queue);

	if (!job)
		job = find_job_in_active_jobs(job_id);

	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	return job;
}

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
struct b2r2_core_job *b2r2_core_job_find_first_with_tag(int tag)
{
	unsigned long flags;
	struct b2r2_core_job *job;

	b2r2_log_info("%s (%d)\n", __func__, tag);

	spin_lock_irqsave(&b2r2_core.lock, flags);
	/* Look through prio queue */
	job = find_tag_in_list(tag, &b2r2_core.prio_queue);

	if (!job)
		job = find_tag_in_active_jobs(tag);

	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	return job;
}

/**
 * is_job_done() - Spin lock protected check if job is done
 *
 * @job: Job to check
 *
 * Returns true if job is done or cancelled
 *
 * b2r2_core.lock must _NOT_ be held when calling this function
 */
static bool is_job_done(struct b2r2_core_job *job)
{
	unsigned long flags;
	bool job_is_done;

	spin_lock_irqsave(&b2r2_core.lock, flags);
	job_is_done =
		job->job_state != B2R2_CORE_JOB_QUEUED &&
		job->job_state != B2R2_CORE_JOB_RUNNING;
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	return job_is_done;
}

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
int b2r2_core_job_wait(struct b2r2_core_job *job)
{
	int ret = 0;

	b2r2_log_info("%s (%p)\n", __func__, job);
	/* Check that we have the job */
	if (job->job_state == B2R2_CORE_JOB_IDLE) {
		/* Never or not queued */
		b2r2_log_info("%s: Job not queued\n", __func__);
		return -ENOENT;
	}

	/* Wait for the job to be done */
	ret = wait_event_interruptible(
		job->event,
		is_job_done(job));

	if (ret)
		b2r2_log_warn(
			 "%s: wait_event_interruptible returns %d, state is %d",
			 __func__, ret, job->job_state);
	return ret;
}

/**
 * cancel_job() - Cancels a job (removes it from prio list or active jobs) and
 *                calls the job callback
 *
 * @job: Job to cancel
 *
 * Returns true if the job was found and cancelled
 *
 * b2r2_core.lock must be held when calling this function
 */
static bool cancel_job(struct b2r2_core_job *job)
{
	bool found_job = false;
	bool job_was_active = false;

	/* Remove from prio list */
	if (job->job_state == B2R2_CORE_JOB_QUEUED) {
		list_del_init(&job->list);
		found_job = true;
	}

	/* Remove from active jobs */
	if (!found_job) {
		if (b2r2_core.n_active_jobs > 0) {
			int i;

			/* Look for timeout:ed jobs and put them in tmp list */
			for (i = 0;
			     i < ARRAY_SIZE(b2r2_core.active_jobs);
			     i++) {
				if (b2r2_core.active_jobs[i] == job) {
					stop_queue((enum b2r2_core_queue)i);
					stop_hw_timer(job);
					b2r2_core.active_jobs[i] = NULL;
					b2r2_core.n_active_jobs--;
					found_job = true;
					job_was_active = true;
				}
			}
		}
	}


	/* Handle done list & callback */
	if (found_job) {
		/* Job is canceled */
		job->job_state = B2R2_CORE_JOB_CANCELED;

		queue_work(b2r2_core.work_queue, &job->work);

		/* Statistics */
		if (!job_was_active)
			b2r2_core.stat_n_jobs_in_prio_list--;

	}

	return found_job;
}

/* b2r2_core.lock _must_ _NOT_ be held when calling this function */
int b2r2_core_job_cancel(struct b2r2_core_job *job)
{
	unsigned long flags;
	int ret = 0;

	b2r2_log_info("%s (%p) (%d)\n", __func__,
		job, job->job_state);
	/* Check that we have the job */
	if (job->job_state == B2R2_CORE_JOB_IDLE) {
		/* Never or not queued */
		b2r2_log_info("%s: Job not queued\n", __func__);
		return -ENOENT;
	}

	/* Remove from prio list */
	spin_lock_irqsave(&b2r2_core.lock, flags);
	cancel_job(job);
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	return ret;
}

/* LOCAL FUNCTIONS BELOW */

#define B2R2_DOMAIN_DISABLE_TIMEOUT (HZ/100)

static void domain_disable_work_function(struct work_struct *work)
{
	if (!mutex_trylock(&b2r2_core.domain_lock))
		return;

	if (b2r2_core.domain_request_count == 0) {
		exit_hw();
		clk_disable(b2r2_core.b2r2_clock);
		regulator_disable(b2r2_core.b2r2_reg);
		b2r2_core.domain_enabled = false;
	}

	mutex_unlock(&b2r2_core.domain_lock);
}

#define B2R2_REGULATOR_RETRY_COUNT 10

static int domain_enable(void)
{
	mutex_lock(&b2r2_core.domain_lock);
	b2r2_core.domain_request_count++;

	if (!b2r2_core.domain_enabled) {
		int retry = 0;
		int ret;

again:
		/*
		 * Since regulator_enable() may sleep we have to handle
		 * interrupts.
		 */
		ret = regulator_enable(b2r2_core.b2r2_reg);
		if ((ret == -EAGAIN) &&
				((retry++) < B2R2_REGULATOR_RETRY_COUNT))
			goto again;
		else if (ret < 0)
			goto regulator_enable_failed;

		clk_enable(b2r2_core.b2r2_clock);
		if (init_hw() < 0)
			goto init_hw_failed;
		b2r2_core.domain_enabled = true;
	}

	mutex_unlock(&b2r2_core.domain_lock);

	return 0;

init_hw_failed:
	b2r2_log_err("%s: Could not initialize hardware!\n", __func__);

	clk_disable(b2r2_core.b2r2_clock);

	if (regulator_disable(b2r2_core.b2r2_reg) < 0)
		b2r2_log_err("%s: regulator_disable failed!\n", __func__);

regulator_enable_failed:
	b2r2_core.domain_request_count--;
	mutex_unlock(&b2r2_core.domain_lock);

	return -EFAULT;
}

static void domain_disable(void)
{
	mutex_lock(&b2r2_core.domain_lock);

	if (b2r2_core.domain_request_count == 0) {
		b2r2_log_err("%s: Unbalanced domain_disable()\n", __func__);
	} else {
		b2r2_core.domain_request_count--;

		/* Cancel any existing work */
		cancel_delayed_work_sync(&b2r2_core.domain_disable_work);

		/* Add a work to disable the power and clock after a delay */
		queue_delayed_work(b2r2_core.work_queue,
				&b2r2_core.domain_disable_work,
				B2R2_DOMAIN_DISABLE_TIMEOUT);
	}

	mutex_unlock(&b2r2_core.domain_lock);
}

/**
 * stop_queue() - Stops the specified queue.
 */
static void stop_queue(enum b2r2_core_queue queue)
{
	/* TODO: Implement! If this function is not implemented canceled jobs will
	use b2r2 which is a waste of resources. Not stopping jobs will also screw up
	the hardware timing, the job the canceled job intrerrupted (if any) will be
	billed for the time between the point where the job is cancelled and when it
	stops. */
}

/**
 * exit_job_list() - Empties a job queue by canceling the jobs
 *
 * b2r2_core.lock _must_ be held when calling this function
 */
static void exit_job_list(struct list_head *job_queue)
{
	while (!list_empty(job_queue)) {
		struct b2r2_core_job *job =
			list_entry(job_queue->next,
				   struct b2r2_core_job,
				   list);
		/* Add reference to prevent job from disappearing
		   in the middle of our work, released below */
		internal_job_addref(job, __func__);

		cancel_job(job);

		/* Matching release to addref above */
		internal_job_release(job, __func__);

	}
}

/**
 * get_next_job_id() - Return a new job id.
 */
static int get_next_job_id(void)
{
	int job_id;

	if (b2r2_core.next_job_id < 1)
		b2r2_core.next_job_id = 1;
	job_id = b2r2_core.next_job_id++;

	return job_id;
}

/**
 * job_work_function() - Work queue function that calls callback(s) and
 *                       checks if B2R2 can accept a new job
 *
 * @ptr: Pointer to work struct (embedded in struct b2r2_core_job)
 */
static void job_work_function(struct work_struct *ptr)
{
	unsigned long flags;
	struct b2r2_core_job    *job = container_of(
		ptr, struct b2r2_core_job, work);

	/* Disable B2R2 */
	domain_disable();

	/* Release resources */
	if (job->release_resources)
		job->release_resources(job, false);

	spin_lock_irqsave(&b2r2_core.lock, flags);

	/* Dispatch a new job if possible */
	check_prio_list(false);

	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	/* Tell the client */
	if (job->callback)
		job->callback(job);

	/* Drop our reference, matches the
	   addref in handle_queue_event or b2r2_core_job_cancel */
	b2r2_core_job_release(job, __func__);
}

#ifdef HANDLE_TIMEOUTED_JOBS
/**
 * timeout_work_function() - Work queue function that checks for
 *                           timeout:ed jobs. B2R2 might silently refuse
 *                           to execute some jobs, i.e. SRC2 fill
 *
 * @ptr: Pointer to work struct (embedded in struct b2r2_core)
 *
 */
static void timeout_work_function(struct work_struct *ptr)
{
	unsigned long flags;
	struct list_head job_list;

	INIT_LIST_HEAD(&job_list);

	/* Cancel all jobs if too long time since last irq */
	spin_lock_irqsave(&b2r2_core.lock, flags);
	if (b2r2_core.n_active_jobs > 0) {
		unsigned long diff =
			(long) jiffies - (long) b2r2_core.jiffies_last_irq;
		if (diff > HZ/2) {
			/* Active jobs and more than a second since last irq! */
			int i;

			/* Look for timeout:ed jobs and put them in tmp list. It's
			important that the application queues are killed in order
			of decreasing priority */
			for (i = 0;
			     i < ARRAY_SIZE(b2r2_core.active_jobs);
			     i++) {
				struct b2r2_core_job *job =
					b2r2_core.active_jobs[i];

				if (job) {
					stop_hw_timer(job);

					b2r2_core.active_jobs[i] = NULL;
					b2r2_core.n_active_jobs--;
					list_add_tail(&job->list,
						      &job_list);
				}
			}

			/* Print the B2R2 register and reset B2R2 */
			printk_regs();
			hw_reset();
		}
	}
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	/* Handle timeout:ed jobs */
	spin_lock_irqsave(&b2r2_core.lock, flags);
	while (!list_empty(&job_list)) {
		struct b2r2_core_job *job =
			list_entry(job_list.next,
				   struct b2r2_core_job,
				   list);

		b2r2_log_warn("%s: Job timeout\n", __func__);

		list_del_init(&job->list);

		/* Job is cancelled */
		job->job_state = B2R2_CORE_JOB_CANCELED;

		/* Handle done */
		wake_up_interruptible(&job->event);

		/* Job callbacks handled via work queue */
		queue_work(b2r2_core.work_queue, &job->work);
	}

	/* Requeue delayed work */
	if (b2r2_core.n_active_jobs)
		queue_delayed_work(
			b2r2_core.work_queue,
			&b2r2_core.timeout_work, HZ/2);

	spin_unlock_irqrestore(&b2r2_core.lock, flags);
}
#endif

/**
 * reset_hw_timer() - Resets a job's hardware timer. Must be called before
 *                    the timer is used.
 *
 * @job: Pointer to job struct
 *
 * b2r2_core.lock _must_ be held when calling this function
 */
static void reset_hw_timer(struct b2r2_core_job *job)
{
	job->nsec_active_in_hw = 0;
}

/**
 * start_hw_timer() - Times how long a job spends in hardware (active).
 *                    Should be called immediatly before starting the
 *                    hardware.
 *
 * @job: Pointer to job struct
 *
 * b2r2_core.lock _must_ be held when calling this function
 */
static void start_hw_timer(struct b2r2_core_job *job)
{
	job->hw_start_time = b2r2_get_curr_nsec();
}

/**
 * stop_hw_timer() - Times how long a job spends in hardware (active).
 *                   Should be called immediatly after the hardware has
 *                   finished.
 *
 * @job: Pointer to job struct
 *
 * b2r2_core.lock _must_ be held when calling this function
 */
static void stop_hw_timer(struct b2r2_core_job *job)
{
	/* Assumes only app queues are used, which is the case right now. */
	/* Not 100% accurate. When a higher prio job interrupts a lower prio job it does
	so after the current node of the low prio job has finished. Currently we can not
	sense when the actual switch takes place so the time reported for a job that
	interrupts a lower prio job will on average contain the time it takes to process
	half a node in the lower prio job in addition to the time it takes to process the
	job's own nodes. This could possibly be solved by adding node notifications but
	that would involve a significant amount of work and consume system resources due
	to the extra interrupts. */
	/* If a job takes more than ~2s (absolute time, including idleing in the hardware)
	the state of the hardware timer will be corrupted and it will not report valid
	values until b2r2 becomes idle (no active jobs on any queues). The maximum length
	can possibly be increased by using 64 bit integers. */

	int i;

	u32 stop_time_raw = b2r2_get_curr_nsec();
	/* We'll add an offset to all positions in time to make the current time equal to
	0xFFFFFFFF. This way we can compare positions in time to each other without having
	to wory about wrapping (so long as all positions in time are in the past). */
	u32 stop_time = 0xFFFFFFFF;
	u32 time_pos_offset = 0xFFFFFFFF - stop_time_raw;
	u32 nsec_in_hw = stop_time - (job->hw_start_time + time_pos_offset);
	job->nsec_active_in_hw += (s32)nsec_in_hw;

	/* Check if we have delayed the start of higher prio jobs. Can happen as queue
	switching only can be done between nodes. */
	for (i = (int)job->queue - 1; i >= (int)B2R2_CORE_QUEUE_AQ1; i--) {
		struct b2r2_core_job *queue_active_job = b2r2_core.active_jobs[i];
		if (NULL == queue_active_job)
			continue;

		queue_active_job->hw_start_time = stop_time_raw;
	}

	/* Check if the job has stolen time from lower prio jobs */
	for (i = (int)job->queue + 1; i < B2R2_NUM_APPLICATIONS_QUEUES; i++) {
		struct b2r2_core_job *queue_active_job = b2r2_core.active_jobs[i];
		u32 queue_active_job_hw_start_time;

		if (NULL == queue_active_job)
			continue;

		queue_active_job_hw_start_time = queue_active_job->hw_start_time + time_pos_offset;

		if (queue_active_job_hw_start_time < stop_time) {
			u32 queue_active_job_nsec_in_hw = stop_time - queue_active_job_hw_start_time;
			u32 num_stolen_nsec = min(queue_active_job_nsec_in_hw, nsec_in_hw);

			queue_active_job->nsec_active_in_hw -= (s32)num_stolen_nsec;

			nsec_in_hw -= num_stolen_nsec;
			stop_time -= num_stolen_nsec;
		}

		if (0 == nsec_in_hw)
			break;
	}
}

/**
 * init_job() - Initializes a job structure from filled in client data.
 *              Reference count will be set to 1
 *
 * @job: Job to initialize
 */
static void init_job(struct b2r2_core_job *job)
{
	job->start_sentinel = START_SENTINEL;
	job->end_sentinel = END_SENTINEL;

	/* Get a job id*/
	job->job_id = get_next_job_id();

	/* Job is idle, never queued */
	job->job_state = B2R2_CORE_JOB_IDLE;

	/* Initialize internal data */
	INIT_LIST_HEAD(&job->list);
	init_waitqueue_head(&job->event);
	INIT_WORK(&job->work, job_work_function);

	/* Map given prio to B2R2 queues */
	if (job->prio < B2R2_CORE_LOWEST_PRIO)
		job->prio = B2R2_CORE_LOWEST_PRIO;
	else if (job->prio > B2R2_CORE_HIGHEST_PRIO)
		job->prio = B2R2_CORE_HIGHEST_PRIO;

	if (job->prio > 10) {
		job->queue = B2R2_CORE_QUEUE_AQ1;
		job->interrupt_context =
			(B2R2BLT_ITSAQ1_LNA_Reached);
		job->control = (B2R2_AQ_Enab | B2R2_AQ_PRIOR_3);
	} else if (job->prio > 0) {
		job->queue = B2R2_CORE_QUEUE_AQ2;
		job->interrupt_context =
			(B2R2BLT_ITSAQ2_LNA_Reached);
		job->control = (B2R2_AQ_Enab | B2R2_AQ_PRIOR_2);
	} else if (job->prio > -10) {
		job->queue = B2R2_CORE_QUEUE_AQ3;
		job->interrupt_context =
			(B2R2BLT_ITSAQ3_LNA_Reached);
		job->control = (B2R2_AQ_Enab | B2R2_AQ_PRIOR_1);
	} else {
		job->queue = B2R2_CORE_QUEUE_AQ4;
		job->interrupt_context =
			(B2R2BLT_ITSAQ4_LNA_Reached);
		job->control = (B2R2_AQ_Enab | B2R2_AQ_PRIOR_0);
	}
}

/**
 * clear_interrupts() - Disables all interrupts
 *
 * b2r2_core.lock must be held
 */
static void  clear_interrupts(void)
{
	writel(0x0, &b2r2_core.hw->BLT_ITM0);
	writel(0x0, &b2r2_core.hw->BLT_ITM1);
	writel(0x0, &b2r2_core.hw->BLT_ITM2);
	writel(0x0, &b2r2_core.hw->BLT_ITM3);
}

/**
 * insert_into_prio_list() - Inserts the job into the sorted list of jobs.
 *                           The list is sorted by priority.
 *
 * @job: Job to insert
 *
 * b2r2_core.lock must be held
 */
static void insert_into_prio_list(struct b2r2_core_job *job)
{
	/* Ref count is increased when job put in list,
	   should be released when job is removed from list */
	internal_job_addref(job, __func__);

	b2r2_core.stat_n_jobs_in_prio_list++;

	/* Sort in the job */
	if (list_empty(&b2r2_core.prio_queue))
		list_add_tail(&job->list, &b2r2_core.prio_queue);
	else {
		struct b2r2_core_job *first_job =
			list_entry(b2r2_core.prio_queue.next,
				   struct b2r2_core_job, list);
		struct b2r2_core_job *last_job =
			list_entry(b2r2_core.prio_queue.prev,
				   struct b2r2_core_job, list);

		/* High prio job? */
		if (job->prio > first_job->prio)
			/* Insert first */
			list_add(&job->list, &b2r2_core.prio_queue);
		else if (job->prio <= last_job->prio)
			/* Insert last */
			list_add_tail(&job->list, &b2r2_core.prio_queue);
		else {
			/* We need to find where to put it */
			struct list_head *ptr;

			list_for_each(ptr, &b2r2_core.prio_queue) {
				struct b2r2_core_job *list_job =
					list_entry(ptr, struct b2r2_core_job,
						   list);
				if (job->prio > list_job->prio) {
					/* Add before */
					list_add_tail(&job->list,
						      &list_job->list);
					break;
				}
			}
		}
	}
	/* The job is now queued */
	job->job_state = B2R2_CORE_JOB_QUEUED;
}

/**
 * check_prio_list() - Checks if the first job(s) in the prio list can
 *                     be dispatched to B2R2
 *
 * @atomic: true if in atomic context (i.e. interrupt context)
 *
 * b2r2_core.lock must be held
 */
static void check_prio_list(bool atomic)
{
	bool dispatched_job;
	int n_dispatched = 0;

	/* Do we have anything in our prio list? */
	do {
		dispatched_job = false;
		if (!list_empty(&b2r2_core.prio_queue)) {
			/* The first job waiting */
			struct b2r2_core_job *job =
				list_first_entry(&b2r2_core.prio_queue,
						 struct b2r2_core_job,
						 list);

			/* Is the B2R2 queue available? */
			if (b2r2_core.active_jobs[job->queue] == NULL) {
				/* Can we acquire resources? */
				if (!job->acquire_resources ||
				    job->acquire_resources(job, atomic) == 0) {
					/* Ok to dispatch job */

					/* Remove from list */
					list_del_init(&job->list);

					/* The job is now active */
					b2r2_core.active_jobs[job->queue] = job;
					b2r2_core.n_active_jobs++;
					job->jiffies = jiffies;
					b2r2_core.jiffies_last_active =
						jiffies;

					/* Kick off B2R2 */
					trigger_job(job);

					dispatched_job = true;
					n_dispatched++;

#ifdef HANDLE_TIMEOUTED_JOBS
					/* Check in one half second
					   if it hangs */
					queue_delayed_work(
						b2r2_core.work_queue,
						&b2r2_core.timeout_work,
						HZ/2);
#endif
				} else {
					/* No resources */
					if (!atomic &&
					    b2r2_core.n_active_jobs == 0) {
						b2r2_log_warn(
							"%s: No resource",
							__func__);
						cancel_job(job);
					}
				}
			}
		}
	} while (dispatched_job);

	b2r2_core.stat_n_jobs_in_prio_list -= n_dispatched;
}

/**
 * find_job_in_list() - Finds job with job_id in list
 *
 * @jobid: Job id to find
 * @list: List to find job id in
 *
 * Reference count will be incremented for found job.
 *
 * b2r2_core.lock must be held
 */
static struct b2r2_core_job *find_job_in_list(int job_id,
					      struct list_head *list)
{
	struct list_head *ptr;

	list_for_each(ptr, list) {
		struct b2r2_core_job *job =
			list_entry(ptr, struct b2r2_core_job, list);
		if (job->job_id == job_id) {
			/* Increase reference count, should be released by
			   the caller of b2r2_core_job_find */
			internal_job_addref(job, __func__);
			return job;
		}
	}
	return NULL;
}

/**
 * find_job_in_active_jobs() - Finds job in active job queues
 *
 * @jobid: Job id to find
 *
 * Reference count will be incremented for found job.
 *
 * b2r2_core.lock must be held
 */
static struct b2r2_core_job *find_job_in_active_jobs(int job_id)
{
	int i;
	struct b2r2_core_job *found_job = NULL;

	if (b2r2_core.n_active_jobs) {
		for (i = 0; i < ARRAY_SIZE(b2r2_core.active_jobs); i++) {
			struct b2r2_core_job *job = b2r2_core.active_jobs[i];

			if (job && job->job_id == job_id) {
				internal_job_addref(job, __func__);
				found_job = job;
				break;
			}
		}
	}
	return found_job;
}

/**
 * find_tag_in_list() - Finds first job with tag in list
 *
 * @tag: Tag to find
 * @list: List to find job id in
 *
 * Reference count will be incremented for found job.
 *
 * b2r2_core.lock must be held
 */
static struct b2r2_core_job *find_tag_in_list(int tag, struct list_head *list)
{
	struct list_head *ptr;

	list_for_each(ptr, list) {
		struct b2r2_core_job *job =
			list_entry(ptr, struct b2r2_core_job, list);
		if (job->tag == tag) {
			/* Increase reference count, should be released by
			   the caller of b2r2_core_job_find */
			internal_job_addref(job, __func__);
			return job;
		}
	}
	return NULL;
}

/**
 * find_tag_in_active_jobs() - Finds job with tag in active job queues
 *
 * @tag: Tag to find
 *
 * Reference count will be incremented for found job.
 *
 * b2r2_core.lock must be held
 */
static struct b2r2_core_job *find_tag_in_active_jobs(int tag)
{
	int i;
	struct b2r2_core_job *found_job = NULL;

	if (b2r2_core.n_active_jobs) {
		for (i = 0; i < ARRAY_SIZE(b2r2_core.active_jobs); i++) {
			struct b2r2_core_job *job = b2r2_core.active_jobs[i];

			if (job && job->tag == tag) {
				internal_job_addref(job, __func__);
				found_job = job;
				break;
			}
		}
	}
	return found_job;
}


#ifdef HANDLE_TIMEOUTED_JOBS
/**
 * hw_reset() - Resets B2R2 hardware
 *
 * b2r2_core.lock must be held
 */
static int hw_reset(void)
{
	u32 uTimeOut = B2R2_DRIVER_TIMEOUT_VALUE;

	/* Tell B2R2 to reset */
	writel(readl(&b2r2_core.hw->BLT_CTL) | B2R2BLT_CTLGLOBAL_soft_reset,
		&b2r2_core.hw->BLT_CTL);
	writel(0x00000000, &b2r2_core.hw->BLT_CTL);

	b2r2_log_info("wait for B2R2 to be idle..\n");

	/** Wait for B2R2 to be idle (on a timeout rather than while loop) */
	while ((uTimeOut > 0) &&
	       ((readl(&b2r2_core.hw->BLT_STA1) &
		 B2R2BLT_STA1BDISP_IDLE) == 0x0))
		uTimeOut--;

	if (uTimeOut == 0) {
		b2r2_log_warn(
			 "error-> after software reset B2R2 is not idle\n");
		return -EAGAIN;
	}

	return 0;

}
#endif

/**
 * trigger_job() - Put job in B2R2 HW queue
 *
 * @job: Job to trigger
 *
 * b2r2_core.lock must be held
 */
static void trigger_job(struct b2r2_core_job *job)
{
	/* Debug prints */
	b2r2_log_info("queue 0x%x \n", job->queue);
	b2r2_log_info("BLT TRIG_IP 0x%x (first node)\n",
		job->first_node_address);
	b2r2_log_info("BLT LNA_CTL 0x%x (last node)\n",
		job->last_node_address);
	b2r2_log_info("BLT TRIG_CTL 0x%x \n", job->control);
	b2r2_log_info("BLT PACE_CTL 0x%x \n", job->pace_control);

	reset_hw_timer(job);
	job->job_state = B2R2_CORE_JOB_RUNNING;

	/* Enable interrupt */
	writel(readl(&b2r2_core.hw->BLT_ITM0) | job->interrupt_context,
		&b2r2_core.hw->BLT_ITM0);

	writel(min_t(u8, max_t(u8, b2r2_core.op_size, B2R2_PLUG_OPCODE_SIZE_8),
				B2R2_PLUG_OPCODE_SIZE_64),
			&b2r2_core.hw->PLUGS1_OP2);
	writel(min_t(u8, b2r2_core.ch_size, B2R2_PLUG_CHUNK_SIZE_128),
			&b2r2_core.hw->PLUGS1_CHZ);
	writel(min_t(u8, b2r2_core.mg_size, B2R2_PLUG_MESSAGE_SIZE_128) |
				(b2r2_core.min_req_time << 16),
			&b2r2_core.hw->PLUGS1_MSZ);
	writel(min_t(u8, b2r2_core.pg_size, B2R2_PLUG_PAGE_SIZE_256),
			&b2r2_core.hw->PLUGS1_PGZ);

	writel(min_t(u8, max_t(u8, b2r2_core.op_size, B2R2_PLUG_OPCODE_SIZE_8),
				B2R2_PLUG_OPCODE_SIZE_64),
			&b2r2_core.hw->PLUGS2_OP2);
	writel(min_t(u8, b2r2_core.ch_size, B2R2_PLUG_CHUNK_SIZE_128),
			&b2r2_core.hw->PLUGS2_CHZ);
	writel(min_t(u8, b2r2_core.mg_size, B2R2_PLUG_MESSAGE_SIZE_128) |
				(b2r2_core.min_req_time << 16),
			&b2r2_core.hw->PLUGS2_MSZ);
	writel(min_t(u8, b2r2_core.pg_size, B2R2_PLUG_PAGE_SIZE_256),
			&b2r2_core.hw->PLUGS2_PGZ);

	writel(min_t(u8, max_t(u8, b2r2_core.op_size, B2R2_PLUG_OPCODE_SIZE_8),
				B2R2_PLUG_OPCODE_SIZE_64),
			&b2r2_core.hw->PLUGS3_OP2);
	writel(min_t(u8, b2r2_core.ch_size, B2R2_PLUG_CHUNK_SIZE_128),
			&b2r2_core.hw->PLUGS3_CHZ);
	writel(min_t(u8, b2r2_core.mg_size, B2R2_PLUG_MESSAGE_SIZE_128) |
				(b2r2_core.min_req_time << 16),
			&b2r2_core.hw->PLUGS3_MSZ);
	writel(min_t(u8, b2r2_core.pg_size, B2R2_PLUG_PAGE_SIZE_256),
			&b2r2_core.hw->PLUGS3_PGZ);

	writel(min_t(u8, max_t(u8, b2r2_core.op_size, B2R2_PLUG_OPCODE_SIZE_8),
				B2R2_PLUG_OPCODE_SIZE_64),
			&b2r2_core.hw->PLUGT_OP2);
	writel(min_t(u8, b2r2_core.ch_size, B2R2_PLUG_CHUNK_SIZE_128),
			&b2r2_core.hw->PLUGT_CHZ);
	writel(min_t(u8, b2r2_core.mg_size, B2R2_PLUG_MESSAGE_SIZE_128) |
				(b2r2_core.min_req_time << 16),
			&b2r2_core.hw->PLUGT_MSZ);
	writel(min_t(u8, b2r2_core.pg_size, B2R2_PLUG_PAGE_SIZE_256),
			&b2r2_core.hw->PLUGT_PGZ);

	/* B2R2 kicks off when LNA is written, LNA write must be last! */
	switch (job->queue) {
	case B2R2_CORE_QUEUE_CQ1:
		writel(job->first_node_address, &b2r2_core.hw->BLT_CQ1_TRIG_IP);
		writel(job->control, &b2r2_core.hw->BLT_CQ1_TRIG_CTL);
		writel(job->pace_control, &b2r2_core.hw->BLT_CQ1_PACE_CTL);
		break;

	case B2R2_CORE_QUEUE_CQ2:
		writel(job->first_node_address, &b2r2_core.hw->BLT_CQ2_TRIG_IP);
		writel(job->control, &b2r2_core.hw->BLT_CQ2_TRIG_CTL);
		writel(job->pace_control, &b2r2_core.hw->BLT_CQ2_PACE_CTL);
		break;

	case B2R2_CORE_QUEUE_AQ1:
		writel(job->control, &b2r2_core.hw->BLT_AQ1_CTL);
		writel(job->first_node_address, &b2r2_core.hw->BLT_AQ1_IP);
		wmb();
		start_hw_timer(job);
		writel(job->last_node_address, &b2r2_core.hw->BLT_AQ1_LNA);
		break;

	case B2R2_CORE_QUEUE_AQ2:
		writel(job->control, &b2r2_core.hw->BLT_AQ2_CTL);
		writel(job->first_node_address, &b2r2_core.hw->BLT_AQ2_IP);
		wmb();
		start_hw_timer(job);
		writel(job->last_node_address, &b2r2_core.hw->BLT_AQ2_LNA);
		break;

	case B2R2_CORE_QUEUE_AQ3:
		writel(job->control, &b2r2_core.hw->BLT_AQ3_CTL);
		writel(job->first_node_address, &b2r2_core.hw->BLT_AQ3_IP);
		wmb();
		start_hw_timer(job);
		writel(job->last_node_address, &b2r2_core.hw->BLT_AQ3_LNA);
		break;

	case B2R2_CORE_QUEUE_AQ4:
		writel(job->control, &b2r2_core.hw->BLT_AQ4_CTL);
		writel(job->first_node_address, &b2r2_core.hw->BLT_AQ4_IP);
		wmb();
		start_hw_timer(job);
		writel(job->last_node_address, &b2r2_core.hw->BLT_AQ4_LNA);
		break;

		/** Handle the default case */
	default:
		break;

	} /* end switch */

}

/**
 * handle_queue_event() - Handles interrupt event for specified B2R2 queue
 *
 * @queue: Queue to handle event for
 *
 * b2r2_core.lock must be held
 */
static void handle_queue_event(enum b2r2_core_queue queue)
{
	struct b2r2_core_job *job;

	job = b2r2_core.active_jobs[queue];
	if (job) {
		if (job->job_state != B2R2_CORE_JOB_RUNNING)
			/* Should be running
			   Severe error. TBD */
			b2r2_log_warn(
				 "%s: Job is not running", __func__);

		stop_hw_timer(job);

		/* Remove from queue */
		BUG_ON(b2r2_core.n_active_jobs == 0);
		b2r2_core.active_jobs[queue] = NULL;
		b2r2_core.n_active_jobs--;
	}

	if (!job) {
		/* No job, error?  */
		b2r2_log_warn("%s: No job", __func__);
		return;
	}


	/* Atomic context release resources, release resources will
	   be called again later from process context (work queue) */
	if (job->release_resources)
		job->release_resources(job, true);

	/* Job is done */
	job->job_state = B2R2_CORE_JOB_DONE;

	/* Handle done */
	wake_up_interruptible(&job->event);

	/* Dispatch to work queue to handle callbacks */
	queue_work(b2r2_core.work_queue, &job->work);
}

/**
 * process_events() - Handles interrupt events
 *
 * @status: Contents of the B2R2 ITS register
 */
static void process_events(u32 status)
{
	u32 mask = 0xF;
	u32 disable_itm_mask = 0;

	b2r2_log_info("Enters process_events \n");
	b2r2_log_info("status 0x%x \n", status);

	/* Composition queue 1 */
	if (status & mask) {
		handle_queue_event(B2R2_CORE_QUEUE_CQ1);
		disable_itm_mask |= mask;
	}
	mask <<= 4;

	/* Composition queue 2 */
	if (status & mask) {
		handle_queue_event(B2R2_CORE_QUEUE_CQ2);
		disable_itm_mask |= mask;
	}
	mask <<= 8;

	/* Application queue 1 */
	if (status & mask) {
		handle_queue_event(B2R2_CORE_QUEUE_AQ1);
		disable_itm_mask |= mask;
	}
	mask <<= 4;

	/* Application queue 2 */
	if (status & mask) {
		handle_queue_event(B2R2_CORE_QUEUE_AQ2);
		disable_itm_mask |= mask;
	}
	mask <<= 4;

	/* Application queue 3 */
	if (status & mask) {
		handle_queue_event(B2R2_CORE_QUEUE_AQ3);
		disable_itm_mask |= mask;
	}
	mask <<= 4;

	/* Application queue 4 */
	if (status & mask) {
		handle_queue_event(B2R2_CORE_QUEUE_AQ4);
		disable_itm_mask |= mask;
	}

	/* Clear received interrupt flags */
	writel(status, &b2r2_core.hw->BLT_ITS);
	/* Disable handled interrupts */
	writel(readl(&b2r2_core.hw->BLT_ITM0) & ~disable_itm_mask,
		 &b2r2_core.hw->BLT_ITM0);

	b2r2_log_info("Returns process_events \n");
}

/**
 * b2r2_irq_handler() - B2R2 interrupt handler
 *
 * @irq: Interrupt number (not used)
 * @x: ??? (Not used)
 */
static irqreturn_t b2r2_irq_handler(int irq, void *x)
{
	unsigned long flags;

	/* Spin lock is need in irq handler (SMP) */
	spin_lock_irqsave(&b2r2_core.lock, flags);

	/* Make sure that we have a clock */

	/* Remember time for last irq (for timeout mgmt) */
	b2r2_core.jiffies_last_irq = jiffies;
	b2r2_core.stat_n_irq++;

	/* Handle the interrupt(s) */
	process_events(readl(&b2r2_core.hw->BLT_ITS));

	/* Check if we can dispatch new jobs */
	check_prio_list(true);

	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	return IRQ_HANDLED;
}


#ifdef CONFIG_DEBUG_FS
/**
 * struct debugfs_reg - Represents one B2R2 register in debugfs
 *
 * @name: Register name
 * @offset: Byte offset in B2R2 for register
 */
struct debugfs_reg {
	const char name[30];
	u32        offset;
};

/**
 * debugfs_regs - Array of B2R2 debugfs registers
 */
static const struct debugfs_reg debugfs_regs[] = {
	{"BLT_SSBA17", offsetof(struct b2r2_memory_map, BLT_SSBA17)},
	{"BLT_SSBA18", offsetof(struct b2r2_memory_map, BLT_SSBA18)},
	{"BLT_SSBA19", offsetof(struct b2r2_memory_map, BLT_SSBA19)},
	{"BLT_SSBA20", offsetof(struct b2r2_memory_map, BLT_SSBA20)},
	{"BLT_SSBA21", offsetof(struct b2r2_memory_map, BLT_SSBA21)},
	{"BLT_SSBA22", offsetof(struct b2r2_memory_map, BLT_SSBA22)},
	{"BLT_SSBA23", offsetof(struct b2r2_memory_map, BLT_SSBA23)},
	{"BLT_SSBA24", offsetof(struct b2r2_memory_map, BLT_SSBA24)},
	{"BLT_STBA5", offsetof(struct b2r2_memory_map, BLT_STBA5)},
	{"BLT_STBA6", offsetof(struct b2r2_memory_map, BLT_STBA6)},
	{"BLT_STBA7", offsetof(struct b2r2_memory_map, BLT_STBA7)},
	{"BLT_STBA8", offsetof(struct b2r2_memory_map, BLT_STBA8)},
	{"BLT_CTL", offsetof(struct b2r2_memory_map, BLT_CTL)},
	{"BLT_ITS", offsetof(struct b2r2_memory_map, BLT_ITS)},
	{"BLT_STA1", offsetof(struct b2r2_memory_map, BLT_STA1)},
	{"BLT_SSBA1", offsetof(struct b2r2_memory_map, BLT_SSBA1)},
	{"BLT_SSBA2", offsetof(struct b2r2_memory_map, BLT_SSBA2)},
	{"BLT_SSBA3", offsetof(struct b2r2_memory_map, BLT_SSBA3)},
	{"BLT_SSBA4", offsetof(struct b2r2_memory_map, BLT_SSBA4)},
	{"BLT_SSBA5", offsetof(struct b2r2_memory_map, BLT_SSBA5)},
	{"BLT_SSBA6", offsetof(struct b2r2_memory_map, BLT_SSBA6)},
	{"BLT_SSBA7", offsetof(struct b2r2_memory_map, BLT_SSBA7)},
	{"BLT_SSBA8", offsetof(struct b2r2_memory_map, BLT_SSBA8)},
	{"BLT_STBA1", offsetof(struct b2r2_memory_map, BLT_STBA1)},
	{"BLT_STBA2", offsetof(struct b2r2_memory_map, BLT_STBA2)},
	{"BLT_STBA3", offsetof(struct b2r2_memory_map, BLT_STBA3)},
	{"BLT_STBA4", offsetof(struct b2r2_memory_map, BLT_STBA4)},
	{"BLT_CQ1_TRIG_IP", offsetof(struct b2r2_memory_map, BLT_CQ1_TRIG_IP)},
	{"BLT_CQ1_TRIG_CTL", offsetof(struct b2r2_memory_map,
				      BLT_CQ1_TRIG_CTL)},
	{"BLT_CQ1_PACE_CTL", offsetof(struct b2r2_memory_map,
				      BLT_CQ1_PACE_CTL)},
	{"BLT_CQ1_IP", offsetof(struct b2r2_memory_map, BLT_CQ1_IP)},
	{"BLT_CQ2_TRIG_IP", offsetof(struct b2r2_memory_map, BLT_CQ2_TRIG_IP)},
	{"BLT_CQ2_TRIG_CTL", offsetof(struct b2r2_memory_map,
				      BLT_CQ2_TRIG_CTL)},
	{"BLT_CQ2_PACE_CTL", offsetof(struct b2r2_memory_map,
				      BLT_CQ2_PACE_CTL)},
	{"BLT_CQ2_IP", offsetof(struct b2r2_memory_map, BLT_CQ2_IP)},
	{"BLT_AQ1_CTL", offsetof(struct b2r2_memory_map, BLT_AQ1_CTL)},
	{"BLT_AQ1_IP", offsetof(struct b2r2_memory_map, BLT_AQ1_IP)},
	{"BLT_AQ1_LNA", offsetof(struct b2r2_memory_map, BLT_AQ1_LNA)},
	{"BLT_AQ1_STA", offsetof(struct b2r2_memory_map, BLT_AQ1_STA)},
	{"BLT_AQ2_CTL", offsetof(struct b2r2_memory_map, BLT_AQ2_CTL)},
	{"BLT_AQ2_IP", offsetof(struct b2r2_memory_map, BLT_AQ2_IP)},
	{"BLT_AQ2_LNA", offsetof(struct b2r2_memory_map, BLT_AQ2_LNA)},
	{"BLT_AQ2_STA", offsetof(struct b2r2_memory_map, BLT_AQ2_STA)},
	{"BLT_AQ3_CTL", offsetof(struct b2r2_memory_map, BLT_AQ3_CTL)},
	{"BLT_AQ3_IP", offsetof(struct b2r2_memory_map, BLT_AQ3_IP)},
	{"BLT_AQ3_LNA", offsetof(struct b2r2_memory_map, BLT_AQ3_LNA)},
	{"BLT_AQ3_STA", offsetof(struct b2r2_memory_map, BLT_AQ3_STA)},
	{"BLT_AQ4_CTL", offsetof(struct b2r2_memory_map, BLT_AQ4_CTL)},
	{"BLT_AQ4_IP", offsetof(struct b2r2_memory_map, BLT_AQ4_IP)},
	{"BLT_AQ4_LNA", offsetof(struct b2r2_memory_map, BLT_AQ4_LNA)},
	{"BLT_AQ4_STA", offsetof(struct b2r2_memory_map, BLT_AQ4_STA)},
	{"BLT_SSBA9", offsetof(struct b2r2_memory_map, BLT_SSBA9)},
	{"BLT_SSBA10", offsetof(struct b2r2_memory_map, BLT_SSBA10)},
	{"BLT_SSBA11", offsetof(struct b2r2_memory_map, BLT_SSBA11)},
	{"BLT_SSBA12", offsetof(struct b2r2_memory_map, BLT_SSBA12)},
	{"BLT_SSBA13", offsetof(struct b2r2_memory_map, BLT_SSBA13)},
	{"BLT_SSBA14", offsetof(struct b2r2_memory_map, BLT_SSBA14)},
	{"BLT_SSBA15", offsetof(struct b2r2_memory_map, BLT_SSBA15)},
	{"BLT_SSBA16", offsetof(struct b2r2_memory_map, BLT_SSBA16)},
	{"BLT_SGA1", offsetof(struct b2r2_memory_map, BLT_SGA1)},
	{"BLT_SGA2", offsetof(struct b2r2_memory_map, BLT_SGA2)},
	{"BLT_ITM0", offsetof(struct b2r2_memory_map, BLT_ITM0)},
	{"BLT_ITM1", offsetof(struct b2r2_memory_map, BLT_ITM1)},
	{"BLT_ITM2", offsetof(struct b2r2_memory_map, BLT_ITM2)},
	{"BLT_ITM3", offsetof(struct b2r2_memory_map, BLT_ITM3)},
	{"BLT_DFV2", offsetof(struct b2r2_memory_map, BLT_DFV2)},
	{"BLT_DFV1", offsetof(struct b2r2_memory_map, BLT_DFV1)},
	{"BLT_PRI", offsetof(struct b2r2_memory_map, BLT_PRI)},
	{"PLUGS1_OP2", offsetof(struct b2r2_memory_map, PLUGS1_OP2)},
	{"PLUGS1_CHZ", offsetof(struct b2r2_memory_map, PLUGS1_CHZ)},
	{"PLUGS1_MSZ", offsetof(struct b2r2_memory_map, PLUGS1_MSZ)},
	{"PLUGS1_PGZ", offsetof(struct b2r2_memory_map, PLUGS1_PGZ)},
	{"PLUGS2_OP2", offsetof(struct b2r2_memory_map, PLUGS2_OP2)},
	{"PLUGS2_CHZ", offsetof(struct b2r2_memory_map, PLUGS2_CHZ)},
	{"PLUGS2_MSZ", offsetof(struct b2r2_memory_map, PLUGS2_MSZ)},
	{"PLUGS2_PGZ", offsetof(struct b2r2_memory_map, PLUGS2_PGZ)},
	{"PLUGS3_OP2", offsetof(struct b2r2_memory_map, PLUGS3_OP2)},
	{"PLUGS3_CHZ", offsetof(struct b2r2_memory_map, PLUGS3_CHZ)},
	{"PLUGS3_MSZ", offsetof(struct b2r2_memory_map, PLUGS3_MSZ)},
	{"PLUGS3_PGZ", offsetof(struct b2r2_memory_map, PLUGS3_PGZ)},
	{"PLUGT_OP2", offsetof(struct b2r2_memory_map, PLUGT_OP2)},
	{"PLUGT_CHZ", offsetof(struct b2r2_memory_map, PLUGT_CHZ)},
	{"PLUGT_MSZ", offsetof(struct b2r2_memory_map, PLUGT_MSZ)},
	{"PLUGT_PGZ", offsetof(struct b2r2_memory_map, PLUGT_PGZ)},
	{"BLT_NIP", offsetof(struct b2r2_memory_map, BLT_NIP)},
	{"BLT_CIC", offsetof(struct b2r2_memory_map, BLT_CIC)},
	{"BLT_INS", offsetof(struct b2r2_memory_map, BLT_INS)},
	{"BLT_ACK", offsetof(struct b2r2_memory_map, BLT_ACK)},
	{"BLT_TBA", offsetof(struct b2r2_memory_map, BLT_TBA)},
	{"BLT_TTY", offsetof(struct b2r2_memory_map, BLT_TTY)},
	{"BLT_TXY", offsetof(struct b2r2_memory_map, BLT_TXY)},
	{"BLT_TSZ", offsetof(struct b2r2_memory_map, BLT_TSZ)},
	{"BLT_S1CF", offsetof(struct b2r2_memory_map, BLT_S1CF)},
	{"BLT_S2CF", offsetof(struct b2r2_memory_map, BLT_S2CF)},
	{"BLT_S1BA", offsetof(struct b2r2_memory_map, BLT_S1BA)},
	{"BLT_S1TY", offsetof(struct b2r2_memory_map, BLT_S1TY)},
	{"BLT_S1XY", offsetof(struct b2r2_memory_map, BLT_S1XY)},
	{"BLT_S2BA", offsetof(struct b2r2_memory_map, BLT_S2BA)},
	{"BLT_S2TY", offsetof(struct b2r2_memory_map, BLT_S2TY)},
	{"BLT_S2XY", offsetof(struct b2r2_memory_map, BLT_S2XY)},
	{"BLT_S2SZ", offsetof(struct b2r2_memory_map, BLT_S2SZ)},
	{"BLT_S3BA", offsetof(struct b2r2_memory_map, BLT_S3BA)},
	{"BLT_S3TY", offsetof(struct b2r2_memory_map, BLT_S3TY)},
	{"BLT_S3XY", offsetof(struct b2r2_memory_map, BLT_S3XY)},
	{"BLT_S3SZ", offsetof(struct b2r2_memory_map, BLT_S3SZ)},
	{"BLT_CWO", offsetof(struct b2r2_memory_map, BLT_CWO)},
	{"BLT_CWS", offsetof(struct b2r2_memory_map, BLT_CWS)},
	{"BLT_CCO", offsetof(struct b2r2_memory_map, BLT_CCO)},
	{"BLT_CML", offsetof(struct b2r2_memory_map, BLT_CML)},
	{"BLT_FCTL", offsetof(struct b2r2_memory_map, BLT_FCTL)},
	{"BLT_PMK", offsetof(struct b2r2_memory_map, BLT_PMK)},
	{"BLT_RSF", offsetof(struct b2r2_memory_map, BLT_RSF)},
	{"BLT_RZI", offsetof(struct b2r2_memory_map, BLT_RZI)},
	{"BLT_HFP", offsetof(struct b2r2_memory_map, BLT_HFP)},
	{"BLT_VFP", offsetof(struct b2r2_memory_map, BLT_VFP)},
	{"BLT_Y_RSF", offsetof(struct b2r2_memory_map, BLT_Y_RSF)},
	{"BLT_Y_RZI", offsetof(struct b2r2_memory_map, BLT_Y_RZI)},
	{"BLT_Y_HFP", offsetof(struct b2r2_memory_map, BLT_Y_HFP)},
	{"BLT_Y_VFP", offsetof(struct b2r2_memory_map, BLT_Y_VFP)},
	{"BLT_KEY1", offsetof(struct b2r2_memory_map, BLT_KEY1)},
	{"BLT_KEY2", offsetof(struct b2r2_memory_map, BLT_KEY2)},
	{"BLT_SAR", offsetof(struct b2r2_memory_map, BLT_SAR)},
	{"BLT_USR", offsetof(struct b2r2_memory_map, BLT_USR)},
	{"BLT_IVMX0", offsetof(struct b2r2_memory_map, BLT_IVMX0)},
	{"BLT_IVMX1", offsetof(struct b2r2_memory_map, BLT_IVMX1)},
	{"BLT_IVMX2", offsetof(struct b2r2_memory_map, BLT_IVMX2)},
	{"BLT_IVMX3", offsetof(struct b2r2_memory_map, BLT_IVMX3)},
	{"BLT_OVMX0", offsetof(struct b2r2_memory_map, BLT_OVMX0)},
	{"BLT_OVMX1", offsetof(struct b2r2_memory_map, BLT_OVMX1)},
	{"BLT_OVMX2", offsetof(struct b2r2_memory_map, BLT_OVMX2)},
	{"BLT_OVMX3", offsetof(struct b2r2_memory_map, BLT_OVMX3)},
	{"BLT_VC1R", offsetof(struct b2r2_memory_map, BLT_VC1R)},
	{"BLT_Y_HFC0", offsetof(struct b2r2_memory_map, BLT_Y_HFC0)},
	{"BLT_Y_HFC1", offsetof(struct b2r2_memory_map, BLT_Y_HFC1)},
	{"BLT_Y_HFC2", offsetof(struct b2r2_memory_map, BLT_Y_HFC2)},
	{"BLT_Y_HFC3", offsetof(struct b2r2_memory_map, BLT_Y_HFC3)},
	{"BLT_Y_HFC4", offsetof(struct b2r2_memory_map, BLT_Y_HFC4)},
	{"BLT_Y_HFC5", offsetof(struct b2r2_memory_map, BLT_Y_HFC5)},
	{"BLT_Y_HFC6", offsetof(struct b2r2_memory_map, BLT_Y_HFC6)},
	{"BLT_Y_HFC7", offsetof(struct b2r2_memory_map, BLT_Y_HFC7)},
	{"BLT_Y_HFC8", offsetof(struct b2r2_memory_map, BLT_Y_HFC8)},
	{"BLT_Y_HFC9", offsetof(struct b2r2_memory_map, BLT_Y_HFC9)},
	{"BLT_Y_HFC10", offsetof(struct b2r2_memory_map, BLT_Y_HFC10)},
	{"BLT_Y_HFC11", offsetof(struct b2r2_memory_map, BLT_Y_HFC11)},
	{"BLT_Y_HFC12", offsetof(struct b2r2_memory_map, BLT_Y_HFC12)},
	{"BLT_Y_HFC13", offsetof(struct b2r2_memory_map, BLT_Y_HFC13)},
	{"BLT_Y_HFC14", offsetof(struct b2r2_memory_map, BLT_Y_HFC14)},
	{"BLT_Y_HFC15", offsetof(struct b2r2_memory_map, BLT_Y_HFC15)},
	{"BLT_Y_VFC0", offsetof(struct b2r2_memory_map, BLT_Y_VFC0)},
	{"BLT_Y_VFC1", offsetof(struct b2r2_memory_map, BLT_Y_VFC1)},
	{"BLT_Y_VFC2", offsetof(struct b2r2_memory_map, BLT_Y_VFC2)},
	{"BLT_Y_VFC3", offsetof(struct b2r2_memory_map, BLT_Y_VFC3)},
	{"BLT_Y_VFC4", offsetof(struct b2r2_memory_map, BLT_Y_VFC4)},
	{"BLT_Y_VFC5", offsetof(struct b2r2_memory_map, BLT_Y_VFC5)},
	{"BLT_Y_VFC6", offsetof(struct b2r2_memory_map, BLT_Y_VFC6)},
	{"BLT_Y_VFC7", offsetof(struct b2r2_memory_map, BLT_Y_VFC7)},
	{"BLT_Y_VFC8", offsetof(struct b2r2_memory_map, BLT_Y_VFC8)},
	{"BLT_Y_VFC9", offsetof(struct b2r2_memory_map, BLT_Y_VFC9)},
	{"BLT_HFC0", offsetof(struct b2r2_memory_map, BLT_HFC0)},
	{"BLT_HFC1", offsetof(struct b2r2_memory_map, BLT_HFC1)},
	{"BLT_HFC2", offsetof(struct b2r2_memory_map, BLT_HFC2)},
	{"BLT_HFC3", offsetof(struct b2r2_memory_map, BLT_HFC3)},
	{"BLT_HFC4", offsetof(struct b2r2_memory_map, BLT_HFC4)},
	{"BLT_HFC5", offsetof(struct b2r2_memory_map, BLT_HFC5)},
	{"BLT_HFC6", offsetof(struct b2r2_memory_map, BLT_HFC6)},
	{"BLT_HFC7", offsetof(struct b2r2_memory_map, BLT_HFC7)},
	{"BLT_HFC8", offsetof(struct b2r2_memory_map, BLT_HFC8)},
	{"BLT_HFC9", offsetof(struct b2r2_memory_map, BLT_HFC9)},
	{"BLT_HFC10", offsetof(struct b2r2_memory_map, BLT_HFC10)},
	{"BLT_HFC11", offsetof(struct b2r2_memory_map, BLT_HFC11)},
	{"BLT_HFC12", offsetof(struct b2r2_memory_map, BLT_HFC12)},
	{"BLT_HFC13", offsetof(struct b2r2_memory_map, BLT_HFC13)},
	{"BLT_HFC14", offsetof(struct b2r2_memory_map, BLT_HFC14)},
	{"BLT_HFC15", offsetof(struct b2r2_memory_map, BLT_HFC15)},
	{"BLT_VFC0", offsetof(struct b2r2_memory_map, BLT_VFC0)},
	{"BLT_VFC1", offsetof(struct b2r2_memory_map, BLT_VFC1)},
	{"BLT_VFC2", offsetof(struct b2r2_memory_map, BLT_VFC2)},
	{"BLT_VFC3", offsetof(struct b2r2_memory_map, BLT_VFC3)},
	{"BLT_VFC4", offsetof(struct b2r2_memory_map, BLT_VFC4)},
	{"BLT_VFC5", offsetof(struct b2r2_memory_map, BLT_VFC5)},
	{"BLT_VFC6", offsetof(struct b2r2_memory_map, BLT_VFC6)},
	{"BLT_VFC7", offsetof(struct b2r2_memory_map, BLT_VFC7)},
	{"BLT_VFC8", offsetof(struct b2r2_memory_map, BLT_VFC8)},
	{"BLT_VFC9", offsetof(struct b2r2_memory_map, BLT_VFC9)},
};

#ifdef HANDLE_TIMEOUTED_JOBS
/**
 * printk_regs() - Print B2R2 registers to printk
 */
static void printk_regs(void)
{
#ifdef CONFIG_B2R2_DEBUG
	int i;

	for (i = 0; i < ARRAY_SIZE(debugfs_regs); i++) {
		unsigned long value = readl(
			(unsigned long *) (((u8 *) b2r2_core.hw) +
					   debugfs_regs[i].offset));
		b2r2_log_regdump("%s: %08lX\n",
			 debugfs_regs[i].name,
			 value);
	}
#endif
}
#endif

/**
 * debugfs_b2r2_reg_read() - Implements debugfs read for B2R2 register
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes read or negative error code
 */
static int debugfs_b2r2_reg_read(struct file *filp, char __user *buf,
				 size_t count, loff_t *f_pos)
{
	size_t dev_size;
	int ret = 0;

	unsigned long value;
	char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

	if (Buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* Read from B2R2 */
	value = readl((unsigned long *) (((u8 *) b2r2_core.hw) +
					 (u32) filp->f_dentry->
					 d_inode->i_private));

	/* Build the string */
	dev_size = sprintf(Buf, "%8lX\n", value);

	/* No more to read if offset != 0 */
	if (*f_pos > dev_size)
		goto out;

	if (*f_pos + count > dev_size)
		count = dev_size - *f_pos;

	/* Return it to user space */
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
 * debugfs_b2r2_reg_write() - Implements debugfs write for B2R2 register
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to write
 * @f_pos: File position
 *
 * Returns number of bytes written or negative error code
 */
static int debugfs_b2r2_reg_write(struct file *filp, const char __user *buf,
				  size_t count, loff_t *f_pos)
{
	char Buf[80];
	u32 reg_value;
	int ret = 0;

	/* Adjust count */
	if (count >= sizeof(Buf))
		count = sizeof(Buf) - 1;
	/* Get it from user space */
	if (copy_from_user(Buf, buf, count))
		return -EINVAL;
	Buf[count] = 0;
	/* Convert from hex string */
	if (sscanf(Buf, "%8lX", (unsigned long *) &reg_value) != 1)
		return -EINVAL;

	writel(reg_value, (u32 *) (((u8 *) b2r2_core.hw) +
			(u32) filp->f_dentry->d_inode->i_private));

	*f_pos += count;
	ret = count;

	return ret;
}

/**
 * debugfs_b2r2_reg_fops() - File operations for B2R2 register debugfs
 */
static const struct file_operations debugfs_b2r2_reg_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_b2r2_reg_read,
	.write = debugfs_b2r2_reg_write,
};

/**
 * debugfs_b2r2_regs_read() - Implements debugfs read for B2R2 register dump
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes written or negative error code
 */
static int debugfs_b2r2_regs_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *f_pos)
{
	size_t dev_size = 0;
	int ret = 0;
	int i;
	char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

	if (Buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* Build a giant string containing all registers */
	for (i = 0; i < ARRAY_SIZE(debugfs_regs); i++) {
		unsigned long value =
			readl((unsigned long *) (((u8 *) b2r2_core.hw) +
						 debugfs_regs[i].offset));
		dev_size += sprintf(Buf + dev_size, "%s: %08lX\n",
				    debugfs_regs[i].name,
				    value);
	}

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
 * debugfs_b2r2_regs_fops() - File operations for B2R2 register dump debugfs
 */
static const struct file_operations debugfs_b2r2_regs_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_b2r2_regs_read,
};

/**
 * debugfs_b2r2_stat_read() - Implements debugfs read for B2R2 statistics
 *
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes read or negative error code
 */
static int debugfs_b2r2_stat_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *f_pos)
{
	size_t dev_size = 0;
	int ret = 0;
	int i = 0;
	char *Buf = kmalloc(sizeof(char) * 4096, GFP_KERNEL);

	if (Buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* Build a string containing all statistics */
	dev_size += sprintf(Buf + dev_size, "Interrupts: %lu\n",
			    b2r2_core.stat_n_irq);
	dev_size += sprintf(Buf + dev_size, "Added jobs: %lu\n",
			    b2r2_core.stat_n_jobs_added);
	dev_size += sprintf(Buf + dev_size, "Removed jobs: %lu\n",
			    b2r2_core.stat_n_jobs_removed);
	dev_size += sprintf(Buf + dev_size, "Jobs in prio list: %lu\n",
			    b2r2_core.stat_n_jobs_in_prio_list);
	dev_size += sprintf(Buf + dev_size, "Active jobs: %lu\n",
			    b2r2_core.n_active_jobs);
	for (i = 0; i < ARRAY_SIZE(b2r2_core.active_jobs); i++)
		dev_size += sprintf(Buf + dev_size, "Job in queue %d: %p\n",
				    i, b2r2_core.active_jobs[i]);
	dev_size += sprintf(Buf + dev_size, "Clock requests: %lu\n",
			    b2r2_core.clock_request_count);

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
 * debugfs_b2r2_stat_fops() - File operations for B2R2 statistics debugfs
 */
static const struct file_operations debugfs_b2r2_stat_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_b2r2_stat_read,
};


/**
 * debugfs_b2r2_clock_read() - Implements debugfs read for
 *                             PMU B2R2 clock register
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to read
 * @f_pos: File position
 *
 * Returns number of bytes read or negative error code
 */
static int debugfs_b2r2_clock_read(struct file *filp, char __user *buf,
				   size_t count, loff_t *f_pos)
{
	/* 10 characters hex number + newline + string terminator; */
	char Buf[10+2];
	size_t dev_size;
	int ret = 0;

	unsigned long value = clk_get_rate(b2r2_core.b2r2_clock);

	dev_size = sprintf(Buf, "%#010lX\n", value);

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
	return ret;
}

/**
 * debugfs_b2r2_clock_write() - Implements debugfs write for
 *                              PMU B2R2 clock register
 * @filp: File pointer
 * @buf: User space buffer
 * @count: Number of bytes to write
 * @f_pos: File position
 *
 * Returns number of bytes written or negative error code
 */
static int debugfs_b2r2_clock_write(struct file *filp, const char __user *buf,
				    size_t count, loff_t *f_pos)
{
	char Buf[80];
	u32 reg_value;
	int ret = 0;

	if (count >= sizeof(Buf))
		count = sizeof(Buf) - 1;
	if (copy_from_user(Buf, buf, count))
		return -EINVAL;
	Buf[count] = 0;
	if (sscanf(Buf, "%8lX", (unsigned long *) &reg_value) != 1)
		return -EINVAL;

	/*not working yet*/
	/*clk_set_rate(b2r2_core.b2r2_clock, (unsigned long) reg_value);*/

	*f_pos += count;
	ret = count;

	return ret;
}

/**
 * debugfs_b2r2_clock_fops() - File operations for PMU B2R2 clock debugfs
 */
static const struct file_operations debugfs_b2r2_clock_fops = {
	.owner = THIS_MODULE,
	.read  = debugfs_b2r2_clock_read,
	.write = debugfs_b2r2_clock_write,
};

#endif

/**
 *
 * init_hw() - B2R2 Hardware reset & initiliaze
 *
 * @pdev: B2R2 platform device
 *
 * 1)Register interrupt handler
 *
 * 2)B2R2 Register map
 *
 * 3)For resetting B2R2 hardware,write to B2R2 Control register the
 * B2R2BLT_CTLGLOBAL_soft_reset and then polling for on
 * B2R2 status register for B2R2BLT_STA1BDISP_IDLE flag.
 *
 * 4)Wait for B2R2 hardware to be idle (on a timeout rather than while loop)
 *
 * 5)Driver status reset
 *
 * 6)Recover from any error without any leaks.
 *
 */
static int init_hw(void)
{
	int result = 0;
	u32 uTimeOut = B2R2_DRIVER_TIMEOUT_VALUE;

	/* Put B2R2 into reset */
	clear_interrupts();

	writel(readl(&b2r2_core.hw->BLT_CTL) | B2R2BLT_CTLGLOBAL_soft_reset,
		&b2r2_core.hw->BLT_CTL);


	/* Set up interrupt handler */
	result = request_irq(b2r2_core.irq, b2r2_irq_handler, 0,
			     "b2r2-interrupt", 0);
	if (result) {
		b2r2_log_err("%s: failed to register IRQ for B2R2\n", __func__);
		goto b2r2_init_request_irq_failed;
	}

	b2r2_log_info("do a global reset..\n");

	/* Release reset */
	writel(0x00000000, &b2r2_core.hw->BLT_CTL);

	b2r2_log_info("wait for B2R2 to be idle..\n");

	/** Wait for B2R2 to be idle (on a timeout rather than while loop) */
	while ((uTimeOut > 0) &&
	       ((readl(&b2r2_core.hw->BLT_STA1) &
		 B2R2BLT_STA1BDISP_IDLE) == 0x0))
		uTimeOut--;
	if (uTimeOut == 0) {
		b2r2_log_err(
			 "%s: B2R2 not idle after SW reset\n", __func__);
		result = -EAGAIN;
		goto b2r2_core_init_hw_timeout;
	}

#ifdef CONFIG_DEBUG_FS
	/* Register debug fs files for register access */
	if (b2r2_core.debugfs_root_dir && !b2r2_core.debugfs_regs_dir) {
		int i;
		b2r2_core.debugfs_regs_dir = debugfs_create_dir("regs",
				b2r2_core.debugfs_root_dir);
		debugfs_create_file("all", 0666, b2r2_core.debugfs_regs_dir,
				0, &debugfs_b2r2_regs_fops);
		/* Create debugfs entries for all static registers */
		for (i = 0; i < ARRAY_SIZE(debugfs_regs); i++)
			debugfs_create_file(debugfs_regs[i].name, 0666,
					b2r2_core.debugfs_regs_dir,
					(void *) debugfs_regs[i].offset,
					&debugfs_b2r2_reg_fops);
	}
#endif

	b2r2_log_info("%s ended..\n", __func__);
	return result;

/** Recover from any error without any leaks */

b2r2_core_init_hw_timeout:

	/** Free B2R2 interrupt handler */

	free_irq(b2r2_core.irq, 0);

b2r2_init_request_irq_failed:

	if (b2r2_core.hw)
		iounmap(b2r2_core.hw);
	b2r2_core.hw = NULL;

	return result;

}


/**
 * exit_hw() - B2R2 Hardware exit
 *
 * b2r2_core.lock _must_ NOT be held
 */
static void exit_hw(void)
{
	unsigned long flags;

	b2r2_log_info("%s started..\n", __func__);

#ifdef CONFIG_DEBUG_FS
	/* Unregister our debugfs entries */
	if (b2r2_core.debugfs_regs_dir) {
		debugfs_remove_recursive(b2r2_core.debugfs_regs_dir);
		b2r2_core.debugfs_regs_dir = NULL;
	}
#endif
	b2r2_log_debug("%s: locking b2r2_core.lock\n", __func__);
	spin_lock_irqsave(&b2r2_core.lock, flags);

	/* Cancel all pending jobs */
	b2r2_log_debug("%s: canceling pending jobs\n", __func__);
	exit_job_list(&b2r2_core.prio_queue);

	/* Soft reset B2R2 (Close all DMA,
	   reset all state to idle, reset regs)*/
	b2r2_log_debug("%s: putting b2r2 in reset\n", __func__);
	writel(readl(&b2r2_core.hw->BLT_CTL) | B2R2BLT_CTLGLOBAL_soft_reset,
		&b2r2_core.hw->BLT_CTL);

	b2r2_log_debug("%s: clearing interrupts\n", __func__);
	clear_interrupts();

	/** Free B2R2 interrupt handler */
	b2r2_log_debug("%s: freeing interrupt handler\n", __func__);
	free_irq(b2r2_core.irq, 0);

	b2r2_log_debug("%s: unlocking b2r2_core.lock\n", __func__);
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	b2r2_log_info("%s ended...\n", __func__);
}

/**
 * b2r2_probe() - This routine loads the B2R2 core driver
 *
 * @pdev: platform device.
 */
static int b2r2_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;

	BUG_ON(pdev == NULL);

	ret = b2r2_debug_init(&pdev->dev);
	if (ret < 0) {
		dev_err(b2r2_core.log_dev, "b2r2_debug_init failed\n");
		goto b2r2_probe_debug_init_failed;
	}

	b2r2_core.log_dev = &pdev->dev;
	b2r2_log_info("init started.\n");

	/* Init spin locks */
	spin_lock_init(&b2r2_core.lock);

	/* Init job queues */
	INIT_LIST_HEAD(&b2r2_core.prio_queue);

#ifdef HANDLE_TIMEOUTED_JOBS
	/* Create work queue for callbacks & timeout */
	INIT_DELAYED_WORK(&b2r2_core.timeout_work, timeout_work_function);
#endif

	/* Work queue for callbacks and timeout management */
	b2r2_core.work_queue = create_workqueue("B2R2");
	if (!b2r2_core.work_queue) {
		ret = -ENOMEM;
		goto b2r2_probe_no_work_queue;
	}

	/* Get the clock for B2R2 */
	b2r2_core.b2r2_clock = clk_get(&pdev->dev, "b2r2");
	if (IS_ERR(b2r2_core.b2r2_clock)) {
		ret = PTR_ERR(b2r2_core.b2r2_clock);
		b2r2_log_err("clk_get b2r2 failed\n");
		goto b2r2_probe_no_clk;
	}

	/* Get the B2R2 regulator */
	b2r2_core.b2r2_reg = regulator_get(&pdev->dev, "vsupply");
	if (IS_ERR(b2r2_core.b2r2_reg)) {
		ret = PTR_ERR(b2r2_core.b2r2_reg);
		b2r2_log_err("regulator_get vsupply failed (dev_name=%s)\n",
				dev_name(&pdev->dev));
		goto b2r2_probe_no_reg;
	}

	/* Init power management */
	mutex_init(&b2r2_core.domain_lock);
	INIT_DELAYED_WORK_DEFERRABLE(&b2r2_core.domain_disable_work,
			domain_disable_work_function);
	b2r2_core.domain_enabled = false;

	/* Map B2R2 into kernel virtual memory space */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		goto b2r2_probe_no_res;

	/* Hook up irq */
	b2r2_core.irq = platform_get_irq(pdev, 0);
	if (b2r2_core.irq <= 0) {
		b2r2_log_info("%s: Failed to request irq (irq=%d)\n", __func__,
								b2r2_core.irq);
		goto b2r2_failed_irq_get;
	}

	b2r2_core.hw = (struct b2r2_memory_map *) ioremap(res->start,
			 res->end - res->start + 1);
	if (b2r2_core.hw == NULL) {

		b2r2_log_info("%s: ioremap failed\n", __func__);
		ret = -ENOMEM;
		goto b2r2_probe_ioremap_failed;
	}

	dev_dbg(b2r2_core.log_dev,
		"b2r2 structure address %p\n",
		b2r2_core.hw);

	/* Initialize b2r2_blt module. FIXME: Module of it's own
	   or perhaps a dedicated module init c file? */
	ret = b2r2_blt_module_init();
	if (ret < 0) {
		b2r2_log_err("b2r2_blt_module_init failed\n");
		goto b2r2_probe_blt_init_fail;
	}

	b2r2_core.op_size = B2R2_PLUG_OPCODE_SIZE_DEFAULT;
	b2r2_core.ch_size = B2R2_PLUG_CHUNK_SIZE_DEFAULT;
	b2r2_core.pg_size = B2R2_PLUG_PAGE_SIZE_DEFAULT;
	b2r2_core.mg_size = B2R2_PLUG_MESSAGE_SIZE_DEFAULT;
	b2r2_core.min_req_time = 0;

#ifdef CONFIG_DEBUG_FS
	b2r2_core.debugfs_root_dir = debugfs_create_dir("b2r2", NULL);
	debugfs_create_file("stat", 0666, b2r2_core.debugfs_root_dir,
			0, &debugfs_b2r2_stat_fops);
	debugfs_create_file("clock", 0666, b2r2_core.debugfs_root_dir,
			0, &debugfs_b2r2_clock_fops);

	debugfs_create_u8("op_size", 0666, b2r2_core.debugfs_root_dir,
			&b2r2_core.op_size);
	debugfs_create_u8("ch_size", 0666, b2r2_core.debugfs_root_dir,
			&b2r2_core.ch_size);
	debugfs_create_u8("pg_size", 0666, b2r2_core.debugfs_root_dir,
			&b2r2_core.pg_size);
	debugfs_create_u8("mg_size", 0666, b2r2_core.debugfs_root_dir,
			&b2r2_core.mg_size);
	debugfs_create_u16("min_req_time", 0666, b2r2_core.debugfs_root_dir,
			&b2r2_core.min_req_time);
#endif

	b2r2_log_info("init done.\n");

	return ret;

/** Recover from any error if something fails */
b2r2_probe_blt_init_fail:
b2r2_probe_ioremap_failed:
b2r2_failed_irq_get:
b2r2_probe_no_res:
	regulator_put(b2r2_core.b2r2_reg);
b2r2_probe_no_reg:
	clk_put(b2r2_core.b2r2_clock);
b2r2_probe_no_clk:
	destroy_workqueue(b2r2_core.work_queue);
	b2r2_core.work_queue = NULL;
b2r2_probe_no_work_queue:

	b2r2_log_info("init done with errors.\n");
b2r2_probe_debug_init_failed:

	return ret;

}



/**
 * b2r2_remove - This routine unloads b2r2 driver
 *
 * @pdev: platform device.
 */
static int b2r2_remove(struct platform_device *pdev)
{
	unsigned long flags;

	BUG_ON(pdev == NULL);

	b2r2_log_info("%s started\n", __func__);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(b2r2_core.debugfs_root_dir);
#endif

	/* Flush B2R2 work queue (call all callbacks) */
	flush_workqueue(b2r2_core.work_queue);

	/* Exit b2r2 blt module */
	b2r2_blt_module_exit();

#ifdef HANDLE_TIMEOUTED_JOBS
	cancel_delayed_work(&b2r2_core.timeout_work);
#endif

	/* Flush B2R2 work queue (call all callbacks for
	   cancelled jobs) */
	flush_workqueue(b2r2_core.work_queue);

	/* Make sure the power is turned off */
	cancel_delayed_work_sync(&b2r2_core.domain_disable_work);

	/** Unmap B2R2 registers */
	b2r2_log_info("unmap b2r2 registers..\n");
	if (b2r2_core.hw) {
		iounmap(b2r2_core.hw);

		b2r2_core.hw = NULL;
	}

	destroy_workqueue(b2r2_core.work_queue);

	spin_lock_irqsave(&b2r2_core.lock, flags);
	b2r2_core.work_queue = NULL;
	spin_unlock_irqrestore(&b2r2_core.lock, flags);

	/* Return the clock */
	clk_put(b2r2_core.b2r2_clock);
	regulator_put(b2r2_core.b2r2_reg);

	b2r2_log_info("%s ended\n", __func__);

	b2r2_core.log_dev = NULL;

	b2r2_debug_exit();

	return 0;

}
/**
 * b2r2_suspend() - This routine puts the B2R2 device in to sustend state.
 * @pdev: platform device.
 *
 * This routine stores the current state of the b2r2 device and puts in to suspend state.
 *
 */
int b2r2_suspend(struct platform_device *pdev, pm_message_t state)
{
	b2r2_log_info("%s\n", __func__);

	/* Flush B2R2 work queue (call all callbacks) */
	flush_workqueue(b2r2_core.work_queue);

#ifdef HANDLE_TIMEOUTED_JOBS
	cancel_delayed_work(&b2r2_core.timeout_work);
#endif

	/* Flush B2R2 work queue (call all callbacks for
	   cancelled jobs) */
	flush_workqueue(b2r2_core.work_queue);

	/* Make sure power is turned off */
	cancel_delayed_work_sync(&b2r2_core.domain_disable_work);

	return 0;
}


/**
 * b2r2_resume() - This routine resumes the B2R2 device from sustend state.
 * @pdev: platform device.
 *
 * This routine restore back the current state of the b2r2 device resumes.
 *
 */
int b2r2_resume(struct platform_device *pdev)
{
	b2r2_log_info("%s\n", __func__);

	return 0;
}

/**
 * struct platform_b2r2_driver - Platform driver configuration for the
 * B2R2 core driver
 */
static struct platform_driver platform_b2r2_driver = {
	.remove = b2r2_remove,
	.driver = {
		.name	= "b2r2",
	},
	/** TODO implement power mgmt functions */
	.suspend = b2r2_suspend,
	.resume =  b2r2_resume,
};


/**
 * b2r2_init() - Module init function for the B2R2 core module
 */
static int __init b2r2_init(void)
{
	printk(KERN_INFO "%s\n", __func__);
	return platform_driver_probe(&platform_b2r2_driver, b2r2_probe);
}
module_init(b2r2_init);

/**
 * b2r2_exit() - Module exit function for the B2R2 core module
 */
static void __exit b2r2_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	platform_driver_unregister(&platform_b2r2_driver);
	return;
}
module_exit(b2r2_exit);


/** Module is having GPL license */

MODULE_LICENSE("GPL");

/** Module author & discription */

MODULE_AUTHOR("Robert Fekete (robert.fekete@stericsson.com)");
MODULE_DESCRIPTION("B2R2 Core driver");

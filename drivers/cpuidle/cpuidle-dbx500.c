/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010-2012
 *
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson.
 *
 * Loosely based on cpuidle.c by Sundar Iyer.
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/clockchips.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/mfd/abx500/ab8500-fg.h>
#include <linux/regulator/db8500-prcmu.h>
#include <linux/cpuidle-dbx500.h>

#include <mach/pm.h>
#include <mach/pm-timer.h>
#include <mach/context.h>
#include <mach/id.h>

#include <plat/mtu.h>

#include "cpuidle-dbx500.h"
#include "cpuidle-dbx500_dbg.h"

static u64 last_timestamp = 0;
cstate_power_usage_t cstate_power_usage[CSTATES_NUM][BUF_SIZE];
int pwr_usg_idx[CSTATES_NUM] = {0, 0, 0, 0, 0};

extern struct ab8500_fg *ab8500_di_;

int clockevents_program_event_legacy(struct clock_event_device *dev, ktime_t expires,
                             ktime_t now)
{
	unsigned long long clc;
	int64_t delta;

	if (unlikely(expires.tv64 < 0)) {
		WARN_ON_ONCE(1);
		return -ETIME;
	}

	delta = ktime_to_ns(ktime_sub(expires, now));

	if (delta <= 0)
		return -ETIME;

	dev->next_event = expires;

 	if (dev->mode == CLOCK_EVT_MODE_SHUTDOWN)
		return 0;

	if (delta > dev->max_delta_ns)
		delta = dev->max_delta_ns;
	if (delta < dev->min_delta_ns)
		delta = dev->min_delta_ns;

		clc = delta * dev->mult;
		clc >>= dev->shift;

	return dev->set_next_event((unsigned long) clc, dev);
}

/*
 * All measurements are with two cpus online (worst case) and at
 * 200 MHz (worst case)
 *
 * Enter latency depends on cpu frequency, and is only depending on
 * code executing on the ARM.
 * Exit latency is both depending on "wake latency" which is the
 * time between the PRCMU has gotten the interrupt and the ARM starts
 * to execute and the time before everything is done on the ARM.
 * The wake latency is more or less constant related to cpu frequency,
 * but can differ depending on what the modem does.
 * Wake latency is not included for plain WFI.
 * For states that uses RTC (Sleep & DeepSleep), wake latency is reduced
 * from clock programming timeout.
 *
 */
#define DEEP_SLEEP_WAKE_UP_LATENCY 8500
/* Wake latency from ApSleep is measured to be around 1.0 to 1.5 ms */
#define MIN_SLEEP_WAKE_UP_LATENCY 1000
#define MAX_SLEEP_WAKE_UP_LATENCY 1500

#define UL_PLL_START_UP_LATENCY 8000 /* us */

#define MAX_STATE_DETERMINE_LOOP_TIME 100000 /* usec */

static struct cstate cstates[] = {
	{
		.enter_latency = 0,
		.exit_latency = 0,
		.threshold = 0,
		.power_usage = 1000,
		.APE = APE_ON,
		.ARM = ARM_ON,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_NO_CHANGE,
		.state = CI_RUNNING,
		.desc = "Running                ",
	},
	{
		/* These figures are not really true. There is a cost for WFI */
		.enter_latency = 0,
		.exit_latency = 0,
		.threshold = 0,
		.power_usage = 10,
		.APE = APE_ON,
		.ARM = ARM_ON,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_NO_CHANGE,
		.state = CI_WFI,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.desc = "Wait for interrupt     ",
	},
	{
		.enter_latency = 170,
		.exit_latency = 70,
		.threshold = 260,
		.power_usage = 4,
		.APE = APE_ON,
		.ARM = ARM_RET,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_IDLE,
		.state = CI_IDLE,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.desc = "ApIdle                 ",
	},
	{
		.enter_latency = 350,
		.exit_latency = MAX_SLEEP_WAKE_UP_LATENCY + 200,
		/*
		 * Note: Sleep time must be longer than 120 us or else
		 * there might be issues with the RTC-RTT block.
		 */
		.threshold = MAX_SLEEP_WAKE_UP_LATENCY + 350 + 200,
		.power_usage = 3,
		.APE = APE_OFF,
		.ARM = ARM_RET,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_SLEEP,
		.state = CI_SLEEP,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.desc = "ApSleep                ",
	},
	{
		.enter_latency = 350,
		.exit_latency = (MAX_SLEEP_WAKE_UP_LATENCY +
				 UL_PLL_START_UP_LATENCY + 200),
		.threshold = (MAX_SLEEP_WAKE_UP_LATENCY +
			      UL_PLL_START_UP_LATENCY + 350 + 200),
		.power_usage = 2,
		.APE = APE_OFF,
		.ARM = ARM_RET,
		.UL_PLL = UL_PLL_OFF,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_SLEEP,
		.state = CI_SLEEP,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.desc = "ApSleep, UL PLL off    ",
	},
#ifdef CONFIG_UX500_CPUIDLE_APDEEPIDLE
	{
		.enter_latency = 400,
		.exit_latency = DEEP_SLEEP_WAKE_UP_LATENCY + 400,
		.threshold = DEEP_SLEEP_WAKE_UP_LATENCY + 400 + 400,
		.power_usage = 2,
		.APE = APE_ON,
		.ARM = ARM_OFF,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_DEEP_IDLE,
		.state = CI_DEEP_IDLE,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.desc = "ApDeepIdle, UL PLL off ",
	},
#endif
	{
		.enter_latency = 410,

		.exit_latency = (MAX_SLEEP_WAKE_UP_LATENCY +
				 UL_PLL_START_UP_LATENCY + 200) + 600,
		.threshold = (MAX_SLEEP_WAKE_UP_LATENCY +
			      UL_PLL_START_UP_LATENCY + 350 + 200) + 600 - 400,

		.power_usage = 1,
		.APE = APE_OFF,
		.ARM = ARM_OFF,
		.UL_PLL = UL_PLL_OFF,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_DEEP_SLEEP,
		.state = CI_DEEP_SLEEP,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.desc = "ApDeepsleep, UL PLL off",
	},
};

static unsigned int cstate_threshold[6] = {
	0 /* CI_RUNNING */, 0 /* CI_WFI */, 260 /* CI_IDLE */,
	MAX_SLEEP_WAKE_UP_LATENCY + 350 + 200 /* CI_SLEEP */,
	((MAX_SLEEP_WAKE_UP_LATENCY +
			UL_PLL_START_UP_LATENCY + 350 + 200)), /* CI_SLEEP, UL off */
	((MAX_SLEEP_WAKE_UP_LATENCY +
			UL_PLL_START_UP_LATENCY + 350 + 200) + 600) - 400 /* CI_DEEPSLEEP, ARM off */
};

// CI_RUNNING
module_param_named(sleep_time_threshold_0, cstate_threshold[0], uint, 0644);

// CI_WFI
module_param_named(sleep_time_threshold_1, cstate_threshold[1], uint, 0644);

// CI_IDLE
module_param_named(sleep_time_threshold_2, cstate_threshold[2], uint, 0644);

// CI_SLEEP
module_param_named(sleep_time_threshold_3, cstate_threshold[3], uint, 0644);

// CI_SLEEP, UL PLL off
module_param_named(sleep_time_threshold_4, cstate_threshold[4], uint, 0644);

// CI_DEEP_SLEEP, UL PLL off, ARM off
module_param_named(sleep_time_threshold_5, cstate_threshold[5], uint, 0644);

struct cpu_state {
	int gov_cstate;
	ktime_t sched_wake_up;
	struct cpuidle_device dev;
	bool restore_arm_core;
	bool restore_arm_ret;
	struct cpuidle_driver *driver;
	struct cpuidle_device device;
};

static DEFINE_PER_CPU(struct cpu_state, *cpu_state);

static DEFINE_SPINLOCK(cpuidle_lock);
static bool restore_ape; /* protected by cpuidle_lock */
static bool restore_arm; /* protected by cpuidle_lock */
static ktime_t time_next;  /* protected by cpuidle_lock */

static struct clock_event_device *mtu_clkevt;
static atomic_t idle_cpus_counter = ATOMIC_INIT(0);
static atomic_t master_counter = ATOMIC_INIT(0);

struct cstate *ux500_ci_get_cstates(int *len)
{
	if (len != NULL)
		(*len) = ARRAY_SIZE(cstates);
	return cstates;
}

static void restore_sequence(struct cpu_state *state)
{
	spin_lock(&cpuidle_lock);

	smp_rmb();
	if (state->restore_arm_core) {
		state->restore_arm_core = false;
		smp_wmb();

		context_restore_cpu_registers();
		context_varm_restore_core();
	}

	smp_rmb();
	if (restore_arm) {

		restore_arm = false;
		smp_wmb();

		/* Restore gic settings */
		context_varm_restore_common();
	}

	/* Restore PPI irqs after in ARM retention */
	if (state->restore_arm_ret) {
		state->restore_arm_ret = false;
		context_gic_dist_restore_ppi_irqs();
	}

	smp_rmb();
	if (restore_ape) {
		ktime_t now;

		restore_ape = false;
		smp_wmb();

		/*
		 * APE has been turned off. Save GPIO wake up cause before
		 * clearing ioforce.
		 */
		context_vape_restore();

		ux500_pm_gpio_save_wake_up_status();

		/* Restore IO ring */
		ux500_pm_prcmu_set_ioforce(false);

		ux500_ci_dbg_console_handle_ape_resume();

		ux500_rtcrtt_off();

		/*
		 * Restore prcmu interrupt to cpu0
		 */
		irq_set_affinity(IRQ_DB8500_PRCMU1, cpumask_of(0));

		/*
		 * If we're returning from ApSleep and the RTC timer
		 * caused the wake up, program the MTU to trigger.
		 */
		now = ktime_get();
		if ((ktime_to_us(now) >= ktime_to_us(time_next)))
			time_next = ktime_add(now, ktime_set(0, 1000));
		/* Make sure have an MTU interrupt waiting for us */
		WARN_ON(clockevents_program_event_legacy(mtu_clkevt,
					  time_next,
					  now));
	}

	spin_unlock(&cpuidle_lock);

}

/**
 * get_remaining_sleep_time() - returns remaining sleep time in
 * microseconds (us)
 */
static u32 get_remaining_sleep_time(ktime_t *next, int *on_cpu)
{
	ktime_t now, t;
	int cpu;
	int delta;
	u32 remaining_sleep_time = UINT_MAX;

	now = ktime_get();

	/* Check next schedule to expire considering both cpus */

	spin_lock(&cpuidle_lock);
	for_each_online_cpu(cpu) {
		t = per_cpu(cpu_state, cpu)->sched_wake_up;

		delta = ktime_to_us(ktime_sub(t, now));

		if (delta < remaining_sleep_time) {
			if (delta > 0)
				remaining_sleep_time = (u32)delta;
			else
				remaining_sleep_time = 0;

			if (next)
				(*next) = t;
			if (on_cpu)
				(*on_cpu) = cpu;
		}
	}
	spin_unlock(&cpuidle_lock);

	return remaining_sleep_time;
}

static bool is_last_cpu_running(void)
{
	smp_rmb();
	return atomic_read(&idle_cpus_counter) == num_online_cpus();
}

extern bool pm_is_running;
static atomic_t last_cstate = ATOMIC_INIT(0);

/*
 * Sometimes get_remaining_sleep_time function might return
 * a bogus (e.g. negative) values.
 */

static atomic_t sleep_time_bogus_count = ATOMIC_INIT(0);
module_param_named(sleep_time_bogus_count, sleep_time_bogus_count.counter, uint, 0444);

/*
 * max_depth_actual[i][j]: cstate i was suggested for CPU j by generic cpuidle driver this amount of times
 */

static atomic_t max_depth_actual[6][2] = {
	{ATOMIC_INIT(0), ATOMIC_INIT(0)},
	{ATOMIC_INIT(0), ATOMIC_INIT(0)},
	{ATOMIC_INIT(0), ATOMIC_INIT(0)},
	{ATOMIC_INIT(0), ATOMIC_INIT(0)},
	{ATOMIC_INIT(0), ATOMIC_INIT(0)},
	{ATOMIC_INIT(0), ATOMIC_INIT(0)},
};

module_param_named(max_depth_cpu1_0, max_depth_actual[1][0].counter, uint, 0444);
module_param_named(max_depth_cpu2_0, max_depth_actual[2][0].counter, uint, 0444);
module_param_named(max_depth_cpu3_0, max_depth_actual[3][0].counter, uint, 0444);
module_param_named(max_depth_cpu4_0, max_depth_actual[4][0].counter, uint, 0444);
module_param_named(max_depth_cpu5_0, max_depth_actual[5][0].counter, uint, 0444);
module_param_named(max_depth_cpu1_1, max_depth_actual[1][1].counter, uint, 0444);
module_param_named(max_depth_cpu2_1, max_depth_actual[2][1].counter, uint, 0444);
module_param_named(max_depth_cpu3_1, max_depth_actual[3][1].counter, uint, 0444);
module_param_named(max_depth_cpu4_1, max_depth_actual[4][1].counter, uint, 0444);
module_param_named(max_depth_cpu5_1, max_depth_actual[5][1].counter, uint, 0444);

/*
 * This amount of times a sleep time was too small to enter c-state i.
 */
static unsigned int sleep_time_too_small_count[6] = {0, 0, 0, 0, 0, 0};

module_param_named(sleep_time_too_small_count_1, sleep_time_too_small_count[1], uint, 0444);
module_param_named(sleep_time_too_small_count_2, sleep_time_too_small_count[2], uint, 0444);
module_param_named(sleep_time_too_small_count_3, sleep_time_too_small_count[3], uint, 0444);
module_param_named(sleep_time_too_small_count_4, sleep_time_too_small_count[4], uint, 0444);
module_param_named(sleep_time_too_small_count_5, sleep_time_too_small_count[5], uint, 0444);

/*
 * Represents a count of times when APE was enabled when c-state i
 * was attempted to be entered and therefore was chosen a lower c-state instead.
 */
static unsigned int ape_enabled_count[6] = {0, 0, 0, 0, 0, 0};

/*
 * Same for modem, uard and vbus
 */
static unsigned int modem_enabled_count[6] = {0, 0, 0, 0, 0, 0};
static unsigned int uart_enabled_count[6] = {0, 0, 0, 0, 0, 0};
static unsigned int vbus_enabled_count = 0;

module_param_named(ape_enabled_count_1, ape_enabled_count[1], uint, 0444);
module_param_named(ape_enabled_count_2, ape_enabled_count[2], uint, 0444);
module_param_named(ape_enabled_count_3, ape_enabled_count[3], uint, 0444);
module_param_named(ape_enabled_count_4, ape_enabled_count[4], uint, 0444);
module_param_named(ape_enabled_count_5, ape_enabled_count[5], uint, 0444);

module_param_named(modem_enabled_count_1, modem_enabled_count[1], uint, 0444);
module_param_named(modem_enabled_count_2, modem_enabled_count[2], uint, 0444);
module_param_named(modem_enabled_count_3, modem_enabled_count[3], uint, 0444);
module_param_named(modem_enabled_count_4, modem_enabled_count[4], uint, 0444);
module_param_named(modem_enabled_count_5, modem_enabled_count[5], uint, 0444);
module_param_named(uart_enabled_count_1, uart_enabled_count[1], uint, 0444);
module_param_named(uart_enabled_count_2, uart_enabled_count[2], uint, 0444);
module_param_named(uart_enabled_count_3, uart_enabled_count[3], uint, 0444);
module_param_named(uart_enabled_count_4, uart_enabled_count[4], uint, 0444);
module_param_named(uart_enabled_count_5, uart_enabled_count[5], uint, 0444);

module_param(vbus_enabled_count, uint, 0444);

#define ONE_SEC 1000 * 1000

static int determine_sleep_state(u32 *sleep_time, int loc_idle_counter,
				 bool gic_frozen, ktime_t entry_time,
				 ktime_t *est_wake_time)
{
	int i, return_state;

	int cpu;
	int max_depth;
	int max_depth_per_cpu[2];
	bool uart, modem, ape;
	s64 delta_us;

	/* If first cpu to sleep, go to most shallow sleep state */
	if (loc_idle_counter != num_online_cpus()) {
		return_state = CI_WFI;
		goto out;
	}

	/*
	 * This loop continuously checks if there is an IRQ and exits
	 * immediately if there is, so we shouldn't count the time spent
	 * in there as "irqs off" time.
	 */
	stop_critical_timings();

	/* If other CPU is going to WFI, but not yet there wait. */
	while (1) {
		if (ux500_pm_other_cpu_wfi())
			break;

		/* Check for pending IRQ's */
		if (ux500_pm_gic_pending_interrupt()) {
			start_critical_timings();
			return -1;
		}

		/* If GIC frozen check for pending IRQ's also via PRCMU */
		if (gic_frozen && ux500_pm_prcmu_pending_interrupt(NULL)) {
			start_critical_timings();
			return -1;
		}

		if (!is_last_cpu_running()) {
			start_critical_timings();
			return_state = CI_WFI;
			goto out;
		}

		delta_us = ktime_us_delta(ktime_get(), entry_time);
		if (unlikely(delta_us > MAX_STATE_DETERMINE_LOOP_TIME)) {
			start_critical_timings();
			pr_warning("%s: CPU=%d stuck in loop for %lld usec\n",
				__func__, smp_processor_id(), delta_us);
			return -1;
		}
	}

	start_critical_timings();

	(*sleep_time) = get_remaining_sleep_time(est_wake_time, NULL);

	if (((*sleep_time) == UINT_MAX) || ((*sleep_time) == 0)) {
			atomic_inc(&sleep_time_bogus_count);
			return_state = CI_WFI;
			goto out;
	}

	/*
	 * Never go deeper than the governor recommends even though it might be
	 * possible from a scheduled wake up point of view
	 */
	max_depth = ux500_ci_dbg_deepest_state();
	max_depth_per_cpu[0] = -1;
	max_depth_per_cpu[1] = -1;

	for_each_online_cpu(cpu) {
		max_depth_per_cpu[cpu] = per_cpu(cpu_state, cpu)->gov_cstate;
		if (max_depth > per_cpu(cpu_state, cpu)->gov_cstate)
			max_depth = per_cpu(cpu_state, cpu)->gov_cstate;
	}

	uart = ux500_ci_dbg_force_ape_on();
	ape = power_state_active_is_enabled();
	modem = prcmu_is_ac_wake_requested();

	if (!ape) {
		if (prcmu_is_mcdeclk_on()) {
			ape++;
			printk(KERN_ERR "cpuidle: wrong ape value because MCDE clk is on\n");
		}
		if (prcmu_is_mmcclk_on()) {
			ape++;
			printk(KERN_ERR "cpuidle: wrong ape value because MMC clk is on\n");
		}
	}

	for (i = max_depth; i > 0; i--) {

		if ((*sleep_time) <= cstate_threshold[i]) {
			sleep_time_too_small_count[i]++;
			continue;
		}

		if (cstates[i].APE == APE_OFF) {
			/* This state says APE should be off */
			if (ape || modem || uart) {
				if (ape)
					ape_enabled_count[i]++;
				else if (modem)
					modem_enabled_count[i]++;
				else if (uart)
					uart_enabled_count[i]++;

				continue;
			}
		}

		/* OK state */
		break;
	}

	/* temporary preventing pwr transition during charging */
	{
		extern bool vbus_state;
		if (vbus_state) {
			i = CI_WFI;
			vbus_enabled_count++;
		}
	}

	ux500_ci_dbg_register_reason(i, ape, modem, uart,
				     (*sleep_time),
				     max_depth);

	return_state = max(CI_WFI, i);
out:
	return return_state;
}


/*
 * last_target[i]: c-state i was targeted by this driver this amount of times
 */
static atomic_t last_target[6] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0),
	ATOMIC_INIT(0), ATOMIC_INIT(0),
	ATOMIC_INIT(0), ATOMIC_INIT(0)
};

module_param_named(last_target_1, last_target[1].counter, uint, 0444);
module_param_named(last_target_2, last_target[2].counter, uint, 0444);
module_param_named(last_target_3, last_target[3].counter, uint, 0444);
module_param_named(last_target_4, last_target[4].counter, uint, 0444);
module_param_named(last_target_5, last_target[5].counter, uint, 0444);

/*
 * deepsleep_state[i]: c-state i was entered by this driver this amount of times
 */
static atomic_t deepsleep_state[6]  = {
	ATOMIC_INIT(0), ATOMIC_INIT(0),
	ATOMIC_INIT(0), ATOMIC_INIT(0),
	ATOMIC_INIT(0), ATOMIC_INIT(0)
};

module_param_named(deepsleep_state_1, deepsleep_state[1].counter, uint, 0444);
module_param_named(deepsleep_state_2, deepsleep_state[2].counter, uint, 0444);
module_param_named(deepsleep_state_3, deepsleep_state[3].counter, uint, 0444);
module_param_named(deepsleep_state_4, deepsleep_state[4].counter, uint, 0444);
module_param_named(deepsleep_state_5, deepsleep_state[5].counter, uint, 0444);

/*
 * Pending interrupts (GIC and PRCMU)
 */
static atomic_t pending_gic_irq = ATOMIC_INIT(0);
static atomic_t pending_prcmu_irq = ATOMIC_INIT(0);

module_param_named(pending_gic_irq, pending_gic_irq.counter, uint, 0444);
module_param_named(pending_prcmu_irq, pending_prcmu_irq.counter, uint, 0444);

static int enter_sleep(struct cpuidle_device *dev,
		       struct cpuidle_state *ci_state)
{
	cstate_power_usage_t *pwr_usage_now;
	u64 now;
	ktime_t time_enter, time_exit, time_wake;
	ktime_t wake_up;
	int sleep_time = 0;
	s64 diff;
	int ret;
	int rtcrtt_program_time = NO_SLEEP_PROGRAMMED;
	int target;
	int cpu, cstate;
	struct cpu_state *state;
	bool slept_well = false;
	int this_cpu = smp_processor_id();
	bool migrate_timer;
	bool master = false;
	int loc_idle_counter;
	ktime_t est_wake_time;

	local_irq_disable();

	time_enter = ktime_get(); /* Time now */

	state = per_cpu(cpu_state, smp_processor_id());

	est_wake_time = wake_up = ktime_add(time_enter,
					    tick_nohz_get_sleep_length());

	spin_lock(&cpuidle_lock);

	/* Save scheduled wake up for this cpu */
	state->sched_wake_up = wake_up;

	/* Retrive the cstate that the governor recommends for this CPU */
	state->gov_cstate = (int) cpuidle_get_statedata(ci_state);

	if (state->gov_cstate > ux500_ci_dbg_deepest_state())
		state->gov_cstate = ux500_ci_dbg_deepest_state();

	if (cstates[state->gov_cstate].ARM != ARM_ON)
		migrate_timer = true;
	else
		migrate_timer = false;

	spin_unlock(&cpuidle_lock);

	loc_idle_counter = atomic_inc_return(&idle_cpus_counter);

	/*
	 * Determine sleep state considering both CPUs and
	 * shared resources like e.g. VAPE
	 */
	target = determine_sleep_state(&sleep_time, loc_idle_counter, false,
				       time_enter, &est_wake_time);

	for_each_online_cpu(cpu) {
		cstate = max(CI_WFI, per_cpu(cpu_state, cpu)->gov_cstate);
		atomic_inc(&max_depth_actual[cstate][cpu]);
	}

	atomic_inc(&last_target[target]);
	atomic_set(&last_cstate, target);

	if (target < 0)
		/* "target" will be last_state in the cpuidle framework */
		goto exit_fast;

	/* Only one CPU should master the sleeping sequence */
	if (cstates[target].ARM != ARM_ON) {
		smp_mb();
		if (atomic_inc_return(&master_counter) == 1)
			master = true;
		else
			atomic_dec(&master_counter);
		smp_mb();
	}

	if (migrate_timer)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER,
				   &this_cpu);

	if (master && (cstates[target].ARM != ARM_ON)) {

		ux500_pm_gic_decouple();

		/* Copy GIC interrupt settings to PRCMU interrupt settings */
		ux500_pm_prcmu_copy_gic_settings();

		smp_rmb();
		loc_idle_counter = atomic_read(&idle_cpus_counter);

		/*
		 * Check if sleep state has changed after GIC has been frozen
		 */
		if (target != determine_sleep_state(&sleep_time,
                                                    loc_idle_counter,
                                                    true,
                                                    time_enter,
                                                    &est_wake_time)) {
			atomic_dec(&master_counter);

			goto exit;
		}

		if (ux500_pm_gic_pending_interrupt()) {
			/* An interrupt found => abort */
			atomic_set(&pending_gic_irq, 1);

			/* no PRCMU irq */
			atomic_set(&pending_prcmu_irq, 0);

			atomic_dec(&master_counter);
			goto exit;
		}

		if (ux500_pm_prcmu_pending_interrupt(NULL)) {
			/* An interrupt found => abort */
			atomic_set(&pending_prcmu_irq, 1);
			atomic_dec(&master_counter);
			goto exit;

		}
		/*
		 * No PRCMU interrupt was pending => continue the
		 * sleeping stages
		 */
		atomic_set(&pending_gic_irq, 0);
		atomic_set(&pending_prcmu_irq, 0);

	}

	if (master && (cstates[target].APE == APE_OFF)) {
		int wake_cpu;

		/* We are going to sleep or deep sleep => prepare for it */

		/* Program the only timer that is available when APE is off */

		sleep_time = get_remaining_sleep_time(&est_wake_time,
						      &wake_cpu);

		if ((sleep_time == UINT_MAX) || (sleep_time == 0)) {
			atomic_dec(&master_counter);
			goto exit;
		}

		if (cstates[target].UL_PLL == UL_PLL_OFF)
			/* Compensate for ULPLL start up time */
			sleep_time -= UL_PLL_START_UP_LATENCY;

		/*
		 * Not checking for negative sleep time since
		 * determine_sleep_state has already checked that
		 * there is enough time.
		 */

		/* Adjust for exit latency */
		sleep_time -= MIN_SLEEP_WAKE_UP_LATENCY;

		ux500_rtcrtt_next(sleep_time);
		rtcrtt_program_time = sleep_time;

		/*
		 * Make sure the cpu that is scheduled first gets
		 * the prcmu interrupt.
		 */
		irq_set_affinity(IRQ_DB8500_PRCMU1, cpumask_of(wake_cpu));

		context_vape_save();

		ux500_ci_dbg_console_handle_ape_suspend();
		ux500_pm_prcmu_set_ioforce(true);

		spin_lock(&cpuidle_lock);
		restore_ape = true;
		time_next = est_wake_time;
		spin_unlock(&cpuidle_lock);
	}

	/* Store and disable PPI irqs in ARM retention */
	if (cstates[state->gov_cstate].ARM == ARM_RET) {
		context_gic_dist_store_ppi_irqs();
		state->restore_arm_ret = true;
	}

	if (master && (cstates[target].ARM == ARM_OFF)) {
		int cpu;

		context_varm_save_common();

		spin_lock(&cpuidle_lock);
		restore_arm = true;
		for_each_possible_cpu(cpu) {
			(per_cpu(cpu_state, cpu))->restore_arm_core = true;
		}
		spin_unlock(&cpuidle_lock);
	}

	if (cstates[state->gov_cstate].ARM == ARM_OFF) {
		context_varm_save_core();

		if (master && (cstates[target].ARM == ARM_OFF))
			context_gic_dist_disable_unneeded_irqs();

		context_save_cpu_registers();

		/*
		 * Due to we have only 100us between requesting a
		 * powerstate and wfi, we clean the cache before as
		 * well to assure the final cache clean before wfi
		 * has as little as possible to do.
		 */
		context_clean_l1_cache_all();
	}

	ux500_ci_dbg_log(target, time_enter);

	ux500_ci_dbg_log_post_mortem(target,
				     time_enter,
				     est_wake_time,
				     state->sched_wake_up,
				     rtcrtt_program_time,
				     master);

	if (master && cstates[target].ARM != ARM_ON) {
		prcmu_set_power_state(cstates[target].pwrst,
				      cstates[target].UL_PLL,
				      /* Is actually the AP PLL */
				      cstates[target].UL_PLL);
	}

	if (master)
		atomic_dec(&master_counter);

	stop_critical_timings();

	/*
	 * If deepsleep/deepidle, Save return address to SRAM and set
	 * this CPU in WFI. This is last core to enter sleep, so we need to
	 * clean both L2 and L1 caches
	 */
	if (cstates[state->gov_cstate].ARM == ARM_OFF)
		context_save_to_sram_and_wfi(cstates[target].ARM == ARM_OFF);
	else
		__asm__ __volatile__
			("dsb\n\t" "wfi\n\t" : : : "memory");

	start_critical_timings();

	if (is_last_cpu_running())
		ux500_ci_dbg_wake_latency(target, sleep_time);

	time_wake = ktime_get();

	ux500_ci_dbg_wake_time(time_wake);

	slept_well = true;

	restore_sequence(state);

exit:
	if (!slept_well)
		/* Recouple GIC with the interrupt bus */
		ux500_pm_gic_recouple();

	/* Use the ARM local timer for this cpu */
	if (migrate_timer)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT,
				   &this_cpu);
exit_fast:

	atomic_dec(&idle_cpus_counter);

	if (target < 0)
		target = CI_RUNNING;

	/* 16 minutes ahead */
	wake_up = ktime_add_us(time_enter,
			       1000000000);

	spin_lock(&cpuidle_lock);
	/* Remove wake up time i.e. set wake up far ahead */
	state->sched_wake_up = wake_up;
	spin_unlock(&cpuidle_lock);

	time_exit = ktime_get();
	diff = ktime_to_us(ktime_sub(time_exit, time_enter));
	if (diff > INT_MAX)
		diff = INT_MAX;

	ret = (int)diff;

	ux500_ci_dbg_console_check_uart();
	if (slept_well) {
		ux500_ci_dbg_exit_latency(target,
					  time_exit, /* now */
					  time_wake, /* exit from wfi */
					  time_enter); /* enter cpuidle */

		atomic_inc(&deepsleep_state[target]);

		if (target >= CI_WFI) {
			pwr_usage_now = &cstate_power_usage[target - 1][pwr_usg_idx[target - 1]];

			now = ktime_to_us(time_exit);

			// always log c-state power usage if it's first time
			if (likely(pwr_usg_idx[target - 1] != 0)) {
				// rate-limit c-states logging
				if (now - last_timestamp < 5ULL * ONE_SEC)
					goto out;
			}

			pwr_usage_now->timestamp    = now;
			pwr_usage_now->current_inst = ab8500_di_->inst_curr;
			pwr_usage_now->current_avg  = ab8500_di_->avg_curr;
			last_timestamp = now;

			//pr_err("[cpuidle] state: %d; time: %llu; curr_inst: %d; cuur_avg: %d;\n",
			//		target, pwr_usage_now->timestamp, pwr_usage_now->current_inst, pwr_usage_now->current_avg);

			pwr_usg_idx[target - 1]++;
			if (unlikely(pwr_usg_idx[target - 1] > BUF_SIZE)) {
				pr_err("%s: buffer cstate_power_usage (%d) overflow\n", __func__, target - 1);
				pwr_usg_idx[target - 1] = 0;
			}
		}
	}
out:

	ux500_ci_dbg_log(CI_RUNNING, time_exit);

	local_irq_enable();

	ux500_ci_dbg_console();

	return ret;
}

static int __init init_cstates(int cpu, struct cpu_state *state)
{
	int i;
	struct cpuidle_state *ci_state;
	struct cpuidle_device *dev;

	dev = &state->dev;
	dev->cpu = cpu;

	for (i = 0; i < ARRAY_SIZE(cstates); i++) {

		ci_state = &dev->states[i];

		cpuidle_set_statedata(ci_state, (void *)i);

		ci_state->exit_latency = cstates[i].exit_latency;
		ci_state->target_residency = cstate_threshold[i];
		ci_state->flags = cstates[i].flags;
		ci_state->enter = enter_sleep;
		ci_state->power_usage = cstates[i].power_usage;
		snprintf(ci_state->name, CPUIDLE_NAME_LEN, "C%d", i);
		strncpy(ci_state->desc, cstates[i].desc, CPUIDLE_DESC_LEN);
	}

	dev->state_count = ARRAY_SIZE(cstates);

	return cpuidle_register_device(dev);
}

struct cpuidle_driver dbx500_cpuidle_driver = {
	.name = "dbx500-cpuidle",
	.owner = THIS_MODULE,
};

static int __init dbx500_cpuidle_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	int cpu;
	struct dbx500_cpuidle_platform_data *pdata;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "error: no platform data.\n");
		goto out;
	}

	pdata = (struct dbx500_cpuidle_platform_data *)pdev->dev.platform_data;

	ret = cpuidle_register_driver(&dbx500_cpuidle_driver);
	if (ret)
		goto out;

	ux500_ci_dbg_init();

	for_each_possible_cpu(cpu) {
		per_cpu(cpu_state, cpu) = kzalloc(sizeof(struct cpu_state),
						  GFP_KERNEL);
		if (!per_cpu(cpu_state, cpu)) {
			ret = -ENOMEM;
			goto out_nomem;
		}
		per_cpu(cpu_state, cpu)->driver = &dbx500_cpuidle_driver;
	}

	for_each_possible_cpu(cpu) {
		ret = init_cstates(cpu, per_cpu(cpu_state, cpu));
		if (ret)
			goto out_cstates;
		dev_info(&pdev->dev, "initiated for CPU%d.\n", cpu);
	}

	mtu_clkevt = nmdk_clkevt_get();
	if (!mtu_clkevt) {
		dev_err(&pdev->dev, "could not get MTU timer.\n");
		ret = -EINVAL;
		goto out_clkevt;
	}

	/* Configure wake up reasons */
	prcmu_enable_wakeups(pdata->wakeups);

	return 0;
out_clkevt:
	for_each_possible_cpu(cpu)
		cpuidle_unregister_device(&per_cpu(cpu_state, cpu)->dev);
out_cstates:
	cpuidle_unregister_driver(&dbx500_cpuidle_driver);

	for_each_possible_cpu(cpu)
		kfree(per_cpu(cpu_state, cpu));
out_nomem:
	ux500_ci_dbg_remove();
out:
	dev_err(&pdev->dev, "initialization failed.\n");
	return ret;
}

static void __exit dbx500_cpuidle_exit(void)
{
	int cpu;

	ux500_ci_dbg_remove();

	for_each_possible_cpu(cpu)
		cpuidle_unregister_device(&per_cpu(cpu_state, cpu)->dev);

	for_each_possible_cpu(cpu)
		kfree(per_cpu(cpu_state, cpu));

	cpuidle_unregister_driver(&dbx500_cpuidle_driver);
}

static struct platform_driver dbx500_cpuidle_plat_driver = {
	.driver = {
		.name = "dbx500-cpuidle",
		.owner = THIS_MODULE,
	},
};

static int __init dbx500_cpuidle_init(void)
{
	return platform_driver_probe(&dbx500_cpuidle_plat_driver,
				     dbx500_cpuidle_probe);
}


late_initcall(dbx500_cpuidle_init);
module_exit(dbx500_cpuidle_exit);

MODULE_DESCRIPTION("U8500 cpuidle driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rickard Andersson <rickard.andersson@stericsson.com>");

/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 * Author: Mattias Wallin <mattias.wallin@stericsson.com> for ST-Ericsson
 */
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/clksrc-dbx500-prcmu.h>
#include <linux/clksrc-db5500-mtimer.h>
#include <linux/of.h>

#include <asm/smp_twd.h>

#include <plat/mtu.h>

#include <mach/setup.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/context.h>

#ifdef CONFIG_DBX500_CONTEXT
static int mtu_context_notifier_call(struct notifier_block *this,
				     unsigned long event, void *data)
{
	if (event == CONTEXT_APE_RESTORE)
		nmdk_clksrc_reset();
	return NOTIFY_OK;
}

static struct notifier_block mtu_context_notifier = {
	.notifier_call = mtu_context_notifier_call,
};
#endif

#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(u5500_twd_local_timer,
			      U5500_TWD_BASE, IRQ_LOCALTIMER);
static DEFINE_TWD_LOCAL_TIMER(u8500_twd_local_timer,
			      U8500_TWD_BASE, IRQ_LOCALTIMER);

static void __init ux500_twd_init(void)
{
	struct twd_local_timer *twd_local_timer;
	int err;

	twd_local_timer = cpu_is_u5500() ? &u5500_twd_local_timer :
					   &u8500_twd_local_timer;

	if (of_have_populated_dt())
		twd_local_timer_of_register();
	else {
		err = twd_local_timer_register(twd_local_timer);
		if (err)
			pr_err("twd_local_timer_register failed %d\n", err);
	}
}
#else
#define ux500_twd_init()	do { } while(0)
#endif

static void __init ux500_timer_init(void)
{
	void __iomem *mtu_timer_base;
	void __iomem *prcmu_timer_base;
	int err;

	if (cpu_is_u5500()) {
		mtu_timer_base = __io_address(U5500_MTU0_BASE);
		prcmu_timer_base = __io_address(U5500_PRCMU_TIMER_3_BASE);
	} else if (cpu_is_u8500() || cpu_is_u9540()) {
		mtu_timer_base = __io_address(U8500_MTU0_BASE);
		prcmu_timer_base = __io_address(U8500_PRCMU_TIMER_4_BASE);
	} else {
		ux500_unknown_soc();
	}

	/*
	 * Here we register the timerblocks active in the system.
	 * Localtimers (twd) is started when both cpu is up and running.
	 * MTU register a clocksource, clockevent and sched_clock.
	 * Since the MTU is located in the VAPE power domain
	 * it will be cleared in sleep which makes it unsuitable.
	 * We however need it as a timer tick (clockevent)
	 * during boot to calibrate delay until twd is started.
	 * RTC-RTT have problems as timer tick during boot since it is
	 * depending on delay which is not yet calibrated. RTC-RTT is in the
	 * always-on powerdomain and is used as clockevent instead of twd when
	 * sleeping.
	 *
	 * The PRCMU timer 4 (3 for DB5500) registers a clocksource and
	 * sched_clock with higher rating than the MTU since it is
	 * always-on.
	 *
	 * On DB5500, the MTIMER is the best clocksource since, unlike the
	 * PRCMU timer, it doesn't occasionally go backwards.
	 */

	nmdk_timer_init(mtu_timer_base);
	if (cpu_is_u5500())
		db5500_mtimer_init(__io_address(U5500_MTIMER_BASE));
	clksrc_dbx500_prcmu_init(prcmu_timer_base);
	ux500_twd_init();

#ifdef CONFIG_DBX500_CONTEXT
	WARN_ON(context_ape_notifier_register(&mtu_context_notifier));
#endif
}

static void ux500_timer_reset(void)
{
	nmdk_clkevt_reset();
	nmdk_clksrc_reset();
}

struct sys_timer ux500_timer = {
	.init		= ux500_timer_init,
	.resume		= ux500_timer_reset,
};

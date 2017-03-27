/*
 * Simple stack backtrace regression test module
 *
 * (C) Copyright 2008 Intel Corporation
 * Author: Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>

static void backtrace_test_normal(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk("Testing a backtrace from process context.\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk("The following trace is a kernel self test and not a bug!\n");
#else
	;
#endif

	dump_stack();
}

static DECLARE_COMPLETION(backtrace_work);

static void backtrace_test_irq_callback(unsigned long data)
{
	dump_stack();
	complete(&backtrace_work);
}

static DECLARE_TASKLET(backtrace_tasklet, &backtrace_test_irq_callback, 0);

static void backtrace_test_irq(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk("Testing a backtrace from irq context.\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk("The following trace is a kernel self test and not a bug!\n");
#else
	;
#endif

	init_completion(&backtrace_work);
	tasklet_schedule(&backtrace_tasklet);
	wait_for_completion(&backtrace_work);
}

#ifdef CONFIG_STACKTRACE
static void backtrace_test_saved(void)
{
	struct stack_trace trace;
	unsigned long entries[8];

#ifdef CONFIG_DEBUG_PRINTK
	printk("Testing a saved backtrace.\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk("The following trace is a kernel self test and not a bug!\n");
#else
	;
#endif

	trace.nr_entries = 0;
	trace.max_entries = ARRAY_SIZE(entries);
	trace.entries = entries;
	trace.skip = 0;

	save_stack_trace(&trace);
	print_stack_trace(&trace, 0);
}
#else
static void backtrace_test_saved(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk("Saved backtrace test skipped.\n");
#else
	;
#endif
}
#endif

static int backtrace_regression_test(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk("====[ backtrace testing ]===========\n");
#else
	;
#endif

	backtrace_test_normal();
	backtrace_test_irq();
	backtrace_test_saved();

#ifdef CONFIG_DEBUG_PRINTK
	printk("====[ end of backtrace testing ]====\n");
#else
	;
#endif
	return 0;
}

static void exitf(void)
{
}

module_init(backtrace_regression_test);
module_exit(exitf);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arjan van de Ven <arjan@linux.intel.com>");

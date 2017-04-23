/*
 *  arch/arm/include/asm/localtimer.h
 *
 *  Copyright (C) 2004-2005 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARM_LOCALTIMER_H
#define __ASM_ARM_LOCALTIMER_H

#include <linux/errno.h>

struct clock_event_device;

struct local_timer_ops {
	int  (*setup)(struct clock_event_device *);
	void (*stop)(struct clock_event_device *);
};

/*
 * Per-cpu timer IRQ handler
 */
irqreturn_t percpu_timer_handler(int irq, void *dev_id);

#ifdef CONFIG_LOCAL_TIMERS
<<<<<<< HEAD
=======

#ifdef CONFIG_HAVE_ARM_TWD

#include "smp_twd.h"

#define local_timer_ack()	twd_timer_ack()

#else

/*
 * Platform provides this to acknowledge a local timer IRQ.
 * Returns true if the local timer IRQ is to be processed.
 */
int local_timer_ack(void);

#endif

>>>>>>> parent of 3c30616... ARM: gic, local timers: use the request_percpu_irq() interface
/*
 * Register a local timer driver
 */
int local_timer_register(struct local_timer_ops *);
#else
static inline int local_timer_register(struct local_timer_ops *ops)
{
	return -ENXIO;
}
#endif

#endif

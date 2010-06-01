#ifndef __PLAT_MTU_H
#define __PLAT_MTU_H

void nmdk_timer_init(void __iomem *base);
void nmdk_clkevt_reset(void);
void nmdk_clksrc_reset(void);

/*
 * Guaranteed runtime conversion range in seconds for
 * the clocksource and clockevent.
 */
#define MTU_MIN_RANGE 4

/* should be set by the platform code */
extern void __iomem *mtu_base;

struct clock_event_device *nmdk_clkevt_get(void);

#endif /* __PLAT_MTU_H */


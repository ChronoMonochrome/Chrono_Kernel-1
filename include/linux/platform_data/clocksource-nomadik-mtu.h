#ifndef __PLAT_MTU_H
#define __PLAT_MTU_H

<<<<<<< HEAD:arch/arm/plat-nomadik/include/plat/mtu.h
/* should be set by the platform code */
extern void __iomem *mtu_base;

=======
void nmdk_timer_init(void __iomem *base, int irq);
>>>>>>> 19f949f:include/linux/platform_data/clocksource-nomadik-mtu.h
void nmdk_clkevt_reset(void);
void nmdk_clksrc_reset(void);

struct clock_event_device *nmdk_clkevt_get(void);

#endif /* __PLAT_MTU_H */


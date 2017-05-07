/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *	Based on ARM realview platform
 *
 * Author: Sundar Iyer <sundar.iyer@stericsson.com>
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/completion.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>

#include <mach/context.h>
#include <mach/setup.h>

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void __ref ux500_cpu_die(unsigned int cpu)
{
	flush_cache_all();

	/* directly enter low power state, skipping secure registers */
	for (;;) {

		context_varm_save_core();
		context_save_cpu_registers();

		context_save_to_sram_and_wfi(false);

		context_restore_cpu_registers();
		context_varm_restore_core();

		if (pen_release == cpu_logical_map(cpu)) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}
	}
}

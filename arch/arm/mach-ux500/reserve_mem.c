/* linux/arch/arm/plat-s5p/reserve_mem.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Reserve mem helper functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <asm/setup.h>
#include <linux/io.h>
#include <mach/memory.h>

#include <linux/cma.h>
void __init s5p_cma_region_reserve(struct cma_region *regions_normal,
				      const char *map)
{
	struct cma_region *reg;
	phys_addr_t paddr_last = 0xFFFFFFFF;

	for (reg = regions_normal; reg->size != 0; reg++) {
		phys_addr_t paddr;

		if (!IS_ALIGNED(reg->size, PAGE_SIZE)) {
			pr_debug("S5P/CMA: size of '%s' is NOT page-aligned\n",
								reg->name);
			reg->size = PAGE_ALIGN(reg->size);
		}


		if (reg->reserved) {
			pr_err("S5P/CMA: '%s' already reserved\n", reg->name);
			continue;
		}

		if (reg->alignment) {
			if ((reg->alignment & ~PAGE_MASK) ||
				(reg->alignment & ~reg->alignment)) {
				pr_err("S5P/CMA: Failed to reserve '%s': "
						"incorrect alignment 0x%08x.\n",
						reg->name, reg->alignment);
				continue;
			}
		} else {
			reg->alignment = PAGE_SIZE;
		}

		if (reg->start) {
			if (!memblock_is_region_reserved(reg->start, reg->size)
			    && (memblock_reserve(reg->start, reg->size) == 0))
				reg->reserved = 1;
			else {
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
				       reg->name);
				continue;
			}

			pr_debug("S5P/CMA: "
				 "Reserved 0x%08x/0x%08x for '%s'\n",
				 reg->start, reg->size, reg->name);
			paddr = reg->start;
		} else {
			paddr = memblock_find_in_range(0,
					MEMBLOCK_ALLOC_ACCESSIBLE,
					reg->size, reg->alignment);
		}

		if (paddr != 0) {
			if (memblock_reserve(paddr, reg->size)) {
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
								reg->name);
				continue;
			}

			reg->start = paddr;
			reg->reserved = 1;

			pr_info("S5P/CMA: Reserved 0x%08x/0x%08x for '%s'\n",
						reg->start, reg->size, reg->name);
		} else {
			pr_err("S5P/CMA: No free space in memory for '%s'\n",
								reg->name);
		}

		if (cma_early_region_register(reg)) {
			pr_err("S5P/CMA: Failed to register '%s'\n",
								reg->name);
			memblock_free(reg->start, reg->size);
		} else {
			paddr_last = min(paddr, paddr_last);
		}
	}

	if (map)
		cma_set_defaults(NULL, map);
}


#if defined(CONFIG_CMA)
void __init exynos4_reserve_mem(void)
{
	static struct cma_region regions[] = {
		{
			.name	= "ion",
			.size	= CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
	};
	static const char map[] __initconst =
		"ion-exynos=ion;";

	s5p_cma_region_reserve(regions, map);
}
#else
inline void exynos4_reserve_mem(void)
{
}
#endif /* CONFIG_CMA */


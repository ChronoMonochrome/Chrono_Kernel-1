/*
 * Copyright (C) ST-Ericsson SA 2010
 * License terms: GNU General Public License (GPL), version 2.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <asm/io.h>

 #include "cm_dma.h"

 #define CMDMA_LIDX (2)
 #define CMDMA_BASE (0x801C0000)
 #define CMDMA_REG_LCLA (0x024)

void __iomem *virtbase = NULL;

static int cmdma_write_cyclic_list_mem2per(
    unsigned int from_addr,
    unsigned int to_addr,
    unsigned int segments,
    unsigned int segmentsize,
    unsigned int LOS);

static int cmdma_write_cyclic_list_per2mem(
    unsigned int from_addr,
    unsigned int to_addr,
    unsigned int segments,
    unsigned int segmentsize,
    unsigned int LOS);

int cmdma_setup_relink_area( unsigned int mem_addr,
    unsigned int per_addr,
    unsigned int segments,
    unsigned int segmentsize,
    unsigned int LOS,
    enum cmdma_type type)
{
    switch (type) {

    case CMDMA_MEM_2_PER:
        return cmdma_write_cyclic_list_mem2per(
            mem_addr,
            per_addr,
            segments,
            segmentsize,
            LOS);

    case CMDMA_PER_2_MEM:
        return cmdma_write_cyclic_list_per2mem(
            per_addr,
            mem_addr,
            segments,
            segmentsize,
            LOS);

    default :
        return -EINVAL;
    }
 }

 static unsigned int cmdma_getlcla( void) {

    if(!virtbase)
        virtbase = ioremap(CMDMA_BASE, CMDMA_REG_LCLA + sizeof(int) );

    return readl(virtbase + CMDMA_REG_LCLA);
 }

 static void cmdma_write_relink_params_mem2per (
    int * relink,
    unsigned int LOS,
    unsigned int nb_element,
    unsigned int src_addr,
    unsigned int dst_addr,
    unsigned int burst_size) {

    relink[0] =  (((long)(nb_element & 0xFFFF)) << 16) |
        (src_addr & 0xFFFF);

    relink[1] = (((src_addr >> 16)  & 0xFFFFUL) << 16) |
        (0x1200UL | (LOS << 1) | (burst_size<<10));

    relink[2] = ((nb_element & 0xFFFF) << 16)  |
        (dst_addr & 0xFFFF);

    relink[3] = (((dst_addr >> 16)  & 0xFFFFUL) << 16 ) |
        0x8201UL | ((LOS+1) << 1) | (burst_size<<10);
}

static void cmdma_write_relink_params_per2mem (
    int * relink,
    unsigned int LOS,
    unsigned int nb_element,
    unsigned int src_addr,
    unsigned int dst_addr,
    unsigned int burst_size) {

    relink[0] =  (((long)(nb_element & 0xFFFF)) << 16) |
        (src_addr & 0xFFFF);

    relink[1] = (((src_addr >> 16)  & 0xFFFFUL) << 16) |
        (0x8201UL | (LOS << 1) | (burst_size<<10));

    relink[2] = ((nb_element & 0xFFFF) << 16)  |
        (dst_addr & 0xFFFF);

    relink[3] = (((dst_addr >> 16)  & 0xFFFFUL) << 16 ) |
        0x1200UL | ((LOS+1) << 1) | (burst_size<<10);
}

static int cmdma_write_cyclic_list_mem2per(
    unsigned int from_addr,
    unsigned int to_addr,
    unsigned int segments,
    unsigned int segmentsize,
    unsigned int LOS) {

    unsigned int i,j;
    int *relink;

    j = LOS;

    for ( i = 0; i < segments; i++) {
        relink = phys_to_virt (cmdma_getlcla() + 1024 * CMDMA_LIDX + 8 * j);

        if (i == (segments-1))
                j = LOS;
            else
                j += 2;

        cmdma_write_relink_params_mem2per (
            relink,
            j,
            segmentsize / 4,
            from_addr,
            to_addr,
            0x2);

        from_addr += segmentsize;
	}

    return 0;
}

static int cmdma_write_cyclic_list_per2mem(
    unsigned int from_addr,
    unsigned int to_addr,
    unsigned int segments,
    unsigned int segmentsize,
    unsigned int LOS) {

    unsigned int i,j;
    int *relink;
    j = LOS;

    for ( i = 0; i < segments; i++) {
        relink = phys_to_virt (cmdma_getlcla() + 1024 * CMDMA_LIDX + 8 * j);

        if (i == (segments-1))
            j = LOS;
        else
            j += 2;

        cmdma_write_relink_params_per2mem (
            relink,
            j,
            segmentsize / 4,
            from_addr,
            to_addr,
            0x2);

        to_addr += segmentsize;
    }

    return 0;
}

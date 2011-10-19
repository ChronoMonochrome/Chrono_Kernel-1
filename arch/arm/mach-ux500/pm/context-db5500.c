/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com>,
 *         Rickard Andersson <rickard.andersson@stericsson.com>,
 *         Sundar Iyer <sundar.iyer@stericsson.com>,
 *         ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/context.h>

/* These registers are DB5500 specific */
#define NODE_HIBW1_ESRAM_IN_0_PRIORITY		0x0
#define NODE_HIBW1_ESRAM_IN_1_PRIORITY		0x4

#define NODE_HIBW1_ESRAM_IN_0_ARB_1_LIMIT	0x18
#define NODE_HIBW1_ESRAM_IN_0_ARB_2_LIMIT	0x1C
#define NODE_HIBW1_ESRAM_IN_0_ARB_3_LIMIT	0x20

#define NODE_HIBW1_ESRAM_IN_1_ARB_1_LIMIT	0x24
#define NODE_HIBW1_ESRAM_IN_1_ARB_2_LIMIT	0x28
#define NODE_HIBW1_ESRAM_IN_1_ARB_3_LIMIT	0x2C

#define NODE_HIBW1_DDR_IN_0_PRIORITY		0x400
#define NODE_HIBW1_DDR_IN_1_PRIORITY		0x404
#define NODE_HIBW1_DDR_IN_2_PRIORITY		0x408

#define NODE_HIBW1_DDR_IN_0_LIMIT		0x424
#define NODE_HIBW1_DDR_IN_1_LIMIT		0x428
#define NODE_HIBW1_DDR_IN_2_LIMIT		0x42C

#define NODE_HIBW1_DDR_OUT_0_PRIORITY		0x430

#define NODE_HIBW2_ESRAM_IN_0_PRIORITY		0x800
#define NODE_HIBW2_ESRAM_IN_1_PRIORITY		0x804

#define NODE_HIBW2_ESRAM_IN_0_ARB_1_LIMIT	0x818
#define NODE_HIBW2_ESRAM_IN_0_ARB_2_LIMIT	0x81C
#define NODE_HIBW2_ESRAM_IN_0_ARB_3_LIMIT	0x820

#define NODE_HIBW2_ESRAM_IN_1_ARB_1_LIMIT	0x824
#define NODE_HIBW2_ESRAM_IN_1_ARB_2_LIMIT	0x828
#define NODE_HIBW2_ESRAM_IN_1_ARB_3_LIMIT	0x82C

#define NODE_HIBW2_DDR_IN_0_PRIORITY		0xC00
#define NODE_HIBW2_DDR_IN_1_PRIORITY		0xC04
#define NODE_HIBW2_DDR_IN_2_PRIORITY		0xC08
#define NODE_HIBW2_DDR_IN_3_PRIORITY		0xC0C

#define NODE_HIBW2_DDR_IN_0_LIMIT		0xC30
#define NODE_HIBW2_DDR_IN_1_LIMIT		0xC34
#define NODE_HIBW2_DDR_IN_2_LIMIT		0xC38
#define NODE_HIBW2_DDR_IN_3_LIMIT		0xC3C

#define NODE_HIBW2_DDR_OUT_0_PRIORITY		0xC40

#define NODE_ESRAM0_IN_0_PRIORITY		0x1000
#define NODE_ESRAM0_IN_1_PRIORITY		0x1004
#define NODE_ESRAM0_IN_2_PRIORITY		0x1008

#define NODE_ESRAM0_IN_0_LIMIT			0x1024
#define NODE_ESRAM0_IN_1_LIMIT			0x1028
#define NODE_ESRAM0_IN_2_LIMIT			0x102C
#define NODE_ESRAM0_OUT_0_PRIORITY		0x1030

#define NODE_ESRAM1_2_IN_0_PRIORITY		0x1400
#define NODE_ESRAM1_2_IN_1_PRIORITY		0x1404
#define NODE_ESRAM1_2_IN_2_PRIORITY		0x1408

#define NODE_ESRAM1_2_IN_0_ARB_1_LIMIT		0x1424
#define NODE_ESRAM1_2_IN_1_ARB_1_LIMIT		0x1428
#define NODE_ESRAM1_2_IN_2_ARB_1_LIMIT		0x142C
#define NODE_ESRAM1_2_OUT_0_PRIORITY		0x1430

#define NODE_ESRAM3_4_IN_0_PRIORITY		0x1800
#define NODE_ESRAM3_4_IN_1_PRIORITY		0x1804
#define NODE_ESRAM3_4_IN_2_PRIORITY		0x1808

#define NODE_ESRAM3_4_IN_0_ARB_1_LIMIT		0x1824
#define NODE_ESRAM3_4_IN_1_ARB_1_LIMIT		0x1828
#define NODE_ESRAM3_4_IN_2_ARB_1_LIMIT		0x182C
#define NODE_ESRAM3_4_OUT_0_PRIORITY		0x1830

/*
 * Save ICN (Interconnect or Interconnect nodes) configuration registers
 * TODO: This can be optimized, for example if we have
 * a static ICN configuration.
 */

static struct {
	void __iomem *base;
	u32 hibw1_esram_in_pri[2];
	u32 hibw1_esram_in0_arb[3];
	u32 hibw1_esram_in1_arb[3];
	u32 hibw1_ddr_in_prio[3];
	u32 hibw1_ddr_in_limit[3];
	u32 hibw1_ddr_out_prio_reg;

	/* HiBw2 node registers */
	u32 hibw2_esram_in_pri[2];
	u32 hibw2_esram_in0_arblimit[3];
	u32 hibw2_esram_in1_arblimit[3];
	u32 hibw2_ddr_in_prio[4];
	u32 hibw2_ddr_in_limit[4];
	u32 hibw2_ddr_out_prio_reg;

	/* ESRAM node registers */
	u32 esram_in_prio[3];
	u32 esram_in_lim[3];
	u32 esram_out_prio_reg;

	u32 esram12_in_prio[3];
	u32 esram12_in_arb_lim[3];
	u32 esram12_out_prio_reg;

	u32 esram34_in_prio[3];
	u32 esram34_in_arb_lim[3];
	u32 esram34_out_prio;
} context_icn;


void u5500_context_save_icn(void)
{
	/* hibw1 */
	context_icn.hibw1_esram_in_pri[0] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_0_PRIORITY);
	context_icn.hibw1_esram_in_pri[1] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_1_PRIORITY);

	context_icn.hibw1_esram_in0_arb[0] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_1_LIMIT);
	context_icn.hibw1_esram_in0_arb[1] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_2_LIMIT);
	context_icn.hibw1_esram_in0_arb[2] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_3_LIMIT);

	context_icn.hibw1_esram_in1_arb[0] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_1_LIMIT);
	context_icn.hibw1_esram_in1_arb[1] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_2_LIMIT);
	context_icn.hibw1_esram_in1_arb[2] =
		readl(context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_3_LIMIT);

	context_icn.hibw1_ddr_in_prio[0] =
		readl(context_icn.base + NODE_HIBW1_DDR_IN_0_PRIORITY);
	context_icn.hibw1_ddr_in_prio[1] =
		readl(context_icn.base + NODE_HIBW1_DDR_IN_1_PRIORITY);
	context_icn.hibw1_ddr_in_prio[2] =
		readl(context_icn.base + NODE_HIBW1_DDR_IN_2_PRIORITY);

	context_icn.hibw1_ddr_in_limit[0] =
		readl(context_icn.base + NODE_HIBW1_DDR_IN_0_LIMIT);
	context_icn.hibw1_ddr_in_limit[1] =
		readl(context_icn.base + NODE_HIBW1_DDR_IN_1_LIMIT);
	context_icn.hibw1_ddr_in_limit[2] =
		readl(context_icn.base + NODE_HIBW1_DDR_IN_2_LIMIT);

	context_icn.hibw1_ddr_out_prio_reg =
		readl(context_icn.base + NODE_HIBW1_DDR_OUT_0_PRIORITY);

	/* hibw2 */
	context_icn.hibw2_esram_in_pri[0] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_0_PRIORITY);
	context_icn.hibw2_esram_in_pri[1] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_1_PRIORITY);

	context_icn.hibw2_esram_in0_arblimit[0] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_1_LIMIT);
	context_icn.hibw2_esram_in0_arblimit[1] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_2_LIMIT);
	context_icn.hibw2_esram_in0_arblimit[2] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_3_LIMIT);

	context_icn.hibw2_esram_in1_arblimit[0] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_1_LIMIT);
	context_icn.hibw2_esram_in1_arblimit[1] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_2_LIMIT);
	context_icn.hibw2_esram_in1_arblimit[2] =
		readl(context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_3_LIMIT);

	context_icn.hibw2_ddr_in_prio[0] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_0_PRIORITY);
	context_icn.hibw2_ddr_in_prio[1] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_1_PRIORITY);
	context_icn.hibw2_ddr_in_prio[2] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_2_PRIORITY);
	context_icn.hibw2_ddr_in_prio[3] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_3_PRIORITY);

	context_icn.hibw2_ddr_in_limit[0] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_0_LIMIT);
	context_icn.hibw2_ddr_in_limit[1] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_1_LIMIT);
	context_icn.hibw2_ddr_in_limit[2] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_2_LIMIT);
	context_icn.hibw2_ddr_in_limit[3] =
		readl(context_icn.base + NODE_HIBW2_DDR_IN_3_LIMIT);

	context_icn.hibw2_ddr_out_prio_reg =
		readl(context_icn.base + NODE_HIBW2_DDR_OUT_0_PRIORITY);

	/* ESRAM0 */
	context_icn.esram_in_prio[0] =
		readl(context_icn.base + NODE_ESRAM0_IN_0_PRIORITY);
	context_icn.esram_in_prio[1] =
		readl(context_icn.base + NODE_ESRAM0_IN_1_PRIORITY);
	context_icn.esram_in_prio[2] =
		readl(context_icn.base + NODE_ESRAM0_IN_2_PRIORITY);

	context_icn.esram_in_lim[0] =
		readl(context_icn.base + NODE_ESRAM0_IN_0_LIMIT);
	context_icn.esram_in_lim[1] =
		readl(context_icn.base + NODE_ESRAM0_IN_1_LIMIT);
	context_icn.esram_in_lim[2] =
		readl(context_icn.base + NODE_ESRAM0_IN_2_LIMIT);

	context_icn.esram_out_prio_reg =
		readl(context_icn.base + NODE_ESRAM0_OUT_0_PRIORITY);

	/* ESRAM1-2 */
	context_icn.esram12_in_prio[0] =
		readl(context_icn.base + NODE_ESRAM1_2_IN_0_PRIORITY);
	context_icn.esram12_in_prio[1] =
		readl(context_icn.base + NODE_ESRAM1_2_IN_1_PRIORITY);
	context_icn.esram12_in_prio[2] =
		readl(context_icn.base + NODE_ESRAM1_2_IN_2_PRIORITY);

	context_icn.esram12_in_arb_lim[0] =
		readl(context_icn.base + NODE_ESRAM1_2_IN_0_ARB_1_LIMIT);
	context_icn.esram12_in_arb_lim[1] =
		readl(context_icn.base + NODE_ESRAM1_2_IN_1_ARB_1_LIMIT);
	context_icn.esram12_in_arb_lim[2] =
		readl(context_icn.base + NODE_ESRAM1_2_IN_2_ARB_1_LIMIT);

	context_icn.esram12_out_prio_reg =
		readl(context_icn.base + NODE_ESRAM1_2_OUT_0_PRIORITY);

	/* ESRAM3-4 */
	context_icn.esram34_in_prio[0] =
		readl(context_icn.base + NODE_ESRAM3_4_IN_0_PRIORITY);
	context_icn.esram34_in_prio[1] =
		readl(context_icn.base + NODE_ESRAM3_4_IN_1_PRIORITY);
	context_icn.esram34_in_prio[2] =
		readl(context_icn.base + NODE_ESRAM3_4_IN_2_PRIORITY);

	context_icn.esram34_in_arb_lim[0] =
		readl(context_icn.base + NODE_ESRAM3_4_IN_0_ARB_1_LIMIT);
	context_icn.esram34_in_arb_lim[1] =
		readl(context_icn.base + NODE_ESRAM3_4_IN_1_ARB_1_LIMIT);
	context_icn.esram34_in_arb_lim[2] =
		readl(context_icn.base + NODE_ESRAM3_4_IN_2_ARB_1_LIMIT);

	context_icn.esram34_out_prio =
		readl(context_icn.base + NODE_ESRAM3_4_OUT_0_PRIORITY);
}

/*
 * Restore ICN configuration registers
 */
void u5500_context_restore_icn(void)
{

	/* hibw1 */
	writel(context_icn.hibw1_esram_in_pri[0],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_0_PRIORITY);
	writel(context_icn.hibw1_esram_in_pri[1],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_1_PRIORITY);

	writel(context_icn.hibw1_esram_in0_arb[0],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_1_LIMIT);
	writel(context_icn.hibw1_esram_in0_arb[1],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_2_LIMIT);
	writel(context_icn.hibw1_esram_in0_arb[2],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_3_LIMIT);

	writel(context_icn.hibw1_esram_in1_arb[0],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_1_LIMIT);
	writel(context_icn.hibw1_esram_in1_arb[1],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_2_LIMIT);
	writel(context_icn.hibw1_esram_in1_arb[2],
	       context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_3_LIMIT);

	writel(context_icn.hibw1_ddr_in_prio[0],
	       context_icn.base + NODE_HIBW1_DDR_IN_0_PRIORITY);
	writel(context_icn.hibw1_ddr_in_prio[1],
	       context_icn.base + NODE_HIBW1_DDR_IN_1_PRIORITY);
	writel(context_icn.hibw1_ddr_in_prio[2],
	       context_icn.base + NODE_HIBW1_DDR_IN_2_PRIORITY);

	writel(context_icn.hibw1_ddr_in_limit[0],
	       context_icn.base + NODE_HIBW1_DDR_IN_0_LIMIT);
	writel(context_icn.hibw1_ddr_in_limit[1],
	       context_icn.base + NODE_HIBW1_DDR_IN_1_LIMIT);
	writel(context_icn.hibw1_ddr_in_limit[2],
	       context_icn.base + NODE_HIBW1_DDR_IN_2_LIMIT);

	writel(context_icn.hibw1_ddr_out_prio_reg,
	       context_icn.base + NODE_HIBW1_DDR_OUT_0_PRIORITY);

	/* hibw2 */
	writel(context_icn.hibw2_esram_in_pri[0],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_0_PRIORITY);
	writel(context_icn.hibw2_esram_in_pri[1],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_1_PRIORITY);

	writel(context_icn.hibw2_esram_in0_arblimit[0],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_1_LIMIT);
	writel(context_icn.hibw2_esram_in0_arblimit[1],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_2_LIMIT);
	writel(context_icn.hibw2_esram_in0_arblimit[2],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_3_LIMIT);

	writel(context_icn.hibw2_esram_in1_arblimit[0],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_1_LIMIT);
	writel(context_icn.hibw2_esram_in1_arblimit[1],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_2_LIMIT);
	writel(context_icn.hibw2_esram_in1_arblimit[2],
	       context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_3_LIMIT);

	writel(context_icn.hibw2_ddr_in_prio[0],
	       context_icn.base + NODE_HIBW2_DDR_IN_0_PRIORITY);
	writel(context_icn.hibw2_ddr_in_prio[1],
	       context_icn.base + NODE_HIBW2_DDR_IN_1_PRIORITY);
	writel(context_icn.hibw2_ddr_in_prio[2],
	       context_icn.base + NODE_HIBW2_DDR_IN_2_PRIORITY);
	writel(context_icn.hibw2_ddr_in_prio[3],
	       context_icn.base + NODE_HIBW2_DDR_IN_3_PRIORITY);

	writel(context_icn.hibw2_ddr_in_limit[0],
	       context_icn.base + NODE_HIBW2_DDR_IN_0_LIMIT);
	writel(context_icn.hibw2_ddr_in_limit[1],
	       context_icn.base + NODE_HIBW2_DDR_IN_1_LIMIT);
	writel(context_icn.hibw2_ddr_in_limit[2],
	       context_icn.base + NODE_HIBW2_DDR_IN_2_LIMIT);
	writel(context_icn.hibw2_ddr_in_limit[3],
	       context_icn.base + NODE_HIBW2_DDR_IN_3_LIMIT);

	writel(context_icn.hibw2_ddr_out_prio_reg,
	       context_icn.base + NODE_HIBW2_DDR_OUT_0_PRIORITY);

	/* ESRAM0 */
	writel(context_icn.esram_in_prio[0],
	       context_icn.base + NODE_ESRAM0_IN_0_PRIORITY);
	writel(context_icn.esram_in_prio[1],
	       context_icn.base + NODE_ESRAM0_IN_1_PRIORITY);
	writel(context_icn.esram_in_prio[2],
	       context_icn.base + NODE_ESRAM0_IN_2_PRIORITY);

	writel(context_icn.esram_in_lim[0],
	       context_icn.base + NODE_ESRAM0_IN_0_LIMIT);
	writel(context_icn.esram_in_lim[1],
	       context_icn.base + NODE_ESRAM0_IN_1_LIMIT);
	writel(context_icn.esram_in_lim[2],
	       context_icn.base + NODE_ESRAM0_IN_2_LIMIT);

	writel(context_icn.esram_out_prio_reg,
	       context_icn.base + NODE_ESRAM0_OUT_0_PRIORITY);

	/* ESRAM1-2 */
	writel(context_icn.esram12_in_prio[0],
	       context_icn.base + NODE_ESRAM1_2_IN_0_PRIORITY);
	writel(context_icn.esram12_in_prio[1],
	       context_icn.base + NODE_ESRAM1_2_IN_1_PRIORITY);
	writel(context_icn.esram12_in_prio[2],
	       context_icn.base + NODE_ESRAM1_2_IN_2_PRIORITY);

	writel(context_icn.esram12_in_arb_lim[0],
	       context_icn.base + NODE_ESRAM1_2_IN_0_ARB_1_LIMIT);
	writel(context_icn.esram12_in_arb_lim[1],
	       context_icn.base + NODE_ESRAM1_2_IN_1_ARB_1_LIMIT);
	writel(context_icn.esram12_in_arb_lim[2],
	       context_icn.base + NODE_ESRAM1_2_IN_2_ARB_1_LIMIT);

	writel(context_icn.esram12_out_prio_reg,
	       context_icn.base + NODE_ESRAM1_2_OUT_0_PRIORITY);

	/* ESRAM3-4 */
	writel(context_icn.esram34_in_prio[0],
	       context_icn.base + NODE_ESRAM3_4_IN_0_PRIORITY);
	writel(context_icn.esram34_in_prio[1],
	       context_icn.base + NODE_ESRAM3_4_IN_1_PRIORITY);
	writel(context_icn.esram34_in_prio[2],
	       context_icn.base + NODE_ESRAM3_4_IN_2_PRIORITY);

	writel(context_icn.esram34_in_arb_lim[0],
	       context_icn.base + NODE_ESRAM3_4_IN_0_ARB_1_LIMIT);
	writel(context_icn.esram34_in_arb_lim[1],
	       context_icn.base + NODE_ESRAM3_4_IN_1_ARB_1_LIMIT);
	writel(context_icn.esram34_in_arb_lim[2],
	       context_icn.base + NODE_ESRAM3_4_IN_2_ARB_1_LIMIT);

	writel(context_icn.esram34_out_prio,
	       context_icn.base + NODE_ESRAM3_4_OUT_0_PRIORITY);

}

void u5500_context_init(void)
{
	context_icn.base = ioremap(U5500_ICN_BASE, SZ_8K);
}

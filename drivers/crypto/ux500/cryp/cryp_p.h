/**
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Shujuan Chen <shujuan.chen@stericsson.com> for ST-Ericsson.
 * Author: Jonas Linde <jonas.linde@stericsson.com> for ST-Ericsson.
 * Author: Joakim Bech <joakim.xx.bech@stericsson.com> for ST-Ericsson.
 * Author: Berne Hebark <berne.herbark@stericsson.com> for ST-Ericsson.
 * Author: Niklas Hernaeus <niklas.hernaeus@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef _CRYP_P_H_
#define _CRYP_P_H_

#include <linux/io.h>
#include <linux/bitops.h>

#include "cryp.h"
#include "cryp_irqp.h"

/**
 * Generic Macros
 */
#define CRYP_SET_BITS(reg_name, mask) \
	writel((readl(reg_name) | mask), reg_name)

#define CRYP_WRITE_BIT(reg_name, val, mask) \
	writel(((readl(reg_name) & ~(mask)) | ((val) & (mask))), reg_name)

#define CRYP_TEST_BITS(reg_name, val) \
	(readl(reg_name) & (val))

#define CRYP_PUT_BITS(reg, val, shift, mask) \
	writel(((readl(reg) & ~(mask)) | \
		(((u32)val << shift) & (mask))), reg)

/**
 * CRYP specific Macros
 */
#define CRYP_PERIPHERAL_ID0		0xE3
#define CRYP_PERIPHERAL_ID1		0x05
#define CRYP_PERIPHERAL_ID2		0x28
#define CRYP_PERIPHERAL_ID3		0x00

#define CRYP_PCELL_ID0			0x0D
#define CRYP_PCELL_ID1			0xF0
#define CRYP_PCELL_ID2			0x05
#define CRYP_PCELL_ID3			0xB1

/**
 * CRYP register default values
 */
#define MAX_DEVICE_SUPPORT		2
#define CRYP_CR_DEFAULT			0x0002
#define CRYP_CR_FFLUSH			BIT(14)
#define CRYP_DMACR_DEFAULT		0x0
#define CRYP_IMSC_DEFAULT		0x0
#define CRYP_DIN_DEFAULT		0x0
#define CRYP_DOUT_DEFAULT		0x0
#define CRYP_KEY_DEFAULT		0x0
#define CRYP_INIT_VECT_DEFAULT		0x0

/**
 * CRYP Control register specific mask
 */
#define CRYP_SECURE_MASK		BIT(0)
#define CRYP_PRLG_MASK			BIT(1)
#define CRYP_ENC_DEC_MASK		BIT(2)
#define CRYP_SR_BUSY_MASK		BIT(4)
#define CRYP_KEY_ACCESS_MASK		BIT(10)
#define CRYP_KSE_MASK			BIT(11)
#define CRYP_START_MASK			BIT(12)
#define CRYP_INIT_MASK			BIT(13)
#define CRYP_FIFO_FLUSH_MASK		BIT(14)
#define CRYP_CRYPEN_MASK		BIT(15)
#define CRYP_INFIFO_READY_MASK		(BIT(0) | BIT(1))
#define CRYP_ALGOMODE_MASK		(BIT(5) | BIT(4) | BIT(3))
#define CRYP_DATA_TYPE_MASK		(BIT(7) | BIT(6))
#define CRYP_KEY_SIZE_MASK		(BIT(9) | BIT(8))

/**
 * Bit position used while setting bits in register
 */
#define CRYP_PRLG_POS			1
#define CRYP_ENC_DEC_POS		2
#define CRYP_ALGOMODE_POS		3
#define CRYP_SR_BUSY_POS		4
#define CRYP_DATA_TYPE_POS		6
#define CRYP_KEY_SIZE_POS		8
#define CRYP_KEY_ACCESS_POS		10
#define CRYP_KSE_POS			11
#define CRYP_START_POS			12
#define CRYP_INIT_POS			13
#define CRYP_CRYPEN_POS			15

/**
 * CRYP Status register
 */
#define CRYP_BUSY_STATUS_MASK  BIT(4)

/**
 * CRYP PCRs------PC_NAND control register
 * BIT_MASK
 */
#define CRYP_DMA_REQ_MASK		(BIT(1) | BIT(0))
#define CRYP_DMA_REQ_MASK_POS		0


struct cryp_system_context {
	/* CRYP Register structure */
	struct cryp_register *p_cryp_reg[MAX_DEVICE_SUPPORT];
};

#endif

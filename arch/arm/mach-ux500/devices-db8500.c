/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio/nomadik.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl022.h>

#include <plat/ste_dma40.h>

#include <mach/hardware.h>
#include <mach/setup.h>
#include <video/mcde.h>
#include <mach/prcmu-fw-api.h>
#include <mach/ste-dma40-db8500.h>

static struct resource dma40_resources[] = {
	[0] = {
		.start = U8500_DMA_BASE,
		.end   = U8500_DMA_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
		.name  = "base",
	},
	[1] = {
		.start = U8500_DMA_LCPA_BASE,
		.end   = U8500_DMA_LCPA_BASE + 2 * SZ_1K - 1,
		.flags = IORESOURCE_MEM,
		.name  = "lcpa",
	},
	[2] = {
		.start = IRQ_DB8500_DMA,
		.end   = IRQ_DB8500_DMA,
		.flags = IORESOURCE_IRQ,
	}
};

/* Default configuration for physcial memcpy */
struct stedma40_chan_cfg dma40_memcpy_conf_phy = {
	.mode = STEDMA40_MODE_PHYSICAL,
	.dir = STEDMA40_MEM_TO_MEM,

	.src_info.data_width = STEDMA40_BYTE_WIDTH,
	.src_info.psize = STEDMA40_PSIZE_PHY_1,
	.src_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,

	.dst_info.data_width = STEDMA40_BYTE_WIDTH,
	.dst_info.psize = STEDMA40_PSIZE_PHY_1,
	.dst_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,
};
/* Default configuration for logical memcpy */
struct stedma40_chan_cfg dma40_memcpy_conf_log = {
	.dir = STEDMA40_MEM_TO_MEM,

	.src_info.data_width = STEDMA40_BYTE_WIDTH,
	.src_info.psize = STEDMA40_PSIZE_LOG_1,
	.src_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,

	.dst_info.data_width = STEDMA40_BYTE_WIDTH,
	.dst_info.psize = STEDMA40_PSIZE_LOG_1,
	.dst_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,
};

/*
 * Mapping between destination event lines and physical device address.
 * The event line is tied to a device and therefore the address is constant.
 * When the address comes from a primecell it will be configured in runtime
 * and we set the address to -1 as a placeholder.
 */
static const dma_addr_t dma40_tx_map[DB8500_DMA_NR_DEV] = {
	/* MUSB - these will be runtime-reconfigured */
	[DB8500_DMA_DEV39_USB_OTG_OEP_8] = -1,
	[DB8500_DMA_DEV16_USB_OTG_OEP_7_15] = -1,
	[DB8500_DMA_DEV17_USB_OTG_OEP_6_14] = -1,
	[DB8500_DMA_DEV18_USB_OTG_OEP_5_13] = -1,
	[DB8500_DMA_DEV19_USB_OTG_OEP_4_12] = -1,
	[DB8500_DMA_DEV36_USB_OTG_OEP_3_11] = -1,
	[DB8500_DMA_DEV37_USB_OTG_OEP_2_10] = -1,
	[DB8500_DMA_DEV38_USB_OTG_OEP_1_9] = -1,
	/* PrimeCells - run-time configured */
	[DB8500_DMA_DEV0_SPI0_TX] = -1,
	[DB8500_DMA_DEV1_SD_MMC0_TX] = -1,
	[DB8500_DMA_DEV2_SD_MMC1_TX] = -1,
	[DB8500_DMA_DEV3_SD_MMC2_TX] = -1,
	[DB8500_DMA_DEV4_I2C1_TX] = -1,
	[DB8500_DMA_DEV5_I2C3_TX] = -1,
	[DB8500_DMA_DEV6_I2C2_TX] = -1,
	[DB8500_DMA_DEV7_I2C4_TX] = -1,
	[DB8500_DMA_DEV8_SSP0_TX] = U8500_SSP0_BASE + SSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV9_SSP1_TX] = -1,
	[DB8500_DMA_DEV11_UART2_TX] = -1,
	[DB8500_DMA_DEV12_UART1_TX] = -1,
	[DB8500_DMA_DEV13_UART0_TX] = -1,
	[DB8500_DMA_DEV14_MSP2_TX] = U8500_MSP2_BASE + MSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV15_I2C0_TX] = -1,
	[DB8500_DMA_DEV20_SLIM0_CH0_TX_HSI_TX_CH0]
		= U8500_HSIT_BASE + 0x0 + STE_HSI_TX_BUFFERX,
	[DB8500_DMA_DEV21_SLIM0_CH1_TX_HSI_TX_CH1]
		= U8500_HSIT_BASE + 0x4 + STE_HSI_TX_BUFFERX,
	[DB8500_DMA_DEV22_SLIM0_CH2_TX_HSI_TX_CH2]
		= U8500_HSIT_BASE + 0x8 + STE_HSI_TX_BUFFERX,
	[DB8500_DMA_DEV23_SLIM0_CH3_TX_HSI_TX_CH3]
		= U8500_HSIT_BASE + 0xC + STE_HSI_TX_BUFFERX,
	[DB8500_DMA_DEV24_DST_SXA0_RX_TX] = -1,
	[DB8500_DMA_DEV25_DST_SXA1_RX_TX] = -1,
	[DB8500_DMA_DEV26_DST_SXA2_RX_TX] = -1,
	[DB8500_DMA_DEV27_DST_SXA3_RX_TX] = -1,
	[DB8500_DMA_DEV28_SD_MM2_TX] = -1,
	[DB8500_DMA_DEV29_SD_MM0_TX] = -1,
	[DB8500_DMA_DEV30_MSP1_TX]
		= U8500_MSP1_BASE + MSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV31_MSP0_TX_SLIM0_CH0_TX]
		= U8500_MSP0_BASE + MSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV32_SD_MM1_TX] = -1,
	[DB8500_DMA_DEV33_SPI2_TX] = -1,
	[DB8500_DMA_DEV34_I2C3_TX2] = -1,
	[DB8500_DMA_DEV35_SPI1_TX] = -1,
	[DB8500_DMA_DEV40_SPI3_TX] = -1,
	[DB8500_DMA_DEV41_SD_MM3_TX] = -1,
	[DB8500_DMA_DEV42_SD_MM4_TX] = -1,
	[DB8500_DMA_DEV43_SD_MM5_TX] = -1,
	[DB8500_DMA_DEV44_DST_SXA4_RX_TX] = -1,
	[DB8500_DMA_DEV45_DST_SXA5_RX_TX] = -1,
	[DB8500_DMA_DEV46_SLIM0_CH8_TX_DST_SXA6_RX_TX] = -1,
	[DB8500_DMA_DEV47_SLIM0_CH9_TX_DST_SXA7_RX_TX] = -1,
	[DB8500_DMA_DEV48_CAC1_TX] = U8500_CRYP1_BASE + CRYP1_TX_REG_OFFSET,
	[DB8500_DMA_DEV49_CAC1_TX_HAC1_TX] = -1,
	[DB8500_DMA_DEV50_HAC1_TX] = -1,
	[DB8500_DMA_MEMCPY_TX_0] = -1,
	[DB8500_DMA_DEV52_SLIM1_CH4_TX_HSI_TX_CH4] = -1,
	[DB8500_DMA_DEV53_SLIM1_CH5_TX_HSI_TX_CH5] = -1,
	[DB8500_DMA_DEV54_SLIM1_CH6_TX_HSI_TX_CH6] = -1,
	[DB8500_DMA_DEV55_SLIM1_CH7_TX_HSI_TX_CH7] = -1,
	[DB8500_DMA_MEMCPY_TX_1] = -1,
	[DB8500_DMA_MEMCPY_TX_2] = -1,
	[DB8500_DMA_MEMCPY_TX_3] = -1,
	[DB8500_DMA_MEMCPY_TX_4] = -1,
	[DB8500_DMA_MEMCPY_TX_5] = -1,
	[DB8500_DMA_DEV61_CAC0_TX] = -1,
	[DB8500_DMA_DEV62_CAC0_TX_HAC0_TX] = -1,
	[DB8500_DMA_DEV63_HAC0_TX] = -1,
};

/* Mapping between source event lines and physical device address */
static const dma_addr_t dma40_rx_map[DB8500_DMA_NR_DEV] = {
	/* MUSB - these will be runtime-reconfigured */
	[DB8500_DMA_DEV39_USB_OTG_IEP_8] = -1,
	[DB8500_DMA_DEV16_USB_OTG_IEP_7_15] = -1,
	[DB8500_DMA_DEV17_USB_OTG_IEP_6_14] = -1,
	[DB8500_DMA_DEV18_USB_OTG_IEP_5_13] = -1,
	[DB8500_DMA_DEV19_USB_OTG_IEP_4_12] = -1,
	[DB8500_DMA_DEV36_USB_OTG_IEP_3_11] = -1,
	[DB8500_DMA_DEV37_USB_OTG_IEP_2_10] = -1,
	[DB8500_DMA_DEV38_USB_OTG_IEP_1_9] = -1,
	/* PrimeCells */
	[DB8500_DMA_DEV0_SPI0_RX] = -1,
	[DB8500_DMA_DEV1_SD_MMC0_RX] = -1,
	[DB8500_DMA_DEV2_SD_MMC1_RX] = -1,
	[DB8500_DMA_DEV3_SD_MMC2_RX] = -1,
	[DB8500_DMA_DEV4_I2C1_RX] = -1,
	[DB8500_DMA_DEV5_I2C3_RX] = -1,
	[DB8500_DMA_DEV6_I2C2_RX] = -1,
	[DB8500_DMA_DEV7_I2C4_RX] = -1,
	[DB8500_DMA_DEV8_SSP0_RX] = -1,
	[DB8500_DMA_DEV9_SSP1_RX] = -1,
	[DB8500_DMA_DEV11_UART2_RX] = -1,
	[DB8500_DMA_DEV12_UART1_RX] = -1,
	[DB8500_DMA_DEV13_UART0_RX] = -1,
	[DB8500_DMA_DEV14_MSP2_RX] = U8500_MSP2_BASE + MSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV15_I2C0_RX] = -1,
	[DB8500_DMA_DEV20_SLIM0_CH0_RX_HSI_RX_CH0]
		= U8500_HSIR_BASE + 0x0 + STE_HSI_RX_BUFFERX,
	[DB8500_DMA_DEV21_SLIM0_CH1_RX_HSI_RX_CH1]
		= U8500_HSIR_BASE + 0x4 + STE_HSI_RX_BUFFERX,
	[DB8500_DMA_DEV22_SLIM0_CH2_RX_HSI_RX_CH2]
		= U8500_HSIR_BASE + 0x8 + STE_HSI_RX_BUFFERX,
	[DB8500_DMA_DEV23_SLIM0_CH3_RX_HSI_RX_CH3]
		= U8500_HSIR_BASE + 0xC + STE_HSI_RX_BUFFERX,
	[DB8500_DMA_DEV24_SRC_SXA0_RX_TX] = -1,
	[DB8500_DMA_DEV25_SRC_SXA1_RX_TX] = -1,
	[DB8500_DMA_DEV26_SRC_SXA2_RX_TX] = -1,
	[DB8500_DMA_DEV27_SRC_SXA3_RX_TX] = -1,
	[DB8500_DMA_DEV28_SD_MM2_RX] = -1,
	[DB8500_DMA_DEV29_SD_MM0_RX] = -1,
	[DB8500_DMA_DEV30_MSP3_RX]
		= U8500_MSP3_BASE + MSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV31_MSP0_RX_SLIM0_CH0_RX]
		= U8500_MSP0_BASE + MSP_TX_RX_REG_OFFSET,
	[DB8500_DMA_DEV32_SD_MM1_RX] = -1,
	[DB8500_DMA_DEV33_SPI2_RX] = -1,
	[DB8500_DMA_DEV34_I2C3_RX2] = -1,
	[DB8500_DMA_DEV35_SPI1_RX] = -1,
	[DB8500_DMA_DEV40_SPI3_RX] = -1,
	[DB8500_DMA_DEV41_SD_MM3_RX] = -1,
	[DB8500_DMA_DEV42_SD_MM4_RX] = -1,
	[DB8500_DMA_DEV43_SD_MM5_RX] = -1,
	[DB8500_DMA_DEV44_SRC_SXA4_RX_TX] = -1,
	[DB8500_DMA_DEV45_SRC_SXA5_RX_TX] = -1,
	[DB8500_DMA_DEV46_SLIM0_CH8_RX_SRC_SXA6_RX_TX] = -1,
	[DB8500_DMA_DEV47_SLIM0_CH9_RX_SRC_SXA7_RX_TX] = -1,
	[DB8500_DMA_DEV48_CAC1_RX] = U8500_CRYP1_BASE + CRYP1_RX_REG_OFFSET,
	/* 49, 50 and 51 are not used */
	[DB8500_DMA_DEV52_SLIM0_CH4_RX_HSI_RX_CH4] = -1,
	[DB8500_DMA_DEV53_SLIM0_CH5_RX_HSI_RX_CH5] = -1,
	[DB8500_DMA_DEV54_SLIM0_CH6_RX_HSI_RX_CH6] = -1,
	[DB8500_DMA_DEV55_SLIM0_CH7_RX_HSI_RX_CH7] = -1,
	/* 56, 57, 58, 59 and 60 are not used */
	[DB8500_DMA_DEV61_CAC0_RX] = -1,
	/* 62 and 63 are not used */
};

/* Reserved event lines for memcpy only */
static int dma40_memcpy_event[] = {
	DB8500_DMA_MEMCPY_TX_0,
	DB8500_DMA_MEMCPY_TX_1,
	DB8500_DMA_MEMCPY_TX_2,
	DB8500_DMA_MEMCPY_TX_3,
	DB8500_DMA_MEMCPY_TX_4,
	DB8500_DMA_MEMCPY_TX_5,
};

static struct stedma40_platform_data dma40_plat_data = {
	.dev_len = DB8500_DMA_NR_DEV,
	.dev_rx = dma40_rx_map,
	.dev_tx = dma40_tx_map,
	.memcpy = dma40_memcpy_event,
	.memcpy_len = ARRAY_SIZE(dma40_memcpy_event),
	.memcpy_conf_phy = &dma40_memcpy_conf_phy,
	.memcpy_conf_log = &dma40_memcpy_conf_log,
	.disabled_channels = {-1},
};

struct platform_device u8500_dma40_device = {
	.dev = {
		.platform_data = &dma40_plat_data,
	},
	.name = "dma40",
	.id = 0,
	.num_resources = ARRAY_SIZE(dma40_resources),
	.resource = dma40_resources
};

static struct resource u8500_shrm_resources[] = {
	[0] = {
		.start = U8500_SHRM_GOP_INTERRUPT_BASE,
		.end = U8500_SHRM_GOP_INTERRUPT_BASE + ((4*4)-1),
		.name = "shrm_gop_register_base",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CA_WAKE_REQ_V1,
		.end = IRQ_CA_WAKE_REQ_V1,
		.name = "ca_irq_wake_req",
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_AC_READ_NOTIFICATION_0_V1,
		.end = IRQ_AC_READ_NOTIFICATION_0_V1,
		.name = "ac_read_notification_0_irq",
		.flags = IORESOURCE_IRQ,
	},
	[3] = {
		.start = IRQ_AC_READ_NOTIFICATION_1_V1,
		.end = IRQ_AC_READ_NOTIFICATION_1_V1,
		.name = "ac_read_notification_1_irq",
		.flags = IORESOURCE_IRQ,
	},
	[4] = {
		.start = IRQ_CA_MSG_PEND_NOTIFICATION_0_V1,
		.end = IRQ_CA_MSG_PEND_NOTIFICATION_0_V1,
		.name = "ca_msg_pending_notification_0_irq",
		.flags = IORESOURCE_IRQ,
	},
	[5] = {
		.start = IRQ_CA_MSG_PEND_NOTIFICATION_1_V1,
		.end = IRQ_CA_MSG_PEND_NOTIFICATION_1_V1,
		.name = "ca_msg_pending_notification_1_irq",
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device u8500_shrm_device = {
	.name = "u8500_shrm",
	.id = 0,
	.dev = {
		.init_name = "shrm_bus",
		.coherent_dma_mask = ~0,
	},

	.num_resources = ARRAY_SIZE(u8500_shrm_resources),
	.resource = u8500_shrm_resources
};

static struct resource mcde_resources[] = {
	[0] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_MCDE_BASE,
		.end   = U8500_MCDE_BASE + U8500_MCDE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK1_BASE,
		.end   = U8500_DSI_LINK1_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK2_BASE,
		.end   = U8500_DSI_LINK2_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK3_BASE,
		.end   = U8500_DSI_LINK3_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[4] = {
		.name  = MCDE_IRQ,
		.start = IRQ_DB8500_DISP,
		.end   = IRQ_DB8500_DISP,
		.flags = IORESOURCE_IRQ,
	},
};

static int mcde_platform_enable_dsipll(void)
{
	return prcmu_enable_dsipll();
}

static int mcde_platform_disable_dsipll(void)
{
	return prcmu_disable_dsipll();
}

static int mcde_platform_set_display_clocks(void)
{
	return prcmu_set_display_clocks();
}

static struct mcde_platform_data mcde_pdata = {
	.num_dsilinks = 3,
	/*
	 * [0] = 3: 24 bits DPI: connect LSB Ch B to D[0:7]
	 * [3] = 4: 24 bits DPI: connect MID Ch B to D[24:31]
	 * [4] = 5: 24 bits DPI: connect MSB Ch B to D[32:39]
	 *
	 * [1] = 3: TV out     : connect LSB Ch B to D[8:15]
	 */
#define DONT_CARE 0
	.outmux = { 3, 3, DONT_CARE, 4, 5 },
#undef DONT_CARE
	.syncmux = 0x00,  /* DPI channel A and B on output pins A and B resp */
	.num_channels = 4,
	.num_overlays = 6,
	.regulator_vana_id = "v-ana",
	.regulator_mcde_epod_id = "vsupply",
	.regulator_esram_epod_id = "v-esram34",
	.clock_dsi_id = "hdmi",
	.clock_dsi_lp_id = "tv",
	.clock_dpi_id = "lcd",
	.clock_mcde_id = "mcde",
	.platform_set_clocks = mcde_platform_set_display_clocks,
	.platform_enable_dsipll = mcde_platform_enable_dsipll,
	.platform_disable_dsipll = mcde_platform_disable_dsipll,
};

struct platform_device u8500_mcde_device = {
	.name = "mcde",
	.id = -1,
	.dev = {
		.platform_data = &mcde_pdata,
	},
	.num_resources = ARRAY_SIZE(mcde_resources),
	.resource = mcde_resources,
};

static struct resource b2r2_resources[] = {
	[0] = {
		.start	= U8500_B2R2_BASE,
		.end	= U8500_B2R2_BASE + ((4*1024)-1),
		.name	= "b2r2_base",
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name  = "B2R2_IRQ",
		.start = IRQ_DB8500_B2R2,
		.end   = IRQ_DB8500_B2R2,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device u8500_b2r2_device = {
	.name	= "b2r2",
	.id	= 0,
	.dev	= {
		.init_name = "b2r2_bus",
		.coherent_dma_mask = ~0,
	},
	.num_resources	= ARRAY_SIZE(b2r2_resources),
	.resource	= b2r2_resources,
};

/*
 * WATCHDOG
 */

static struct resource ux500_wdt_resources[] = {
	[0] = {
		.start  = U8500_TWD_BASE,
		.end    = U8500_TWD_BASE+0x37,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_LOCALWDOG,
		.end  = IRQ_LOCALWDOG,
		.flags  = IORESOURCE_IRQ,
	}
};

struct platform_device ux500_wdt_device = {
	.name           = "mpcore_wdt",
	.id             = -1,
	.resource       = ux500_wdt_resources,
	.num_resources  = ARRAY_SIZE(ux500_wdt_resources),
};

/*
 * Thermal Sensor
 */

static struct resource u8500_thsens_resources[] = {
	{
		.name = "IRQ_HOTMON_LOW",
		.start  = IRQ_PRCMU_HOTMON_LOW,
		.end    = IRQ_PRCMU_HOTMON_LOW,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name = "IRQ_HOTMON_HIGH",
		.start  = IRQ_PRCMU_HOTMON_HIGH,
		.end    = IRQ_PRCMU_HOTMON_HIGH,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device u8500_thsens_device = {
	.name           = "db8500_temp",
	.resource       = u8500_thsens_resources,
	.num_resources  = ARRAY_SIZE(u8500_thsens_resources),
};

struct resource keypad_resources[] = {
	[0] = {
		.start = U8500_SKE_BASE,
		.end = U8500_SKE_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_DB8500_KB,
		.end = IRQ_DB8500_KB,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device u8500_ske_keypad_device = {
	.name = "nmk-ske-keypad",
	.id = -1,
	.num_resources = ARRAY_SIZE(keypad_resources),
	.resource = keypad_resources,
};

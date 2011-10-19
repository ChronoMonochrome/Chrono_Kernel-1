/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/mfd/abx500/ab5500.h>
#include <linux/amba/pl022.h>
#include <linux/i2s/i2s.h>

#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/pincfg.h>
#include <plat/i2c.h>
#include <plat/gpio-nomadik.h>

#include <mach/hardware.h>
#include <mach/ste-dma40-db5500.h>
#include <mach/msp.h>
#include <mach/devices.h>
#include <mach/setup.h>

#include "pins-db5500.h"
#include "devices-db5500.h"
#include <linux/led-lm3530.h>

/*
 * GPIO
 */

static pin_cfg_t u5500_pins[] = {
	/* I2C */
	GPIO218_I2C2_SCL        | PIN_INPUT_PULLUP,
	GPIO219_I2C2_SDA        | PIN_INPUT_PULLUP,

	/* DISPLAY_ENABLE */
	GPIO226_GPIO        | PIN_OUTPUT_LOW,

	/* Backlight Enbale */
	GPIO224_GPIO        | PIN_OUTPUT_HIGH,
};

/*
 * Touchscreen
 */
static struct synaptics_rmi4_platform_data rmi4_i2c_platformdata = {
	.irq_number	= NOMADIK_GPIO_TO_IRQ(179),
	.irq_type	= (IRQF_TRIGGER_FALLING | IRQF_SHARED),
#if CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE == 270
	.x_flip		= true,
	.y_flip		= false,
#else
	.x_flip		= false,
	.y_flip		= true,
#endif
	.regulator_en	= false,
};

/*
 * I2C
 */

#define U5500_I2C_CONTROLLER(id, _slsu, _tft, _rft, clk, _sm) \
static struct nmk_i2c_controller u5500_i2c##id##_data = { \
	/*				\
	 * slave data setup time, which is	\
	 * 250 ns,100ns,10ns which is 14,6,2	\
	 * respectively for a 48 Mhz	\
	 * i2c clock			\
	 */				\
	.slsu		= _slsu,	\
	/* Tx FIFO threshold */		\
	.tft		= _tft,		\
	/* Rx FIFO threshold */		\
	.rft		= _rft,		\
	/* std. mode operation */	\
	.clk_freq	= clk,		\
	.sm		= _sm,		\
}

/*
 * The board uses 3 i2c controllers, initialize all of
 * them with slave data setup time of 250 ns,
 * Tx & Rx FIFO threshold values as 1 and standard
 * mode of operation
 */

U5500_I2C_CONTROLLER(1,	0xe, 1, 1, 400000, I2C_FREQ_MODE_FAST);
U5500_I2C_CONTROLLER(2,	0xe, 1, 1, 400000, I2C_FREQ_MODE_FAST);
U5500_I2C_CONTROLLER(3,	0xe, 1, 1, 400000, I2C_FREQ_MODE_FAST);

static struct lm3530_platform_data u5500_als_platform_data = {
	.mode = LM3530_BL_MODE_MANUAL,
	.als_input_mode = LM3530_INPUT_ALS1,
	.max_current = LM3530_FS_CURR_26mA,
	.pwm_pol_hi = true,
	.als_avrg_time = LM3530_ALS_AVRG_TIME_512ms,
	.brt_ramp_law = 1,      /* Linear */
	.brt_ramp_fall = LM3530_RAMP_TIME_8s,
	.brt_ramp_rise = LM3530_RAMP_TIME_8s,
	.als1_resistor_sel = LM3530_ALS_IMPD_13_53kOhm,
	.als2_resistor_sel = LM3530_ALS_IMPD_Z,
	.als_vmin = 730,	/* mV */
	.als_vmax = 1020,	/* mV */
	.brt_val = 0x7F,	/* Max brightness */
};

static struct i2c_board_info __initdata u5500_i2c1_devices[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x4B),
		.platform_data = &rmi4_i2c_platformdata,
	},
};

static struct i2c_board_info __initdata u5500_i2c2_devices[] = {
	{
		/* Backlight */
		I2C_BOARD_INFO("lm3530-led", 0x36),
		.platform_data = &u5500_als_platform_data,
	},
};

static struct resource ab5500_resources[] = {
	[0] = {
		/*TODO Change this when prcmu driver arrives */
		.start = IRQ_DB5500_AB5500,
		.end = IRQ_DB5500_AB5500,
		.flags = IORESOURCE_IRQ
	}
};

static struct ab5500_platform_data ab5500_plf_data = {
	.irq = {
		.base = IRQ_AB5500_BASE,
		.count = AB5500_NR_IRQS,
	},
	.dev_data = {
	},
	.dev_data_sz = {
	},
	.init_settings = NULL,
	.init_settings_sz = 0,
};

static struct platform_device u5500_ab5500_device = {
	.name = "ab5500-core",
	.id = 0,
	.dev = {
		.platform_data = &ab5500_plf_data,
	},
	.num_resources = 1,
	.resource = ab5500_resources,
};

static struct platform_device *u5500_platform_devices[] __initdata = {
	&u5500_ab5500_device,
	&u5500_mcde_device,
	&ux500_hwmem_device,
	&u5500_b2r2_device,
};

static void __init u5500_i2c_init(struct device *parent)
{
	db5500_add_i2c1(parent, &u5500_i2c1_data);
	db5500_add_i2c2(parent, &u5500_i2c2_data);
	db5500_add_i2c3(parent, &u5500_i2c3_data);

	i2c_register_board_info(1, ARRAY_AND_SIZE(u5500_i2c1_devices));
	i2c_register_board_info(2, ARRAY_AND_SIZE(u5500_i2c2_devices));
}

/*
 * MSP
 */

#define MSP_DMA(num, eventline)					\
static struct stedma40_chan_cfg msp##num##_dma_rx = {		\
	.high_priority = true,					\
	.dir = STEDMA40_PERIPH_TO_MEM,				\
	.src_dev_type = eventline##_RX,				\
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,		\
	.src_info.psize = STEDMA40_PSIZE_LOG_4,			\
	.dst_info.psize = STEDMA40_PSIZE_LOG_4,			\
};								\
								\
static struct stedma40_chan_cfg msp##num##_dma_tx = {		\
	.high_priority = true,					\
	.dir = STEDMA40_MEM_TO_PERIPH,				\
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,		\
	.dst_dev_type = eventline##_TX,				\
	.src_info.psize = STEDMA40_PSIZE_LOG_4,			\
	.dst_info.psize = STEDMA40_PSIZE_LOG_4,			\
}

MSP_DMA(0, DB5500_DMA_DEV9_MSP0);
MSP_DMA(1, DB5500_DMA_DEV10_MSP1);
MSP_DMA(2, DB5500_DMA_DEV11_MSP2);

static struct msp_i2s_platform_data u5500_msp0_data = {
	.id		= MSP_0_I2S_CONTROLLER,
	.msp_i2s_dma_rx	= &msp0_dma_rx,
	.msp_i2s_dma_tx	= &msp0_dma_tx,
};

static struct msp_i2s_platform_data u5500_msp1_data = {
	.id		= MSP_1_I2S_CONTROLLER,
	.msp_i2s_dma_rx	= &msp1_dma_rx,
	.msp_i2s_dma_tx	= &msp1_dma_tx,
};

static struct msp_i2s_platform_data u5500_msp2_data = {
	.id		= MSP_2_I2S_CONTROLLER,
	.msp_i2s_dma_rx	= &msp2_dma_rx,
	.msp_i2s_dma_tx	= &msp2_dma_tx,
};

static struct i2s_board_info stm_i2s_board_info[] __initdata = {
	{
		.modalias	= "i2s_device.0",
		.id		= 0,
		.chip_select	= 0,
	},
	{
		.modalias	= "i2s_device.1",
		.id		= 1,
		.chip_select	= 1,
	},
	{
		.modalias	= "i2s_device.2",
		.id		= 2,
		.chip_select	= 2,
	},
};

/*
 * SPI
 */

static struct pl022_ssp_controller u5500_spi1_data = {
	.bus_id		= 1,
	.num_chipselect	= 4,	/* 3 possible CS lines + 1 for tests */
};

static void __init u5500_spi_init(void)
{
	db5500_add_spi1(&u5500_spi1_data);
}

static void __init u5500_msp_init(void)
{
	db5500_add_msp0_i2s(&u5500_msp0_data);
	db5500_add_msp1_i2s(&u5500_msp1_data);
	db5500_add_msp2_i2s(&u5500_msp2_data);

	i2s_register_board_info(ARRAY_AND_SIZE(stm_i2s_board_info));
}

static void __init u5500_uart_init(struct device *parent)
{
	db5500_add_uart0(parent, NULL);
	db5500_add_uart1(parent, NULL);
	db5500_add_uart2(parent, NULL);
}

static void __init u5500_init_machine(void)
{
	struct device *parent = NULL;
	int i;

	parent = u5500_init_devices();
	nmk_config_pins(u5500_pins, ARRAY_SIZE(u5500_pins));

	u5500_i2c_init(parent);
	u5500_msp_init();
	u5500_spi_init();

	u5500_sdi_init(parent);
	u5500_uart_init(parent);

	for (i = 0; i < ARRAY_SIZE(u5500_platform_devices); i++)
		u5500_platform_devices[i]->dev.parent = parent;

	platform_add_devices(u5500_platform_devices,
		ARRAY_SIZE(u5500_platform_devices));
}

MACHINE_START(U5500, "ST-Ericsson U5500 Platform")
	.atag_offset	= 0x100,
	.map_io		= u5500_map_io,
	.init_irq	= ux500_init_irq,
	.timer		= &ux500_timer,
	.handle_irq	= gic_handle_irq,
	.init_machine	= u5500_init_machine,
MACHINE_END

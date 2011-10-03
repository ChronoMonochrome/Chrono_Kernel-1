/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pfn.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/dbx500-prcmu.h>

#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <linux/dmaengine.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <linux/i2s/i2s.h>
#include <mach/msp.h>
#include <linux/dma-mapping.h>

struct regulator *msp_vape_supply;

#define STM_MSP_NAME "STM_MSP"
#define MSP_NAME "msp"
#define DRIVER_DEBUG_PFX "MSP"
#define DRIVER_DEBUG CONFIG_STM_MSP_DEBUG
#define DRIVER_DBG "MSP"
#define NMDK_DBG /* message level */

extern struct driver_debug_st DBG_ST;
 /* Protocol desciptors */
static const struct msp_protocol_desc protocol_desc_tab[] = {
	I2S_PROTOCOL_DESC,
	PCM_PROTOCOL_DESC,
	PCM_COMPAND_PROTOCOL_DESC,
	AC97_PROTOCOL_DESC,
	SPI_MASTER_PROTOCOL_DESC,
	SPI_SLAVE_PROTOCOL_DESC,
};

/* Local static functions */
static int msp_dma_xfer(struct msp *msp, struct i2s_message *msg);
static int msp_polling_xfer(struct msp *msp, struct i2s_message *msg);
static int msp_interrupt_xfer(struct msp *msp, struct i2s_message *msg);
static int msp_start_dma(struct msp *msp, int transmit, dma_addr_t data,
			 size_t bytes);
static int configure_protocol(struct msp *msp,
			      struct msp_config *config);
static int configure_clock(struct msp *msp,
			   struct msp_config *config);
static int configure_multichannel(struct msp *msp,
				  struct msp_config *config);
static int stm_msp_configure_enable(struct i2s_controller *i2s_cont,
				    void *configuration);
static int stm_msp_transceive_data(struct i2s_controller *i2s_cont,
				   struct i2s_message *message);

static int stm_msp_disable(struct msp *msp, int direction,
			   i2s_flag flag);
static int stm_msp_close(struct i2s_controller *i2s_cont, i2s_flag flag);
static int stm_msp_hw_status(struct i2s_controller *i2s_cont);

#define I2S_DEVICE "i2s_device"
static struct i2s_algorithm i2s_algo = {
	.cont_setup		= stm_msp_configure_enable,
	.cont_transfer		= stm_msp_transceive_data,
	.cont_cleanup		= stm_msp_close,
	.cont_hw_status		= stm_msp_hw_status,
};

/**
 * stm_msp_write - writel a value to specified register
 * @value: value
 * @reg: pointer to register' address
 * Context: atomic(can be both process and interrupt)
 * Returns void.
 */
static inline void stm_msp_write(u32 value, void __iomem *reg)
{
	writel(value, reg);
}

/**
 * stm_msp_read - readl a value to specified register
 * @reg: pointer to register' address
 * Context: atomic(can be both process and interrupt)
 * Returns u32 register's value.
 */
static inline u32 stm_msp_read(void __iomem *reg)
{
	return readl(reg);
}

static void u8_msp_read(struct trans_data *xfer_data)
{
	struct i2s_message *message = &xfer_data->message;
	while ((message->rx_offset < message->rxbytes) &&
	       !((stm_msp_read(xfer_data->msp->registers + MSP_FLR)) &
		 RX_FIFO_EMPTY)) {
		message->rx_offset += 1;
		*(u8 *) message->rxdata =
		    (u8) stm_msp_read(xfer_data->msp->registers + MSP_DR);
		message->rxdata += 1;
	}
}

static void u16_msp_read(struct trans_data *xfer_data)
{
	struct i2s_message *message = &xfer_data->message;
	while ((message->rx_offset < message->rxbytes) &&
	       !((stm_msp_read(xfer_data->msp->registers + MSP_FLR)) &
		 RX_FIFO_EMPTY)) {
		message->rx_offset += 2;
		*(u16 *) message->rxdata =
		    (u16) stm_msp_read(xfer_data->msp->registers + MSP_DR);
		message->rxdata += 2;
	}
}

/**
 * u32_msp_read - Msp 32bit read function.
 * @xfer_data: transfer data structure.
 *
 * It reads 32bit data from msp receive fifo until it gets empty.
 *
 * Returns void.
 */
static void u32_msp_read(struct trans_data *xfer_data)
{
	struct i2s_message *message = &xfer_data->message;
	while ((message->rx_offset < message->rxbytes) &&
	       !((stm_msp_read(xfer_data->msp->registers + MSP_FLR)) &
		 RX_FIFO_EMPTY)) {
		*(u32 *) message->rxdata =
		    (u32) stm_msp_read(xfer_data->msp->registers + MSP_DR);
		message->rx_offset += 4;
		message->rxdata += 4;
	}
}
static void u8_msp_write(struct trans_data *xfer_data)
{
	struct i2s_message *message = &xfer_data->message;
	while ((message->tx_offset < message->txbytes) &&
	       !((stm_msp_read(xfer_data->msp->registers + MSP_FLR)) &
		 TX_FIFO_FULL)) {
		message->tx_offset += 1;
		stm_msp_write(*(u8 *) message->txdata,
			      xfer_data->msp->registers + MSP_DR);
		message->txdata += 1;
	}
}

static void u16_msp_write(struct trans_data *xfer_data)
{
	struct i2s_message *message = &xfer_data->message;
	while ((message->tx_offset < message->txbytes) &&
	       !((stm_msp_read(xfer_data->msp->registers + MSP_FLR)) &
		 TX_FIFO_FULL)) {
		message->tx_offset += 2;
		stm_msp_write(*(u16 *) message->txdata,
			      xfer_data->msp->registers + MSP_DR);
		message->txdata += 2;
	}
}

/**
 * u32_msp_write - Msp 32bit write function.
 * @xfer_data: transfer data structure.
 *
 * It writes 32bit data to msp transmit fifo until it gets full.
 *
 * Returns void.
 */
static void u32_msp_write(struct trans_data *xfer_data)
{
	struct i2s_message *message = &xfer_data->message;
	while ((message->tx_offset < message->txbytes) &&
	       !((stm_msp_read(xfer_data->msp->registers + MSP_FLR)) &
		 TX_FIFO_FULL)) {
		message->tx_offset += 4;
		stm_msp_write(*(u32 *) message->txdata,
			      xfer_data->msp->registers + MSP_DR);
		message->txdata += 4;
	}
}

/**
 * set_transmit_protocol_descriptor - Set the Transmit Configuration register.
 * @msp: main msp controller structure.
 * @protocol_desc: pointer to protocol descriptor structure.
 * @data_size: Run time configurable element length.
 *
 * It will setup transmit configuration register of msp.
 * Various values related to a particular protocol can be set like, elemnet
 * length, frame length, endianess etc.
 *
 * Returns void.
 */
static void set_transmit_protocol_descriptor(struct msp *msp,
						struct msp_protocol_desc
							*protocol_desc,
						enum msp_data_size data_size)
{
	u32 temp_reg = 0;

	temp_reg |= MSP_P2_ENABLE_BIT(protocol_desc->tx_phase_mode);
	temp_reg |= MSP_P2_START_MODE_BIT(protocol_desc->tx_phase2_start_mode);
	temp_reg |= MSP_P1_FRAME_LEN_BITS(protocol_desc->tx_frame_length_1);
	temp_reg |= MSP_P2_FRAME_LEN_BITS(protocol_desc->tx_frame_length_2);
	if (msp->def_elem_len) {
		temp_reg |=
		    MSP_P1_ELEM_LEN_BITS(protocol_desc->tx_element_length_1);
		temp_reg |=
		    MSP_P2_ELEM_LEN_BITS(protocol_desc->tx_element_length_2);
		if (protocol_desc->tx_element_length_1 ==
		    protocol_desc->tx_element_length_2) {
			msp->actual_data_size =
			    protocol_desc->tx_element_length_1;
		} else {
			msp->actual_data_size = data_size;
		}
	} else {
		temp_reg |= MSP_P1_ELEM_LEN_BITS(data_size);
		temp_reg |= MSP_P2_ELEM_LEN_BITS(data_size);
		msp->actual_data_size = data_size;
	}
	temp_reg |= MSP_DATA_DELAY_BITS(protocol_desc->tx_data_delay);
	temp_reg |=
	    MSP_SET_ENDIANNES_BIT(protocol_desc->tx_bit_transfer_format);
	temp_reg |= MSP_FRAME_SYNC_POL(protocol_desc->tx_frame_sync_pol);
	temp_reg |= MSP_DATA_WORD_SWAP(protocol_desc->tx_half_word_swap);
	temp_reg |= MSP_SET_COMPANDING_MODE(protocol_desc->compression_mode);
	temp_reg |= MSP_SET_FRAME_SYNC_IGNORE(protocol_desc->frame_sync_ignore);

	stm_msp_write(temp_reg, msp->registers + MSP_TCF);
}

/**
 * set_receive_protocol_descriptor - Set the Receive Configuration register.
 * @msp: main msp controller structure.
 * @protocol_desc: pointer to protocol descriptor structure.
 * @data_size: Run time configurable element length.
 *
 * It will setup receive configuration register of msp.
 * Various values related to a particular protocol can be set like, elemnet
 * length, frame length, endianess etc.
 *
 * Returns void.
 */
static void set_receive_protocol_descriptor(struct msp *msp,
						struct msp_protocol_desc
							*protocol_desc,
							enum msp_data_size
								data_size)
{
	u32 temp_reg = 0;

	temp_reg |= MSP_P2_ENABLE_BIT(protocol_desc->rx_phase_mode);
	temp_reg |= MSP_P2_START_MODE_BIT(protocol_desc->rx_phase2_start_mode);
	temp_reg |= MSP_P1_FRAME_LEN_BITS(protocol_desc->rx_frame_length_1);
	temp_reg |= MSP_P2_FRAME_LEN_BITS(protocol_desc->rx_frame_length_2);
	if (msp->def_elem_len) {
		temp_reg |=
		    MSP_P1_ELEM_LEN_BITS(protocol_desc->rx_element_length_1);
		temp_reg |=
		    MSP_P2_ELEM_LEN_BITS(protocol_desc->rx_element_length_2);
		if (protocol_desc->rx_element_length_1 ==
		    protocol_desc->rx_element_length_2) {
			msp->actual_data_size =
			    protocol_desc->rx_element_length_1;
		} else {
			msp->actual_data_size = data_size;
		}
	} else {
		temp_reg |= MSP_P1_ELEM_LEN_BITS(data_size);
		temp_reg |= MSP_P2_ELEM_LEN_BITS(data_size);
		msp->actual_data_size = data_size;
	}

	temp_reg |= MSP_DATA_DELAY_BITS(protocol_desc->rx_data_delay);
	temp_reg |=
	    MSP_SET_ENDIANNES_BIT(protocol_desc->rx_bit_transfer_format);
	temp_reg |= MSP_FRAME_SYNC_POL(protocol_desc->rx_frame_sync_pol);
	temp_reg |= MSP_DATA_WORD_SWAP(protocol_desc->rx_half_word_swap);
	temp_reg |= MSP_SET_COMPANDING_MODE(protocol_desc->expansion_mode);
	temp_reg |= MSP_SET_FRAME_SYNC_IGNORE(protocol_desc->frame_sync_ignore);

	stm_msp_write(temp_reg, msp->registers + MSP_RCF);

}

/**
 * configure_protocol - Configures transmit and receive protocol.
 * @msp: main msp controller structure.
 * @config: configuration structure passed by client driver
 *
 * This will configure transmit and receive protocol decriptors.
 *
 * Returns error(-1) on failure else success(0).
 */
static int configure_protocol(struct msp *msp,
			      struct msp_config *config)
{
	int direction;
	struct msp_protocol_desc *protocol_desc;
	enum msp_data_size data_size;
	u32 temp_reg = 0;

	data_size = config->data_size;
	msp->def_elem_len = config->def_elem_len;
	direction = config->direction;
	if (config->default_protocol_desc == 1) {
		if (config->protocol >= MSP_INVALID_PROTOCOL) {
			printk(KERN_ERR
			       "invalid protocol in configure_protocol()\n");
			return -EINVAL;
		}
		protocol_desc =
		    (struct msp_protocol_desc *)&protocol_desc_tab[config->
								   protocol];
	} else {
		protocol_desc =
		    (struct msp_protocol_desc *)&config->protocol_desc;
	}

	if (data_size < MSP_DATA_BITS_DEFAULT
	    || data_size > MSP_DATA_BITS_32) {
		printk(KERN_ERR
		       "invalid data size requested in configure_protocol()\n");
		return -EINVAL;
	}

	switch (direction) {
	case MSP_TRANSMIT_MODE:
		set_transmit_protocol_descriptor(msp, protocol_desc, data_size);
		break;
	case MSP_RECEIVE_MODE:
		set_receive_protocol_descriptor(msp, protocol_desc, data_size);
		break;
	case MSP_BOTH_T_R_MODE:
		set_transmit_protocol_descriptor(msp, protocol_desc, data_size);
		set_receive_protocol_descriptor(msp, protocol_desc, data_size);
		break;
	default:
		printk(KERN_ERR "Invalid direction given\n");
		return -EINVAL;
	}
	/* The below code is needed for both Rx and Tx path can't separate
	 * them.
	 */
	temp_reg = stm_msp_read(msp->registers + MSP_GCR) & ~TX_CLK_POL_RISING;
	temp_reg |= MSP_TX_CLKPOL_BIT(protocol_desc->tx_clock_pol);
	stm_msp_write(temp_reg, msp->registers + MSP_GCR);
	temp_reg = stm_msp_read(msp->registers + MSP_GCR) & ~RX_CLK_POL_RISING;
	temp_reg |= MSP_RX_CLKPOL_BIT(protocol_desc->rx_clock_pol);
	stm_msp_write(temp_reg, msp->registers + MSP_GCR);

	return 0;
}

/**
 * configure_clock - Set clock in sample rate generator.
 * @msp: main msp controller structure.
 * @config: configuration structure passed by client driver
 *
 * This will set the frame width and period. Also enable sample rate generator
 *
 * Returns error(-1) on failure else success(0).
 */
static int configure_clock(struct msp *msp,
			   struct msp_config *config)
{

	u32 dummy;
	u32 frame_per = 0;
	u32 sck_div = 0;
	u32 frame_width = 0;
	u32 temp_reg = 0;
	u32 bit_clock = 0;
	struct msp_protocol_desc *protocol_desc = NULL;
	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) &
		       (~(SRG_ENABLE))), msp->registers + MSP_GCR);

	if (config->default_protocol_desc) {
		protocol_desc =
		    (struct msp_protocol_desc *)&protocol_desc_tab[config->
								   protocol];
	} else {
		protocol_desc =
		    (struct msp_protocol_desc *)&config->protocol_desc;
	}

	switch (config->protocol) {
	case MSP_PCM_PROTOCOL:
	case MSP_PCM_COMPAND_PROTOCOL:
		frame_width = protocol_desc->frame_width;
		sck_div =
		    config->input_clock_freq / (config->frame_freq *
						(protocol_desc->
						 total_clocks_for_one_frame));
		frame_per = protocol_desc->frame_period;
		break;
	case MSP_I2S_PROTOCOL:
		frame_width = protocol_desc->frame_width;
		sck_div =
		    config->input_clock_freq / (config->frame_freq *
						(protocol_desc->
						 total_clocks_for_one_frame));
		frame_per = protocol_desc->frame_period;

		break;
	case MSP_AC97_PROTOCOL:
		/* Not supported */
		printk(KERN_WARNING "AC97 protocol not supported\n");
		return -ENOSYS;
	default:
		printk(KERN_ERR "Invalid mode attempted for setting clocks\n");
		return -EINVAL;
	}

	temp_reg = (sck_div - 1) & SCK_DIV_MASK;
	temp_reg |= FRAME_WIDTH_BITS(frame_width);
	temp_reg |= FRAME_PERIOD_BITS(frame_per);
	stm_msp_write(temp_reg, msp->registers + MSP_SRG);

	bit_clock = (config->input_clock_freq)/(sck_div + 1);
	/* If the bit clock is higher than 19.2MHz, Vape should be run in 100% OPP */
	/* Only consider OPP 100% when bit-clock is used, i.e. MSP master mode */
	if ((bit_clock > 19200000) && ((config->tx_clock_sel != 0) || (config->rx_clock_sel != 0))) {
		prcmu_qos_update_requirement(PRCMU_QOS_APE_OPP, "msp_i2s", 100);
		msp->vape_opp_constraint = 1;
	} else {
		prcmu_qos_update_requirement(PRCMU_QOS_APE_OPP, "msp_i2s", 50);
		msp->vape_opp_constraint = 0;
	}

	/* Wait a bit */
	dummy = ((stm_msp_read(msp->registers + MSP_SRG)) >> FRWID_SHIFT) & 0x0000003F;

	/* Enable clock */
	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) | ((SRG_ENABLE))),
		      msp->registers + MSP_GCR);

	/* Another wait */
	dummy =
	    ((stm_msp_read(msp->registers + MSP_SRG)) >> FRWID_SHIFT) &
	    0x0000003F;
	return 0;
}

/**
 * configure_multichannel - Enable multichannel support for transmit & receive.
 * @msp: main msp controller structure.
 * @config: configuration structure passed by client driver
 *
 * This will enable multichannel support for transmit and receive.
 * It will set Receive comparator also if configured.
 *
 * Returns error(-1) on failure else success(0).
 */
static int configure_multichannel(struct msp *msp,
				  struct msp_config *config)
{
	struct msp_protocol_desc *protocol_desc;
	struct msp_multichannel_config *mult_config;
	if (config->default_protocol_desc == 1) {
		if (config->protocol >= MSP_INVALID_PROTOCOL) {
			printk(KERN_ERR
			       "invalid protocol in configure_protocol()\n");
			return -EINVAL;
		}
		protocol_desc =
		    (struct msp_protocol_desc *)&protocol_desc_tab[config->
								   protocol];
	} else {
		protocol_desc =
		    (struct msp_protocol_desc *)&config->protocol_desc;
	}
	mult_config = &config->multichannel_config;
	if (true == mult_config->tx_multichannel_enable) {
		if (MSP_SINGLE_PHASE == protocol_desc->tx_phase_mode) {
			stm_msp_write((stm_msp_read(msp->registers + MSP_MCR) |
				       ((mult_config->
					 tx_multichannel_enable << TMCEN_BIT) &
					(0x0000020))),
				      msp->registers + MSP_MCR);
			stm_msp_write(mult_config->tx_channel_0_enable,
				      msp->registers + MSP_TCE0);
			stm_msp_write(mult_config->tx_channel_1_enable,
				      msp->registers + MSP_TCE1);
			stm_msp_write(mult_config->tx_channel_2_enable,
				      msp->registers + MSP_TCE2);
			stm_msp_write(mult_config->tx_channel_3_enable,
				      msp->registers + MSP_TCE3);
		} else {
			printk(KERN_ERR "Not in authorised mode\n");
			return -1;
		}
	}
	if (true == mult_config->rx_multichannel_enable) {
		if (MSP_SINGLE_PHASE == protocol_desc->rx_phase_mode) {
			stm_msp_write((stm_msp_read(msp->registers + MSP_MCR) |
				       ((mult_config->
					 rx_multichannel_enable << RMCEN_BIT) &
					(0x0000001))),
				      msp->registers + MSP_MCR);
			stm_msp_write(mult_config->rx_channel_0_enable,
				      msp->registers + MSP_RCE0);
			stm_msp_write(mult_config->rx_channel_1_enable,
				      msp->registers + MSP_RCE1);
			stm_msp_write(mult_config->rx_channel_2_enable,
				      msp->registers + MSP_RCE2);
			stm_msp_write(mult_config->rx_channel_3_enable,
				      msp->registers + MSP_RCE3);
		} else {
			printk(KERN_ERR "Not in authorised mode\n");
			return -1;
		}
		if (mult_config->rx_comparison_enable_mode) {
			stm_msp_write((stm_msp_read(msp->registers + MSP_MCR) |
				       ((mult_config->
					 rx_comparison_enable_mode << RCMPM_BIT)
					& (0x0000018))),
				      msp->registers + MSP_MCR);

			stm_msp_write(mult_config->comparison_mask,
				      msp->registers + MSP_RCM);
			stm_msp_write(mult_config->comparison_value,
				      msp->registers + MSP_RCV);

		}
	}
	return 0;

}

/**
 * configure_dma - configure dma channel for transmit or receive.
 * @msp: msp structure
 * @config: configuration structure.
 * Context: process
 *
 * It will configure dma channels and request them in Logical mode for both
 * transmit and recevie modes.It also register the respective callback handlers
 * for DMA.
 *
 * Returns void.
 */
void configure_dma(struct msp *msp, struct msp_config *config)
{
	struct stedma40_chan_cfg *rx_dma_info = msp->dma_cfg_rx;
	struct stedma40_chan_cfg *tx_dma_info = msp->dma_cfg_tx;
	dma_cap_mask_t mask;

	if (config->direction == MSP_TRANSMIT_MODE
	    || config->direction == MSP_BOTH_T_R_MODE) {

		if (msp->tx_pipeid != NULL) {
			dma_release_channel(msp->tx_pipeid);
			msp->tx_pipeid = NULL;
		}

		if (config->data_size == MSP_DATA_BITS_32)
			tx_dma_info->src_info.data_width = STEDMA40_WORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_16)
			tx_dma_info->src_info.data_width
				= STEDMA40_HALFWORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_8)
			tx_dma_info->src_info.data_width
				= STEDMA40_BYTE_WIDTH;
		else
			printk(KERN_ERR "Wrong data size\n");

		if (config->data_size == MSP_DATA_BITS_32)
			tx_dma_info->dst_info.data_width = STEDMA40_WORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_16)
			tx_dma_info->dst_info.data_width
				= STEDMA40_HALFWORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_8)
			tx_dma_info->dst_info.data_width
				= STEDMA40_BYTE_WIDTH;
		else
			printk(KERN_ERR "Wrong data size\n");

		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		msp->tx_pipeid = dma_request_channel(mask, stedma40_filter,
						     tx_dma_info);
	}
	if (config->direction == MSP_RECEIVE_MODE
	    || config->direction == MSP_BOTH_T_R_MODE) {

		if (msp->rx_pipeid != NULL) {
			dma_release_channel(msp->rx_pipeid);
			msp->rx_pipeid = NULL;
		}

		if (config->data_size == MSP_DATA_BITS_32)
			rx_dma_info->src_info.data_width = STEDMA40_WORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_16)
			rx_dma_info->src_info.data_width
				= STEDMA40_HALFWORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_8)
			rx_dma_info->src_info.data_width = STEDMA40_BYTE_WIDTH;
		else
			printk(KERN_ERR "Wrong data size\n");

		if (config->data_size == MSP_DATA_BITS_32)
			rx_dma_info->dst_info.data_width = STEDMA40_WORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_16)
			rx_dma_info->dst_info.data_width
				= STEDMA40_HALFWORD_WIDTH;
		else if (config->data_size == MSP_DATA_BITS_8)
			rx_dma_info->dst_info.data_width = STEDMA40_BYTE_WIDTH;
		else
			printk(KERN_ERR "Wrong data size\n");

		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		msp->rx_pipeid = dma_request_channel(mask, stedma40_filter,
						     rx_dma_info);
	}

}

/**
 * msp_enable - Setup the msp configuration.
 * @msp: msp data contains main msp structure.
 * @config: configuration structure sent by i2s client driver.
 * Context: process
 *
 * Main msp configuring functions to configure msp in accordance with msp
 * protocol descriptor, configuring msp clock,setup transfer mode selected by
 * user like DMA, interrupt or polling and in the end enable RX and Tx path.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int msp_enable(struct msp *msp, struct msp_config *config)
{
	int status = 0;
	int state;

	/* Check msp state whether in RUN or CONFIGURED Mode */
	state = msp->msp_state;
	if (state == MSP_STATE_IDLE) {
		if (msp->plat_init) {
			status = msp->plat_init();
			if (status) {
				printk(KERN_ERR "Error in msp_i2s_init,"
					" status is %d\n", status);
				return status;
			}
		}
	}

	/* Configure msp with protocol dependent settings */
	configure_protocol(msp, config);
	configure_clock(msp, config);
	if (config->multichannel_configured == 1) {
		status = configure_multichannel(msp, config);
		if (status)
			printk(KERN_ERR "multichannel can't be configured\n");
	}
	msp->work_mode = config->work_mode;

	if (msp->work_mode == MSP_DMA_MODE && !msp->dma_cfg_rx) {
		switch (config->direction)  {
		case MSP_RECEIVE_MODE:
		case MSP_BOTH_T_R_MODE:
			dev_err(&msp->i2s_cont->dev, "RX DMA not available");
			return -EINVAL;
		}
	}

	if (msp->work_mode == MSP_DMA_MODE && !msp->dma_cfg_tx) {
		switch (config->direction)  {
		case MSP_TRANSMIT_MODE:
		case MSP_BOTH_T_R_MODE:
			dev_err(&msp->i2s_cont->dev, "TX DMA not available");
			return -EINVAL;
		}
	}

	switch (config->direction) {
	case MSP_TRANSMIT_MODE:
		/*Currently they are ignored
		   stm_msp_write((stm_msp_read(msp->registers + MSP_IMSC) |
		   TRANSMIT_UNDERRUN_ERR_INT |
		   TRANSMIT_FRAME_SYNC_ERR_INT),
		   msp->registers + MSP_IMSC); */
		if (config->work_mode == MSP_DMA_MODE) {
			stm_msp_write(stm_msp_read(msp->registers + MSP_DMACR) |
				      TX_DMA_ENABLE,
				      msp->registers + MSP_DMACR);

			msp->xfer_data.tx_handler = config->handler;
			msp->xfer_data.tx_callback_data =
			    config->tx_callback_data;
			configure_dma(msp, config);
		}
		if (config->work_mode == MSP_POLLING_MODE) {
			stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
				       (TX_ENABLE)), msp->registers + MSP_GCR);
		}
		if (msp->work_mode != MSP_DMA_MODE) {
			switch (msp->actual_data_size) {
			case MSP_DATA_BITS_8:
				msp->write = u8_msp_write;
				break;
			case MSP_DATA_BITS_10:
			case MSP_DATA_BITS_12:
			case MSP_DATA_BITS_14:
			case MSP_DATA_BITS_16:
				msp->write = u16_msp_write;
				break;
			case MSP_DATA_BITS_20:
			case MSP_DATA_BITS_24:
			case MSP_DATA_BITS_32:
			default:
				msp->write = u32_msp_write;
				break;
			}
			msp->xfer_data.tx_handler = config->handler;
			msp->xfer_data.tx_callback_data =
			    config->tx_callback_data;
			msp->xfer_data.rx_callback_data =
			    config->rx_callback_data;
			msp->xfer_data.msp = msp;
		}
		break;
	case MSP_RECEIVE_MODE:
		/*Currently they are ignored
		   stm_msp_write(stm_msp_read(msp->registers + MSP_IMSC) |
		   RECEIVE_OVERRUN_ERROR_INT | RECEIVE_FRAME_SYNC_ERR_INT,
		   msp->registers + MSP_IMSC); */
		if (config->work_mode == MSP_DMA_MODE) {
			stm_msp_write(stm_msp_read(msp->registers + MSP_DMACR) |
				      RX_DMA_ENABLE,
				      msp->registers + MSP_DMACR);

			msp->xfer_data.rx_handler = config->handler;
			msp->xfer_data.rx_callback_data =
			    config->rx_callback_data;

			configure_dma(msp, config);
		}
		if (config->work_mode == MSP_POLLING_MODE) {
			stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
				       (RX_ENABLE)), msp->registers + MSP_GCR);
		}
		if (msp->work_mode != MSP_DMA_MODE) {
			switch (msp->actual_data_size) {
			case MSP_DATA_BITS_8:
				msp->read = u8_msp_read;
				break;
			case MSP_DATA_BITS_10:
			case MSP_DATA_BITS_12:
			case MSP_DATA_BITS_14:
			case MSP_DATA_BITS_16:
				msp->read = u16_msp_read;
				break;
			case MSP_DATA_BITS_20:
			case MSP_DATA_BITS_24:
			case MSP_DATA_BITS_32:
			default:
				msp->read = u32_msp_read;
				break;
			}
			msp->xfer_data.rx_handler = config->handler;
			msp->xfer_data.tx_callback_data =
			    config->tx_callback_data;
			msp->xfer_data.rx_callback_data =
			    config->rx_callback_data;
			msp->xfer_data.msp = msp;
		}

		break;
	case MSP_BOTH_T_R_MODE:
		/*Currently they are ignored
		   stm_msp_write(stm_msp_read(msp->registers + MSP_IMSC) |
		   RECEIVE_OVERRUN_ERROR_INT | RECEIVE_FRAME_SYNC_ERR_INT |
		   TRANSMIT_UNDERRUN_ERR_INT | TRANSMIT_FRAME_SYNC_ERR_INT ,
		   msp->registers + MSP_IMSC); */
		if (config->work_mode == MSP_DMA_MODE) {
			stm_msp_write(stm_msp_read(msp->registers + MSP_DMACR) |
				      RX_DMA_ENABLE | TX_DMA_ENABLE,
				      msp->registers + MSP_DMACR);

			msp->xfer_data.tx_handler = config->handler;
			msp->xfer_data.rx_handler = config->handler;
			msp->xfer_data.tx_callback_data =
			    config->tx_callback_data;
			msp->xfer_data.rx_callback_data =
			    config->rx_callback_data;

			configure_dma(msp, config);
		}
		if (config->work_mode == MSP_POLLING_MODE) {
			stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
				       (TX_ENABLE)), msp->registers + MSP_GCR);
			stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
				       (RX_ENABLE)), msp->registers + MSP_GCR);
		}
		if (msp->work_mode != MSP_DMA_MODE) {
			switch (msp->actual_data_size) {
			case MSP_DATA_BITS_8:
				msp->read = u8_msp_read;
				msp->write = u8_msp_write;
				break;
			case MSP_DATA_BITS_10:
			case MSP_DATA_BITS_12:
			case MSP_DATA_BITS_14:
			case MSP_DATA_BITS_16:
				msp->read = u16_msp_read;
				msp->write = u16_msp_write;
				break;
			case MSP_DATA_BITS_20:
			case MSP_DATA_BITS_24:
			case MSP_DATA_BITS_32:
			default:
				msp->read = u32_msp_read;
				msp->write = u32_msp_write;
				break;
			}
			msp->xfer_data.tx_handler = config->handler;
			msp->xfer_data.rx_handler = config->handler;
			msp->xfer_data.tx_callback_data =
			    config->tx_callback_data;
			msp->xfer_data.rx_callback_data =
			    config->rx_callback_data;
			msp->xfer_data.msp = msp;
		}

		break;
	default:
		printk(KERN_ERR "Invalid direction parameter\n");
		if (msp->plat_exit)
			msp->plat_exit();
		status = -EINVAL;
		return status;
	}

	switch (config->work_mode) {
	case MSP_DMA_MODE:
		msp->transfer = msp_dma_xfer;
		break;
	case MSP_POLLING_MODE:
		msp->transfer = msp_polling_xfer;
		break;
	case MSP_INTERRUPT_MODE:
		msp->transfer = msp_interrupt_xfer;
		break;
	default:
		msp->transfer = NULL;
	}

	stm_msp_write(config->iodelay, msp->registers + MSP_IODLY);

	/* enable frame generation logic */
	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
		       (FRAME_GEN_ENABLE)), msp->registers + MSP_GCR);

	return status;
}

/**
 * flush_rx_fifo - Flush Rx fifo MSP controller.
 * @msp: msp structure.
 *
 * This function flush the rx fifo of msp controller.
 *
 * Returns error(-1) in case of failure else success(0)
 */
static void flush_rx_fifo(struct msp *msp)
{
	u32 dummy = 0;
	u32 limit = 32;
	u32 cur = stm_msp_read(msp->registers + MSP_GCR);
	stm_msp_write(cur | RX_ENABLE, msp->registers + MSP_GCR);
	while (!(stm_msp_read(msp->registers + MSP_FLR) & RX_FIFO_EMPTY)
	       && limit--) {
		dummy = stm_msp_read(msp->registers + MSP_DR);
	}
	stm_msp_write(cur, msp->registers + MSP_GCR);
}

/**
 * flush_tx_fifo - Flush Tx fifo MSP controller.
 * @msp: msp structure.
 *
 * This function flush the tx fifo using test intergration register to read data
 * from tx fifo directly.
 *
 * Returns error(-1) in case of failure else success(0)
 */
static void flush_tx_fifo(struct msp *msp)
{
	u32 dummy = 0;
	u32 limit = 32;
	u32 cur = stm_msp_read(msp->registers + MSP_GCR);
	stm_msp_write(cur | TX_ENABLE, msp->registers + MSP_GCR);
	stm_msp_write(0x3, msp->registers + MSP_ITCR);
	while (!(stm_msp_read(msp->registers + MSP_FLR) & TX_FIFO_EMPTY)
	       && limit--) {
		dummy = stm_msp_read(msp->registers + MSP_TSTDR);
	}
	stm_msp_write(0x0, msp->registers + MSP_ITCR);
	stm_msp_write(cur, msp->registers + MSP_GCR);
}

/**
 * stm_msp_configure_enable - configures and enables the MSP controller.
 * @i2s_cont: i2s controller sent by i2s device.
 * @configuration: specifies the configuration parameters.
 *
 * This function configures the msp controller with the client configuration.
 *
 * Returns error(-1) in case of failure else success(0)
 */
static int stm_msp_configure_enable(struct i2s_controller *i2s_cont,
				    void *configuration)
{
	u32 old_reg;
	u32 new_reg;
	u32 mask;
	int res;
	struct msp_config *config =
	    (struct msp_config *)configuration;
	struct msp *msp = (struct msp *)i2s_cont->data;

	if (in_interrupt()) {
		printk(KERN_ERR
		       "can't call configure_enable in interrupt context\n");
		return -1;
	}

	/* Two simultanous configuring msp is avoidable */
	down(&msp->lock);

	/* Don't enable regulator if its msp1 or msp3 */
	if (!(msp->reg_enabled) && msp->id != MSP_1_I2S_CONTROLLER
				&& msp->id != MSP_3_I2S_CONTROLLER) {
		res = regulator_enable(msp_vape_supply);
		if (res != 0) {
			dev_err(&msp->i2s_cont->dev,
					"Failed to enable regulator\n");
			up(&msp->lock);
			return res;
		}
		msp->reg_enabled = 1;
	}

	switch (msp->users) {
	case 0:
		clk_enable(msp->clk);
		msp->direction = config->direction;
		break;
	case 1:
		if (msp->direction == MSP_BOTH_T_R_MODE ||
		    config->direction == msp->direction ||
		    config->direction == MSP_BOTH_T_R_MODE) {
			dev_notice(&i2s_cont->dev, "%s: MSP in use in the "
				   "desired direction.\n", __func__);
			up(&msp->lock);
			return -EBUSY;
		}
		msp->direction = MSP_BOTH_T_R_MODE;
		break;
	default:
		dev_notice(&i2s_cont->dev, "%s: MSP in use in both "
			   "directions.\n", __func__);
		up(&msp->lock);
		return -EBUSY;
	}
	msp->users++;

	/* First do the global config register */
	mask =
	    RX_CLK_SEL_MASK | TX_CLK_SEL_MASK | RX_FRAME_SYNC_MASK |
	    TX_FRAME_SYNC_MASK | RX_SYNC_SEL_MASK | TX_SYNC_SEL_MASK |
	    RX_FIFO_ENABLE_MASK | TX_FIFO_ENABLE_MASK | SRG_CLK_SEL_MASK |
	    LOOPBACK_MASK | TX_EXTRA_DELAY_MASK;

	new_reg =
	    (config->tx_clock_sel | config->rx_clock_sel | config->
	     rx_frame_sync_pol | config->tx_frame_sync_pol | config->
	     rx_frame_sync_sel | config->tx_frame_sync_sel | config->
	     rx_fifo_config | config->tx_fifo_config | config->
	     srg_clock_sel | config->loopback_enable | config->tx_data_enable);

	old_reg = stm_msp_read(msp->registers + MSP_GCR);
	old_reg &= ~mask;
	new_reg |= old_reg;
	stm_msp_write(new_reg, msp->registers + MSP_GCR);

	if (msp_enable(msp, config) != 0) {
		printk(KERN_ERR "error enabling MSP\n");
		return -EBUSY;
	}
	if (config->loopback_enable & 0x80)
		msp->loopback_enable = 1;
	/*Sometimes FIFO doesn't gets empty hence limit is provided */
	flush_tx_fifo(msp);
	/*This has been added in order to fix fifo flush problem
	   When last xfer occurs some data remains in fifo. In order to
	   flush that data delay is needed */
	msleep(10);
	/* wait for fifo to flush */
	flush_rx_fifo(msp);

	/* RX_BUSY take a while to clear */
	msleep(10);

	msp->msp_state = MSP_STATE_CONFIGURED;
	up(&msp->lock);
	return 0;
}

static int msp_start_dma(struct msp *msp, int transmit, dma_addr_t data,
			 size_t bytes)
{
	struct dma_async_tx_descriptor *desc;
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, pfn_to_page(PFN_DOWN(data)), bytes,
		    offset_in_page(data));
	sg_dma_address(&sg) = data;
	sg_dma_len(&sg) = bytes;

	if (transmit) {
		if (!msp->tx_pipeid)
			return -EINVAL;
		desc = msp->tx_pipeid->device->
				device_prep_slave_sg(msp->tx_pipeid,
						&sg, 1, DMA_TO_DEVICE,
						DMA_PREP_INTERRUPT
						| DMA_CTRL_ACK);
		if (!desc)
			return -ENOMEM;

		desc->callback = msp->xfer_data.tx_handler;
		desc->callback_param = msp->xfer_data.tx_callback_data;
		desc->tx_submit(desc);
		dma_async_issue_pending(msp->tx_pipeid);
	} else {
		if (!msp->rx_pipeid)
			return -EINVAL;

		desc = msp->rx_pipeid->device->
				device_prep_slave_sg(msp->rx_pipeid,
						&sg, 1, DMA_FROM_DEVICE,
						DMA_PREP_INTERRUPT
						| DMA_CTRL_ACK);
		if (!desc)
			return -EBUSY;

		desc->callback = msp->xfer_data.rx_handler;
		desc->callback_param = msp->xfer_data.rx_callback_data;
		desc->tx_submit(desc);
		dma_async_issue_pending(msp->rx_pipeid);
	}

	return 0;
}

static int msp_single_dma_tx(struct msp *msp, dma_addr_t data, size_t bytes)
{
	int status;
	status = msp_start_dma(msp, 1, data, bytes);
	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) | (TX_ENABLE)),
		msp->registers + MSP_GCR);
	return status;
}

static int msp_single_dma_rx(struct msp *msp, dma_addr_t data, size_t bytes)
{
	int status;
	status = msp_start_dma(msp, 0, data, bytes);
	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) | (RX_ENABLE)),
		msp->registers + MSP_GCR);
	return status;
}

static void msp_cyclic_dma_start(struct msp *msp,
				dma_addr_t buf_addr,
				size_t buf_len,
				size_t period_len,
				enum dma_data_direction direction)
{
	int ret;
	struct dma_async_tx_descriptor *cdesc;
	struct dma_chan *pipeid = (direction == DMA_TO_DEVICE) ?
				msp->tx_pipeid :
				msp->rx_pipeid;

	pr_debug("%s: buf_addr = %p\n", __func__, (void *) buf_addr);
	pr_debug("%s: buf_len = %d\n", __func__, buf_len);
	pr_debug("%s: perios_len = %d\n", __func__, period_len);

	/* setup the cyclic description */
	cdesc = pipeid->device->device_prep_dma_cyclic(pipeid,
			buf_addr, /* reuse the sq list for the moment */
			buf_len,
			period_len,
			direction);

	if (IS_ERR(cdesc)) {
		pr_err("%s: Error: device_prep_dma_cyclic failed (%ld)!\n",
			__func__,
			PTR_ERR(cdesc));
		return;
	}

	cdesc->callback = (direction == DMA_TO_DEVICE) ?
				msp->xfer_data.tx_handler :
				msp->xfer_data.rx_handler;
	cdesc->callback_param = (direction == DMA_TO_DEVICE) ?
				msp->xfer_data.tx_callback_data :
				msp->xfer_data.rx_callback_data;

	/* submit to the dma */
	ret = dmaengine_submit(cdesc);

	/* start the dma */
	dma_async_issue_pending(pipeid);

	return;
}

/* Legacy function. Used by HATS driver. */
static void msp_loopback_inf_start_dma(struct msp *msp,
				dma_addr_t data,
				size_t bytes)
{
#if 0
	struct stedma40_cyclic_desc *rxcdesc;
	struct stedma40_cyclic_desc *txcdesc;
	struct scatterlist rxsg[2];
	struct scatterlist txsg[2];
	size_t len = bytes >> 1;
	int ret;

	sg_init_table(rxsg, ARRAY_SIZE(rxsg));
	sg_init_table(txsg, ARRAY_SIZE(txsg));

	sg_dma_len(&rxsg[0]) = len;
	sg_dma_len(&rxsg[1]) = len;
	sg_dma_len(&txsg[0]) = len;
	sg_dma_len(&txsg[1]) = len;

	sg_dma_address(&rxsg[0]) = data;
	sg_dma_address(&rxsg[1]) = data + len;

	sg_dma_address(&txsg[0]) = data + len;
	sg_dma_address(&txsg[1]) = data;

	rxcdesc = stedma40_cyclic_prep_sg(msp->rx_pipeid,
					  rxsg, ARRAY_SIZE(rxsg),
					  DMA_FROM_DEVICE, 0);
	if (IS_ERR(rxcdesc))
		return;

	txcdesc = stedma40_cyclic_prep_sg(msp->tx_pipeid,
					  txsg, ARRAY_SIZE(txsg),
					  DMA_TO_DEVICE, 0);
	if (IS_ERR(txcdesc))
		goto free_rx;

	ret = stedma40_cyclic_start(msp->rx_pipeid);
	if (ret)
		goto free_tx;

	ret = stedma40_cyclic_start(msp->tx_pipeid);
	if (ret)
		goto stop_rx;

	msp->infinite = true;

	return;

stop_rx:
	stedma40_cyclic_stop(msp->rx_pipeid);
free_tx:
	stedma40_cyclic_free(msp->tx_pipeid);
free_rx:
	stedma40_cyclic_free(msp->rx_pipeid);
#endif
  return;
}

/**
 * msp_dma_xfer - Handles DMA transfers over i2s bus.
 * @msp: main msp structure.
 * @msg: i2s_message contains info about transmit and receive data.
 * Context: process
 *
 * This will first check whether data buffer is dmaable or not.
 * Call dma_map_single apis etc to make it dmaable dma. Starts the dma transfer
 * for TX and RX parallely and wait for it to get completed.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int msp_dma_xfer(struct msp *msp, struct i2s_message *msg)
{
	int status = 0;

	switch (msg->i2s_transfer_mode) {
	default:
	case I2S_TRANSFER_MODE_SINGLE_DMA:
		if (msg->i2s_direction == I2S_DIRECTION_RX ||
				msg->i2s_direction == I2S_DIRECTION_BOTH)
			if (msg->rxdata && (msg->rxbytes > 0)) {
				if (!msg->dma_flag)
					msg->rxdata =
						(void *)dma_map_single(NULL,
								msg->rxdata,
								msg->rxbytes,
								DMA_FROM_DEVICE
								);
				status = msp_single_dma_rx(msp,
						(dma_addr_t)msg->rxdata,
						msg->rxbytes);
			}
		if (msg->i2s_direction == I2S_DIRECTION_TX ||
				msg->i2s_direction == I2S_DIRECTION_BOTH)
			if (msg->txdata && (msg->txbytes > 0)) {
				if (!msg->dma_flag)
					msg->txdata =
						(void *)dma_map_single(NULL,
								msg->txdata,
								msg->txbytes,
								DMA_TO_DEVICE);
				status = msp_single_dma_tx(msp,
						(dma_addr_t)msg->txdata,
						msg->txbytes);
			}
		break;

	case I2S_TRANSFER_MODE_CYCLIC_DMA:
		if (msg->i2s_direction == I2S_DIRECTION_TX) {
			msp_cyclic_dma_start(msp,
					msg->buf_addr,
					msg->buf_len,
					msg->period_len,
					DMA_TO_DEVICE);
			stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
				(TX_ENABLE)),
				msp->registers + MSP_GCR);
		} else {
			msp_cyclic_dma_start(msp,
					msg->buf_addr,
					msg->buf_len,
					msg->period_len,
					DMA_FROM_DEVICE);
			stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
				(RX_ENABLE)),
				msp->registers + MSP_GCR);
		}
		break;

	case I2S_TRANSFER_MODE_INF_LOOPBACK:
		msp_loopback_inf_start_dma(msp,
					(dma_addr_t)msg->rxdata,
					msg->rxbytes);
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
			       (RX_ENABLE)),
				msp->registers + MSP_GCR);
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
			       (TX_ENABLE)),
				msp->registers + MSP_GCR);
		break;
	}

	return status;
}

#if 0
/**
 * msp_handle_irq - Interrupt handler routine.
 * @irq: irq no.
 * @dev_id: device structure registered in request irq.
 *
 * Returns error(-1) on failure else success(0).
 */
static irqreturn_t msp_handle_irq(int irq, void *dev_id)
{
	u32 irq_status;
	struct msp *msp = (struct msp *)dev_id;
	struct i2s_message *message = &msp->xfer_data.message;
	u32 irq_mask = 0;
	irq_status = stm_msp_read(msp->registers + MSP_MIS);
	irq_mask = stm_msp_read(msp->registers + MSP_IMSC);
/* Disable the interrupt to prevent immediate recurrence */
	stm_msp_write(stm_msp_read(msp->registers + MSP_IMSC) & ~irq_status,
		      msp->registers + MSP_IMSC);

/* Clear the interrupt */
	stm_msp_write(irq_status, msp->registers + MSP_ICR);
/* Check for an error condition */
	msp->msp_io_error = irq_status & (RECEIVE_OVERRUN_ERROR_INT |
					  RECEIVE_FRAME_SYNC_ERR_INT |
					  TRANSMIT_UNDERRUN_ERR_INT |
					  TRANSMIT_FRAME_SYNC_ERR_INT);

	 /*Currently they are ignored */
	if (irq_status & RECEIVE_OVERRUN_ERROR_INT)
		;
	if (irq_status & TRANSMIT_UNDERRUN_ERR_INT)
		;

	/* This code has been added basically to support loopback mode
	 * Basically Transmit interrupt is not disabled even after its
	 * completion so that receive fifo gets an additional interrupt
	 */
	if (irq_mask & (RECEIVE_SERVICE_INT)
	    && (irq_mask & (TRANSMIT_SERVICE_INT)) && (msp->loopback_enable)) {
		if (msp->read)
			msp->read(&msp->xfer_data);
		if (msp->write)
			msp->write(&msp->xfer_data);
		if (message->rx_offset >= message->rxbytes) {
			if (msp->xfer_data.rx_handler)
				msp->xfer_data.rx_handler(msp->
							  xfer_data.
							  rx_callback_data,
							  message->rx_offset);
			msp->xfer_data.rx_handler = NULL;
			return IRQ_HANDLED;
		}

		if (message->tx_offset >= message->txbytes) {
			if (msp->xfer_data.tx_handler)
				msp->xfer_data.tx_handler(msp->xfer_data.
							  tx_callback_data,
							  message->tx_offset);
			msp->xfer_data.tx_handler = NULL;
		}
		stm_msp_write(irq_mask, msp->registers + MSP_IMSC);
		return IRQ_HANDLED;
	}

	if (irq_status & RECEIVE_SERVICE_INT) {
		if (msp->read)
			msp->read(&msp->xfer_data);
		if (message->rx_offset >= message->rxbytes) {
			irq_mask &= ~RECEIVE_SERVICE_INT;
			stm_msp_write(irq_mask, msp->registers + MSP_IMSC);
			if (msp->xfer_data.rx_handler)
				msp->xfer_data.rx_handler(msp->
							  xfer_data.
							  rx_callback_data,
							  message->rx_offset);
			if (!(irq_status & TRANSMIT_SERVICE_INT))
				return IRQ_HANDLED;
		}
	}
	if (irq_status & TRANSMIT_SERVICE_INT) {
		if (msp->write)
			msp->write(&msp->xfer_data);
		if (message->tx_offset >= message->txbytes) {
			irq_mask &= ~TRANSMIT_SERVICE_INT;
			stm_msp_write(irq_mask, msp->registers + MSP_IMSC);
			if (msp->xfer_data.tx_handler)
				msp->xfer_data.tx_handler(msp->xfer_data.
							  tx_callback_data,
							  message->tx_offset);
			return IRQ_HANDLED;
		}
	}
	stm_msp_write(irq_mask, msp->registers + MSP_IMSC);
	return IRQ_HANDLED;

}
#endif

/**
 * msp_interrupt_xfer - Handles Interrupt transfers over i2s bus.
 * @msp: main msp structure.
 * @msg: i2s_message contains info about transmit and receive data.
 * Context: Process or interrupt.
 *
 * This implements transfer and receive functions used in interrupt mode.
 * This can be used in interrupt context if a callback handler is registered
 * by client driver. This has been to improve performance in interrupt mode.
 * Hence can't use sleep in this function.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int msp_interrupt_xfer(struct msp *msp, struct i2s_message *msg)
{
	struct i2s_message *message;
	u32 irq_mask = 0;

	if (msg->i2s_transfer_mode != I2S_TRANSFER_MODE_NON_DMA)
		return -EINVAL;

	if (msg->txbytes) {
		msp->xfer_data.message.txbytes = msg->txbytes;
		msp->xfer_data.message.txdata = msg->txdata;
		msp->xfer_data.message.tx_offset = 0;
	}
	if (msg->rxbytes) {
		msp->xfer_data.message.rxbytes = msg->rxbytes;
		msp->xfer_data.message.rxdata = msg->rxdata;
		msp->xfer_data.message.rx_offset = 0;
	}
	message = &msp->xfer_data.message;
	if ((message->txdata == NULL || message->txbytes == 0)
	    && (message->rxdata == NULL || message->rxbytes == 0)) {
		printk(KERN_ERR
		       "transmit_receive_data is NULL with bytes > 0\n");
		return -EINVAL;
	}

	msp->msp_io_error = 0;

	if (message->tx_offset < message->txbytes) {
		irq_mask |= TRANSMIT_SERVICE_INT;
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
			       (TX_ENABLE)), msp->registers + MSP_GCR);
	}
	if (message->rx_offset < message->rxbytes) {
		irq_mask |= RECEIVE_SERVICE_INT;
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
			       (RX_ENABLE)), msp->registers + MSP_GCR);
	}
	stm_msp_write((stm_msp_read(msp->registers + MSP_IMSC) |
		       irq_mask), msp->registers + MSP_IMSC);
	return 0;
}

/**
 * func_notify_timer - Handles Polling hang issue over i2s bus.
 * @data: main msp data address
 * Context: Interrupt.
 *
 * This is  used to handle error condition in transfer and receive function used
 * in polling mode.
 * Sometimes due to passing wrong protocol desc , polling transfer may hang.
 * To prevent this, timer is added.
 *
 * Returns void.
 */
static void func_notify_timer(unsigned long data)
{
	struct msp *msp = (struct msp *)data;
	if (msp->polling_flag) {
		msp->msp_io_error = 1;
		printk(KERN_ERR
		       "Polling is taking two much time, may be it got hang\n");
		del_timer(&msp->notify_timer);
	}
}

/**
 * msp_polling_xfer - Handles Polling transfers over i2s bus.
 * @msp: main msp structure.
 * @msg: i2s_message contains info about transmit and receive data.
 * Context: Process.
 *
 * This implements transfer and receive functions used in polling mode. This is
 * blocking fucntion.
 * It is recommended to use interrupt or dma mode for better performance rather
 * than the polling mode.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int msp_polling_xfer(struct msp *msp, struct i2s_message *msg)
{
	struct i2s_message *message;
	u32 time_expire = 0;
	u32 tr_ex = 0, rr_ex = 0;
	u32 msec_jiffies = 0;

	if (msg->i2s_transfer_mode != I2S_TRANSFER_MODE_NON_DMA)
		return -EINVAL;

	if (msg->txbytes) {
		msp->xfer_data.message.txbytes = msg->txbytes;
		msp->xfer_data.message.txdata = msg->txdata;
		msp->xfer_data.message.tx_offset = 0;
		tr_ex = msg->txbytes;
	}
	if (msg->rxbytes) {
		msp->xfer_data.message.rxbytes = msg->rxbytes;
		msp->xfer_data.message.rxdata = msg->rxdata;
		msp->xfer_data.message.rx_offset = 0;
		rr_ex = msg->rxbytes;
	}
	message = &msp->xfer_data.message;
	time_expire = (tr_ex + rr_ex) / 1024;
	if (!time_expire)
		msec_jiffies = 500;
	else
		msec_jiffies = time_expire * 500;
	msp->notify_timer.expires = jiffies + msecs_to_jiffies(msec_jiffies);
	down(&msp->lock);
	if (message->txdata == NULL && message->txbytes > 0) {
		printk(KERN_ERR
		       "transmit_receive_data is NULL with bytes > 0\n");
		return -EINVAL;
	}

	if (message->rxdata == NULL && message->rxbytes > 0) {
		printk(KERN_ERR
		       "transmit_receive_data is NULL with bytes > 0\n");
		return -EINVAL;
	}
	msp->msp_io_error = 0;
	msp->polling_flag = 1;
	add_timer(&msp->notify_timer);
	while (message->tx_offset < message->txbytes
	       || message->rx_offset < message->rxbytes) {
		if (msp->msp_io_error)
			break;
		if (msp->read)
			msp->read(&msp->xfer_data);
		if (msp->write)
			msp->write(&msp->xfer_data);
	}
	msp->polling_flag = 0;
	del_timer(&msp->notify_timer);
	up(&msp->lock);
	return message->txbytes + message->rxbytes;
}

/**
 * stm_msp_transceive_data - Main i2s transfer function.
 * @i2s_cont: i2s controller structure passed by client driver.
 * @message: i2s message structure contains transceive info.
 * Context: process or interrupt.
 *
 * This function is registered over i2s_xfer funtions. It will handle main i2s
 * transfer over i2s bus in various modes.It call msp transfer function on which
 * suitable transfer function is already registered i.e dma ,interrupt or
 * polling function.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int stm_msp_transceive_data(struct i2s_controller *i2s_cont,
				   struct i2s_message *message)
{
	int status = 0;
	struct msp *msp = (struct msp *)i2s_cont->data;

	if (!message || (msp->msp_state == MSP_STATE_IDLE)) {
		printk(KERN_ERR "Message is NULL\n");
		return -EPERM;
	}

	msp->msp_state = MSP_STATE_RUN;
	if (msp->transfer)
		status = msp->transfer(msp, message);

	if (msp->msp_state == MSP_STATE_RUN)
		msp->msp_state = MSP_STATE_CONFIGURED;

	return status;
}

/**
 * msp_disable_receive - Disable receive functionality.
 * @msp: main msp structure.
 * Context: process.
 *
 * This function will disable msp controller's receive functionality like dma,
 * interrupt receive data buffer all are disabled.
 *
 * Returns void.
 */
static void msp_disable_receive(struct msp *msp)
{
	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) &
		       (~RX_ENABLE)), msp->registers + MSP_GCR);
	stm_msp_write((stm_msp_read(msp->registers + MSP_DMACR) &
		       (~RX_DMA_ENABLE)), msp->registers + MSP_DMACR);
	stm_msp_write((stm_msp_read(msp->registers + MSP_IMSC) &
		       (~
			(RECEIVE_SERVICE_INT |
			 RECEIVE_OVERRUN_ERROR_INT))),
		      msp->registers + MSP_IMSC);
	msp->xfer_data.message.rxbytes = 0;
	msp->xfer_data.message.rx_offset = 0;
	msp->xfer_data.message.rxdata = NULL;
	msp->read = NULL;

}

/**
 * msp_disable_transmit - Disable transmit functionality.
 * @msp: main msp structure.
 * Context: process.
 *
 * This function will disable msp controller's transmit functionality like dma,
 * interrupt transmit data buffer all are disabled.
 *
 * Returns void.
 */
static void msp_disable_transmit(struct msp *msp)
{

	stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) &
		       (~TX_ENABLE)), msp->registers + MSP_GCR);
	stm_msp_write((stm_msp_read(msp->registers + MSP_DMACR) &
		       (~TX_DMA_ENABLE)), msp->registers + MSP_DMACR);
	stm_msp_write((stm_msp_read(msp->registers + MSP_IMSC) &
		       (~
			(TRANSMIT_SERVICE_INT |
			 TRANSMIT_UNDERRUN_ERR_INT))),
		      msp->registers + MSP_IMSC);
	msp->xfer_data.message.txbytes = 0;
	msp->xfer_data.message.tx_offset = 0;
	msp->xfer_data.message.txdata = NULL;
	msp->write = NULL;

}

/**
 * stm_msp_disable - disable the given msp controller
 * @msp: specifies the msp contoller data
 * @direction: specifies the transmit/receive direction
 * @flag: It indicates the functionality that needs to be disabled.
 *
 * Returns error(-1) in case of failure else success(0)
 */
static int stm_msp_disable(struct msp *msp, int direction, i2s_flag flag)
{
	int limit = 32;
	u32 dummy = 0;
	int status = 0;
	if (!
	    (stm_msp_read(msp->registers + MSP_GCR) &
	     ((TX_ENABLE | RX_ENABLE)))) {
		return 0;
	}
	if (msp->work_mode == MSP_DMA_MODE) {
		if (flag == DISABLE_ALL || flag == DISABLE_TRANSMIT) {
			if (msp->tx_pipeid != NULL) {
				dmaengine_terminate_all(msp->tx_pipeid);
				dma_release_channel(msp->tx_pipeid);
				msp->tx_pipeid = NULL;
			}
		}
		if ((flag == DISABLE_ALL || flag == DISABLE_RECEIVE)) {
			if (msp->rx_pipeid != NULL) {
				dmaengine_terminate_all(msp->rx_pipeid);
				dma_release_channel(msp->rx_pipeid);
				msp->rx_pipeid = NULL;
			}
		}
	}
	if (flag == DISABLE_TRANSMIT)
		msp_disable_transmit(msp);
	else if (flag == DISABLE_RECEIVE)
		msp_disable_receive(msp);
	else {
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) |
			       (LOOPBACK_MASK)), msp->registers + MSP_GCR);
		/* Flush Tx fifo */
		while ((!
			(stm_msp_read(msp->registers + MSP_FLR) &
			 TX_FIFO_EMPTY)) && limit--)
			dummy = stm_msp_read(msp->registers + MSP_DR);

		/* Disable Transmit channel */
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) &
			       (~TX_ENABLE)), msp->registers + MSP_GCR);
		limit = 32;
		/* Flush Rx Fifo */
		while ((!
			(stm_msp_read(msp->registers + MSP_FLR) &
			 RX_FIFO_EMPTY)) && limit--)
			dummy = stm_msp_read(msp->registers + MSP_DR);
		/* Disable Loopback and Receive channel */
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) &
			       (~(RX_ENABLE | LOOPBACK_MASK))),
			      msp->registers + MSP_GCR);
		/*This has been added in order to fix fifo flush problem
		   When last xfer occurs some data remains in fifo. In order to
		   flush that data delay is needed */
		msleep(10);
		msp_disable_transmit(msp);
		msp_disable_receive(msp);

	}

	/* disable sample rate and frame generators */
	if (flag == DISABLE_ALL) {
		msp->msp_state = MSP_STATE_IDLE;
		stm_msp_write((stm_msp_read(msp->registers + MSP_GCR) &
			       (~(FRAME_GEN_ENABLE | SRG_ENABLE))),
			      msp->registers + MSP_GCR);
		memset(&msp->xfer_data, 0, sizeof(struct trans_data));
		if (msp->plat_exit)
			status = msp->plat_exit();
			if (status)
				printk(KERN_ERR "Error in msp_i2s_exit\n");
		if (msp->work_mode == MSP_POLLING_MODE
		    && msp->msp_state == MSP_STATE_RUN) {
			up(&msp->lock);
		}
		msp->transfer = NULL;
		stm_msp_write(0, msp->registers + MSP_GCR);
		stm_msp_write(0, msp->registers + MSP_TCF);
		stm_msp_write(0, msp->registers + MSP_RCF);
		stm_msp_write(0, msp->registers + MSP_DMACR);
		stm_msp_write(0, msp->registers + MSP_SRG);
		stm_msp_write(0, msp->registers + MSP_MCR);
		stm_msp_write(0, msp->registers + MSP_RCM);
		stm_msp_write(0, msp->registers + MSP_RCV);
		stm_msp_write(0, msp->registers + MSP_TCE0);
		stm_msp_write(0, msp->registers + MSP_TCE1);
		stm_msp_write(0, msp->registers + MSP_TCE2);
		stm_msp_write(0, msp->registers + MSP_TCE3);
		stm_msp_write(0, msp->registers + MSP_RCE0);
		stm_msp_write(0, msp->registers + MSP_RCE1);
		stm_msp_write(0, msp->registers + MSP_RCE2);
		stm_msp_write(0, msp->registers + MSP_RCE3);
	}
	return status;
}

/**
 * stm_msp_close - Close the current i2s connection btw controller and client.
 * @i2s_cont: i2s controller structure
 * @flag: It indicates the functionality that needs to be disabled.
 * Context: process
 *
 * It will call msp_disable and reset the msp configuration. Disables Rx and Tx
 * channels, free gpio irqs and interrupt pins.
 * Called by i2s client driver to indicate the completion of use of i2s bus.
 * It is registered on i2s_close function.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int stm_msp_close(struct i2s_controller *i2s_cont, i2s_flag flag)
{
	int status = 0;
	struct msp *msp = (struct msp *)i2s_cont->data;
	down(&msp->lock);
	if (msp->users == 0) {
		pr_err("MSP already closed!\n");
		status = -EINVAL;
		goto end;
	}
	dev_dbg(&i2s_cont->dev, "%s: users = %d, flag = %d.\n",
	       __func__, msp->users, flag);
	/* We need to call it twice for DISABLE_ALL*/
	msp->users = flag == DISABLE_ALL ? 0 : msp->users - 1;
	if (msp->users)
		status = stm_msp_disable(msp, MSP_BOTH_T_R_MODE, flag);
	else {
		status = stm_msp_disable(msp, MSP_BOTH_T_R_MODE, DISABLE_ALL);
		clk_disable(msp->clk);
		if (msp->reg_enabled) {
			status = regulator_disable(msp_vape_supply);
			msp->reg_enabled = 0;
		}
		if (status != 0) {
			dev_err(&msp->i2s_cont->dev,
				"Failed to disable regulator\n");
			clk_enable(msp->clk);
			goto end;
		}
	}
	if (status)
		goto end;
	if (msp->users)
		msp->direction = flag == DISABLE_TRANSMIT ?
			MSP_RECEIVE_MODE : MSP_TRANSMIT_MODE;

	if (msp->vape_opp_constraint == 1) {
		prcmu_qos_update_requirement(PRCMU_QOS_APE_OPP, "msp_i2s", 50);
		msp->vape_opp_constraint = 0;
	}
end:
	up(&msp->lock);
	return status;

}

static int stm_msp_hw_status(struct i2s_controller *i2s_cont)
{
	struct msp *msp = (struct msp *)i2s_cont->data;

	int status = stm_msp_read(msp->registers + MSP_RIS) & 0xee;
	if (status)
		stm_msp_write(status, msp->registers + MSP_ICR);

	return status;
}

		/*Platform driver's functions */
/**
 * msp_probe - Probe function
 * @pdev: platform device structure.
 * Context: process
 *
 * Probe function of msp platform driver.Handles allocation of memory and irq
 * resource. It creates i2s_controller and one i2s_device per msp controller.
 *
 * Returns error(-1) in case of failure or success(0).
 */
int msp_probe(struct platform_device *pdev)
{
	int status = 0;
	struct device *dev;
	s16 platform_num = 0;
	struct resource *res = NULL;
	int irq;
	struct i2s_controller *i2s_cont;
	struct msp_i2s_platform_data *platform_data;
	struct msp *msp;

	if (!pdev)
		return -EPERM;
	msp = kzalloc(sizeof(*msp), GFP_KERNEL);

	platform_data = (struct msp_i2s_platform_data *)pdev->dev.platform_data;

	msp->id = platform_data->id;
	msp->plat_init = platform_data->msp_i2s_init;
	msp->plat_exit = platform_data->msp_i2s_exit;

	msp->dma_cfg_rx = platform_data->msp_i2s_dma_rx;
	msp->dma_cfg_tx = platform_data->msp_i2s_dma_tx;

	dev = &pdev->dev;
	platform_num = msp->id - 1;

	sema_init(&msp->lock, 1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "probe - MEM resources not defined\n");
		status = -EINVAL;
		goto free_msp;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		status = -EINVAL;
		goto free_msp;
	}
	msp->irq = irq;

	msp->registers = ioremap(res->start, (res->end - res->start + 1));
	if (msp->registers == NULL) {
		status = -EINVAL;
		goto free_msp;
	}

	msp_vape_supply = regulator_get(NULL, "v-ape");
	if (IS_ERR(msp_vape_supply)) {
		status = PTR_ERR(msp_vape_supply);
		printk(KERN_WARNING "msp i2s : failed to get v-ape supply\n");
		goto free_irq;
	}
	prcmu_qos_add_requirement(PRCMU_QOS_APE_OPP, "msp_i2s", 50);
	msp->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(msp->clk)) {
		status = PTR_ERR(msp->clk);
		goto free_irq;
	}

	init_timer(&msp->notify_timer);
	msp->notify_timer.expires = jiffies + msecs_to_jiffies(1000);
	msp->notify_timer.function = func_notify_timer;
	msp->notify_timer.data = (unsigned long)msp;

	msp->rx_pipeid = NULL;
	msp->tx_pipeid = NULL;
	msp->read = NULL;
	msp->write = NULL;
	msp->transfer = NULL;
	msp->msp_state = MSP_STATE_IDLE;
	msp->loopback_enable = 0;

	dev_set_drvdata(&pdev->dev, msp);
	/* I2S Controller is allocated and added in I2S controller class. */
	i2s_cont =
	    (struct i2s_controller *)kzalloc(sizeof(*i2s_cont), GFP_KERNEL);
	if (!i2s_cont) {
		dev_err(&pdev->dev, "i2s controller alloc failed \n");
		status = -EINVAL;
		goto del_timer;
	}
	i2s_cont->dev.parent = dev;
	i2s_cont->algo = &i2s_algo;
	i2s_cont->data = (void *)msp;
	i2s_cont->id = platform_num;
	snprintf(i2s_cont->name, sizeof(i2s_cont->name),
		 "MSP_I2S.%04x", platform_num);

	status = i2s_add_controller(i2s_cont);
	if (status) {
		dev_err(&pdev->dev, "i2s add controller failed (%d)\n", status);
		goto free_cont;
	}
	msp->i2s_cont = i2s_cont;
	return status;
free_cont:
	kfree(msp->i2s_cont);
del_timer:
	del_timer_sync(&msp->notify_timer);
	clk_put(msp->clk);
free_irq:
	iounmap(msp->registers);
free_msp:
	kfree(msp);
	return status;
}

/**
 * msp_remove - remove function
 * @pdev: platform device structure.
 * Context: process
 *
 * remove function of msp platform driver.Handles dellocation of memory and irq
 * resource. It deletes i2s_controller and one i2s_device per msp controller
 * created in msp_probe.
 *
 * Returns error(-1) in case of failure or success(0).
 */
static int msp_remove(struct platform_device *pdev)
{
	struct msp *msp =
	    (struct msp *)dev_get_drvdata(&pdev->dev);
	int status = 0;
	i2s_del_controller(msp->i2s_cont);
	del_timer_sync(&msp->notify_timer);
	clk_put(msp->clk);
	iounmap(msp->registers);
	prcmu_qos_remove_requirement(PRCMU_QOS_APE_OPP, "msp_i2s");
	regulator_put(msp_vape_supply);
	kfree(msp);
	return status;
}
#ifdef CONFIG_PM
/**
 * msp_suspend - MSP suspend function registered with PM framework.
 * @pdev: Reference to platform device structure of the device
 * @state: power mgmt state.
 *
 * This function is invoked when the system is going into sleep, called
 * by the power management framework of the linux kernel.
 * Nothing is required as controller is configured with every transfer.
 * It is assumed that no active tranfer is in progress at this time.
 * Client driver should make sure of this.
 *
 */

int msp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct msp *msp =
		(struct msp *)dev_get_drvdata(&pdev->dev);

	down(&msp->lock);
	if (msp->users > 0) {
		up(&msp->lock);
		return -EBUSY;
	}
	up(&msp->lock);

	return 0;
}
/**
 * msp_resume - MSP Resume function registered with PM framework.
 * @pdev: Reference to platform device structure of the device
 *
 * This function is invoked when the system is coming out of sleep, called
 * by the power management framework of the linux kernel.
 * Nothing is required.
 *
 */

int msp_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define msp_suspend NULL
#define msp_resume NULL
#endif

static struct platform_driver msp_i2s_driver = {
	.probe = msp_probe,
	.remove = msp_remove,
	.suspend = msp_suspend,
	.resume = msp_resume,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "MSP_I2S",
		   },
};

static int __init stm_msp_mod_init(void)
{
	return platform_driver_register(&msp_i2s_driver);
}

static void __exit stm_msp_exit(void)
{
	platform_driver_unregister(&msp_i2s_driver);
	return;
}

module_init(stm_msp_mod_init);
module_exit(stm_msp_exit);

MODULE_AUTHOR("Sandeep Kaushik");
MODULE_DESCRIPTION("STM MSP driver");
MODULE_LICENSE("GPL");

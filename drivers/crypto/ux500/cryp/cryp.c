/**
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Shujuan Chen <shujuan.chen@stericsson.com> for ST-Ericsson.
 * Author: Jonas Linde <jonas.linde@stericsson.com> for ST-Ericsson.
 * Author: Niklas Hernaeus <niklas.hernaeus@stericsson.com> for ST-Ericsson.
 * Author: Joakim Bech <joakim.xx.bech@stericsson.com> for ST-Ericsson.
 * Author: Berne Hebark <berne.herbark@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "cryp_p.h"
#include "cryp.h"

/**
 * cryp_wait_until_done - wait until the device logic is not busy
 */
void cryp_wait_until_done(struct cryp_device_data *device_data)
{
	while (cryp_is_logic_busy(device_data))
		cpu_relax();
}

/**
 * cryp_check - This routine checks Peripheral and PCell Id
 * @device_data: Pointer to the device data struct for base address.
 */
int cryp_check(struct cryp_device_data *device_data)
{
	if (NULL == device_data)
		return -EINVAL;

	/* Check Peripheral and Pcell Id Register for CRYP */
	if ((CRYP_PERIPHERAL_ID0 == readl(&device_data->base->periphId0))
	    && (CRYP_PERIPHERAL_ID1 == readl(&device_data->base->periphId1))
	    && (CRYP_PERIPHERAL_ID2 == readl(&device_data->base->periphId2))
	    && (CRYP_PERIPHERAL_ID3 == readl(&device_data->base->periphId3))
	    && (CRYP_PCELL_ID0 == readl(&device_data->base->pcellId0))
	    && (CRYP_PCELL_ID1 == readl(&device_data->base->pcellId1))
	    && (CRYP_PCELL_ID2 == readl(&device_data->base->pcellId2))
	    && (CRYP_PCELL_ID3 == readl(&device_data->base->pcellId3))) {
		return 0;
	}

	return -EPERM;
}

/**
 * cryp_reset - This routine loads the cryp register with the default values
 * @device_data: Pointer to the device data struct for base address.
 */
void cryp_reset(struct cryp_device_data *device_data)
{
	writel(CRYP_DMACR_DEFAULT, &device_data->base->dmacr);
	writel(CRYP_IMSC_DEFAULT, &device_data->base->imsc);

	writel(CRYP_KEY_DEFAULT, &device_data->base->key_1_l);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_1_r);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_2_l);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_2_r);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_3_l);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_3_r);
	writel(CRYP_INIT_VECT_DEFAULT, &device_data->base->init_vect_0_l);
	writel(CRYP_INIT_VECT_DEFAULT, &device_data->base->init_vect_0_r);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_4_l);
	writel(CRYP_KEY_DEFAULT, &device_data->base->key_4_r);
	writel(CRYP_INIT_VECT_DEFAULT, &device_data->base->init_vect_1_l);
	writel(CRYP_INIT_VECT_DEFAULT, &device_data->base->init_vect_1_r);

	/* Last step since the protection mode bits need to be modified. */
	writel(CRYP_CR_DEFAULT | CRYP_CR_FFLUSH, &device_data->base->cr);

	/*
	 * CRYP_INFIFO_READY_MASK is the expected value on the status register
	 * when starting a new calculation, which means Input FIFO is not full
	 * and input FIFO is empty.
	 */
	while (readl(&device_data->base->status) != CRYP_INFIFO_READY_MASK)
		cpu_relax();
}

/**
 * cryp_activity - This routine enables/disable the cryptography function.
 * @device_data: Pointer to the device data struct for base address.
 * @cryp_activity: Enable/Disable functionality
 */
void cryp_activity(struct cryp_device_data *device_data,
		   enum cryp_crypen cryp_crypen)
{
	CRYP_PUT_BITS(&device_data->base->cr,
		      cryp_crypen,
		      CRYP_CRYPEN_POS,
		      CRYP_CRYPEN_MASK);
}

/**
 * cryp_start - starts the computation
 * @device_data: Pointer to the device data struct for base address.
 * @cryp_start: Enable/Disable functionality
 */
void cryp_start(struct cryp_device_data *device_data)
{
	CRYP_PUT_BITS(&device_data->base->cr,
		      CRYP_START_ENABLE,
		      CRYP_START_POS,
		      CRYP_START_MASK);
}

/**
 * cryp_init_signal - This routine submit the initialization values.
 * @device_data: Pointer to the device data struct for base address.
 * @cryp_init_bit: Enable/Disable init signal
 */
void cryp_init_signal(struct cryp_device_data *device_data,
		      enum cryp_init cryp_init_bit)
{
	CRYP_PUT_BITS(&device_data->base->cr,
		      cryp_init_bit,
		      CRYP_INIT_POS,
		      CRYP_INIT_MASK);
}

/**
 * cryp_key_preparation - This routine prepares key for decryption.
 * @device_data: Pointer to the device data struct for base address.
 * @cryp_prepkey: Enable/Disable
 */
void cryp_key_preparation(struct cryp_device_data *device_data,
			  enum cryp_key_prep cryp_prepkey)
{
	CRYP_PUT_BITS(&device_data->base->cr,
		      cryp_prepkey,
		      CRYP_KSE_POS,
		      CRYP_KSE_MASK);
}

/**
 * cryp_flush_inoutfifo - Resets both the input and the output FIFOs
 * @device_data: Pointer to the device data struct for base address.
 */
void cryp_flush_inoutfifo(struct cryp_device_data *device_data)
{
	CRYP_SET_BITS(&device_data->base->cr, CRYP_FIFO_FLUSH_MASK);
}

/**
 * cryp_set_dir -
 * @device_data: Pointer to the device data struct for base address.
 * @dir: Crypto direction, encrypt/decrypt
 */
void cryp_set_dir(struct cryp_device_data *device_data, int dir)
{
	CRYP_PUT_BITS(&device_data->base->cr,
		      dir,
		      CRYP_ENC_DEC_POS,
		      CRYP_ENC_DEC_MASK);

	CRYP_PUT_BITS(&device_data->base->cr,
		      CRYP_DATA_TYPE_8BIT_SWAP,
		      CRYP_DATA_TYPE_POS,
		      CRYP_DATA_TYPE_MASK);
}

/**
 * cryp_cen_flush -
 * @device_data: Pointer to the device data struct for base address.
 */
void cryp_cen_flush(struct cryp_device_data *device_data)
{
	CRYP_PUT_BITS(&device_data->base->cr,
		      CRYP_STATE_DISABLE,
		      CRYP_KEY_ACCESS_POS,
		      CRYP_KEY_ACCESS_MASK);
	CRYP_SET_BITS(&device_data->base->cr,
		      CRYP_FIFO_FLUSH_MASK);
	CRYP_PUT_BITS(&device_data->base->cr,
		      CRYP_CRYPEN_ENABLE,
		      CRYP_CRYPEN_POS,
		      CRYP_CRYPEN_MASK);
}

/**
 * cryp_set_configuration - This routine set the cr CRYP IP
 * @device_data: Pointer to the device data struct for base address.
 * @p_cryp_config: Pointer to the configuration parameter
 */
int cryp_set_configuration(struct cryp_device_data *device_data,
			   struct cryp_config *p_cryp_config)
{
	if (NULL == device_data)
		return -EINVAL;
	if (NULL == p_cryp_config)
		return -EINVAL;

	/* Since more than one bit is written macro put_bits is used*/
	CRYP_PUT_BITS(&device_data->base->cr,
		      p_cryp_config->key_access,
		      CRYP_KEY_ACCESS_POS,
		      CRYP_KEY_ACCESS_MASK);
	CRYP_PUT_BITS(&device_data->base->cr,
		      p_cryp_config->key_size,
		      CRYP_KEY_SIZE_POS,
		      CRYP_KEY_SIZE_MASK);
	CRYP_PUT_BITS(&device_data->base->cr,
		      p_cryp_config->data_type,
		      CRYP_DATA_TYPE_POS,
		      CRYP_DATA_TYPE_MASK);

	/* Prepare key for decryption */
	if ((CRYP_ALGORITHM_DECRYPT == p_cryp_config->encrypt_or_decrypt) &&
	    ((CRYP_ALGO_AES_ECB == p_cryp_config->algo_mode) ||
	     (CRYP_ALGO_AES_CBC == p_cryp_config->algo_mode))) {
		CRYP_PUT_BITS(&device_data->base->cr,
			      CRYP_ALGO_AES_ECB,
			      CRYP_ALGOMODE_POS,
			      CRYP_ALGOMODE_MASK);
		CRYP_PUT_BITS(&device_data->base->cr,
			      CRYP_CRYPEN_ENABLE,
			      CRYP_CRYPEN_POS,
			      CRYP_CRYPEN_MASK);
		CRYP_PUT_BITS(&device_data->base->cr,
			      KSE_ENABLED,
			      CRYP_KSE_POS,
			      CRYP_KSE_MASK);

		cryp_wait_until_done(device_data);

		CRYP_PUT_BITS(&device_data->base->cr,
			      CRYP_CRYPEN_DISABLE,
			      CRYP_CRYPEN_POS,
			      CRYP_CRYPEN_MASK);
	}

	CRYP_PUT_BITS(&device_data->base->cr,
		      CRYP_CRYPEN_ENABLE,
		      CRYP_CRYPEN_POS,
		      CRYP_CRYPEN_MASK);
	CRYP_PUT_BITS(&device_data->base->cr,
		      p_cryp_config->algo_mode,
		      CRYP_ALGOMODE_POS,
		      CRYP_ALGOMODE_MASK);
	CRYP_PUT_BITS(&device_data->base->cr,
		      p_cryp_config->encrypt_or_decrypt,
		      CRYP_ENC_DEC_POS,
		      CRYP_ENC_DEC_MASK);

	return 0;
}

/**
 * cryp_get_configuration - gets the parameter of the control register of IP
 * @device_data: Pointer to the device data struct for base address.
 * @p_cryp_config: Gets the configuration parameter from cryp ip.
 */
int cryp_get_configuration(struct cryp_device_data *device_data,
			   struct cryp_config *p_cryp_config)
{
	if (NULL == p_cryp_config)
		return -EINVAL;

	p_cryp_config->key_access =
		((readl(&device_data->base->cr) & CRYP_KEY_ACCESS_MASK) ?
		 CRYP_STATE_ENABLE :
		 CRYP_STATE_DISABLE);
	p_cryp_config->key_size =
		((readl(&device_data->base->cr) & CRYP_KEY_SIZE_MASK) >>
		 CRYP_KEY_SIZE_POS);

	p_cryp_config->encrypt_or_decrypt =
		((readl(&device_data->base->cr) & CRYP_ENC_DEC_MASK) ?
		 CRYP_ALGORITHM_DECRYPT :
		 CRYP_ALGORITHM_ENCRYPT);

	p_cryp_config->data_type =
		((readl(&device_data->base->cr) & CRYP_DATA_TYPE_MASK) >>
		 CRYP_DATA_TYPE_POS);
	p_cryp_config->algo_mode =
		((readl(&device_data->base->cr) & CRYP_ALGOMODE_MASK) >>
		 CRYP_ALGOMODE_POS);

	return 0;
}

/**
 * cryp_configure_protection - set the protection bits in the CRYP logic.
 * @device_data: Pointer to the device data struct for base address.
 * @p_protect_config:	Pointer to the protection mode and
 *			secure mode configuration
 */
int cryp_configure_protection(struct cryp_device_data *device_data,
			      struct cryp_protection_config *p_protect_config)
{
	if (NULL == p_protect_config)
		return -EINVAL;

	CRYP_WRITE_BIT(&device_data->base->cr,
		       (u32) p_protect_config->secure_access,
		       CRYP_SECURE_MASK);
	CRYP_PUT_BITS(&device_data->base->cr,
		      p_protect_config->privilege_access,
		      CRYP_PRLG_POS,
		      CRYP_PRLG_MASK);

	return 0;
}

/**
 * cryp_is_logic_busy - returns the busy status of the CRYP logic
 * @device_data: Pointer to the device data struct for base address.
 */
int cryp_is_logic_busy(struct cryp_device_data *device_data)
{
	return CRYP_TEST_BITS(&device_data->base->status,
			      CRYP_BUSY_STATUS_MASK);
}

/**
 * cryp_get_status - This routine returns the complete status of the cryp logic
 * @device_data: Pointer to the device data struct for base address.
 */
/*
int cryp_get_status(struct cryp_device_data *device_data)
{
	return (int) readl(device_data->base->status);
}
*/

/**
 * cryp_configure_for_dma - configures the CRYP IP for DMA operation
 * @device_data: Pointer to the device data struct for base address.
 * @dma_req: Specifies the DMA request type value.
 */
void cryp_configure_for_dma(struct cryp_device_data *device_data,
			    enum cryp_dma_req_type dma_req)
{
	CRYP_SET_BITS(&device_data->base->dmacr,
		      (u32) dma_req);
}

/**
 * cryp_configure_key_values - configures the key values for CRYP operations
 * @device_data: Pointer to the device data struct for base address.
 * @key_reg_index: Key value index register
 * @key_value: The key value struct
 */
int cryp_configure_key_values(struct cryp_device_data *device_data,
			      enum cryp_key_reg_index key_reg_index,
			      struct cryp_key_value key_value)
{
	while (cryp_is_logic_busy(device_data))
		cpu_relax();

	switch (key_reg_index) {
	case CRYP_KEY_REG_1:
		writel(key_value.key_value_left,
		       &device_data->base->key_1_l);
		writel(key_value.key_value_right,
		       &device_data->base->key_1_r);
		break;
	case CRYP_KEY_REG_2:
		writel(key_value.key_value_left,
		       &device_data->base->key_2_l);
		writel(key_value.key_value_right,
		       &device_data->base->key_2_r);
		break;
	case CRYP_KEY_REG_3:
		writel(key_value.key_value_left,
		       &device_data->base->key_3_l);
		writel(key_value.key_value_right,
		       &device_data->base->key_3_r);
		break;
	case CRYP_KEY_REG_4:
		writel(key_value.key_value_left,
		       &device_data->base->key_4_l);
		writel(key_value.key_value_right,
		       &device_data->base->key_4_r);
		break;
	default:
		return -EINVAL;
	}

	return 0;

}

/**
 * cryp_configure_init_vector - configures the initialization vector register
 * @device_data: Pointer to the device data struct for base address.
 * @init_vector_index: Specifies the index of the init vector.
 * @init_vector_value: Specifies the value for the init vector.
 */
int cryp_configure_init_vector(struct cryp_device_data *device_data,
			       enum cryp_init_vector_index
			       init_vector_index,
			       struct cryp_init_vector_value
			       init_vector_value)
{
	while (cryp_is_logic_busy(device_data))
		cpu_relax();

	switch (init_vector_index) {
	case CRYP_INIT_VECTOR_INDEX_0:
		writel(init_vector_value.init_value_left,
		       &device_data->base->init_vect_0_l);
		writel(init_vector_value.init_value_right,
		       &device_data->base->init_vect_0_r);
		break;
	case CRYP_INIT_VECTOR_INDEX_1:
		writel(init_vector_value.init_value_left,
		       &device_data->base->init_vect_1_l);
		writel(init_vector_value.init_value_right,
		       &device_data->base->init_vect_1_r);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * cryp_prep_ctx_mgmt - Prepares for handling the context of the block
 * @device_data: Pointer to the device data struct for base address.
 */
static void cryp_prep_ctx_mgmt(struct cryp_device_data *device_data)
{
	cryp_configure_for_dma(device_data, CRYP_DMA_DISABLE_BOTH);
	cryp_activity(device_data, CRYP_CRYPEN_DISABLE);
	cryp_wait_until_done(device_data);
}

/**
 * cryp_save_device_context -	Store hardware registers and
 *				other device context parameter
 * @device_data: Pointer to the device data struct for base address.
 * @ctx: Crypto device context
 */
void cryp_save_device_context(struct cryp_device_data *device_data,
			      struct cryp_device_context *ctx)
{
	struct cryp_register *src_reg = device_data->base;

	cryp_prep_ctx_mgmt(device_data);

	ctx->din = readl(&src_reg->din);

	ctx->dout = readl(&src_reg->dout);

	ctx->cr = readl(&src_reg->cr);
	ctx->dmacr = readl(&src_reg->dmacr);
	ctx->imsc = readl(&src_reg->imsc);

	ctx->key_1_l = readl(&src_reg->key_1_l);
	ctx->key_1_r = readl(&src_reg->key_1_r);
	ctx->key_2_l = readl(&src_reg->key_2_l);
	ctx->key_2_r = readl(&src_reg->key_2_r);
	ctx->key_3_l = readl(&src_reg->key_3_l);
	ctx->key_3_r = readl(&src_reg->key_3_r);
	ctx->key_4_l = readl(&src_reg->key_4_l);
	ctx->key_4_r = readl(&src_reg->key_4_r);

	ctx->init_vect_0_l = readl(&src_reg->init_vect_0_l);
	ctx->init_vect_0_r = readl(&src_reg->init_vect_0_r);
	ctx->init_vect_1_l = readl(&src_reg->init_vect_1_l);
	ctx->init_vect_1_r = readl(&src_reg->init_vect_1_r);
}

/**
 * cryp_restore_device_context -	Restore hardware registers and
 *					other device context parameter
 * @device_data: Pointer to the device data struct for base address.
 * @ctx: Crypto device context
 */
void cryp_restore_device_context(struct cryp_device_data *device_data,
				 struct cryp_device_context *ctx)
{
	struct cryp_register *reg = device_data->base;

	cryp_prep_ctx_mgmt(device_data);

	writel(ctx->din, &reg->din);
	writel(ctx->dout, &reg->dout);
	writel(ctx->cr, &reg->cr);
	writel(ctx->dmacr, &reg->dmacr);
	writel(ctx->imsc, &reg->imsc);
	writel(ctx->key_1_l, &reg->key_1_l);
	writel(ctx->key_1_r, &reg->key_1_r);
	writel(ctx->key_2_l, &reg->key_2_l);
	writel(ctx->key_2_r, &reg->key_2_r);
	writel(ctx->key_3_l, &reg->key_3_l);
	writel(ctx->key_3_r, &reg->key_3_r);
	writel(ctx->key_4_l, &reg->key_4_l);
	writel(ctx->key_4_r, &reg->key_4_r);
	writel(ctx->init_vect_0_l, &reg->init_vect_0_l);
	writel(ctx->init_vect_0_r, &reg->init_vect_0_r);
	writel(ctx->init_vect_1_l, &reg->init_vect_1_l);
	writel(ctx->init_vect_1_r, &reg->init_vect_1_r);
}

/**
 * cryp_write_indata - This routine writes 32 bit data into the data input
 *		       register of the cryptography IP.
 * @device_data: Pointer to the device data struct for base address.
 * @write_data: Data word to write
 */
int cryp_write_indata(struct cryp_device_data *device_data, u32 write_data)
{
	if (NULL == device_data)
		return -EINVAL;
	writel(write_data, &device_data->base->din);

	return 0;
}

/**
 * cryp_read_indata - This routine reads the 32 bit data from the data input
 *		      register into the specified location.
 * @device_data: Pointer to the device data struct for base address.
 * @p_read_data: Read the data from the input FIFO.
 */
int cryp_read_indata(struct cryp_device_data *device_data, u32 *p_read_data)
{
	if (NULL == device_data)
		return -EINVAL;
	if (NULL == p_read_data)
		return -EINVAL;

	*p_read_data = readl(&device_data->base->din);

	return 0;
}

/**
 * cryp_read_outdata - This routine reads the data from the data output
 *		       register of the CRYP logic
 * @device_data: Pointer to the device data struct for base address.
 * @read_data: Read the data from the output FIFO.
 */
int cryp_read_outdata(struct cryp_device_data *device_data, u32 *read_data)
{
	if (NULL == device_data)
		return -EINVAL;
	if (NULL == read_data)
		return -EINVAL;

	*read_data = readl(&device_data->base->dout);

	return 0;
}

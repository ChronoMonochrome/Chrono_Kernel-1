/*
 * Cryptographic API.
 * Support for Nomadik hardware crypto engine.

 * Copyright (C) ST-Ericsson SA 2010
 * Author: Shujuan Chen <shujuan.chen@stericsson.com> for ST-Ericsson
 * Author: Joakim Bech <joakim.xx.bech@stericsson.com> for ST-Ericsson
 * Author: Berne Hebark <berne.herbark@stericsson.com> for ST-Ericsson.
 * Author: Niklas Hernaeus <niklas.hernaeus@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/klist.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/crypto.h>

#include <mach/regulator.h>
#include <linux/bitops.h>

#include <crypto/internal/hash.h>
#include <crypto/sha.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>

#include <mach/hardware.h>

#include "hash_alg.h"

#define DEV_DBG_NAME "hashX hashX:"

/**
 * struct hash_driver_data - data specific to the driver.
 *
 * @device_list:	A list of registered devices to choose from.
 * @device_allocation:	A semaphore initialized with number of devices.
 */
struct hash_driver_data {
	struct klist		device_list;
	struct semaphore	device_allocation;
};

static struct hash_driver_data	driver_data;

/* Declaration of functions */
/**
 * hash_messagepad - Pads a message and write the nblw bits.
 * @device_data:	Structure for the hash device.
 * @message:		Last word of a message
 * @index_bytes:	The number of bytes in the last message
 *
 * This function manages the final part of the digest calculation, when less
 * than 512 bits (64 bytes) remain in message. This means index_bytes < 64.
 *
 * Reentrancy: Non Re-entrant.
 */
static void hash_messagepad(struct hash_device_data *device_data,
		const u32 *message, u8 index_bytes);

/**
 * hash_disable_power - Request to disable power and clock.
 * @device_data:	Structure for the hash device.
 * @save_device_state:	If true, saves the current hw state.
 *
 * This function request for disabling power (regulator) and clock,
 * and could also save current hw state.
 */
static int hash_disable_power(
		struct hash_device_data *device_data,
		bool			save_device_state)
{
	int ret = 0;
	struct device *dev = device_data->dev;

	dev_dbg(dev, "[%s]", __func__);

	spin_lock(&device_data->power_state_lock);
	if (!device_data->power_state)
		goto out;

	if (save_device_state && device_data->current_ctx) {
		hash_save_state(device_data,
				&device_data->current_ctx->state);
		device_data->restore_dev_state = true;
	}

	clk_disable(device_data->clk);
	ret = ux500_regulator_atomic_disable(device_data->regulator);
	if (ret)
		dev_err(dev, "[%s] regulator_disable() failed!", __func__);

	device_data->power_state = false;

out:
	spin_unlock(&device_data->power_state_lock);

	return ret;
}

/**
 * hash_enable_power - Request to enable power and clock.
 * @device_data:		Structure for the hash device.
 * @restore_device_state:	If true, restores a previous saved hw state.
 *
 * This function request for enabling power (regulator) and clock,
 * and could also restore a previously saved hw state.
 */
static int hash_enable_power(
		struct hash_device_data *device_data,
		bool			restore_device_state)
{
	int ret = 0;
	struct device *dev = device_data->dev;
	dev_dbg(dev, "[%s]", __func__);

	spin_lock(&device_data->power_state_lock);
	if (!device_data->power_state) {
		ret = ux500_regulator_atomic_enable(device_data->regulator);
		if (ret) {
			dev_err(dev, "[%s]: regulator_enable() failed!",
					__func__);
			goto out;
		}
		ret = clk_enable(device_data->clk);
		if (ret) {
			dev_err(dev, "[%s]: clk_enable() failed!",
					__func__);
			ret = ux500_regulator_atomic_disable(
					device_data->regulator);
			goto out;
		}
		device_data->power_state = true;
	}

	if (device_data->restore_dev_state) {
		if (restore_device_state) {
			device_data->restore_dev_state = false;
			hash_resume_state(device_data,
				&device_data->current_ctx->state);
		}
	}
out:
	spin_unlock(&device_data->power_state_lock);

	return ret;
}

/**
 * hash_get_device_data - Checks for an available hash device and return it.
 * @hash_ctx:		Structure for the hash context.
 * @device_data:	Structure for the hash device.
 *
 * This function check for an available hash device and return it to
 * the caller.
 * Note! Caller need to release the device, calling up().
 */
static int hash_get_device_data(struct hash_ctx *ctx,
				struct hash_device_data **device_data)
{
	int			ret;
	struct klist_iter	device_iterator;
	struct klist_node	*device_node;
	struct hash_device_data *local_device_data = NULL;

	pr_debug(DEV_DBG_NAME " [%s]", __func__);

	/* Wait until a device is available */
	ret = down_interruptible(&driver_data.device_allocation);
	if (ret)
		return ret;  /* Interrupted */

	/* Select a device */
	klist_iter_init(&driver_data.device_list, &device_iterator);
	device_node = klist_next(&device_iterator);
	while (device_node) {
		local_device_data = container_of(device_node,
					   struct hash_device_data, list_node);
		spin_lock(&local_device_data->ctx_lock);
		/* current_ctx allocates a device, NULL = unallocated */
		if (local_device_data->current_ctx) {
			device_node = klist_next(&device_iterator);
		} else {
			local_device_data->current_ctx = ctx;
			ctx->device = local_device_data;
			spin_unlock(&local_device_data->ctx_lock);
			break;
		}
		spin_unlock(&local_device_data->ctx_lock);
	}
	klist_iter_exit(&device_iterator);

	if (!device_node) {
		/**
		 * No free device found.
		 * Since we allocated a device with down_interruptible, this
		 * should not be able to happen.
		 * Number of available devices, which are contained in
		 * device_allocation, is therefore decremented by not doing
		 * an up(device_allocation).
		 */
		return -EBUSY;
	}

	*device_data = local_device_data;

	return 0;
}

/**
 * init_hash_hw - Initialise the hash hardware for a new calculation.
 * @device_data:	Structure for the hash device.
 * @req:		The hash request for the job.
 *
 * This function will enable the bits needed to clear and start a new
 * calculation.
 */
static int init_hash_hw(struct hash_device_data *device_data,
		struct ahash_request *req)
{
	int ret = 0;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct hash_ctx *ctx = crypto_ahash_ctx(tfm);

	dev_dbg(device_data->dev, "[%s] (ctx=0x%x)!", __func__, (u32)ctx);

	ret = hash_setconfiguration(device_data, &ctx->config);
	if (ret) {
		dev_err(device_data->dev, "[%s] hash_setconfiguration() "
				"failed!", __func__);
		return ret;
	}

	hash_begin(device_data, ctx);

	return ret;
}

/**
 * hash_init - Common hash init function for SHA1/SHA2 (SHA256).
 * @req: The hash request for the job.
 *
 * Initialize structures.
 */
static int hash_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct hash_ctx *ctx = crypto_ahash_ctx(tfm);

	pr_debug(DEV_DBG_NAME "[%s] (ctx=0x%x)!", __func__, (u32)ctx);

	memset(&ctx->state, 0, sizeof(struct hash_state));
	ctx->updated = 0;
	return 0;
}

/**
 * hash_processblock - This function processes a single block of 512 bits (64
 *                     bytes), word aligned, starting at message.
 * @device_data:	Structure for the hash device.
 * @message:		Block (512 bits) of message to be written to
 *			the HASH hardware.
 *
 * Reentrancy: Non Re-entrant.
 */
static void hash_processblock(
		struct hash_device_data *device_data,
		const u32 *message)
{
	u32 count;

	/*
	 * NBLW bits. Reset the number of bits in last word (NBLW).
	 */
	HASH_CLEAR_BITS(&device_data->base->str, HASH_STR_NBLW_MASK);

	/*
	 * Write message data to the HASH_DIN register.
	 */
	for (count = 0; count < (HASH_BLOCK_SIZE / sizeof(u32)); count += 4) {
		HASH_SET_DIN(message[0]);
		HASH_SET_DIN(message[1]);
		HASH_SET_DIN(message[2]);
		HASH_SET_DIN(message[3]);
		message += 4;
	}
}

/**
 * hash_messagepad - Pads a message and write the nblw bits.
 * @device_data:	Structure for the hash device.
 * @message:		Last word of a message.
 * @index_bytes:	The number of bytes in the last message.
 *
 * This function manages the final part of the digest calculation, when less
 * than 512 bits (64 bytes) remain in message. This means index_bytes < 64.
 *
 * Reentrancy: Non Re-entrant.
 */
static void hash_messagepad(struct hash_device_data *device_data,
		const u32 *message, u8 index_bytes)
{
	dev_dbg(device_data->dev, "[%s] (bytes in final msg=%d))",
			__func__, index_bytes);
	/*
	 * Clear hash str register, only clear NBLW
	 * since DCAL will be reset by hardware.
	 */
	writel((readl(&device_data->base->str) & ~HASH_STR_NBLW_MASK),
			&device_data->base->str);

	/* Main loop */
	while (index_bytes >= 4) {
		HASH_SET_DIN(message[0]);
		index_bytes -= 4;
		message++;
	}

	if (index_bytes)
		HASH_SET_DIN(message[0]);

	while (device_data->base->str & HASH_STR_DCAL_MASK)
		cpu_relax();

	/* num_of_bytes == 0 => NBLW <- 0 (32 bits valid in DATAIN) */
	HASH_SET_NBLW(index_bytes * 8);
	dev_dbg(device_data->dev, "[%s] DIN=0x%08x NBLW=%d", __func__,
			readl(&device_data->base->din),
			readl(&device_data->base->str));
	HASH_SET_DCAL;
	dev_dbg(device_data->dev, "[%s] after dcal -> DIN=0x%08x NBLW=%d",
			__func__, readl(&device_data->base->din),
			readl(&device_data->base->str));

	while (device_data->base->str & HASH_STR_DCAL_MASK)
		cpu_relax();
}

/**
 * hash_incrementlength - Increments the length of the current message.
 * @ctx: Hash context
 * @incr: Length of message processed already
 *
 * Overflow cannot occur, because conditions for overflow are checked in
 * hash_hw_update.
 */
static void hash_incrementlength(struct hash_ctx *ctx, u32 incr)
{
	ctx->state.length.low_word += incr;

	/* Check for wrap-around */
	if (ctx->state.length.low_word < incr)
		ctx->state.length.high_word++;
}

/**
 * hash_setconfiguration - Sets the required configuration for the hash
 *                         hardware.
 * @device_data:	Structure for the hash device.
 * @config:		Pointer to a configuration structure.
 *
 * Reentrancy: Non Re-entrant
 * Reentrancy issues:
 *              1. Global variable registry(cofiguration register,
 *                 parameter register, divider register) is being modified
 *
 * Comments     1.  : User need to call hash_begin API after calling this
 *                    API i.e. the current configuration is set only when
 *                    bit INIT is set and we set INIT bit in hash_begin.
 *                    Changing the configuration during a computation has
 *                    no effect so we first set configuration by calling
 *                    this API and then set the INIT bit for the HASH
 *                    processor and the curent configuration is taken into
 *                    account. As reading INIT bit (with correct protection
 *                    rights) will always return 0b so we can't make a check
 *                    at software level. So the user has to initialize the
 *                    device for new configuration to take in to effect.
 *              2.    The default value of data format is 00b ie the format
 *                    of data entered in HASH_DIN register is 32-bit data.
 *                    The data written in HASH_DIN is used directly by the
 *                    HASH processing, without re ordering.
 */
int hash_setconfiguration(struct hash_device_data *device_data,
		struct hash_config *config)
{
	int ret = 0;
	dev_dbg(device_data->dev, "[%s] ", __func__);

	if (config->algorithm != HASH_ALGO_SHA1 &&
	    config->algorithm != HASH_ALGO_SHA256)
		return -EPERM;

	/*
	 * DATAFORM bits. Set the DATAFORM bits to 0b11, which means the data
	 * to be written to HASH_DIN is considered as 32 bits.
	 */
	HASH_SET_DATA_FORMAT(config->data_format);

	/*
	 * Empty message bit. This bit is needed when the hash input data
	 * contain the empty message. Always set in current impl. but with
	 * no impact on data different than empty message.
	 */
	HASH_SET_BITS(&device_data->base->cr, HASH_CR_EMPTYMSG_MASK);

	/*
	 * ALGO bit. Set to 0b1 for SHA-1 and 0b0 for SHA-256
	 */
	switch (config->algorithm) {
	case HASH_ALGO_SHA1:
		HASH_SET_BITS(&device_data->base->cr, HASH_CR_ALGO_MASK);
		break;

	case HASH_ALGO_SHA256:
		HASH_CLEAR_BITS(&device_data->base->cr, HASH_CR_ALGO_MASK);
		break;

	default:
		dev_err(device_data->dev, "[%s] Incorrect algorithm.",
				__func__);
		return -EPERM;
	}

	/*
	 * MODE bit. This bit selects between HASH or HMAC mode for the
	 * selected algorithm. 0b0 = HASH and 0b1 = HMAC.
	 */
	if (HASH_OPER_MODE_HASH == config->oper_mode) {
		HASH_CLEAR_BITS(&device_data->base->cr,
				HASH_CR_MODE_MASK);
	} else {	/* HMAC mode or wrong hash mode */
		ret = -EPERM;
		dev_err(device_data->dev, "[%s] HASH_INVALID_PARAMETER!",
				__func__);
	}
	return ret;
}

/**
 * hash_begin - This routine resets some globals and initializes the hash
 *              hardware.
 * @device_data:	Structure for the hash device.
 * @ctx:		Hash context.
 *
 * Reentrancy: Non Re-entrant
 *
 * Comments     1.  : User need to call hash_setconfiguration API before
 *                    calling this API i.e. the current configuration is set
 *                    only when bit INIT is set and we set INIT bit in
 *                    hash_begin. Changing the configuration during a
 *                    computation has no effect so we first set
 *                    configuration by calling this API and then set the
 *                    INIT bit for the HASH processor and the current
 *                    configuration is taken into account. As reading INIT
 *                    bit (with correct protection rights) will always
 *                    return 0b so we can't make a check at software level.
 *                    So the user has to initialize the device for new
 *                    configuration to take in to effect.
 */
void hash_begin(struct hash_device_data *device_data, struct hash_ctx *ctx)
{
	/* HW and SW initializations */
	/* Note: there is no need to initialize buffer and digest members */
	dev_dbg(device_data->dev, "[%s] ", __func__);

	while (device_data->base->str & HASH_STR_DCAL_MASK)
		cpu_relax();

	/*
	 * INIT bit. Set this bit to 0b1 to reset the HASH processor core and
	 * prepare the initialize the HASH accelerator to compute the message
	 * digest of a new message.
	 */
	HASH_INITIALIZE;

	/*
	 * NBLW bits. Reset the number of bits in last word (NBLW).
	 */
	HASH_CLEAR_BITS(&device_data->base->str, HASH_STR_NBLW_MASK);
}

/**
 * hash_hw_update - Updates current HASH computation hashing another part of
 *                  the message.
 * @req:	Byte array containing the message to be hashed (caller
 *		allocated).
 *
 * Reentrancy: Non Re-entrant
 */
int hash_hw_update(struct ahash_request *req)
{
	int ret = 0;
	u8 index;
	u32 count;
	u8 *p_buffer;
	struct hash_device_data *device_data;
	u8 *p_data_buffer;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct crypto_hash_walk walk;
	int msg_length = crypto_hash_walk_first(req, &walk);

	pr_debug(DEV_DBG_NAME "[%s] ", __func__);

	if (msg_length == 0)
		return -EPERM;

	index = ctx->state.index;
	p_buffer = (u8 *)ctx->state.buffer;

	/* Check if ctx->state.length + msg_length
	   overflows */
	if (msg_length >
	    (ctx->state.length.low_word + msg_length)
	    && HASH_HIGH_WORD_MAX_VAL ==
	    (ctx->state.length.high_word)) {
		dev_err(device_data->dev, "[%s] HASH_MSG_LENGTH_OVERFLOW!",
				__func__);
		return -EPERM;
	}

	ret = hash_get_device_data(ctx, &device_data);
	if (ret)
		return ret;

	/* Enable device power (and clock) */
	ret = hash_enable_power(device_data, false);
	if (ret) {
		dev_err(device_data->dev, "[%s]: "
				"hash_enable_power() failed!", __func__);
		goto out;
	}

	/* Main loop */
	while (0 != msg_length) {
		p_data_buffer = walk.data;
		if ((index + msg_length) < HASH_BLOCK_SIZE) {
			for (count = 0; count < msg_length; count++) {
				p_buffer[index + count] =
				    *(p_data_buffer + count);
			}

			index += msg_length;
		} else {
			if (!ctx->updated) {
				ret = init_hash_hw(device_data, req);
				if (ret) {
					dev_err(device_data->dev, "[%s] "
						"init_hash_hw() failed!",
						__func__);
					goto out;
				}
				ctx->updated = 1;
			} else {
				ret = hash_resume_state(device_data,
						&ctx->state);
				if (ret) {
					dev_err(device_data->dev, "[%s] "
						"hash_resume_state() failed!",
						__func__);
					goto out_power;
				}
			}

			/*
			 * If 'p_data_buffer' is four byte aligned and local
			 * buffer does not have any data, we can write data
			 * directly from 'p_data_buffer' to HW peripheral,
			 * otherwise we first copy data to a local buffer
			 */
			if ((0 == (((u32) p_data_buffer) % 4))
					&& (0 == index)) {
				hash_processblock(device_data,
						(const u32 *)p_data_buffer);
			} else {
				for (count = 0;
				     count < (u32)(HASH_BLOCK_SIZE - index);
				     count++) {
					p_buffer[index + count] =
					    *(p_data_buffer + count);
				}

				hash_processblock(device_data,
						(const u32 *)p_buffer);
			}

			hash_incrementlength(ctx, HASH_BLOCK_SIZE);
			index = 0;

			ret = hash_save_state(device_data, &ctx->state);
			if (ret) {
				dev_err(device_data->dev, "[%s] "
					"hash_save_state() failed!", __func__);
				goto out_power;
			}
		}
		msg_length = crypto_hash_walk_done(&walk, 0);
	}

	ctx->state.index = index;

	dev_dbg(device_data->dev, "[%s] END(msg_length=%d in bits, in=%d, "
		"bin=%d))", __func__, msg_length, ctx->state.index,
		ctx->state.bit_index);
out_power:
	/* Disable power (and clock) */
	if (hash_disable_power(device_data, false))
		dev_err(device_data->dev, "[%s]: "
				"hash_disable_power() failed!", __func__);
out:
	spin_lock(&device_data->ctx_lock);
	device_data->current_ctx = NULL;
	ctx->device = NULL;
	spin_unlock(&device_data->ctx_lock);

	/*
	 * The down_interruptible part for this semaphore is called in
	 * cryp_get_device_data.
	 */
	up(&driver_data.device_allocation);

	return ret;
}

/**
 * hash_resume_state - Function that resumes the state of an calculation.
 * @device_data:	Pointer to the device structure.
 * @device_state:	The state to be restored in the hash hardware
 *
 * Reentrancy: Non Re-entrant
 */
int hash_resume_state(struct hash_device_data *device_data,
		const struct hash_state *device_state)
{
	u32 temp_cr;
	s32 count;
	int hash_mode = HASH_OPER_MODE_HASH;

	dev_dbg(device_data->dev, "[%s] (state(0x%x)))",
			__func__, (u32) device_state);

	if (NULL == device_state) {
		dev_err(device_data->dev, "[%s] HASH_INVALID_PARAMETER!",
				__func__);
		return -EPERM;
	}

	/* Check correctness of index and length members */
	if (device_state->index > HASH_BLOCK_SIZE
	    || (device_state->length.low_word % HASH_BLOCK_SIZE) != 0) {
		dev_err(device_data->dev, "[%s] HASH_INVALID_PARAMETER!",
				__func__);
		return -EPERM;
	}

	/*
	 * INIT bit. Set this bit to 0b1 to reset the HASH processor core and
	 * prepare the initialize the HASH accelerator to compute the message
	 * digest of a new message.
	 */
	HASH_INITIALIZE;

	temp_cr = device_state->temp_cr;
	writel(temp_cr & HASH_CR_RESUME_MASK, &device_data->base->cr);

	if (device_data->base->cr & HASH_CR_MODE_MASK)
		hash_mode = HASH_OPER_MODE_HMAC;
	else
		hash_mode = HASH_OPER_MODE_HASH;

	for (count = 0; count < HASH_CSR_COUNT; count++) {
		if ((count >= 36) && (hash_mode == HASH_OPER_MODE_HASH))
			break;

		writel(device_state->csr[count],
				&device_data->base->csrx[count]);
	}

	writel(device_state->csfull, &device_data->base->csfull);
	writel(device_state->csdatain, &device_data->base->csdatain);

	writel(device_state->str_reg, &device_data->base->str);
	writel(temp_cr, &device_data->base->cr);

	return 0;
}

/**
 * hash_save_state - Function that saves the state of hardware.
 * @device_data:	Pointer to the device structure.
 * @device_state:	The strucure where the hardware state should be saved.
 *
 * Reentrancy: Non Re-entrant
 */
int hash_save_state(struct hash_device_data *device_data,
		struct hash_state *device_state)
{
	u32 temp_cr;
	u32 count;
	int hash_mode = HASH_OPER_MODE_HASH;

	dev_dbg(device_data->dev, "[%s] state(0x%x)))",
		 __func__, (u32) device_state);

	if (NULL == device_state) {
		dev_err(device_data->dev, "[%s] HASH_INVALID_PARAMETER!",
				__func__);
		return -EPERM;
	}

	/* Write dummy value to force digest intermediate calculation. This
	 * actually makes sure that there isn't any ongoing calculation in the
	 * hardware.
	 */
	while (device_data->base->str & HASH_STR_DCAL_MASK)
		cpu_relax();

	temp_cr = readl(&device_data->base->cr);

	device_state->str_reg = readl(&device_data->base->str);

	device_state->din_reg = readl(&device_data->base->din);

	if (device_data->base->cr & HASH_CR_MODE_MASK)
		hash_mode = HASH_OPER_MODE_HMAC;
	else
		hash_mode = HASH_OPER_MODE_HASH;

	for (count = 0; count < HASH_CSR_COUNT; count++) {
		if ((count >= 36) && (hash_mode == HASH_OPER_MODE_HASH))
			break;

		device_state->csr[count] =
			readl(&device_data->base->csrx[count]);
	}

	device_state->csfull = readl(&device_data->base->csfull);
	device_state->csdatain = readl(&device_data->base->csdatain);

	device_state->temp_cr = temp_cr;

	return 0;
}

/**
 * hash_check_hw - This routine checks for peripheral Ids and PCell Ids.
 * @device_data:
 *
 */
int hash_check_hw(struct hash_device_data *device_data)
{
	int ret = 0;

	dev_dbg(device_data->dev, "[%s] ", __func__);

	if (NULL == device_data) {
		ret = -EPERM;
		dev_err(device_data->dev, "[%s] HASH_INVALID_PARAMETER!",
			__func__);
		goto out;
	}

	/* Checking Peripheral Ids  */
	if ((HASH_P_ID0 == readl(&device_data->base->periphid0))
		&& (HASH_P_ID1 == readl(&device_data->base->periphid1))
		&& (HASH_P_ID2 == readl(&device_data->base->periphid2))
		&& (HASH_P_ID3 == readl(&device_data->base->periphid3))
		&& (HASH_CELL_ID0 == readl(&device_data->base->cellid0))
		&& (HASH_CELL_ID1 == readl(&device_data->base->cellid1))
		&& (HASH_CELL_ID2 == readl(&device_data->base->cellid2))
		&& (HASH_CELL_ID3 == readl(&device_data->base->cellid3))
	   ) {
		ret = 0;
		goto out;;
	} else {
		ret = -EPERM;
		dev_err(device_data->dev, "[%s] HASH_UNSUPPORTED_HW!",
				__func__);
		goto out;
	}
out:
	return ret;
}

/**
 * hash_get_digest - Gets the digest.
 * @device_data:	Pointer to the device structure.
 * @digest:		User allocated byte array for the calculated digest.
 * @algorithm:		The algorithm in use.
 *
 * Reentrancy: Non Re-entrant, global variable registry (hash control register)
 * is being modified.
 *
 * Note that, if this is called before the final message has been handle it
 * will return the intermediate message digest.
 */
void hash_get_digest(struct hash_device_data *device_data,
		u8 *digest, int algorithm)
{
	u32 temp_hx_val, count;
	int loop_ctr;

	if (algorithm != HASH_ALGO_SHA1 && algorithm != HASH_ALGO_SHA256) {
		dev_err(device_data->dev, "[%s] Incorrect algorithm %d",
				__func__, algorithm);
		return;
	}

	if (algorithm == HASH_ALGO_SHA1)
		loop_ctr = HASH_SHA1_DIGEST_SIZE / sizeof(u32);
	else
		loop_ctr = HASH_SHA2_DIGEST_SIZE / sizeof(u32);

	dev_dbg(device_data->dev, "[%s] digest array:(0x%x)",
			__func__, (u32) digest);

	/* Copy result into digest array */
	for (count = 0; count < loop_ctr; count++) {
		temp_hx_val = readl(&device_data->base->hx[count]);
		digest[count * 4] = (u8) ((temp_hx_val >> 24) & 0xFF);
		digest[count * 4 + 1] = (u8) ((temp_hx_val >> 16) & 0xFF);
		digest[count * 4 + 2] = (u8) ((temp_hx_val >> 8) & 0xFF);
		digest[count * 4 + 3] = (u8) ((temp_hx_val >> 0) & 0xFF);
	}
}

/**
 * hash_update - The hash update function for SHA1/SHA2 (SHA256).
 * @req: The hash request for the job.
 */
static int ahash_update(struct ahash_request *req)
{
	int ret = 0;

	pr_debug(DEV_DBG_NAME "[%s] ", __func__);

	ret = hash_hw_update(req);
	if (ret) {
		pr_err(DEV_DBG_NAME "[%s] hash_hw_update() failed!", __func__);
		goto out;
	}

out:
	return ret;
}

/**
 * hash_final - The hash final function for SHA1/SHA2 (SHA256).
 * @req:	The hash request for the job.
 */
static int ahash_final(struct ahash_request *req)
{
	int ret = 0;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct hash_ctx *ctx = crypto_ahash_ctx(tfm);
	struct hash_device_data *device_data;
	u8 digest[HASH_MSG_DIGEST_SIZE];

	pr_debug(DEV_DBG_NAME "[%s] ", __func__);
	ret = hash_get_device_data(ctx, &device_data);
	if (ret)
		return ret;

	dev_dbg(device_data->dev, "[%s] (ctx=0x%x)!", __func__, (u32) ctx);

	/* Enable device power (and clock) */
	ret = hash_enable_power(device_data, false);
	if (ret) {
		dev_err(device_data->dev, "[%s]: "
				"hash_enable_power() failed!", __func__);
		goto out;
	}

	if (!ctx->updated) {
		ret = init_hash_hw(device_data, req);
		if (ret) {
			dev_err(device_data->dev, "[%s] init_hash_hw() "
					"failed!", __func__);
			goto out_power;
		}
	} else {
		ret = hash_resume_state(device_data, &ctx->state);

		if (ret) {
			dev_err(device_data->dev, "[%s] hash_resume_state() "
					"failed!", __func__);
			goto out_power;
		}
	}

	hash_messagepad(device_data, ctx->state.buffer,
			ctx->state.index);

	hash_get_digest(device_data, digest, ctx->config.algorithm);
	memcpy(req->result, digest, ctx->digestsize);

out_power:
	/* Disable power (and clock) */
	if (hash_disable_power(device_data, false))
		dev_err(device_data->dev, "[%s] hash_disable_power() failed!",
				__func__);

out:
	spin_lock(&device_data->ctx_lock);
	device_data->current_ctx = NULL;
	ctx->device = NULL;
	spin_unlock(&device_data->ctx_lock);

	/*
	 * The down_interruptible part for this semaphore is called in
	 * cryp_get_device_data.
	 */
	up(&driver_data.device_allocation);

	return ret;
}

static int ahash_sha1_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct hash_ctx *ctx = crypto_ahash_ctx(tfm);

	pr_debug(DEV_DBG_NAME "[%s]: (ctx=0x%x)!", __func__, (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA1;
	ctx->config.oper_mode = HASH_OPER_MODE_HASH;
	ctx->digestsize = SHA1_DIGEST_SIZE;

	return hash_init(req);
}

static int ahash_sha256_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct hash_ctx *ctx = crypto_ahash_ctx(tfm);

	pr_debug(DEV_DBG_NAME "[%s]: (ctx=0x%x)!", __func__, (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA256;
	ctx->config.oper_mode = HASH_OPER_MODE_HASH;
	ctx->digestsize = SHA256_DIGEST_SIZE;

	return hash_init(req);
}

static int ahash_sha1_digest(struct ahash_request *req)
{
	int ret2, ret1 = ahash_sha1_init(req);

	if (ret1)
		goto out;

	ret1 = ahash_update(req);
	ret2 = ahash_final(req);

out:
	return ret1 ? ret1 : ret2;
}

static int ahash_sha256_digest(struct ahash_request *req)
{
	int ret2, ret1 = ahash_sha256_init(req);

	if (ret1)
		goto out;

	ret1 = ahash_update(req);
	ret2 = ahash_final(req);

out:
	return ret1 ? ret1 : ret2;
}

static struct ahash_alg ahash_sha1_alg = {
	.init			 = ahash_sha1_init,
	.update			 = ahash_update,
	.final			 = ahash_final,
	.digest			 = ahash_sha1_digest,
	.halg.digestsize	 = SHA1_DIGEST_SIZE,
	.halg.statesize		 = sizeof(struct hash_ctx),
	.halg.base = {
		.cra_name	 = "sha1",
		.cra_driver_name = "sha1-u8500",
		.cra_flags	 = CRYPTO_ALG_TYPE_AHASH | CRYPTO_ALG_ASYNC,
		.cra_blocksize	 = SHA1_BLOCK_SIZE,
		.cra_ctxsize	 = sizeof(struct hash_ctx),
		.cra_module	 = THIS_MODULE,
	}
};

static struct ahash_alg ahash_sha256_alg = {
	.init			 = ahash_sha256_init,
	.update			 = ahash_update,
	.final			 = ahash_final,
	.digest			 = ahash_sha256_digest,
	.halg.digestsize	 = SHA256_DIGEST_SIZE,
	.halg.statesize		 = sizeof(struct hash_ctx),
	.halg.base = {
		.cra_name        = "sha256",
		.cra_driver_name = "sha256-u8500",
		.cra_flags       = CRYPTO_ALG_TYPE_AHASH | CRYPTO_ALG_ASYNC,
		.cra_blocksize   = SHA256_BLOCK_SIZE,
		.cra_ctxsize	 = sizeof(struct hash_ctx),
		.cra_type	 = &crypto_ahash_type,
		.cra_module      = THIS_MODULE,
	}
};

/**
 * struct hash_alg *u8500_hash_algs[] -
 */
static struct ahash_alg *u8500_ahash_algs[] = {
	&ahash_sha1_alg,
	&ahash_sha256_alg
};

/**
 * hash_algs_register_all -
 */
static int ahash_algs_register_all(void)
{
	int ret;
	int i;
	int count;

	pr_debug("[%s]", __func__);

	for (i = 0; i < ARRAY_SIZE(u8500_ahash_algs); i++) {
		ret = crypto_register_ahash(u8500_ahash_algs[i]);
		if (ret) {
			count = i;
			pr_err("[%s] alg registration failed",
				u8500_ahash_algs[i]->halg.base.cra_driver_name);
			goto unreg;
		}
	}
	return 0;
unreg:
	for (i = 0; i < count; i++)
		crypto_unregister_ahash(u8500_ahash_algs[i]);
	return ret;
}

/**
 * hash_algs_unregister_all -
 */
static void ahash_algs_unregister_all(void)
{
	int i;

	pr_debug(DEV_DBG_NAME " [%s]", __func__);

	for (i = 0; i < ARRAY_SIZE(u8500_ahash_algs); i++)
		crypto_unregister_ahash(u8500_ahash_algs[i]);
}

/**
 * u8500_hash_probe - Function that probes the hash hardware.
 * @pdev: The platform device.
 */
static int u8500_hash_probe(struct platform_device *pdev)
{
	int			ret = 0;
	struct resource		*res = NULL;
	struct hash_device_data *device_data;
	struct device		*dev = &pdev->dev;

	dev_dbg(dev, "[%s] (pdev=0x%x)", __func__, (u32) pdev);
	device_data = kzalloc(sizeof(struct hash_device_data), GFP_ATOMIC);
	if (!device_data) {
		dev_dbg(dev, "[%s] kzalloc() failed!", __func__);
		ret = -ENOMEM;
		goto out;
	}

	device_data->dev = dev;
	device_data->current_ctx = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(dev, "[%s] platform_get_resource() failed!", __func__);
		ret = -ENODEV;
		goto out_kfree;
	}

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (res == NULL) {
		dev_dbg(dev, "[%s] request_mem_region() failed!", __func__);
		ret = -EBUSY;
		goto out_kfree;
	}

	device_data->base = ioremap(res->start, resource_size(res));
	if (!device_data->base) {
		dev_err(dev, "[%s] ioremap() failed!",
				__func__);
		ret = -ENOMEM;
		goto out_free_mem;
	}
	spin_lock_init(&device_data->ctx_lock);
	spin_lock_init(&device_data->power_state_lock);

	/* Enable power for HASH1 hardware block */
	device_data->regulator = ux500_regulator_get(dev);

	if (IS_ERR(device_data->regulator)) {
		dev_err(dev, "[%s] regulator_get() failed!", __func__);
		ret = PTR_ERR(device_data->regulator);
		device_data->regulator = NULL;
		goto out_unmap;
	}

	/* Enable the clock for HASH1 hardware block */
	device_data->clk = clk_get(dev, NULL);
	if (IS_ERR(device_data->clk)) {
		dev_err(dev, "[%s] clk_get() failed!", __func__);
		ret = PTR_ERR(device_data->clk);
		goto out_regulator;
	}

	/* Enable device power (and clock) */
	ret = hash_enable_power(device_data, false);
	if (ret) {
		dev_err(dev, "[%s]: hash_enable_power() failed!", __func__);
		goto out_clk;
	}

	ret = hash_check_hw(device_data);
	if (ret) {
		dev_err(dev, "[%s] hash_check_hw() failed!", __func__);
		goto out_power;
	}

	platform_set_drvdata(pdev, device_data);

	/* Put the new device into the device list... */
	klist_add_tail(&device_data->list_node, &driver_data.device_list);
	/* ... and signal that a new device is available. */
	up(&driver_data.device_allocation);

	ret = ahash_algs_register_all();
	if (ret) {
		dev_err(dev, "[%s] ahash_algs_register_all() "
				"failed!", __func__);
		goto out_power;
	}

	if (hash_disable_power(device_data, false))
		dev_err(dev, "[%s]: hash_disable_power() failed!", __func__);

	dev_info(dev, "[%s] successfully probed", __func__);
	return 0;

out_power:
	hash_disable_power(device_data, false);

out_clk:
	clk_put(device_data->clk);

out_regulator:
	ux500_regulator_put(device_data->regulator);

out_unmap:
	iounmap(device_data->base);

out_free_mem:
	release_mem_region(res->start, resource_size(res));

out_kfree:
	kfree(device_data);
out:
	return ret;
}

/**
 * u8500_hash_remove - Function that removes the hash device from the platform.
 * @pdev: The platform device.
 */
static int u8500_hash_remove(struct platform_device *pdev)
{
	struct resource		*res;
	struct hash_device_data *device_data;
	struct device		*dev = &pdev->dev;

	dev_dbg(dev, "[%s] (pdev=0x%x)", __func__, (u32) pdev);

	device_data = platform_get_drvdata(pdev);
	if (!device_data) {
		dev_err(dev, "[%s]: platform_get_drvdata() failed!",
			__func__);
		return -ENOMEM;
	}

	/* Try to decrease the number of available devices. */
	if (down_trylock(&driver_data.device_allocation))
		return -EBUSY;

	/* Check that the device is free */
	spin_lock(&device_data->ctx_lock);
	/* current_ctx allocates a device, NULL = unallocated */
	if (device_data->current_ctx) {
		/* The device is busy */
		spin_unlock(&device_data->ctx_lock);
		/* Return the device to the pool. */
		up(&driver_data.device_allocation);
		return -EBUSY;
	}

	spin_unlock(&device_data->ctx_lock);

	/* Remove the device from the list */
	if (klist_node_attached(&device_data->list_node))
		klist_remove(&device_data->list_node);

	/* If this was the last device, remove the services */
	if (list_empty(&driver_data.device_list.k_list))
		ahash_algs_unregister_all();

	if (hash_disable_power(device_data, false))
		dev_err(dev, "[%s]: hash_disable_power() failed",
			__func__);

	clk_put(device_data->clk);
	ux500_regulator_put(device_data->regulator);

	iounmap(device_data->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));

	kfree(device_data);

	return 0;
}

/**
 * u8500_hash_shutdown - Function that shutdown the hash device.
 * @pdev: The platform device
 */
static void u8500_hash_shutdown(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct hash_device_data *device_data;

	dev_dbg(&pdev->dev, "[%s]", __func__);

	device_data = platform_get_drvdata(pdev);
	if (!device_data) {
		dev_err(&pdev->dev, "[%s] platform_get_drvdata() failed!",
				__func__);
		return;
	}

	/* Check that the device is free */
	spin_lock(&device_data->ctx_lock);
	/* current_ctx allocates a device, NULL = unallocated */
	if (!device_data->current_ctx) {
		if (down_trylock(&driver_data.device_allocation))
			dev_dbg(&pdev->dev, "[%s]: Cryp still in use!"
				"Shutting down anyway...", __func__);
		/**
		 * (Allocate the device)
		 * Need to set this to non-null (dummy) value,
		 * to avoid usage if context switching.
		 */
		device_data->current_ctx++;
	}
	spin_unlock(&device_data->ctx_lock);

	/* Remove the device from the list */
	if (klist_node_attached(&device_data->list_node))
		klist_remove(&device_data->list_node);

	/* If this was the last device, remove the services */
	if (list_empty(&driver_data.device_list.k_list))
		ahash_algs_unregister_all();

	iounmap(device_data->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));

	if (hash_disable_power(device_data, false))
		dev_err(&pdev->dev, "[%s] hash_disable_power() failed",
				__func__);
}

/**
 * u8500_hash_suspend - Function that suspends the hash device.
 * @pdev:	The platform device.
 * @state:	-
 */
static int u8500_hash_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	struct hash_device_data *device_data;
	struct hash_ctx *temp_ctx = NULL;

	dev_dbg(&pdev->dev, "[%s]", __func__);

	device_data = platform_get_drvdata(pdev);
	if (!device_data) {
		dev_err(&pdev->dev, "[%s] platform_get_drvdata() failed!",
				__func__);
		return -ENOMEM;
	}

	spin_lock(&device_data->ctx_lock);
	if (!device_data->current_ctx)
		device_data->current_ctx++;
	spin_unlock(&device_data->ctx_lock);

	if (device_data->current_ctx == ++temp_ctx) {
		if (down_interruptible(&driver_data.device_allocation))
			dev_dbg(&pdev->dev, "[%s]: down_interruptible() "
					"failed", __func__);
		ret = hash_disable_power(device_data, false);

	} else
		ret = hash_disable_power(device_data, true);

	if (ret)
		dev_err(&pdev->dev, "[%s]: hash_disable_power()", __func__);

	return ret;
}

/**
 * u8500_hash_resume - Function that resume the hash device.
 * @pdev:	The platform device.
 */
static int u8500_hash_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct hash_device_data *device_data;
	struct hash_ctx *temp_ctx = NULL;

	dev_dbg(&pdev->dev, "[%s]", __func__);

	device_data = platform_get_drvdata(pdev);
	if (!device_data) {
		dev_err(&pdev->dev, "[%s] platform_get_drvdata() failed!",
				__func__);
		return -ENOMEM;
	}

	spin_lock(&device_data->ctx_lock);
	if (device_data->current_ctx == ++temp_ctx)
		device_data->current_ctx = NULL;
	spin_unlock(&device_data->ctx_lock);

	if (!device_data->current_ctx)
		up(&driver_data.device_allocation);
	else
		ret = hash_enable_power(device_data, true);

	if (ret)
		dev_err(&pdev->dev, "[%s]: hash_enable_power() failed!",
			__func__);

	return ret;
}

static struct platform_driver hash_driver = {
	.probe  = u8500_hash_probe,
	.remove = u8500_hash_remove,
	.shutdown = u8500_hash_shutdown,
	.suspend  = u8500_hash_suspend,
	.resume   = u8500_hash_resume,
	.driver = {
		.owner = THIS_MODULE,
		.name  = "hash1",
	}
};

/**
 * u8500_hash_mod_init - The kernel module init function.
 */
static int __init u8500_hash_mod_init(void)
{
	pr_debug("[%s] is called!", __func__);

	klist_init(&driver_data.device_list, NULL, NULL);
	/* Initialize the semaphore to 0 devices (locked state) */
	sema_init(&driver_data.device_allocation, 0);

	return platform_driver_register(&hash_driver);
}

/**
 * u8500_hash_mod_fini - The kernel module exit function.
 */
static void __exit u8500_hash_mod_fini(void)
{
	pr_debug("[%s] is called!", __func__);
	platform_driver_unregister(&hash_driver);
	return;
}

module_init(u8500_hash_mod_init);
module_exit(u8500_hash_mod_fini);

MODULE_DESCRIPTION("Driver for ST-Ericsson U8500 HASH engine.");
MODULE_LICENSE("GPL");

MODULE_ALIAS("sha1-u8500");
MODULE_ALIAS("sha256-u8500");

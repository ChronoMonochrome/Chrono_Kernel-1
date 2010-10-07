/*
 * Cryptographic API.
 *
 * Support for Nomadik hardware crypto engine.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <linux/crypto.h>
#include <crypto/internal/hash.h>
#include <crypto/sha.h>

#include <mach/debug.h>
#include <mach/hardware.h>

#include "hash_alg.h"

#define DRIVER_NAME            "DRIVER HASH"
/* enables/disables debug msgs */
#define DRIVER_DEBUG            1
#define DRIVER_DEBUG_PFX        DRIVER_NAME
#define DRIVER_DBG              KERN_ERR

#define MAX_HASH_DIGEST_BYTE_SIZE	32
#define HASH_BLOCK_BYTE_SIZE		64

#define HASH_ACC_SYNC_CONTROL
#ifdef HASH_ACC_SYNC_CONTROL
static struct mutex hash_hw_acc_mutex;
#endif

int debug;
static int mode;
static int contextsaving;
static struct hash_system_context g_sys_ctx;

/**
 * struct hash_driver_data - IO Base and clock.
 * @base: The IO base for the block
 * @clk: FIXME, add comment
 */
struct hash_driver_data {
	void __iomem *base;
	struct clk *clk;
};

/**
 * struct hash_ctx - The context used for hash calculations.
 * @key: The key used in the operation
 * @keylen: The length of the key
 * @updated: Indicates if hardware is initialized for new operations
 * @state: The state of the current calculations
 * @config: The current configuration
 */
struct hash_ctx {
	u8 key[HASH_BLOCK_BYTE_SIZE];
	u32 keylen;
	u8 updated;
	struct hash_state state;
	struct hash_config config;
};

/**
 * struct hash_tfm_ctx - Transform context
 * @key: The key stored in the transform context
 * @keylen: The length of the key in the transform context
 */
struct hash_tfm_ctx {
	u8 key[HASH_BLOCK_BYTE_SIZE];
	u32 keylen;
};

/* Declaration of functions */
static void hash_messagepad(int hid, const u32 *message, u8 index_bytes);

/**
 * hexdump - Dumps buffers in hex.
 * @buf: The buffer to dump
 * @len: The length of the buffer
 */
static void hexdump(unsigned char *buf, unsigned int len)
{
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET,
		       16, 1, buf, len, false);
}

/**
 * clear_reg_str - Clear the registry hash_str.
 * @hid: Hardware device ID
 *
 * This function will clear the dcal bit and the nblw bits.
 */
static inline void clear_reg_str(int hid)
{
	/* We will only clear the valid registers and not the reserved */
	g_sys_ctx.registry[hid]->str &= ~HASH_STR_DCAL_MASK;
	g_sys_ctx.registry[hid]->str &= ~HASH_STR_NBLW_MASK;
}

/**
 * write_nblw - Writes the number of valid bytes to nblw.
 * @hid: Hardware device ID
 * @bytes: The number of valid bytes in last word of a message
 *
 * Note that this function only writes, i.e. it does not clear the registry
 * before it writes the new data.
 */
static inline void write_nblw(int hid, int bytes)
{
	g_sys_ctx.registry[hid]->str |=
		((bytes * 8) & HASH_STR_NBLW_MASK);
}

/**
 * write_dcal - Write/set the dcal bit.
 * @hid: Hardware device ID
 */
static inline void write_dcal(int hid)
{
	g_sys_ctx.registry[hid]->str |= (1 << HASH_STR_DCAL_POS);
}

/**
 * pad_message - Function that pads a message.
 * @hid: Hardware device ID
 *
 * FIXME: This function should be replaced.
 */
static inline void pad_message(int hid)
{
	hash_messagepad(hid, g_sys_ctx.state[hid].buffer,
			g_sys_ctx.state[hid].index);
}

/**
 * write_key - Writes the key to the hardware registries.
 * @hid: Hardware device ID
 * @key: The key used in the operation
 * @keylen: The length of the key
 *
 * Note that in this function we DO NOT write to the NBLW registry even though
 * the hardware reference manual says so. There must be incorrect information in
 * the manual or there must be a bug in the state machine in the hardware.
 */
static void write_key(int hid, const u8 *key, u32 keylen)
{
	u32 word = 0;
	clear_reg_str(hid);

	while (keylen >= 4) {
		word = ((u32) (key[3] & 255) << 24) |
			((u32) (key[2] & 255) << 16) |
			((u32) (key[1] & 255) << 8) |
			((u32) (key[0] & 255));

		HASH_SET_DIN(word);
		keylen -= 4;
		key += 4;
	}

	/* This takes care of the remaining bytes on the last word */
	if (keylen) {
		word = 0;
		while (keylen) {
			word |= (key[keylen - 1] << (8 * (keylen - 1)));
			keylen--;
		}
		HASH_SET_DIN(word);
	}

	write_dcal(hid);
}

/**
 * init_hash_hw - Initialise the hash hardware for a new calculation.
 * @desc: The hash descriptor for the job
 *
 * This function will enable the bits needed to clear and start a new
 * calculation.
 */
static int init_hash_hw(struct shash_desc *desc)
{
	int ret = 0;
	int hash_error = HASH_OK;
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	stm_dbg(debug, "[init_hash_hw] (ctx=0x%x)!", (u32)ctx);

	hash_error = hash_setconfiguration(HASH_DEVICE_ID_1, &ctx->config);
	if (hash_error != HASH_OK) {
		stm_error("hash_setconfiguration() failed!");
		ret = -1;
		goto out;
	}

	hash_error = hash_begin(HASH_DEVICE_ID_1);
	if (hash_error != HASH_OK) {
		stm_error("hash_begin() failed!");
		ret = -1;
		goto out;
	}

	if (ctx->config.oper_mode == HASH_OPER_MODE_HMAC) {
		stm_dbg(debug, "[init_hash_hw] update key=0x%0x, len=%d",
				(u32) ctx->key, ctx->keylen);
		write_key(HASH_DEVICE_ID_1, ctx->key, ctx->keylen);
	}

out:
	return ret;
}

/**
 * hash_init - Common hash init function for SHA1/SHA2 (SHA256).
 * @desc: The hash descriptor for the job
 *
 * Initialize structures and copy the key from the transform context to the
 * descriptor context if the mode is HMAC.
 */
static int hash_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);
	struct hash_tfm_ctx *tfm_ctx = crypto_tfm_ctx(&desc->tfm->base);

	stm_dbg(debug, "[hash_init]: (ctx=0x%x)!", (u32)ctx);

	if (ctx->config.oper_mode == HASH_OPER_MODE_HMAC) {
		if (tfm_ctx->key) {
			memcpy(ctx->key, tfm_ctx->key, tfm_ctx->keylen);
			ctx->keylen = tfm_ctx->keylen;
		}
	}

	memset(&ctx->state, 0, sizeof(struct hash_state));
	ctx->updated = 0;

	return 0;
}

/**
 * hash_update - The hash update function for SHA1/SHA2 (SHA256).
 * @desc: The hash descriptor for the job
 * @data: Message that should be hashed
 * @len: The length of the message that should be hashed
 */
static int hash_update(struct shash_desc *desc, const u8 *data,
		       unsigned int len)
{
	int ret = 0;
	int hash_error = HASH_OK;
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	stm_dbg(debug, "[hash_update]: (ctx=0x%x, data=0x%x, len=%d)!",
		(u32)ctx, (u32)data, len);

#ifdef HASH_ACC_SYNC_CONTROL
	mutex_lock(&hash_hw_acc_mutex);
#endif

	if (!ctx->updated) {
		ret = init_hash_hw(desc);
		if (ret) {
			stm_error("init_hash_hw() failed!");
			goto out;
		}
	}

	if (contextsaving) {
		if (ctx->updated) {
			hash_error =
			    hash_resume_state(HASH_DEVICE_ID_1, &ctx->state);
			if (hash_error != HASH_OK) {
				stm_error("hash_resume_state() failed!");
				ret = -1;
				goto out;
			}
		}
	}

	/* NOTE: The length of the message is in the form of number of bits */
	hash_error = hash_hw_update(HASH_DEVICE_ID_1, data, len * 8);
	if (hash_error != HASH_OK) {
		stm_error("hash_hw_update() failed!");
		ret = -1;
		goto out;
	}

	if (contextsaving) {
		hash_error =
		    hash_save_state(HASH_DEVICE_ID_1, &ctx->state);
		if (hash_error != HASH_OK) {
			stm_error("hash_save_state() failed!");
			ret = -1;
			goto out;
		}

	}
	ctx->updated = 1;

out:
#ifdef HASH_ACC_SYNC_CONTROL
	mutex_unlock(&hash_hw_acc_mutex);
#endif
	return ret;
}

/**
 * hash_final - The hash final function for SHA1/SHA2 (SHA256).
 * @desc: The hash descriptor for the job
 * @out: Pointer for the calculated digest
 */
static int hash_final(struct shash_desc *desc, u8 *out)
{
	int ret = 0;
	int hash_error = HASH_OK;
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	int digestsize = crypto_shash_digestsize(desc->tfm);
	u8 digest[HASH_MSG_DIGEST_SIZE];

	stm_dbg(debug, "[hash_final]: (ctx=0x%x)!", (u32) ctx);

#ifdef HASH_ACC_SYNC_CONTROL
	mutex_lock(&hash_hw_acc_mutex);
#endif

	if (contextsaving) {
		hash_error = hash_resume_state(HASH_DEVICE_ID_1, &ctx->state);

		if (hash_error != HASH_OK) {
			stm_error("hash_resume_state() failed!");
			ret = -1;
			goto out;
		}
	}

	pad_message(HASH_DEVICE_ID_1);

	if (ctx->config.oper_mode == HASH_OPER_MODE_HMAC)
		write_key(HASH_DEVICE_ID_1, ctx->key, ctx->keylen);

	hash_error = hash_get_digest(HASH_DEVICE_ID_1, digest);

	memcpy(out, digest, digestsize);

out:
#ifdef HASH_ACC_SYNC_CONTROL
	mutex_unlock(&hash_hw_acc_mutex);
#endif

	return ret;
}

/**
 * hash_setkey - The setkey function for providing the key during HMAC
 *               calculations.
 * @tfm: Pointer to the transform
 * @key: The key used in the operation
 * @keylen: The length of the key
 * @alg: The algorithm to use in the operation
 */
static int hash_setkey(struct crypto_shash *tfm, const u8 *key,
		       unsigned int keylen, int alg)
{
	int ret = 0;
	int hash_error = HASH_OK;

	struct hash_tfm_ctx *ctx_tfm = crypto_shash_ctx(tfm);

	stm_dbg(debug, "[hash_setkey]: (ctx_tfm=0x%x, key=0x%x, keylen=%d)!",
		(u32) ctx_tfm, (u32) key, keylen);

	/* Truncate the key to block size */
	if (keylen > HASH_BLOCK_BYTE_SIZE) {
		struct hash_config config;
		u8 digest[MAX_HASH_DIGEST_BYTE_SIZE];
		unsigned int digestsize = crypto_shash_digestsize(tfm);

		config.algorithm = alg;
		config.data_format = HASH_DATA_8_BITS;
		config.oper_mode = HASH_OPER_MODE_HASH;

#ifdef HASH_ACC_SYNC_CONTROL
		mutex_lock(&hash_hw_acc_mutex);
#endif
		hash_error = hash_compute(HASH_DEVICE_ID_1, key, keylen * 8,
					  &config, digest);
#ifdef HASH_ACC_SYNC_CONTROL
		mutex_unlock(&hash_hw_acc_mutex);
#endif
		if (hash_error != HASH_OK) {
			stm_error("Error: hash_compute() failed!");
			ret = -1;
			goto out;
		}

		memcpy(ctx_tfm->key, digest, digestsize);
		ctx_tfm->keylen = digestsize;
	} else {
		memcpy(ctx_tfm->key, key, keylen);
		ctx_tfm->keylen = keylen;
	}

out:
	return ret;
}

/**
 * sha1_init - SHA1 init function.
 * @desc: The hash descriptor for the job
 */
static int sha1_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	stm_dbg(debug, "[sha1_init]: (ctx=0x%x)!", (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA1;
	ctx->config.oper_mode = HASH_OPER_MODE_HASH;

	return hash_init(desc);
}

/**
 * sha256_init - SHA2 (SHA256) init function.
 * @desc: The hash descriptor for the job
 */
static int sha256_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	stm_dbg(debug, "[sha256_init]: (ctx=0x%x)!", (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA2;
	ctx->config.oper_mode = HASH_OPER_MODE_HASH;

	return hash_init(desc);
}

/**
 * hmac_sha1_init - SHA1 HMAC init function.
 * @desc: The hash descriptor for the job
 */
static int hmac_sha1_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	stm_dbg(debug, "[hmac_sha1_init]: (ctx=0x%x)!", (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA1;
	ctx->config.oper_mode = HASH_OPER_MODE_HMAC;
	ctx->config.hmac_key = HASH_SHORT_KEY;

	return hash_init(desc);
}

/**
 * hmac_sha256_init - SHA2 (SHA256) HMAC init function.
 * @desc: The hash descriptor for the job
 */
static int hmac_sha256_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	stm_dbg(debug, "[hmac_sha256_init]: (ctx=0x%x)!", (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA2;
	ctx->config.oper_mode = HASH_OPER_MODE_HMAC;
	ctx->config.hmac_key = HASH_SHORT_KEY;

	return hash_init(desc);
}

/**
 * hmac_sha1_setkey - SHA1 HMAC setkey function.
 * @tfm: Pointer to the transform
 * @key: The key used in the operation
 * @keylen: The length of the key
 */
static int hmac_sha1_setkey(struct crypto_shash *tfm, const u8 *key,
			    unsigned int keylen)
{
	stm_dbg(debug, "[hmac_sha1_setkey]: (tfm=0x%x, key=0x%x, keylen=%d)!",
		(u32) tfm, (u32) key, keylen);

	return hash_setkey(tfm, key, keylen, HASH_ALGO_SHA1);
}

/**
 * hmac_sha256_setkey - SHA2 (SHA256) HMAC setkey function.
 * @tfm: Pointer to the transform
 * @key: The key used in the operation
 * @keylen: The length of the key
 */
static int hmac_sha256_setkey(struct crypto_shash *tfm, const u8 *key,
			      unsigned int keylen)
{
	stm_dbg(debug, "[hmac_sha256_setkey]: (tfm=0x%x, key=0x%x, keylen=%d)!",
		(u32) tfm, (u32) key, keylen);

	return hash_setkey(tfm, key, keylen, HASH_ALGO_SHA2);
}

static struct shash_alg sha1_alg = {
	.digestsize = SHA1_DIGEST_SIZE,
	.init       = sha1_init,
	.update     = hash_update,
	.final      = hash_final,
	.descsize   = sizeof(struct hash_ctx),
	.base = {
		 .cra_name        = "sha1",
		 .cra_driver_name = "sha1-u8500",
		 .cra_flags       = CRYPTO_ALG_TYPE_DIGEST |
			 CRYPTO_ALG_TYPE_SHASH,
		 .cra_blocksize   = SHA1_BLOCK_SIZE,
		 .cra_ctxsize     = sizeof(struct hash_tfm_ctx),
		 .cra_module      = THIS_MODULE,
		 }
};

static struct shash_alg sha256_alg = {
	.digestsize = SHA256_DIGEST_SIZE,
	.init       = sha256_init,
	.update     = hash_update,
	.final      = hash_final,
	.descsize   = sizeof(struct hash_ctx),
	.base = {
		 .cra_name        = "sha256",
		 .cra_driver_name = "sha256-u8500",
		 .cra_flags       = CRYPTO_ALG_TYPE_DIGEST |
			 CRYPTO_ALG_TYPE_SHASH,
		 .cra_blocksize   = SHA256_BLOCK_SIZE,
		 .cra_ctxsize     = sizeof(struct hash_tfm_ctx),
		 .cra_module      = THIS_MODULE,
		 }
};

static struct shash_alg hmac_sha1_alg = {
	.digestsize = SHA1_DIGEST_SIZE,
	.init       = hmac_sha1_init,
	.update     = hash_update,
	.final      = hash_final,
	.setkey     = hmac_sha1_setkey,
	.descsize   = sizeof(struct hash_ctx),
	.base = {
		 .cra_name        = "hmac(sha1)",
		 .cra_driver_name = "hmac(sha1-u8500)",
		 .cra_flags       = CRYPTO_ALG_TYPE_DIGEST |
			 CRYPTO_ALG_TYPE_SHASH,
		 .cra_blocksize   = SHA1_BLOCK_SIZE,
		 .cra_ctxsize     = sizeof(struct hash_tfm_ctx),
		 .cra_module      = THIS_MODULE,
		 }
};

static struct shash_alg hmac_sha256_alg = {
	.digestsize = SHA256_DIGEST_SIZE,
	.init       = hmac_sha256_init,
	.update     = hash_update,
	.final      = hash_final,
	.setkey     = hmac_sha256_setkey,
	.descsize   = sizeof(struct hash_ctx),
	.base = {
		 .cra_name        = "hmac(sha256)",
		 .cra_driver_name = "hmac(sha256-u8500)",
		 .cra_flags       = CRYPTO_ALG_TYPE_DIGEST |
			 CRYPTO_ALG_TYPE_SHASH,
		 .cra_blocksize   = SHA256_BLOCK_SIZE,
		 .cra_ctxsize     = sizeof(struct hash_tfm_ctx),
		 .cra_module      = THIS_MODULE,
		 }
};

/**
 * u8500_hash_probe - Function that probes the hash hardware.
 * @pdev: The platform device
 */
static int u8500_hash_probe(struct platform_device *pdev)
{
	int ret = 0;
	int hash_error = HASH_OK;
	struct resource *res = NULL;
	struct hash_driver_data *hash_drv_data;

	stm_dbg(debug, "[u8500_hash_probe]: (pdev=0x%x)", (u32) pdev);

	stm_dbg(debug, "[u8500_hash_probe]: Calling kzalloc()!");
	hash_drv_data = kzalloc(sizeof(struct hash_driver_data), GFP_KERNEL);
	if (!hash_drv_data) {
		stm_dbg(debug, "kzalloc() failed!");
		ret = -ENOMEM;
		goto out;
	}

	stm_dbg(debug, "[u8500_hash_probe]: Calling platform_get_resource()!");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		stm_dbg(debug, "platform_get_resource() failed");
		ret = -ENODEV;
		goto out_kfree;
	}

	stm_dbg(debug, "[u8500_hash_probe]: Calling request_mem_region()!");
	res = request_mem_region(res->start, res->end - res->start + 1,
				 pdev->name);
	if (res == NULL) {
		stm_dbg(debug, "request_mem_region() failed");
		ret = -EBUSY;
		goto out_kfree;
	}

	stm_dbg(debug, "[u8500_hash_probe]: Calling ioremap()!");
	hash_drv_data->base = ioremap(res->start, res->end - res->start + 1);
	if (!hash_drv_data->base) {
		stm_error
		    ("[u8500_hash] ioremap of hash1 register memory failed!");
		ret = -ENOMEM;
		goto out_free_mem;
	}

	stm_dbg(debug, "[u8500_hash_probe]: Calling clk_get()!");
	/* Enable the clk for HASH1 hardware block */
	hash_drv_data->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(hash_drv_data->clk)) {
		stm_error("clk_get() failed!");
		ret = PTR_ERR(hash_drv_data->clk);
		goto out_unmap;
	}

	stm_dbg(debug, "[u8500_hash_probe]: Calling clk_enable()!");
	ret = clk_enable(hash_drv_data->clk);
	if (ret) {
		stm_error("clk_enable() failed!");
		goto out_unmap;
	}

	stm_dbg(debug,
		"[u8500_hash_probe]: Calling hash_init_base_address()->"
		"(base=0x%x,DEVICE_ID=%d)!",
		(u32) hash_drv_data->base, HASH_DEVICE_ID_1);

	/* Setting base address */
	hash_error =
	    hash_init_base_address(HASH_DEVICE_ID_1,
		      (t_logical_address) hash_drv_data->base);
	if (hash_error != HASH_OK) {
		stm_error("hash_init_base_address() failed!");
		ret = -1;	/*TODO: what error code should be used here!? */
		goto out_clk;
	}
#ifdef HASH_ACC_SYNC_CONTROL
	stm_dbg(debug, "[u8500_hash_probe]: Calling mutex_init()!");
	mutex_init(&hash_hw_acc_mutex);
#endif

	if (mode == 0) {
		stm_dbg(debug,
			"[u8500_hash_probe]: To register all algorithms!");

		ret = crypto_register_shash(&sha1_alg);
		if (ret) {
			stm_error("Could not register sha1_alg!");
			goto out_clk;
		}
		stm_dbg(debug, "[u8500_hash_probe]: sha1_alg registered!");

		ret = crypto_register_shash(&sha256_alg);
		if (ret) {
			stm_error("Could not register sha256_alg!");
			goto out_unreg1;
		}
		stm_dbg(debug, "[u8500_hash_probe]: sha256_alg registered!");

		ret = crypto_register_shash(&hmac_sha1_alg);
		if (ret) {
			stm_error("Could not register hmac_sha1_alg!");
			goto out_unreg2;
		}
		stm_dbg(debug, "[u8500_hash_probe]: hmac_sha1_alg registered!");

		ret = crypto_register_shash(&hmac_sha256_alg);
		if (ret) {
			stm_error("Could not register hmac_sha256_alg!");
			goto out_unreg3;
		}
		stm_dbg(debug,
			"[u8500_hash_probe]: hmac_sha256_alg registered!");
	}

	if (mode == 10) {
		stm_dbg(debug,
			"[u8500_hash_probe]: To register only sha1 and sha256"
			" algorithms!");

		ret = crypto_register_shash(&sha1_alg);
		if (ret) {
			stm_error("Could not register sha1_alg!");
			goto out_clk;
		}

		ret = crypto_register_shash(&sha256_alg);
		if (ret) {
			stm_error("Could not register sha256_alg!");
			goto out_unreg1_tmp;
		}
	}

	stm_dbg(debug, "[u8500_hash_probe]: Calling platform_set_drvdata()!");
	platform_set_drvdata(pdev, hash_drv_data);
	return 0;

	if (mode == 0) {
out_unreg1:
		crypto_unregister_shash(&sha1_alg);
out_unreg2:
		crypto_unregister_shash(&sha256_alg);
out_unreg3:
		crypto_unregister_shash(&hmac_sha1_alg);
	}

	if (mode == 10) {
out_unreg1_tmp:
		crypto_unregister_shash(&sha1_alg);
	}

out_clk:
	clk_disable(hash_drv_data->clk);
	clk_put(hash_drv_data->clk);

out_unmap:
	iounmap(hash_drv_data->base);

out_free_mem:
	release_mem_region(res->start, res->end - res->start + 1);

out_kfree:
	kfree(hash_drv_data);
out:
	return ret;
}

/**
 * u8500_hash_remove - Function that removes the hash device from the platform.
 * @pdev: The platform device
 */
static int u8500_hash_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct hash_driver_data *hash_drv_data;

	stm_dbg(debug, "[u8500_hash_remove]: (pdev=0x%x)", (u32) pdev);

	stm_dbg(debug, "[u8500_hash_remove]: Calling platform_get_drvdata()!");
	hash_drv_data = platform_get_drvdata(pdev);

	if (mode == 0) {
		stm_dbg(debug,
			"[u8500_hash_remove]: To unregister all algorithms!");
		crypto_unregister_shash(&sha1_alg);
		crypto_unregister_shash(&sha256_alg);
		crypto_unregister_shash(&hmac_sha1_alg);
		crypto_unregister_shash(&hmac_sha256_alg);
	}

	if (mode == 10) {
		stm_dbg(debug,
			"[u8500_hash_remove]: To unregister only sha1 and "
			"sha256 algorithms!");
		crypto_unregister_shash(&sha1_alg);
		crypto_unregister_shash(&sha256_alg);
	}
#ifdef HASH_ACC_SYNC_CONTROL
	stm_dbg(debug, "[u8500_hash_remove]: Calling mutex_destroy()!");
	mutex_destroy(&hash_hw_acc_mutex);
#endif

	stm_dbg(debug, "[u8500_hash_remove]: Calling clk_disable()!");
	clk_disable(hash_drv_data->clk);

	stm_dbg(debug, "[u8500_hash_remove]: Calling clk_put()!");
	clk_put(hash_drv_data->clk);

	stm_dbg(debug, "[u8500_hash_remove]: Calling iounmap(): base = 0x%x",
		(u32) hash_drv_data->base);
	iounmap(hash_drv_data->base);

	stm_dbg(debug, "[u8500_hash_remove]: Calling platform_get_resource()!");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	stm_dbg(debug,
		"[u8500_hash_remove]: Calling release_mem_region()"
		"->res->start=0x%x, res->end = 0x%x!",
		res->start, res->end);
	release_mem_region(res->start, res->end - res->start + 1);

	stm_dbg(debug, "[u8500_hash_remove]: Calling kfree()!");
	kfree(hash_drv_data);

	return 0;
}

static struct platform_driver hash_driver = {
	.probe  = u8500_hash_probe,
	.remove = u8500_hash_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name  = "hash1",
		   },
};

/**
 * u8500_hash_mod_init - The kernel module init function.
 */
static int __init u8500_hash_mod_init(void)
{
	stm_dbg(debug, "u8500_hash_mod_init() is called!");

	return platform_driver_register(&hash_driver);
}

/**
 * u8500_hash_mod_fini - The kernel module exit function.
 */
static void __exit u8500_hash_mod_fini(void)
{
	stm_dbg(debug, "u8500_hash_mod_fini() is called!");

	platform_driver_unregister(&hash_driver);
	return;
}

/**
 * hash_processblock - This function processes a single block of 512 bits (64
 *                     bytes), word aligned, starting at message.
 * @hid: Hardware device ID
 * @message: Block (512 bits) of message to be written to the HASH hardware
 *
 * Reentrancy: Non Re-entrant.
 */
static void hash_processblock(int hid, const u32 *message)
{
	u32 count;

	HCL_CLEAR_BITS(g_sys_ctx.registry[hid]->str,
		       HASH_STR_DCAL_MASK);
	HCL_CLEAR_BITS(g_sys_ctx.registry[hid]->str,
		       HASH_STR_NBLW_MASK);

	/* Partially unrolled loop */
	for (count = 0; count < (HASH_BLOCK_SIZE / sizeof(u32));
	     count += 4) {
		HASH_SET_DIN(message[0]);
		HASH_SET_DIN(message[1]);
		HASH_SET_DIN(message[2]);
		HASH_SET_DIN(message[3]);
		message += 4;
	}
}

/**
 * hash_messagepad - Pads a message and write the nblw bits.
 * @hid: Hardware device ID
 * @message: Last word of a message
 * @index_bytes: The number of bytes in the last message
 *
 * This function manages the final part of the digest calculation, when less
 * than 512 bits (64 bytes) remain in message. This means index_bytes < 64.
 *
 * Reentrancy: Non Re-entrant.
 */
static void hash_messagepad(int hid, const u32 *message, u8 index_bytes)
{
	stm_dbg(debug, "[u8500_hash_alg] hash_messagepad"
			"(bytes in final msg=%d))", index_bytes);

	clear_reg_str(hid);

	/* Main loop */
	while (index_bytes >= 4) {
		HASH_SET_DIN(message[0]);
		index_bytes -= 4;
		message++;
	}

	if (index_bytes)
		HASH_SET_DIN(message[0]);

	/* num_of_bytes == 0 => NBLW <- 0 (32 bits valid in DATAIN) */
	HASH_SET_NBLW(index_bytes * 8);
	stm_dbg(debug, "[u8500_hash_alg] hash_messagepad -> DIN=0x%08x NBLW=%d",
			g_sys_ctx.registry[hid]->din,
			g_sys_ctx.registry[hid]->str);
	HASH_SET_DCAL;
	stm_dbg(debug, "[u8500_hash_alg] hash_messagepad d -> "
			"DIN=0x%08x NBLW=%d",
			g_sys_ctx.registry[hid]->din,
			g_sys_ctx.registry[hid]->str);

}

/**
 * hash_incrementlength - Increments the length of the current message.
 * @hid: Hardware device ID
 * @incr: Length of message processed already
 *
 * Overflow cannot occur, because conditions for overflow are checked in
 * hash_hw_update.
 */
static void hash_incrementlength(int hid, u32 incr)
{
	g_sys_ctx.state[hid].length.low_word += incr;

	/* Check for wrap-around */
	if (g_sys_ctx.state[hid].length.low_word < incr)
		g_sys_ctx.state[hid].length.high_word++;
}

/**
 * hash_setconfiguration - Sets the required configuration for the hash
 *                         hardware.
 * @hid: Hardware device ID
 * @p_config: Pointer to a configuration structure
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
int hash_setconfiguration(int hid, struct hash_config *p_config)
{
	int hash_error = HASH_OK;

	stm_dbg(debug, "[u8500_hash_alg] hash_setconfiguration())");

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	HASH_SET_DATA_FORMAT(p_config->data_format);

	HCL_SET_BITS(g_sys_ctx.registry[hid]->cr,
		     HASH_CR_EMPTYMSG_MASK);

	/* This bit selects between SHA-1 or SHA-2 algorithm */
	if (HASH_ALGO_SHA2 == p_config->algorithm) {
		HCL_CLEAR_BITS(g_sys_ctx.registry[hid]->cr,
				HASH_CR_ALGO_MASK);
	} else {		/* SHA1 algorithm */

		HCL_SET_BITS(g_sys_ctx.registry[hid]->cr,
				HASH_CR_ALGO_MASK);
	}

	/* This bit selects between HASH or HMAC mode for the selected
	   algorithm */
	if (HASH_OPER_MODE_HASH == p_config->oper_mode) {
		HCL_CLEAR_BITS(g_sys_ctx.registry
			       [hid]->cr, HASH_CR_MODE_MASK);
	} else {		/* HMAC mode */

		HCL_SET_BITS(g_sys_ctx.registry[hid]->cr,
				HASH_CR_MODE_MASK);

		/* This bit selects between short key (<= 64 bytes) or long key
		   (>64 bytes) in HMAC mode */
		if (HASH_SHORT_KEY == p_config->hmac_key) {
			HCL_CLEAR_BITS(g_sys_ctx.registry[hid]->cr,
				       HASH_CR_LKEY_MASK);
		} else {
			HCL_SET_BITS(g_sys_ctx.registry[hid]->cr,
				     HASH_CR_LKEY_MASK);
		}
	}

	return hash_error;
}

/**
 * hash_begin - This routine resets some globals and initializes the hash
 *              hardware.
 * @hid: Hardware device ID
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
int hash_begin(int hid)
{
	int hash_error = HASH_OK;

	/* HW and SW initializations */
	/* Note: there is no need to initialize buffer and digest members */

	stm_dbg(debug, "[u8500_hash_alg] hash_begin())");

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	g_sys_ctx.state[hid].index = 0;
	g_sys_ctx.state[hid].bit_index = 0;
	g_sys_ctx.state[hid].length.high_word = 0;
	g_sys_ctx.state[hid].length.low_word = 0;

	HASH_INITIALIZE;

	HCL_CLEAR_BITS(g_sys_ctx.registry[hid]->str,
		       HASH_STR_DCAL_MASK);
	HCL_CLEAR_BITS(g_sys_ctx.registry[hid]->str,
		       HASH_STR_NBLW_MASK);

	return hash_error;
}

/**
 * hash_hw_update - Updates current HASH computation hashing another part of
 *                  the message.
 * @hid: Hardware device ID
 * @p_data_buffer: Byte array containing the message to be hashed (caller
 *                 allocated)
 * @msg_length: Length of message to be hashed (in bits)
 *
 * Reentrancy: Non Re-entrant
 */
int hash_hw_update(int hid, const u8 *p_data_buffer, u32 msg_length)
{
	int hash_error = HASH_OK;
	u8 index;
	u8 *p_buffer;
	u32 count;

	stm_dbg(debug, "[u8500_hash_alg] hash_hw_update(msg_length=%d / %d), "
			"in=%d, bin=%d))",
			msg_length,
			msg_length / 8,
			g_sys_ctx.state[hid].index,
			g_sys_ctx.state[hid].bit_index);

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
	return hash_error;
	}

	index = g_sys_ctx.state[hid].index;

	p_buffer = (u8 *)g_sys_ctx.state[hid].buffer;

	/* Number of bytes in the message */
	msg_length /= 8;

	/* Check parameters */
	if (NULL == p_data_buffer) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	/* Check if g_sys_ctx.state.length + msg_length
	   overflows */
	if (msg_length >
	    (g_sys_ctx.state[hid].length.low_word + msg_length)
	    && HASH_HIGH_WORD_MAX_VAL ==
	    (g_sys_ctx.state[hid].length.high_word)) {
		hash_error = HASH_MSG_LENGTH_OVERFLOW;
		stm_error("[u8500_hash_alg] HASH_MSG_LENGTH_OVERFLOW!");
		return hash_error;
	}

	/* Main loop */
	while (0 != msg_length) {
		if ((index + msg_length) < HASH_BLOCK_SIZE) {
			for (count = 0; count < msg_length; count++) {
				/*TODO: memcpy? */
				p_buffer[index + count] =
				    *(p_data_buffer + count);
			}

			index += msg_length;
			msg_length = 0;
		} else {
			/* if 'p_data_buffer' is four byte aligned and local
			 * buffer does not have any data, we can write data
			 * directly from 'p_data_buffer' to HW peripheral,
			 * otherwise we first copy data to a local buffer
			 */
			if ((0 == (((u32) p_data_buffer) % 4))
					&& (0 == index)) {
				hash_processblock(hid,
						(const u32 *)p_data_buffer);
			} else {
				for (count = 0;
				     count < (u32)(HASH_BLOCK_SIZE - index);
				     count++) {
					p_buffer[index + count] =
					    *(p_data_buffer + count);
				}

				hash_processblock(hid, (const u32 *)p_buffer);
			}

			hash_incrementlength(hid, HASH_BLOCK_SIZE);
			p_data_buffer += (HASH_BLOCK_SIZE - index);
			msg_length -= (HASH_BLOCK_SIZE - index);
			index = 0;
		}
	}

	g_sys_ctx.state[hid].index = index;

	stm_dbg(debug, "[u8500_hash_alg] hash_hw_update END(msg_length=%d in "
			"bits, in=%d, bin=%d))",
			msg_length,
			g_sys_ctx.state[hid].index,
			g_sys_ctx.state[hid].bit_index);

	return hash_error;
}

/**
 * hash_end_key - Function that ends a message, i.e. pad and triggers the last
 *                calculation.
 * @hid: Hardware device ID
 *
 * This function also clear the registries that have been involved in
 * computation.
 */
int hash_end_key(int hid)
{
	int hash_error = HASH_OK;
	u8 count = 0;

	stm_dbg(debug, "[u8500_hash_alg] hash_end_key(index=%d))",
			g_sys_ctx.state[hid].index);

	hash_messagepad(hid, g_sys_ctx.state[hid].buffer,
			     g_sys_ctx.state[hid].index);

	 /* Wait till the DCAL bit get cleared, So that we get the final
	  * message digest not intermediate value.
	  */
	while (g_sys_ctx.registry[hid]->str & HASH_STR_DCAL_MASK)
		;

	/* Reset the HASH state */
	g_sys_ctx.state[hid].index = 0;
	g_sys_ctx.state[hid].bit_index = 0;

	for (count = 0; count < HASH_BLOCK_SIZE / sizeof(u32); count++)
		g_sys_ctx.state[hid].buffer[count] = 0;

	g_sys_ctx.state[hid].length.high_word = 0;
	g_sys_ctx.state[hid].length.low_word = 0;

	return hash_error;
}

/**
 * hash_resume_state - Function that resumes the state of an calculation.
 * @hid: Hardware device ID
 * @device_state: The state to be restored in the hash hardware
 *
 * Reentrancy: Non Re-entrant
 */
int hash_resume_state(int hid, const struct hash_state *device_state)
{
	u32 temp_cr;
	int hash_error = HASH_OK;
	s32 count;

	stm_dbg(debug, "[u8500_hash_alg] hash_resume_state(state(0x%x)))",
		(u32) device_state);

	if (NULL == device_state) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	/* Check correctness of index and length members */
	if (device_state->index > HASH_BLOCK_SIZE
	    || (device_state->length.low_word % HASH_BLOCK_SIZE) != 0) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	for (count = 0; count < (s32) (HASH_BLOCK_SIZE / sizeof(u32));
	     count++) {
		g_sys_ctx.state[hid].buffer[count] =
		    device_state->buffer[count];
	}

	g_sys_ctx.state[hid].index = device_state->index;
	g_sys_ctx.state[hid].bit_index = device_state->bit_index;
	g_sys_ctx.state[hid].length = device_state->length;

	HASH_INITIALIZE;

	temp_cr = device_state->temp_cr;
	g_sys_ctx.registry[hid]->cr =
	    temp_cr & HASH_CR_RESUME_MASK;

	for (count = 0; count < HASH_CSR_COUNT; count++) {
		if ((count >= 36) &&
				!(g_sys_ctx.registry[hid]->cr &
					HASH_CR_MODE_MASK)) {
			break;
		}
		g_sys_ctx.registry[hid]->csrx[count] =
			device_state->csr[count];
	}

	g_sys_ctx.registry[hid]->csfull = device_state->csfull;
	g_sys_ctx.registry[hid]->csdatain = device_state->csdatain;

	g_sys_ctx.registry[hid]->str = device_state->str_reg;
	g_sys_ctx.registry[hid]->cr = temp_cr;

	return hash_error;
}

/**
 * hash_save_state - Function that saves the state of hardware.
 * @hid: Hardware device ID
 * @device_state: The strucure where the hardware state should be saved
 *
 * Reentrancy: Non Re-entrant
 */
int hash_save_state(int hid, struct hash_state *device_state)
{
	u32 temp_cr;
	u32 count;
	int hash_error = HASH_OK;

	stm_dbg(debug, "[u8500_hash_alg] hash_save_state( state(0x%x)))",
		(u32) device_state);

	if (NULL == device_state) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	for (count = 0; count < HASH_BLOCK_SIZE / sizeof(u32); count++) {
		device_state->buffer[count] =
		    g_sys_ctx.state[hid].buffer[count];
	}

	device_state->index = g_sys_ctx.state[hid].index;
	device_state->bit_index = g_sys_ctx.state[hid].bit_index;
	device_state->length = g_sys_ctx.state[hid].length;

	/* Write dummy value to force digest intermediate calculation. This
	 * actually makes sure that there isn't any ongoing calculation in the
	 * hardware.
	 */
	while (g_sys_ctx.registry[hid]->str & HASH_STR_DCAL_MASK)
		;

	temp_cr = g_sys_ctx.registry[hid]->cr;

	device_state->str_reg = g_sys_ctx.registry[hid]->str;

	device_state->din_reg = g_sys_ctx.registry[hid]->din;

	for (count = 0; count < HASH_CSR_COUNT; count++) {
		if ((count >= 36)
				&& !(g_sys_ctx.registry[hid]->cr &
					HASH_CR_MODE_MASK)) {
			break;
		}

		device_state->csr[count] =
			g_sys_ctx.registry[hid]->csrx[count];
	}

	device_state->csfull = g_sys_ctx.registry[hid]->csfull;
	device_state->csdatain = g_sys_ctx.registry[hid]->csdatain;

	/* end if */
	device_state->temp_cr = temp_cr;

	return hash_error;
}

/**
 * hash_end - Ends current HASH computation, passing back the hash to the user.
 * @hid: Hardware device ID
 * @digest: User allocated byte array for the calculated digest
 *
 * Reentrancy: Non Re-entrant
 */
int hash_end(int hid, u8 digest[HASH_MSG_DIGEST_SIZE])
{
	int hash_error = HASH_OK;
	u32 count;
	/* Standard SHA-1 digest for null string for HASH mode */
	u8 zero_message_hash_sha1[HASH_MSG_DIGEST_SIZE] = {
		0xDA, 0x39, 0xA3, 0xEE,
		0x5E, 0x6B, 0x4B, 0x0D,
		0x32, 0x55, 0xBF, 0xEF,
		0x95, 0x60, 0x18, 0x90,
		0xAF, 0xD8, 0x07, 0x09,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	/* Standard SHA-2 digest for null string for HASH mode */
	u8 zero_message_hash_sha2[HASH_MSG_DIGEST_SIZE] = {
		0xD4, 0x1D, 0x8C, 0xD9,
		0x8F, 0x00, 0xB2, 0x04,
		0xE9, 0x80, 0x09, 0x98,
		0xEC, 0xF8, 0x42, 0x7E,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	/* Standard SHA-1 digest for null string for HMAC mode,with no key */
	u8 zero_message_hmac_sha1[HASH_MSG_DIGEST_SIZE] = {
		0xFB, 0xDB, 0x1D, 0x1B,
		0x18, 0xAA, 0x6C, 0x08,
		0x32, 0x4B, 0x7D, 0x64,
		0xB7, 0x1F, 0xB7, 0x63,
		0x70, 0x69, 0x0E, 0x1D,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	/* Standard SHA2 digest for null string for HMAC mode,with no key */
	u8 zero_message_hmac_sha2[HASH_MSG_DIGEST_SIZE] = {
		0x74, 0xE6, 0xF7, 0x29,
		0x8A, 0x9C, 0x2D, 0x16,
		0x89, 0x35, 0xF5, 0x8C,
		0x00, 0x1B, 0xAD, 0x88,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};

	stm_dbg(debug, "[u8500_hash_alg] hash_end(digest array (0x%x)))",
		(u32) digest);

	if (NULL == digest) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	if (0 == g_sys_ctx.state[hid].index &&
		0 == g_sys_ctx.state[hid].length.high_word &&
		0 == g_sys_ctx.state[hid].length.low_word) {
		if (g_sys_ctx.registry[hid]->cr & HASH_CR_MODE_MASK) {
			if (g_sys_ctx.registry[hid]->cr & HASH_CR_ALGO_MASK) {
				/* hash of an empty message was requested */
				for (count = 0; count < HASH_MSG_DIGEST_SIZE;
				     count++) {
					digest[count] =
					    zero_message_hmac_sha1[count];
				}
			} else {	/* SHA-2 algo */

				/* hash of an empty message was requested */
				for (count = 0; count < HASH_MSG_DIGEST_SIZE;
				     count++) {
					digest[count] =
					    zero_message_hmac_sha2[count];
				}
			}
		} else {	/* HASH mode */

			if (g_sys_ctx.registry[hid]->cr & HASH_CR_ALGO_MASK) {
				/* hash of an empty message was requested */
				for (count = 0; count < HASH_MSG_DIGEST_SIZE;
				     count++) {
					digest[count] =
					    zero_message_hash_sha1[count];
				}
			} else {	/* SHA-2 algo */

				/* hash of an empty message was requested */
				for (count = 0; count < HASH_MSG_DIGEST_SIZE;
				     count++) {
					digest[count] =
					    zero_message_hash_sha2[count];
				}
			}
		}

		HASH_SET_DCAL;
	} else {
		hash_messagepad(hid,
				g_sys_ctx.state[hid].buffer,
				g_sys_ctx.state[hid].index);

		/* Wait till the DCAL bit get cleared, So that we get the final
		 * message digest not intermediate value. */
		while (g_sys_ctx.registry[hid]->str & HASH_STR_DCAL_MASK)
			;

		hash_error = hash_get_digest(hid, digest);

		/* Reset the HASH state */
		g_sys_ctx.state[hid].index = 0;
		g_sys_ctx.state[hid].bit_index = 0;
		for (count = 0; count < HASH_BLOCK_SIZE / sizeof(u32);
		     count++) {
			g_sys_ctx.state[hid].buffer[count]
			    = 0;
		}

		g_sys_ctx.state[hid].length.high_word = 0;
		g_sys_ctx.state[hid].length.low_word = 0;
	}

	if (debug)
		hexdump(digest, HASH_MSG_DIGEST_SIZE);

	return hash_error;
}

/**
 * hash_initialize_globals - Initialize global variables to their default reset
 *                           value.
 * @hid: Hardware device ID
 *
 * Reentrancy: Non Re-entrant, global structure g_sys_ctx elements are being
 * modified
 */
static void hash_initialize_globals(int hid)
{
	u8 loop_count;

	/* Resetting the values of global variables except the registry */
	g_sys_ctx.state[hid].temp_cr = HASH_RESET_INDEX_VAL;
	g_sys_ctx.state[hid].str_reg = HASH_RESET_INDEX_VAL;
	g_sys_ctx.state[hid].din_reg = HASH_RESET_INDEX_VAL;

	for (loop_count = 0; loop_count < HASH_CSR_COUNT; loop_count++) {
		g_sys_ctx.state[hid].csr[loop_count] =
		    HASH_RESET_CSRX_REG_VALUE;
	}

	g_sys_ctx.state[hid].csfull = HASH_RESET_CSFULL_REG_VALUE;
	g_sys_ctx.state[hid].csdatain = HASH_RESET_CSDATAIN_REG_VALUE;

	for (loop_count = 0; loop_count < (HASH_BLOCK_SIZE / sizeof(u32));
	     loop_count++) {
		g_sys_ctx.state[hid].buffer[loop_count] =
		    HASH_RESET_BUFFER_VAL;
	}

	g_sys_ctx.state[hid].length.high_word = HASH_RESET_LEN_HIGH_VAL;
	g_sys_ctx.state[hid].length.low_word = HASH_RESET_LEN_LOW_VAL;
	g_sys_ctx.state[hid].index = HASH_RESET_INDEX_VAL;
	g_sys_ctx.state[hid].bit_index = HASH_RESET_BIT_INDEX_VAL;
}

/**
 * hash_reset - This routine will reset the global variable to default reset
 * value and HASH registers to their power on reset values.
 * @hid: Hardware device ID
 *
 * Reentrancy: Non Re-entrant, global structure g_sys_ctx elements are being
 * modified.
 */
int hash_reset(int hid)
{
	int hash_error = HASH_OK;
	u8 loop_count;

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;

		return hash_error;
	}

	/* Resetting the values of global variables except the registry */
	hash_initialize_globals(hid);

	/* Resetting HASH control register to power-on-reset values */
	g_sys_ctx.registry[hid]->str = HASH_RESET_START_REG_VALUE;

	for (loop_count = 0; loop_count < HASH_CSR_COUNT; loop_count++) {
		g_sys_ctx.registry[hid]->csrx[loop_count] =
		    HASH_RESET_CSRX_REG_VALUE;
	}

	g_sys_ctx.registry[hid]->csfull = HASH_RESET_CSFULL_REG_VALUE;
	g_sys_ctx.registry[hid]->csdatain =
	    HASH_RESET_CSDATAIN_REG_VALUE;

     /* Resetting the HASH Control reg. This also reset the PRIVn and SECn
      * bits and hence the device registers will not be accessed anymore and
      * should be done in the last HASH register access statement.
      */
	g_sys_ctx.registry[hid]->cr = HASH_RESET_CONTROL_REG_VALUE;

	return hash_error;
}

/**
 * hash_init_base_address - This routine initializes hash register base
 * address. It also checks for peripheral Ids and PCell Ids.
 * @hid: Hardware device ID
 * @base_address: Hash hardware base address
 *
 * Reentrancy: Non Re-entrant, global variable registry (register base address)
 * is being modified.
 */
int hash_init_base_address(int hid, t_logical_address base_address)
{
	int hash_error = HASH_OK;

	stm_dbg(debug, "[u8500_hash_alg] hash_init_base_address())");

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;

		return hash_error;
	}

	if (0 != base_address) {
		/*--------------------------------------*
		 * Initializing the registers structure *
		 *--------------------------------------*/
		g_sys_ctx.registry[hid] = (struct hash_register *) base_address;

		/*--------------------------*
		 * Checking Peripheral Ids  *
		 *--------------------------*/
		if ((HASH_P_ID0 ==
		     g_sys_ctx.registry[hid]->periphid0)
		    && (HASH_P_ID1 ==
			g_sys_ctx.registry[hid]->periphid1)
		    && (HASH_P_ID2 ==
			g_sys_ctx.registry[hid]->periphid2)
		    && (HASH_P_ID3 ==
			g_sys_ctx.registry[hid]->periphid3)
		    && (HASH_CELL_ID0 ==
			g_sys_ctx.registry[hid]->cellid0)
		    && (HASH_CELL_ID1 ==
			g_sys_ctx.registry[hid]->cellid1)
		    && (HASH_CELL_ID2 ==
			g_sys_ctx.registry[hid]->cellid2)
		    && (HASH_CELL_ID3 ==
			g_sys_ctx.registry[hid]->cellid3)
		    ) {

			/* Resetting the values of global variables except the
			   registry */
			hash_initialize_globals(hid);
			hash_error = HASH_OK;
			return hash_error;
		} else {
			hash_error = HASH_UNSUPPORTED_HW;
			stm_error("[u8500_hash_alg] HASH_UNSUPPORTED_HW!");
			return hash_error;
		}
	} /* end if */
	else {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}
}

/**
 * hash_get_digest - Gets the digest.
 * @hid: Hardware device ID
 * @digest: User allocated byte array for the calculated digest
 *
 * Reentrancy: Non Re-entrant, global variable registry (hash control register)
 * is being modified.
 *
 * Note that, if this is called before the final message has been handle it will
 * return the intermediate message digest.
 */
int hash_get_digest(int hid, u8 *digest)
{
	u32 temp_hx_val, count;
	int hash_error = HASH_OK;

	stm_dbg(debug,
		"[u8500_hash_alg] hash_get_digest(digest array:(0x%x))",
		(u32) digest);

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}

	/* Copy result into digest array */
	for (count = 0; count < (HASH_MSG_DIGEST_SIZE / sizeof(u32));
	     count++) {
		temp_hx_val = HASH_GET_HX(count);
		digest[count * 4] = (u8) ((temp_hx_val >> 24) & 0xFF);
		digest[count * 4 + 1] = (u8) ((temp_hx_val >> 16) & 0xFF);
		digest[count * 4 + 2] = (u8) ((temp_hx_val >> 8) & 0xFF);
		digest[count * 4 + 3] = (u8) ((temp_hx_val >> 0) & 0xFF);
	}

	return hash_error;
}

/**
 * hash_compute - Performs a complete HASH calculation on the message passed.
 * @hid: Hardware device ID
 * @p_data_buffer: Pointer to the message to be hashed
 * @msg_length: The length of the message
 * @p_hash_config: Structure with configuration data for the hash hardware
 * @digest: User allocated byte array for the calculated digest
 *
 * Reentrancy: Non Re-entrant
 */
int hash_compute(int hid,
		const u8 *p_data_buffer,
		u32 msg_length,
		struct hash_config *p_hash_config,
		u8 digest[HASH_MSG_DIGEST_SIZE]) {
	int hash_error = HASH_OK;

	stm_dbg(debug, "[u8500_hash_alg] hash_compute())");

	if (!((HASH_DEVICE_ID_0 == hid)
	      || (HASH_DEVICE_ID_1 == hid))) {
		hash_error = HASH_INVALID_PARAMETER;
		stm_error("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_error;
	}


	 /* WARNING: return code must be checked if
	  * behaviour of hash_begin changes.
	  */
	hash_error = hash_setconfiguration(hid, p_hash_config);
	if (HASH_OK != hash_error) {
		stm_error("[u8500_hash_alg] hash_setconfiguration() failed!");
		return hash_error;
	}

	hash_error = hash_begin(hid);
	if (HASH_OK != hash_error) {
		stm_error("[u8500_hash_alg] hash_begin() failed!");
		return hash_error;
	}

	hash_error = hash_hw_update(hid, p_data_buffer, msg_length);
	if (HASH_OK != hash_error) {
		stm_error("[u8500_hash_alg] hash_hw_update() failed!");
		return hash_error;
	}

	hash_error = hash_end(hid, digest);
	if (HASH_OK != hash_error) {
		stm_error("[u8500_hash_alg] hash_end() failed!");
		return hash_error;
	}

	return hash_error;
}

module_init(u8500_hash_mod_init);
module_exit(u8500_hash_mod_fini);

module_param(mode, int, 0);
module_param(debug, int, 0);
module_param(contextsaving, int, 0);

MODULE_DESCRIPTION("Driver for ST-Ericsson U8500 HASH engine.");
MODULE_LICENSE("GPL");

MODULE_ALIAS("sha1-u8500");
MODULE_ALIAS("sha256-u8500");
MODULE_ALIAS("hmac(sha1-u8500)");
MODULE_ALIAS("hmac(sha256-u8500)");

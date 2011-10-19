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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/crypto.h>
#include <linux/regulator/consumer.h>
#include <linux/bitops.h>

#include <crypto/internal/hash.h>
#include <crypto/sha.h>

#include <mach/hardware.h>

#include "hash_alg.h"

#define DRIVER_NAME            "DRIVER HASH"
/* Enable/Disables debug msgs */
#define DRIVER_DEBUG            1
#define DRIVER_DEBUG_PFX        DRIVER_NAME
#define DRIVER_DBG              KERN_DEBUG

#define MAX_HASH_DIGEST_BYTE_SIZE	32

static struct mutex hash_hw_acc_mutex;

static int debug;
static struct hash_system_context sys_ctx_g;
static struct hash_driver_data *internal_drv_data;

/**
 * struct hash_driver_data - IO Base and clock.
 * @base: The IO base for the block.
 * @clk: The clock.
 * @regulator: The current regulator.
 * @power_state: TRUE = power state on, FALSE = power state off.
 * @power_state_mutex: Mutex for power_state.
 * @restore_dev_ctx: TRUE = saved ctx, FALSE = no saved ctx.
 */
struct hash_driver_data {
	void __iomem *base;
	struct device *dev;
	struct clk *clk;
	struct regulator *regulator;
	bool power_state;
	struct mutex power_state_mutex;
	bool restore_dev_state;
};

/* Declaration of functions */
static void hash_messagepad(int hid, const u32 *message, u8 index_bytes);

/**
 * clear_reg_str - Clear the registry hash_str.
 * @hid: Hardware device ID
 *
 * This function will clear the dcal bit and the nblw bits.
 */
static inline void clear_reg_str(int hid)
{
	/*
	 * We will only clear NBLW since writing 0 to DCAL is done by the
	 * hardware
	 */
	sys_ctx_g.registry[hid]->str &= ~HASH_STR_NBLW_MASK;
}

static int hash_disable_power(
		struct device *dev,
		struct hash_driver_data *device_data,
		bool save_device_state)
{
	int ret = 0;

	dev_dbg(dev, "[%s]", __func__);

	mutex_lock(&device_data->power_state_mutex);
	if (!device_data->power_state)
		goto out;

	if (save_device_state) {
		hash_save_state(HASH_DEVICE_ID_1,
				&sys_ctx_g.state[HASH_DEVICE_ID_1]);
		device_data->restore_dev_state = true;
	}

	clk_disable(device_data->clk);
	ret = regulator_disable(device_data->regulator);
	if (ret)
		dev_err(dev, "[%s]: "
				"regulator_disable() failed!",
				__func__);

	device_data->power_state = false;

out:
	mutex_unlock(&device_data->power_state_mutex);

	return ret;
}

static int hash_enable_power(
		struct device *dev,
		struct hash_driver_data *device_data,
		bool restore_device_state)
{
	int ret = 0;

	dev_dbg(dev, "[%s]", __func__);

	mutex_lock(&device_data->power_state_mutex);
	if (!device_data->power_state) {
		ret = regulator_enable(device_data->regulator);
		if (ret) {
			dev_err(dev, "[%s]: regulator_enable() failed!",
					__func__);
			goto out;
		}

		ret = clk_enable(device_data->clk);
		if (ret) {
			dev_err(dev, "[%s]: clk_enable() failed!",
					__func__);
			regulator_disable(device_data->regulator);
			goto out;
		}
		device_data->power_state = true;
	}

	if (device_data->restore_dev_state) {
		if (restore_device_state) {
			device_data->restore_dev_state = false;
			hash_resume_state(HASH_DEVICE_ID_1,
				&sys_ctx_g.state[HASH_DEVICE_ID_1]);
		}
	}
out:
	mutex_unlock(&device_data->power_state_mutex);

	return ret;
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
	int hash_rv;
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	pr_debug("[init_hash_hw] (ctx=0x%x)!", (u32)ctx);

	hash_rv = hash_setconfiguration(HASH_DEVICE_ID_1, &ctx->config);
	if (hash_rv != HASH_OK) {
		pr_err("hash_setconfiguration() failed!");
		ret = -EPERM;
		return ret;
	}

	hash_begin(ctx);

	return ret;
}

/**
 * hash_init - Common hash init function for SHA1/SHA2 (SHA256).
 * @desc: The hash descriptor for the job
 *
 * Initialize structures.
 */
static int hash_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	pr_debug("[hash_init]: (ctx=0x%x)!", (u32)ctx);

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
	int hash_rv = HASH_OK;

	pr_debug("[hash_update]: (data=0x%x, len=%d)!",
		(u32)data, len);

	mutex_lock(&hash_hw_acc_mutex);

	/* NOTE: The length of the message is in the form of number of bits */
	hash_rv = hash_hw_update(desc, HASH_DEVICE_ID_1, data, len * 8);
	if (hash_rv != HASH_OK) {
		pr_err("hash_hw_update() failed!");
		ret = -EPERM;
		goto out;
	}

out:
	mutex_unlock(&hash_hw_acc_mutex);
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
	int hash_rv = HASH_OK;
	struct hash_ctx *ctx = shash_desc_ctx(desc);
	struct hash_driver_data *device_data = internal_drv_data;

	int digestsize = crypto_shash_digestsize(desc->tfm);
	u8 digest[HASH_MSG_DIGEST_SIZE];

	pr_debug("[hash_final]: (ctx=0x%x)!", (u32) ctx);

	mutex_lock(&hash_hw_acc_mutex);

	/* Enable device power (and clock) */
	ret = hash_enable_power(device_data->dev, device_data, false);
	if (ret) {
		dev_err(device_data->dev, "[%s]: "
				"hash_enable_power() failed!", __func__);
		goto out;
	}

	if (!ctx->updated) {
		ret = init_hash_hw(desc);
		if (ret) {
			pr_err("init_hash_hw() failed!");
			goto out_power;
		}
	} else {
		hash_rv = hash_resume_state(HASH_DEVICE_ID_1, &ctx->state);

		if (hash_rv != HASH_OK) {
			pr_err("hash_resume_state() failed!");
			ret = -EPERM;
			goto out_power;
		}
	}

	hash_messagepad(HASH_DEVICE_ID_1, ctx->state.buffer,
			ctx->state.index);

	hash_get_digest(HASH_DEVICE_ID_1, digest, ctx->config.algorithm);

	memcpy(out, digest, digestsize);

out_power:
	/* Disable power (and clock) */
	if (hash_disable_power(device_data->dev, device_data, false))
		dev_err(device_data->dev, "[%s]: "
				"hash_disable_power() failed!", __func__);

out:
	mutex_unlock(&hash_hw_acc_mutex);

	return ret;
}

/**
 * sha1_init - SHA1 init function.
 * @desc: The hash descriptor for the job
 */
static int sha1_init(struct shash_desc *desc)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	pr_debug("[sha1_init]: (ctx=0x%x)!", (u32) ctx);

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

	pr_debug("[sha256_init]: (ctx=0x%x)!", (u32) ctx);

	ctx->config.data_format = HASH_DATA_8_BITS;
	ctx->config.algorithm = HASH_ALGO_SHA2;
	ctx->config.oper_mode = HASH_OPER_MODE_HASH;

	return hash_init(desc);
}

static int hash_export(struct shash_desc *desc, void *out)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	pr_debug("[hash_export]: (ctx=0x%X)  (out=0x%X)",
		 (u32) ctx, (u32) out);
	memcpy(out, ctx, sizeof(*ctx));
	return 0;
}

static int hash_import(struct shash_desc *desc, const void *in)
{
	struct hash_ctx *ctx = shash_desc_ctx(desc);

	pr_debug("[hash_import]: (ctx=0x%x)  (in =0x%X)",
		 (u32) ctx, (u32) in);
	memcpy(ctx, in, sizeof(*ctx));
	return 0;
}

static struct shash_alg sha1_alg = {
	.digestsize = SHA1_DIGEST_SIZE,
	.init       = sha1_init,
	.update     = hash_update,
	.final      = hash_final,
	.export     = hash_export,
	.import     = hash_import,
	.descsize   = sizeof(struct hash_ctx),
	.statesize  = sizeof(struct hash_ctx),
	.base = {
			.cra_name        = "sha1",
			.cra_driver_name = "sha1-u8500",
			.cra_flags       = CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize   = SHA1_BLOCK_SIZE,
			.cra_module      = THIS_MODULE,
		}
};

static struct shash_alg sha256_alg = {
	.digestsize = SHA256_DIGEST_SIZE,
	.init       = sha256_init,
	.update     = hash_update,
	.final      = hash_final,
	.export     = hash_export,
	.import     = hash_import,
	.descsize   = sizeof(struct hash_ctx),
	.statesize  = sizeof(struct hash_ctx),
	.base = {
			.cra_name        = "sha256",
			.cra_driver_name = "sha256-u8500",
			.cra_flags       = CRYPTO_ALG_TYPE_SHASH,
			.cra_blocksize   = SHA256_BLOCK_SIZE,
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
	int hash_rv = HASH_OK;
	struct resource *res = NULL;
	struct hash_driver_data *hash_drv_data;

	pr_debug("[u8500_hash_probe]: (pdev=0x%x)", (u32) pdev);

	pr_debug("[u8500_hash_probe]: Calling kzalloc()!");
	hash_drv_data = kzalloc(sizeof(struct hash_driver_data), GFP_KERNEL);
	if (!hash_drv_data) {
		pr_debug("kzalloc() failed!");
		ret = -ENOMEM;
		goto out;
	}

	hash_drv_data->dev = &pdev->dev;

	pr_debug("[u8500_hash_probe]: Calling platform_get_resource()!");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_debug("platform_get_resource() failed");
		ret = -ENODEV;
		goto out_kfree;
	}

	pr_debug("[u8500_hash_probe]: Calling request_mem_region()!");
	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (res == NULL) {
		pr_debug("request_mem_region() failed");
		ret = -EBUSY;
		goto out_kfree;
	}

	pr_debug("[u8500_hash_probe]: Calling ioremap()!");
	hash_drv_data->base = ioremap(res->start, resource_size(res));
	if (!hash_drv_data->base) {
		pr_err("[u8500_hash] "
		       "ioremap of hash1 register memory failed!");
		ret = -ENOMEM;
		goto out_free_mem;
	}
	mutex_init(&hash_drv_data->power_state_mutex);

	/* Enable power for HASH hardware block */
	hash_drv_data->regulator = regulator_get(&pdev->dev, "v-ape");
	if (IS_ERR(hash_drv_data->regulator)) {
		dev_err(&pdev->dev, "[u8500_hash] "
				    "could not get hash regulator\n");
		ret = PTR_ERR(hash_drv_data->regulator);
		hash_drv_data->regulator = NULL;
		goto out_unmap;
	}

	pr_debug("[u8500_hash_probe]: Calling clk_get()!");
	/* Enable the clk for HASH1 hardware block */
	hash_drv_data->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(hash_drv_data->clk)) {
		pr_err("clk_get() failed!");
		ret = PTR_ERR(hash_drv_data->clk);
		goto out_regulator;
	}

	/* Enable device power (and clock) */
	ret = hash_enable_power(&pdev->dev, hash_drv_data, false);
	if (ret) {
		dev_err(&pdev->dev, "[%s]: hash_enable_power() failed!",
				__func__);
		goto out_clk;
	}

	pr_debug("[u8500_hash_probe]: Calling hash_init_base_address()->"
		 "(base=0x%x,DEVICE_ID=%d)!",
		 (u32) hash_drv_data->base, HASH_DEVICE_ID_1);

	/* Setting base address */
	hash_rv =
	    hash_init_base_address(HASH_DEVICE_ID_1,
		      (t_logical_address) hash_drv_data->base);
	if (hash_rv != HASH_OK) {
		pr_err("hash_init_base_address() failed!");
		ret = -EPERM;
		goto out_power;
	}
	pr_debug("[u8500_hash_probe]: Calling mutex_init()!");
	mutex_init(&hash_hw_acc_mutex);

	pr_debug("[u8500_hash_probe]: To register only sha1 and sha256"
		 " algorithms!");
	internal_drv_data = hash_drv_data;

	ret = crypto_register_shash(&sha1_alg);
	if (ret) {
		pr_err("Could not register sha1_alg!");
		goto out_power;
	}
	pr_debug("[u8500_hash_probe]: sha1_alg registered!");

	ret = crypto_register_shash(&sha256_alg);
	if (ret) {
		pr_err("Could not register sha256_alg!");
		goto out_unreg1_tmp;
	}

	pr_debug("[u8500_hash_probe]: Calling platform_set_drvdata()!");
	platform_set_drvdata(pdev, hash_drv_data);

	if (hash_disable_power(&pdev->dev, hash_drv_data, false))
		dev_err(&pdev->dev, "[%s]: hash_disable_power()"
				" failed!", __func__);

	return 0;

out_unreg1_tmp:
	crypto_unregister_shash(&sha1_alg);

out_power:
	hash_disable_power(&pdev->dev, hash_drv_data, false);

out_clk:
	clk_put(hash_drv_data->clk);

out_regulator:
	regulator_put(hash_drv_data->regulator);

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

	pr_debug("[u8500_hash_remove]: (pdev=0x%x)", (u32) pdev);

	pr_debug("[u8500_hash_remove]: Calling platform_get_drvdata()!");
	hash_drv_data = platform_get_drvdata(pdev);

	pr_debug("[u8500_hash_remove]: To unregister only sha1 and "
		 "sha256 algorithms!");
	crypto_unregister_shash(&sha1_alg);
	crypto_unregister_shash(&sha256_alg);

	pr_debug("[u8500_hash_remove]: Calling mutex_destroy()!");
	mutex_destroy(&hash_hw_acc_mutex);

	pr_debug("[u8500_hash_remove]: Calling clk_disable()!");
	clk_disable(hash_drv_data->clk);

	pr_debug("[u8500_hash_remove]: Calling clk_put()!");
	clk_put(hash_drv_data->clk);

	pr_debug("[u8500_hash_remove]: Calling regulator_disable()!");
	regulator_disable(hash_drv_data->regulator);

	pr_debug("[u8500_hash_remove]: Calling iounmap(): base = 0x%x",
		 (u32) hash_drv_data->base);
	iounmap(hash_drv_data->base);

	pr_debug("[u8500_hash_remove]: Calling platform_get_resource()!");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	pr_debug("[u8500_hash_remove]: Calling release_mem_region()"
		 "->res->start=0x%x, res->end = 0x%x!",
		res->start, res->end);
	release_mem_region(res->start, res->end - res->start + 1);

	pr_debug("[u8500_hash_remove]: Calling kfree()!");
	kfree(hash_drv_data);

	return 0;
}

static void u8500_hash_shutdown(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct hash_driver_data *hash_drv_data;

	dev_dbg(&pdev->dev, "[%s]", __func__);

	hash_drv_data = platform_get_drvdata(pdev);
	if (!hash_drv_data) {
		dev_err(&pdev->dev, "[%s]: "
				"platform_get_drvdata() failed!", __func__);
		return;
	}

	crypto_unregister_shash(&sha1_alg);
	crypto_unregister_shash(&sha256_alg);

	mutex_destroy(&hash_hw_acc_mutex);

	iounmap(hash_drv_data->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));

	if (hash_disable_power(&pdev->dev, hash_drv_data, false))
		dev_err(&pdev->dev, "[%s]: "
				"hash_disable_power() failed", __func__);

	clk_put(hash_drv_data->clk);
	regulator_put(hash_drv_data->regulator);
}

static int u8500_hash_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	struct hash_driver_data *hash_drv_data;

	dev_dbg(&pdev->dev, "[%s]", __func__);

	/* Handle state? */
	hash_drv_data = platform_get_drvdata(pdev);
	if (!hash_drv_data) {
		dev_err(&pdev->dev, "[%s]: "
				"platform_get_drvdata() failed!", __func__);
		return -ENOMEM;
	}

	ret = hash_disable_power(&pdev->dev, hash_drv_data, true);
	if (ret)
		dev_err(&pdev->dev, "[%s]: "
				"hash_disable_power()", __func__);

	return ret;
}

static int u8500_hash_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct hash_driver_data *hash_drv_data;

	dev_dbg(&pdev->dev, "[%s]", __func__);

	hash_drv_data = platform_get_drvdata(pdev);
	if (!hash_drv_data) {
		dev_err(&pdev->dev, "[%s]: "
				"platform_get_drvdata() failed!", __func__);
		return -ENOMEM;
	}

	if (hash_drv_data->restore_dev_state) {
		ret = hash_enable_power(&pdev->dev, hash_drv_data, true);
		if (ret)
			dev_err(&pdev->dev, "[%s]: "
				"hash_enable_power() failed!", __func__);
	}

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
		   },
};

/**
 * u8500_hash_mod_init - The kernel module init function.
 */
static int __init u8500_hash_mod_init(void)
{
	pr_debug("u8500_hash_mod_init() is called!");

	return platform_driver_register(&hash_driver);
}

/**
 * u8500_hash_mod_fini - The kernel module exit function.
 */
static void __exit u8500_hash_mod_fini(void)
{
	pr_debug("u8500_hash_mod_fini() is called!");

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

	clear_bit(HASH_STR_NBLW_MASK, (void *)sys_ctx_g.registry[hid]->str);

	/* Partially unrolled loop */
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
	pr_debug("[u8500_hash_alg] hash_messagepad"
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

	while (sys_ctx_g.registry[hid]->str & HASH_STR_DCAL_MASK)
		cpu_relax();

	/* num_of_bytes == 0 => NBLW <- 0 (32 bits valid in DATAIN) */
	HASH_SET_NBLW(index_bytes * 8);
	pr_debug("[u8500_hash_alg] hash_messagepad -> DIN=0x%08x NBLW=%d",
			sys_ctx_g.registry[hid]->din,
			sys_ctx_g.registry[hid]->str);
	HASH_SET_DCAL;
	pr_debug("[u8500_hash_alg] hash_messagepad after dcal -> "
		 "DIN=0x%08x NBLW=%d",
			sys_ctx_g.registry[hid]->din,
			sys_ctx_g.registry[hid]->str);

	while (sys_ctx_g.registry[hid]->str & HASH_STR_DCAL_MASK)
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
	int hash_rv = HASH_OK;

	pr_debug("[u8500_hash_alg] hash_setconfiguration())");

	if (p_config->algorithm != HASH_ALGO_SHA1 &&
	    p_config->algorithm != HASH_ALGO_SHA2)
		return HASH_INVALID_PARAMETER;

	HASH_SET_DATA_FORMAT(p_config->data_format);

	HCL_SET_BITS(sys_ctx_g.registry[hid]->cr, HASH_CR_EMPTYMSG_MASK);

	switch (p_config->algorithm) {
	case HASH_ALGO_SHA1:
		HCL_SET_BITS(sys_ctx_g.registry[hid]->cr, HASH_CR_ALGO_MASK);
		break;

	case HASH_ALGO_SHA2:
		HCL_CLEAR_BITS(sys_ctx_g.registry[hid]->cr, HASH_CR_ALGO_MASK);
		break;

	default:
		pr_debug("[u8500_hash_alg] Incorrect algorithm.");
		return HASH_INVALID_PARAMETER;
	}

	/* This bit selects between HASH or HMAC mode for the selected
	   algorithm */
	if (HASH_OPER_MODE_HASH == p_config->oper_mode) {
		HCL_CLEAR_BITS(sys_ctx_g.registry
			       [hid]->cr, HASH_CR_MODE_MASK);
	} else {		/* HMAC mode or wrong hash mode */
		hash_rv = HASH_INVALID_PARAMETER;
		pr_err("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
	}

	return hash_rv;
}

/**
 * hash_begin - This routine resets some globals and initializes the hash
 *              hardware.
 * @ctx: Hash context
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
void hash_begin(struct hash_ctx *ctx)
{
	/* HW and SW initializations */
	/* Note: there is no need to initialize buffer and digest members */

	pr_debug("[u8500_hash_alg] hash_begin())");

	while (sys_ctx_g.registry[HASH_DEVICE_ID_1]->str & HASH_STR_DCAL_MASK)
		cpu_relax();

	HASH_INITIALIZE;

	HCL_CLEAR_BITS(sys_ctx_g.registry[HASH_DEVICE_ID_1]->str,
		       HASH_STR_NBLW_MASK);
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
int hash_hw_update(struct shash_desc *desc,
		   int hid,
		   const u8 *p_data_buffer,
		   u32 msg_length)
{
	int hash_rv = HASH_OK;
	u8 index;
	u8 *p_buffer;
	u32 count;
	struct hash_ctx *ctx = shash_desc_ctx(desc);
	struct hash_driver_data *device_data = internal_drv_data;

	pr_debug("[u8500_hash_alg] hash_hw_update(msg_length=%d / %d), "
		 "in=%d, bin=%d))",
			msg_length,
			msg_length / 8,
			ctx->state.index,
			ctx->state.bit_index);

	index = ctx->state.index;

	p_buffer = (u8 *)ctx->state.buffer;

	/* Number of bytes in the message */
	msg_length /= 8;

	/* Check parameters */
	if (NULL == p_data_buffer) {
		hash_rv = HASH_INVALID_PARAMETER;
		pr_err("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_rv;
	}

	/* Check if ctx->state.length + msg_length
	   overflows */
	if (msg_length >
	    (ctx->state.length.low_word + msg_length)
	    && HASH_HIGH_WORD_MAX_VAL ==
	    (ctx->state.length.high_word)) {
		hash_rv = HASH_MSG_LENGTH_OVERFLOW;
		pr_err("[u8500_hash_alg] HASH_MSG_LENGTH_OVERFLOW!");
		return hash_rv;
	}

	/* Enable device power (and clock) */
	hash_rv = hash_enable_power(device_data->dev, device_data, false);
	if (hash_rv) {
		dev_err(device_data->dev, "[%s]: "
				"hash_enable_power() failed!", __func__);
		goto out;
	}

	/* Main loop */
	while (0 != msg_length) {
		if ((index + msg_length) < HASH_BLOCK_SIZE) {
			for (count = 0; count < msg_length; count++) {
				p_buffer[index + count] =
				    *(p_data_buffer + count);
			}

			index += msg_length;
			msg_length = 0;
		} else {
			if (!ctx->updated) {
				hash_rv = init_hash_hw(desc);
				if (hash_rv != HASH_OK) {
					pr_err("init_hash_hw() failed!");
					goto out;
				}
				ctx->updated = 1;
			} else {
				hash_rv =
				    hash_resume_state(HASH_DEVICE_ID_1,
						      &ctx->state);
				if (hash_rv != HASH_OK) {
					pr_err("hash_resume_state()"
					       " failed!");
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

			hash_incrementlength(ctx, HASH_BLOCK_SIZE);
			p_data_buffer += (HASH_BLOCK_SIZE - index);
			msg_length -= (HASH_BLOCK_SIZE - index);
			index = 0;

			hash_rv =
			    hash_save_state(HASH_DEVICE_ID_1, &ctx->state);
			if (hash_rv != HASH_OK) {
				pr_err("hash_save_state() failed!");
				goto out_power;
			}
		}
	}

	ctx->state.index = index;

	pr_debug("[u8500_hash_alg] hash_hw_update END(msg_length=%d in "
		 "bits, in=%d, bin=%d))",
			msg_length,
			ctx->state.index,
			ctx->state.bit_index);
out_power:
	/* Disable power (and clock) */
	if (hash_disable_power(device_data->dev, device_data, false))
		dev_err(device_data->dev, "[%s]: "
				"hash_disable_power() failed!", __func__);
out:
	return hash_rv;
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
	int hash_rv = HASH_OK;
	s32 count;
	int hash_mode = HASH_OPER_MODE_HASH;

	pr_debug("[u8500_hash_alg] hash_resume_state(state(0x%x)))",
		 (u32) device_state);

	if (NULL == device_state) {
		hash_rv = HASH_INVALID_PARAMETER;
		pr_err("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_rv;
	}

	/* Check correctness of index and length members */
	if (device_state->index > HASH_BLOCK_SIZE
	    || (device_state->length.low_word % HASH_BLOCK_SIZE) != 0) {
		hash_rv = HASH_INVALID_PARAMETER;
		pr_err("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_rv;
	}

	HASH_INITIALIZE;

	temp_cr = device_state->temp_cr;
	sys_ctx_g.registry[hid]->cr =
	    temp_cr & HASH_CR_RESUME_MASK;

	if (sys_ctx_g.registry[hid]->cr & HASH_CR_MODE_MASK)
		hash_mode = HASH_OPER_MODE_HMAC;
	else
		hash_mode = HASH_OPER_MODE_HASH;

	for (count = 0; count < HASH_CSR_COUNT; count++) {
		if ((count >= 36) && (hash_mode == HASH_OPER_MODE_HASH))
			break;

		sys_ctx_g.registry[hid]->csrx[count] =
			device_state->csr[count];
	}

	sys_ctx_g.registry[hid]->csfull = device_state->csfull;
	sys_ctx_g.registry[hid]->csdatain = device_state->csdatain;

	sys_ctx_g.registry[hid]->str = device_state->str_reg;
	sys_ctx_g.registry[hid]->cr = temp_cr;

	return hash_rv;
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
	int hash_rv = HASH_OK;
	int hash_mode = HASH_OPER_MODE_HASH;

	pr_debug("[u8500_hash_alg] hash_save_state( state(0x%x)))",
		 (u32) device_state);

	if (NULL == device_state) {
		hash_rv = HASH_INVALID_PARAMETER;
		pr_err("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_rv;
	}

	/* Write dummy value to force digest intermediate calculation. This
	 * actually makes sure that there isn't any ongoing calculation in the
	 * hardware.
	 */
	while (sys_ctx_g.registry[hid]->str & HASH_STR_DCAL_MASK)
		cpu_relax();

	temp_cr = sys_ctx_g.registry[hid]->cr;

	device_state->str_reg = sys_ctx_g.registry[hid]->str;

	device_state->din_reg = sys_ctx_g.registry[hid]->din;

	if (sys_ctx_g.registry[hid]->cr & HASH_CR_MODE_MASK)
		hash_mode = HASH_OPER_MODE_HMAC;
	else
		hash_mode = HASH_OPER_MODE_HASH;

	for (count = 0; count < HASH_CSR_COUNT; count++) {
		if ((count >= 36) && (hash_mode == HASH_OPER_MODE_HASH))
			break;

		device_state->csr[count] =
			sys_ctx_g.registry[hid]->csrx[count];
	}

	device_state->csfull = sys_ctx_g.registry[hid]->csfull;
	device_state->csdatain = sys_ctx_g.registry[hid]->csdatain;

	device_state->temp_cr = temp_cr;

	return hash_rv;
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
	int hash_rv = HASH_OK;

	pr_debug("[u8500_hash_alg] hash_init_base_address())");

	if (0 != base_address) {
		/* Initializing the registers structure */
		sys_ctx_g.registry[hid] =
			(struct hash_register *) base_address;

		/* Checking Peripheral Ids  */
		if ((HASH_P_ID0 == sys_ctx_g.registry[hid]->periphid0)
		    && (HASH_P_ID1 == sys_ctx_g.registry[hid]->periphid1)
		    && (HASH_P_ID2 == sys_ctx_g.registry[hid]->periphid2)
		    && (HASH_P_ID3 == sys_ctx_g.registry[hid]->periphid3)
		    && (HASH_CELL_ID0 == sys_ctx_g.registry[hid]->cellid0)
		    && (HASH_CELL_ID1 == sys_ctx_g.registry[hid]->cellid1)
		    && (HASH_CELL_ID2 == sys_ctx_g.registry[hid]->cellid2)
		    && (HASH_CELL_ID3 == sys_ctx_g.registry[hid]->cellid3)
		    ) {
			hash_rv = HASH_OK;
			return hash_rv;
		} else {
			hash_rv = HASH_UNSUPPORTED_HW;
			pr_err("[u8500_hash_alg] HASH_UNSUPPORTED_HW!");
			return hash_rv;
		}
	} /* end if */
	else {
		hash_rv = HASH_INVALID_PARAMETER;
		pr_err("[u8500_hash_alg] HASH_INVALID_PARAMETER!");
		return hash_rv;
	}
}

/**
 * hash_get_digest - Gets the digest.
 * @hid: Hardware device ID
 * @digest: User allocated byte array for the calculated digest
 * @algorithm: The algorithm in use.
 *
 * Reentrancy: Non Re-entrant, global variable registry (hash control register)
 * is being modified.
 *
 * Note that, if this is called before the final message has been handle it
 * will return the intermediate message digest.
 */
void hash_get_digest(int hid, u8 *digest, int algorithm)
{
	u32 temp_hx_val, count;
	int loop_ctr;

	if (algorithm != HASH_ALGO_SHA1 && algorithm != HASH_ALGO_SHA2) {
		pr_err("[hash_get_digest] Incorrect algorithm %d", algorithm);
		return;
	}

	if (algorithm == HASH_ALGO_SHA1)
		loop_ctr = HASH_SHA1_DIGEST_SIZE / sizeof(u32);
	else
		loop_ctr = HASH_SHA2_DIGEST_SIZE / sizeof(u32);

	pr_debug("[u8500_hash_alg] hash_get_digest(digest array:(0x%x))",
		 (u32) digest);

	/* Copy result into digest array */
	for (count = 0; count < loop_ctr; count++) {
		temp_hx_val = HASH_GET_HX(count);
		digest[count * 4] = (u8) ((temp_hx_val >> 24) & 0xFF);
		digest[count * 4 + 1] = (u8) ((temp_hx_val >> 16) & 0xFF);
		digest[count * 4 + 2] = (u8) ((temp_hx_val >> 8) & 0xFF);
		digest[count * 4 + 3] = (u8) ((temp_hx_val >> 0) & 0xFF);
	}
}


module_init(u8500_hash_mod_init);
module_exit(u8500_hash_mod_fini);

module_param(debug, int, 0);

MODULE_DESCRIPTION("Driver for ST-Ericsson U8500 HASH engine.");
MODULE_LICENSE("GPL");

MODULE_ALIAS("sha1-u8500");
MODULE_ALIAS("sha256-u8500");

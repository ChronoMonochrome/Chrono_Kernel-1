/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Shujuan Chen (shujuan.chen@stericsson.com)
 * Author: Joakim Bech (joakim.xx.bech@stericsson.com)
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef _HASH_ALG_H
#define _HASH_ALG_H

#include <mach/hcl_defs.h>

/* Number of bytes the message digest */
#define HASH_MSG_DIGEST_SIZE    32
#define HASH_BLOCK_SIZE         64
#define HASH_SHA1_DIGEST_SIZE	20
#define HASH_SHA2_DIGEST_SIZE   32

/* Version defines */
#define HASH_HCL_VERSION_ID     1
#define HASH_HCL_MAJOR_ID       2
#define HASH_HCL_MINOR_ID       1

#define MAX_HASH_DEVICE     2

/* Maximum value of the length's high word */
#define HASH_HIGH_WORD_MAX_VAL  0xFFFFFFFFUL

/* Power on Reset values HASH registers */
#define HASH_RESET_CONTROL_REG_VALUE    0x0
#define HASH_RESET_START_REG_VALUE      0x0

/* Number of context swap registers */
#define HASH_CSR_COUNT              52

#define HASH_RESET_CSRX_REG_VALUE      0x0
#define HASH_RESET_CSFULL_REG_VALUE    0x0
#define HASH_RESET_CSDATAIN_REG_VALUE  0x0

#define HASH_RESET_INDEX_VAL        0x0
#define HASH_RESET_BIT_INDEX_VAL    0x0
#define HASH_RESET_BUFFER_VAL       0x0
#define HASH_RESET_LEN_HIGH_VAL     0x0
#define HASH_RESET_LEN_LOW_VAL      0x0

/* Control register bitfields */
#define HASH_CR_RESUME_MASK     0x11FCF

#define HASH_CR_SWITCHON_POS    31
#define HASH_CR_SWITCHON_MASK   MASK_BIT31

#define HASH_CR_EMPTYMSG_POS    20
#define HASH_CR_EMPTYMSG_MASK   MASK_BIT20

#define HASH_CR_DINF_POS        12
#define HASH_CR_DINF_MASK       MASK_BIT12

#define HASH_CR_NBW_POS         8
#define HASH_CR_NBW_MASK        0x00000F00UL

#define HASH_CR_LKEY_POS        16
#define HASH_CR_LKEY_MASK       MASK_BIT16

#define HASH_CR_ALGO_POS        7
#define HASH_CR_ALGO_MASK       MASK_BIT7

#define HASH_CR_MODE_POS        6
#define HASH_CR_MODE_MASK       MASK_BIT6

#define HASH_CR_DATAFORM_POS    4
#define HASH_CR_DATAFORM_MASK   (MASK_BIT4 | MASK_BIT5)

#define HASH_CR_DMAE_POS        3
#define HASH_CR_DMAE_MASK       MASK_BIT3

#define HASH_CR_INIT_POS        2
#define HASH_CR_INIT_MASK       MASK_BIT2

#define HASH_CR_PRIVN_POS       1
#define HASH_CR_PRIVN_MASK      MASK_BIT1

#define HASH_CR_SECN_POS        0
#define HASH_CR_SECN_MASK       MASK_BIT0

/* Start register bitfields */
#define HASH_STR_DCAL_POS   8
#define HASH_STR_DCAL_MASK  MASK_BIT8

#define HASH_STR_NBLW_POS   0
#define HASH_STR_NBLW_MASK  0x0000001FUL

#define HASH_NBLW_MAX_VAL   0x1F

/* PrimeCell IDs */
#define HASH_P_ID0          0xE0
#define HASH_P_ID1          0x05
#define HASH_P_ID2          0x38
#define HASH_P_ID3          0x00
#define HASH_CELL_ID0       0x0D
#define HASH_CELL_ID1       0xF0
#define HASH_CELL_ID2       0x05
#define HASH_CELL_ID3       0xB1

#define HASH_SET_DIN(val)   HCL_WRITE_REG( \
		sys_ctx_g.registry[HASH_DEVICE_ID_1]->din, (val))

#define HASH_INITIALIZE \
	HCL_WRITE_BITS( \
		sys_ctx_g.registry[HASH_DEVICE_ID_1]->cr, \
		0x01 << HASH_CR_INIT_POS, \
		HASH_CR_INIT_MASK)

#define HASH_SET_DATA_FORMAT(data_format) \
		HCL_WRITE_BITS( \
			sys_ctx_g.registry[HASH_DEVICE_ID_1]->cr, \
			(u32) (data_format) << HASH_CR_DATAFORM_POS, \
			HASH_CR_DATAFORM_MASK)

#define HASH_GET_HX(pos) \
		HCL_READ_REG(sys_ctx_g.registry[HASH_DEVICE_ID_1]->hx[pos])

#define HASH_SET_NBLW(val) \
		HCL_WRITE_BITS( \
			sys_ctx_g.registry[HASH_DEVICE_ID_1]->str, \
			(u32) (val) << HASH_STR_NBLW_POS, \
			HASH_STR_NBLW_MASK)

#define HASH_SET_DCAL \
		HCL_WRITE_BITS( \
			sys_ctx_g.registry[HASH_DEVICE_ID_1]->str, \
			0x01 << HASH_STR_DCAL_POS, \
			HASH_STR_DCAL_MASK)

#define HASH_BLOCK_BYTE_SIZE		64

/**
 * struct uint64 - Structure to handle 64 bits integers.
 * @high_word: Most significant bits
 * @low_word: Least significant bits
 *
 * Used to handle 64 bits integers.
 */
struct uint64 {
	u32 high_word;
	u32 low_word;
};

/**
 * struct hash_register - Contains all registers in u8500 hash hardware.
 * @cr: HASH control register (0x000)
 * @din: HASH data input register (0x004)
 * @str: HASH start register (0x008)
 * @hx: HASH digest register 0..7 (0x00c-0x01C)
 * @padding0: Reserved (0x02C)
 * @itcr: Integration test control register (0x080)
 * @itip: Integration test input register (0x084)
 * @itop: Integration test output register (0x088)
 * @padding1: Reserved (0x08C)
 * @csfull: HASH context full register (0x0F8)
 * @csdatain: HASH context swap data input register (0x0FC)
 * @csrx: HASH context swap register 0..51 (0x100-0x1CC)
 * @padding2: Reserved (0x1D0)
 * @periphid0: HASH peripheral identification register 0 (0xFE0)
 * @periphid1: HASH peripheral identification register 1 (0xFE4)
 * @periphid2: HASH peripheral identification register 2 (0xFE8)
 * @periphid3: HASH peripheral identification register 3 (0xFEC)
 * @cellid0: HASH PCell identification register 0 (0xFF0)
 * @cellid1: HASH PCell identification register 1 (0xFF4)
 * @cellid2: HASH PCell identification register 2 (0xFF8)
 * @cellid3: HASH PCell identification register 3 (0xFFC)
 *
 * The device communicates to the HASH via 32-bit-wide control registers
 * accessible via the 32-bit width AMBA rev. 2.0 AHB Bus. Below is a structure
 * with the registers used.
 */
struct hash_register {
	u32 cr;
	u32 din;
	u32 str;
	u32 hx[8];

	u32 padding0[(0x080 - 0x02C) / sizeof(u32)];

	u32 itcr;
	u32 itip;
	u32 itop;

	u32 padding1[(0x0F8 - 0x08C) / sizeof(u32)];

	u32 csfull;
	u32 csdatain;
	u32 csrx[HASH_CSR_COUNT];

	u32 padding2[(0xFE0 - 0x1D0) / sizeof(u32)];

	u32 periphid0;
	u32 periphid1;
	u32 periphid2;
	u32 periphid3;

	u32 cellid0;
	u32 cellid1;
	u32 cellid2;
	u32 cellid3;
};

/**
 * struct hash_state - Hash context state.
 * @temp_cr: Temporary HASH Control Register
 * @str_reg: HASH Start Register
 * @din_reg: HASH Data Input Register
 * @csr[52]: HASH Context Swap Registers 0-39
 * @csfull: HASH Context Swap Registers 40 ie Status flags
 * @csdatain: HASH Context Swap Registers 41 ie Input data
 * @buffer: Working buffer for messages going to the hardware
 * @length: Length of the part of the message hashed so far (floor(N/64) * 64)
 * @index: Valid number of bytes in buffer (N % 64)
 * @bit_index: Valid number of bits in buffer (N % 8)
 *
 * This structure is used between context switches, i.e. when ongoing jobs are
 * interupted with new jobs. When this happens we need to store intermediate
 * results in software.
 *
 * WARNING: "index" is the  member of the structure, to be sure  that "buffer"
 * is aligned on a 4-bytes boundary. This is highly implementation dependent
 * and MUST be checked whenever this code is ported on new platforms.
 */
struct hash_state {
	u32 temp_cr;
	u32 str_reg;
	u32 din_reg;
	u32 csr[52];
	u32 csfull;
	u32 csdatain;
	u32 buffer[HASH_BLOCK_SIZE / sizeof(u32)];
	struct uint64 length;
	u8 index;
	u8 bit_index;
};

/**
 * struct hash_system_context - Structure for the global system context.
 * @registry: Pointer to the registry of the hash hardware
 * @state: State of the hash device
 */
struct hash_system_context {
	/*
	 * Pointer to HASH registers structure. We know that this gives a
	 * checkpatch warning and in the current design it needs to be a
	 * volatile. We will change it when we will rewrite the driver similar
	 * to how we have done in cryp-part. We have also read
	 * Documentation/volatile-considered-harmful.txt as checkpatch tell
	 * us to do.
	 */
	volatile struct hash_register *registry[MAX_HASH_DEVICE];

	/* State of HASH device */
	struct hash_state state[MAX_HASH_DEVICE];
};

/**
 * enum hash_device_id - HASH device ID.
 * @HASH_DEVICE_ID_0: Hash hardware with ID 0
 * @HASH_DEVICE_ID_1: Hash hardware with ID 1
 */
enum hash_device_id {
	HASH_DEVICE_ID_0 = 0,
	HASH_DEVICE_ID_1 = 1
};

/**
 * enum hash_data_format - HASH data format.
 * @HASH_DATA_32_BITS: 32 bits data format
 * @HASH_DATA_16_BITS: 16 bits data format
 * @HASH_DATA_8_BITS: 8 bits data format
 * @HASH_DATA_1_BITS: 1 bit data format
 */
enum hash_data_format {
	HASH_DATA_32_BITS = 0x0,
	HASH_DATA_16_BITS = 0x1,
	HASH_DATA_8_BITS = 0x2,
	HASH_DATA_1_BIT = 0x3
};

/**
 * struct hash_protection_config - Device protection configuration.
 * @privilege_access: FIXME, add comment.
 * @secure_access: FIXME, add comment.
 */
struct hash_protection_config {
	int privilege_access;
	int secure_access;
};

/**
 * enum hash_algo - Enumeration for selecting between SHA1 or SHA2 algorithm
 * @HASH_ALGO_SHA1: Indicates that SHA1 is used.
 * @HASH_ALGO_SHA2: Indicates that SHA2 (SHA256) is used.
 */
enum hash_algo {
	HASH_ALGO_SHA1 = 0x0,
	HASH_ALGO_SHA2 = 0x1
};

/**
 * enum hash_op - Enumeration for selecting between HASH or HMAC mode
 * @HASH_OPER_MODE_HASH: Indicates usage of normal HASH mode
 * @HASH_OPER_MODE_HMAC: Indicates usage of HMAC
 */
enum hash_op {
	HASH_OPER_MODE_HASH = 0x0,
	HASH_OPER_MODE_HMAC = 0x1
};

/**
 * struct hash_config - Configuration data for the hardware
 * @data_format: Format of data entered into the hash data in register
 * @algorithm: Algorithm selection bit
 * @oper_mode: Operating mode selection bit
 */
struct hash_config {
	int data_format;
	int algorithm;
	int oper_mode;
};

/**
 * enum hash_rv - Return values / error codes for hash.
 */
enum hash_rv {
	HASH_OK = 0,
	HASH_MSG_LENGTH_OVERFLOW,
	HASH_INVALID_PARAMETER,
	HASH_UNSUPPORTED_HW
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

int hash_init_base_address(int hash_device_id, t_logical_address base_address);

int hash_setconfiguration(int hash_device_id, struct hash_config *p_config);

void hash_begin(struct hash_ctx *ctx);

void hash_get_digest(int hid, u8 *digest, int algorithm);

int hash_hw_update(struct shash_desc *desc,
		int hash_device_id,
		const u8 *p_data_buffer,
		u32 msg_length);

int hash_end(struct hash_ctx *ctx, u8 digest[HASH_MSG_DIGEST_SIZE]);

int hash_save_state(int hash_device_id, struct hash_state *state);

int hash_resume_state(int hash_device_id, const struct hash_state *state);

#endif

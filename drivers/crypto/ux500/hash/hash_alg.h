#ifndef _HASH_ALG_H
#define _HASH_ALG_H
/*
 * Copyright (C) 2010 ST-Ericsson.
 * Copyright (C) 2010 STMicroelectronics.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <mach/hcl_defs.h>

/* Number of bytes the message digest */
#define HASH_MSG_DIGEST_SIZE    32
#define HASH_BLOCK_SIZE         64

#define __HASH_ENHANCED

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

#define HASH_SET_DIN(val)   HCL_WRITE_REG(g_sys_ctx.registry[hid]->din, (val))

#define HASH_INITIALIZE \
	HCL_WRITE_BITS( \
		g_sys_ctx.registry[hid]->cr, \
		0x01 << HASH_CR_INIT_POS, \
		HASH_CR_INIT_MASK)

#define HASH_SET_DATA_FORMAT(data_format) \
		HCL_WRITE_BITS( \
			g_sys_ctx.registry[hid]->cr, \
			(u32) (data_format) << HASH_CR_DATAFORM_POS, \
			HASH_CR_DATAFORM_MASK)

#define HASH_GET_HX(pos) \
		HCL_READ_REG(g_sys_ctx.registry[hid]->hx[pos])

#define HASH_SET_HX(pos, val) \
		HCL_WRITE_REG(g_sys_ctx.registry[hid]->hx[pos], (val));

#define HASH_SET_NBLW(val) \
		HCL_WRITE_BITS( \
			g_sys_ctx.registry[hid]->str, \
			(u32) (val) << HASH_STR_NBLW_POS, \
			HASH_STR_NBLW_MASK)

#define HASH_SET_DCAL \
		HCL_WRITE_BITS( \
			g_sys_ctx.registry[hid]->str, \
			0x01 << HASH_STR_DCAL_POS, \
			HASH_STR_DCAL_MASK)

/**
 * struct uint64 - Structure to handle 64 bits integers.
 * @high_word: Most significant bits
 * @high_word: Least significant bits
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

	u32 padding0[(0x080 - 0x02C) >> 2];

	u32 itcr;
	u32 itip;
	u32 itop;

	u32 padding1[(0x0F8 - 0x08C) >> 2];

	u32 csfull;
	u32 csdatain;
	u32 csrx[HASH_CSR_COUNT];

	u32 padding2[(0xFE0 - 0x1D0) >> 2];

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
	/* Pointer to HASH registers structure */
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
 * enum hash_device_state - Device state
 * @DISABLE: Disable the hash hardware
 * @ENABLE: Enable the hash hardware
 */
enum hash_device_state {
	DISABLE = 0,
	ENABLE = 1
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
 * enum hash_input_status - Data Input flag status.
 * @HASH_DIN_EMPTY: Indicates that nothing is in data registers
 * @HASH_DIN_FULL: Indicates that data registers are full
 */
enum hash_input_status {
	HASH_DIN_EMPTY = 0,
	HASH_DIN_FULL = 1
};

/**
 * Number of words already pushed
 */
enum hash_nbw_pushed {
	HASH_NBW_00 = 0x00,
	HASH_NBW_01 = 0x01,
	HASH_NBW_02 = 0x02,
	HASH_NBW_03 = 0x03,
	HASH_NBW_04 = 0x04,
	HASH_NBW_05 = 0x05,
	HASH_NBW_06 = 0x06,
	HASH_NBW_07 = 0x07,
	HASH_NBW_08 = 0x08,
	HASH_NBW_09 = 0x09,
	HASH_NBW_10 = 0x0A,
	HASH_NBW_11 = 0x0B,
	HASH_NBW_12 = 0x0C,
	HASH_NBW_13 = 0x0D,
	HASH_NBW_14 = 0x0E,
	HASH_NBW_15 = 0x0F
};

/**
 * struct hash_device_status - Device status for DINF, NBW, and NBLW bit
 *                             fields.
 * @dinf_status: HASH data in full flag
 * @nbw_status: Number of words already pushed
 * @nblw_status: Number of Valid Bits Last Word of the Message
 */
struct hash_device_status {
	int dinf_status;
	int nbw_status;
	u8 nblw_status;
};

/**
 * enum hash_dma_request - Enumeration for HASH DMA request types.
 */
enum hash_dma_request {
	HASH_DISABLE_DMA_REQ = 0x0,
	HASH_ENABLE_DMA_REQ = 0x1
};

/**
 * enum hash_digest_cal - Enumeration for digest calculation.
 * @HASH_DISABLE_DCAL: Indicates that DCAL bit is not set/used.
 * @HASH_ENABLE_DCAL: Indicates that DCAL bit is set/used.
 */
enum hash_digest_cal {
	HASH_DISABLE_DCAL = 0x0,
	HASH_ENABLE_DCAL = 0x1
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
 * enum hash_key_type - Enumeration for selecting between long and short key.
 * @HASH_SHORT_KEY: Key used is shorter or equal to block size (64 bytes)
 * @HASH_LONG_KEY: Key used is greater than block size (64 bytes)
 */
enum hash_key_type {
	HASH_SHORT_KEY = 0x0,
	HASH_LONG_KEY = 0x1
};

/**
 * struct hash_config - Configuration data for the hardware
 * @data_format: Format of data entered into the hash data in register
 * @algorithm: Algorithm selection bit
 * @oper_mode: Operating mode selection bit
 * @hmac_key: Long key selection bit HMAC mode
 */
struct hash_config {
	int data_format;
	int algorithm;
	int oper_mode;
	int hmac_key;
};


/**
 * enum hash_error - Error codes for hash.
 */
enum hash_error {
	HASH_OK = 0,
	HASH_MSG_LENGTH_OVERFLOW,
	HASH_INTERNAL_ERROR,
	HASH_NOT_CONFIGURED,
	HASH_REQUEST_PENDING,
	HASH_REQUEST_NOT_APPLICABLE,
	HASH_INVALID_PARAMETER,
	HASH_UNSUPPORTED_FEATURE,
	HASH_UNSUPPORTED_HW
};

int hash_init_base_address(int hash_device_id, t_logical_address base_address);

int HASH_GetVersion(t_version *p_version);

int HASH_Reset(int hash_devive_id);

int HASH_ConfigureDmaRequest(int hash_device_id, int request_state);

int HASH_ConfigureLastValidBits(int hash_device_id, u8 nblw_val);

int HASH_ConfigureDigestCal(int hash_device_id, int dcal_state);

int HASH_ConfigureProtection(int hash_device_id,
		struct hash_protection_config
		*p_protect_config);

int hash_setconfiguration(int hash_device_id, struct hash_config *p_config);

int hash_begin(int hash_device_id);

int hash_get_digest(int hash_device_id, u8 digest[HASH_MSG_DIGEST_SIZE]);

int HASH_ClockGatingOff(int hash_device_id);

struct hash_device_status HASH_GetDeviceStatus(int hash_device_id);

t_bool HASH_IsDcalOngoing(int hash_device_id);

int hash_hw_update(int hash_device_id,
		const u8 *p_data_buffer,
		u32 msg_length);

int hash_end(int hash_device_id, u8 digest[HASH_MSG_DIGEST_SIZE]);

int hash_compute(int hash_device_id,
		const u8 *p_data_buffer,
		u32 msg_length,
		struct hash_config *p_hash_config,
		u8 digest[HASH_MSG_DIGEST_SIZE]);

int hash_end_key(int hash_device_id);

int hash_save_state(int hash_device_id, struct hash_state *state);

int hash_resume_state(int hash_device_id, const struct hash_state *state);

#ifdef __cplusplus
}
#endif
#endif


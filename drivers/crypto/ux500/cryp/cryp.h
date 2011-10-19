/**
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Shujuan Chen <shujuan.chen@stericsson.com> for ST-Ericsson.
 * Author: Jonas Linde <jonas.linde@stericsson.com> for ST-Ericsson.
 * Author: Joakim Bech <joakim.xx.bech@stericsson.com> for ST-Ericsson.
 * Author: Berne Hebark <berne.herbark@stericsson.com> for ST-Ericsson.
 * Author: Niklas Hernaeus <niklas.hernaeus@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef _CRYP_H_
#define _CRYP_H_

#include <linux/completion.h>
#include <linux/dmaengine.h>
#include <linux/klist.h>
#include <linux/mutex.h>

/* Module Defines */
#define CRYP_MODULE_NAME	"CRYP HCL Module"

#define DEV_DBG_NAME "crypX crypX:"

/* CRYP enable/disable */
enum cryp_crypen {
	CRYP_CRYPEN_DISABLE = 0,
	CRYP_CRYPEN_ENABLE = 1
};

/* CRYP Start Computation enable/disable */
enum cryp_start {
	CRYP_START_DISABLE = 0,
	CRYP_START_ENABLE = 1
};

/* CRYP Init Signal enable/disable */
enum cryp_init {
	CRYP_INIT_DISABLE = 0,
	CRYP_INIT_ENABLE = 1
};

/* Cryp State enable/disable */
enum cryp_state {
	CRYP_STATE_DISABLE = 0,
	CRYP_STATE_ENABLE = 1
};

/* Key preparation bit enable */
enum cryp_key_prep {
	KSE_DISABLED,
	KSE_ENABLED
};

/* Key size for AES*/
#define	CRYP_KEY_SIZE_128 (0)
#define	CRYP_KEY_SIZE_192 (1)
#define	CRYP_KEY_SIZE_256 (2)

/* Data type Swap */
#define	CRYP_DATA_TYPE_32BIT_SWAP (0)
#define	CRYP_DATA_TYPE_16BIT_SWAP (1)
#define	CRYP_DATA_TYPE_8BIT_SWAP  (2)
#define	CRYP_DATA_TYPE_BIT_SWAP   (3)

/* AES modes */
enum cryp_algo_mode {
	CRYP_ALGO_TDES_ECB,
	CRYP_ALGO_TDES_CBC,
	CRYP_ALGO_DES_ECB,
	CRYP_ALGO_DES_CBC,
	CRYP_ALGO_AES_ECB,
	CRYP_ALGO_AES_CBC,
	CRYP_ALGO_AES_CTR,
	CRYP_ALGO_AES_XTS
};

/* Cryp Encryption or Decryption */
enum cryp_algorithm_dir {
	CRYP_ALGORITHM_ENCRYPT,
	CRYP_ALGORITHM_DECRYPT
};

/* Hardware access method */
enum cryp_mode {
	CRYP_MODE_POLLING,
	CRYP_MODE_INTERRUPT,
	CRYP_MODE_DMA
};

/**
 * struct cryp_config -
 * @key_access: Cryp state enable/disable
 * @key_size: Key size for AES
 * @data_type: Data type Swap
 * @algo_mode: AES modes
 * @encrypt_or_decrypt: Cryp Encryption or Decryption
 *
 * CRYP configuration structure to be passed to set configuration
 */
struct cryp_config {
	enum cryp_state key_access;
	int key_size;
	int data_type;
	enum cryp_algo_mode algo_mode;
	enum cryp_algorithm_dir encrypt_or_decrypt;
};

/**
 * struct cryp_protection_config -
 * @privilege_access: Privileged cryp state enable/disable
 * @secure_access: Secure cryp state enable/disable
 *
 * Protection configuration structure for setting privilage access
 */
struct cryp_protection_config {
	enum cryp_state privilege_access;
	enum cryp_state secure_access;
};

/* Cryp status */
enum cryp_status_id {
	CRYP_STATUS_BUSY = 0x10,
	CRYP_STATUS_OUTPUT_FIFO_FULL = 0x08,
	CRYP_STATUS_OUTPUT_FIFO_NOT_EMPTY = 0x04,
	CRYP_STATUS_INPUT_FIFO_NOT_FULL = 0x02,
	CRYP_STATUS_INPUT_FIFO_EMPTY = 0x01
};

/* Cryp DMA interface */
enum cryp_dma_req_type {
	CRYP_DMA_DISABLE_BOTH,
	CRYP_DMA_ENABLE_IN_DATA,
	CRYP_DMA_ENABLE_OUT_DATA,
	CRYP_DMA_ENABLE_BOTH_DIRECTIONS
};

enum cryp_dma_channel {
	CRYP_DMA_RX = 0,
	CRYP_DMA_TX
};

/* Key registers */
enum cryp_key_reg_index {
	CRYP_KEY_REG_1,
	CRYP_KEY_REG_2,
	CRYP_KEY_REG_3,
	CRYP_KEY_REG_4
};

/* Key register left and right */
struct cryp_key_value {
	u32 key_value_left;
	u32 key_value_right;
};

/* Cryp Initialization structure */
enum cryp_init_vector_index {
	CRYP_INIT_VECTOR_INDEX_0,
	CRYP_INIT_VECTOR_INDEX_1
};

/* struct cryp_init_vector_value -
 * @init_value_left
 * @init_value_right
 * */
struct cryp_init_vector_value {
	u32 init_value_left;
	u32 init_value_right;
};

/**
 * struct cryp_device_context - structure for a cryp context.
 * @cr: control register
 * @dmacr: DMA control register
 * @imsc: Interrupt mask set/clear register
 * @key_1_l: Key 1l register
 * @key_1_r: Key 1r register
 * @key_2_l: Key 2l register
 * @key_2_r: Key 2r register
 * @key_3_l: Key 3l register
 * @key_3_r: Key 3r register
 * @key_4_l: Key 4l register
 * @key_4_r: Key 4r register
 * @init_vect_0_l: Initialization vector 0l register
 * @init_vect_0_r: Initialization vector 0r register
 * @init_vect_1_l: Initialization vector 1l register
 * @init_vect_1_r: Initialization vector 0r register
 * @din: Data in register
 * @dout: Data out register
 *
 * CRYP power management specifc structure.
 */
struct cryp_device_context {
	u32 cr;
	u32 dmacr;
	u32 imsc;

	u32 key_1_l;
	u32 key_1_r;
	u32 key_2_l;
	u32 key_2_r;
	u32 key_3_l;
	u32 key_3_r;
	u32 key_4_l;
	u32 key_4_r;

	u32 init_vect_0_l;
	u32 init_vect_0_r;
	u32 init_vect_1_l;
	u32 init_vect_1_r;

	u32 din;
	u32 dout;
};

struct cryp_dma {
	dma_cap_mask_t mask;
	struct completion cryp_dma_complete;
	struct dma_chan *chan_cryp2mem;
	struct dma_chan *chan_mem2cryp;
	struct stedma40_chan_cfg *cfg_cryp2mem;
	struct stedma40_chan_cfg *cfg_mem2cryp;
	int sg_src_len;
	int sg_dst_len;
	struct scatterlist *sg_src;
	struct scatterlist *sg_dst;
	int nents_src;
	int nents_dst;
};

/**
 * struct cryp_device_data - structure for a cryp device.
 * @base: Pointer to the hardware base address.
 * @dev: Pointer to the devices dev structure.
 * @cryp_irq_complete: Pointer to an interrupt completion structure.
 * @clk: Pointer to the device's clock control.
 * @pwr_regulator: Pointer to the device's power control.
 * @power_status: Current status of the power.
 * @ctx_lock: Lock for current_ctx.
 * @current_ctx: Pointer to the currently allocated context.
 * @list_node: For inclusion into a klist.
 * @dma: The dma structure holding channel configuration.
 * @power_state: TRUE = power state on, FALSE = power state off.
 * @power_state_mutex: Mutex for power_state.
 * @restore_dev_ctx: TRUE = saved ctx, FALSE = no saved ctx.
 */
struct cryp_device_data {
	struct cryp_register __iomem *base;
	struct device *dev;
	struct completion cryp_irq_complete;
	struct clk *clk;
	struct regulator *pwr_regulator;
	int power_status;
	struct spinlock ctx_lock;
	struct cryp_ctx *current_ctx;
	struct klist_node list_node;
	struct cryp_dma dma;
	bool power_state;
	struct mutex power_state_mutex;
	bool restore_dev_ctx;
};

void cryp_wait_until_done(struct cryp_device_data *device_data);

/* Initialization functions */

int cryp_check(struct cryp_device_data *device_data);

void cryp_reset(struct cryp_device_data *device_data);

void cryp_activity(struct cryp_device_data *device_data,
		   enum cryp_crypen cryp_crypen);

void cryp_start(struct cryp_device_data *device_data);

void cryp_init_signal(struct cryp_device_data *device_data,
		      enum cryp_init cryp_init);

void cryp_key_preparation(struct cryp_device_data *device_data,
			  enum cryp_key_prep cryp_key_prep);

void cryp_flush_inoutfifo(struct cryp_device_data *device_data);

void cryp_cen_flush(struct cryp_device_data *device_data);

void cryp_set_dir(struct cryp_device_data *device_data, int dir);

int cryp_set_configuration(struct cryp_device_data *device_data,
			   struct cryp_config *p_cryp_config);

int cryp_get_configuration(struct cryp_device_data *device_data,
			   struct cryp_config *p_cryp_config);

void cryp_configure_for_dma(struct cryp_device_data *device_data,
			    enum cryp_dma_req_type dma_req);

int cryp_configure_key_values(struct cryp_device_data *device_data,
			      enum cryp_key_reg_index key_reg_index,
			      struct cryp_key_value key_value);

int cryp_configure_init_vector(struct cryp_device_data *device_data,
			       enum cryp_init_vector_index
			       init_vector_index,
			       struct cryp_init_vector_value
			       init_vector_value);

int cryp_configure_protection(struct cryp_device_data *device_data,
			      struct cryp_protection_config *p_protect_config);

/* Power management funtions */
void cryp_save_device_context(struct cryp_device_data *device_data,
			      struct cryp_device_context *ctx);

void cryp_restore_device_context(struct cryp_device_data *device_data,
				 struct cryp_device_context *ctx);

/* Data transfer and status bits. */
int cryp_is_logic_busy(struct cryp_device_data *device_data);

int cryp_get_status(struct cryp_device_data *device_data);

/**
 * cryp_write_indata - This routine writes 32 bit data into the data input
 *		       register of the cryptography IP.
 * @device_data: Pointer to the device data struct for base address.
 * @write_data: Data to write.
 */
int cryp_write_indata(struct cryp_device_data *device_data, u32 write_data);

/**
 * cryp_read_outdata - This routine reads the data from the data output
 *		       register of the CRYP logic
 * @device_data: Pointer to the device data struct for base address.
 * @read_data: Read the data from the output FIFO.
 */
int cryp_read_outdata(struct cryp_device_data *device_data, u32 *read_data);

#endif /* _CRYP_H_ */

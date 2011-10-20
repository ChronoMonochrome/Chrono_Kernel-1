/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef __I2S_TEST_PROT_DRIVER_IF_H__
#define __I2S_TEST_PROT_DRIVER_IF_H__
#include <mach/msp.h>
struct test_prot_config {
	u32 tx_config_desc;
	u32 rx_config_desc;
	u32 frame_freq;
	u32 frame_size;
	u32 data_size;
	u32 direction;
	u32 protocol;
	u32 work_mode;
	struct msp_protocol_desc protocol_desc;
	void (*handler) (void *data);
	void *tx_callback_data;
	void *rx_callback_data;
	int multichannel_configured;
	struct msp_multichannel_config multichannel_config;
};

int i2s_testprot_drv_open(int i2s_device_num);
int i2s_testprot_drv_configure(int i2s_device_num,
			       struct test_prot_config *config);
int i2s_config_default_protocol(int i2s_device_num,
			       struct test_prot_config *config);
int __i2s_testprot_drv_configure(int i2s_device_num,
			       struct test_prot_config *config,
			       bool use_default);
int i2s_testprot_drv_transfer(int i2s_device_num, void *txdata, size_t txbytes,
			      void *rxdata, size_t rxbytes,
			      enum i2s_transfer_mode_t transfer_mode);
int i2s_testprot_drv_close(int i2s_device_num);

#endif

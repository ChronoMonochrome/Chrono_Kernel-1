/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef __BOARD_U5500_H
#define __BOARD_U5500_H

#define GPIO_SDMMC_CD          180
#define GPIO_MMC_CARD_CTRL     227
#define GPIO_MMC_CARD_VSEL     185
#define GPIO_PRIMARY_CAM_XSHUTDOWN  1
#define GPIO_SECONDARY_CAM_XSHUTDOWN  2


struct ab5500_regulator_platform_data;
extern struct ab5500_regulator_platform_data u5500_ab5500_regulator_data;

extern void u5500_pins_init(void);
extern void __init u5500_regulators_init(void);

#endif

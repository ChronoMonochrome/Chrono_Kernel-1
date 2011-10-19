/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Martin Persson for ST-Ericsson
 *         Etienne Carriere <etienne.carriere@stericsson.com> for ST-Ericsson
 *
 */

#ifndef PRCMU_DEBUG_H
#define PRCMU_DEBUG_H

#ifdef CONFIG_DBX500_PRCMU_DEBUG
void prcmu_debug_ape_opp_log(u8 opp);
void prcmu_debug_ddr_opp_log(u8 opp);
void prcmu_debug_arm_opp_log(u8 opp);
#else
static inline void prcmu_debug_ape_opp_log(u8 opp) {}
static inline void prcmu_debug_ddr_opp_log(u8 opp) {}
static inline void prcmu_debug_arm_opp_log(u8 opp) {}
#endif
#endif

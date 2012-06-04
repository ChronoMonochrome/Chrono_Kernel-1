/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Mali related Ux500 platform initialization
 *
 * Author: Marta Lofstedt <marta.lofstedt@stericsson.com> for
 * ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for ST-Ericsson's Ux500 platforms
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"

#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/workqueue.h>

#include <linux/mfd/dbx500-prcmu.h>

#define MALI_HIGH_TO_LOW_LEVEL_UTILIZATION_LIMIT 64
#define MALI_LOW_TO_HIGH_LEVEL_UTILIZATION_LIMIT 192

static int is_running;
static struct regulator *regulator;
static struct clk *clk_sga;
static u32 last_utilization;
static struct work_struct mali_utilization_work;
static struct workqueue_struct *mali_utilization_workqueue;

/* Rationale behind the values for:
* MALI_HIGH_LEVEL_UTILIZATION_LIMIT and MALI_LOW_LEVEL_UTILIZATION_LIMIT
* When operating at half clock frequency a faster clock is requested when
* reaching 75% utilization. When operating at full clock frequency a slower
* clock is requested when reaching 25% utilization. There is a margin of 25%
* at the high range of the slow clock to avoid complete saturation of the
* hardware and there is some overlap to avoid an oscillating situation where
* the clock goes back and forth from high to low.
*
* Utilization on full speed clock
* 0               64             128             192              255
* |---------------|---------------|---------------|---------------|
*                 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*                 |       ^
*                 V       |
* XXXXXXXXXXXXXXXXXXXXXXXXX
* 0       64     128     192      255
* |-------|-------|-------|-------|
* Utilization on half speed clock
*/
void mali_utilization_function(struct work_struct *ptr)
{
	/*By default, platform start with 50% APE OPP and 25% DDR OPP*/
	static u32 has_requested_low = 1;

	if (last_utilization > MALI_LOW_TO_HIGH_LEVEL_UTILIZATION_LIMIT) {
		if (has_requested_low) {
			MALI_DEBUG_PRINT(5, ("MALI GPU utilization: %u SIGNAL_HIGH\n", last_utilization));
			/*Request 100% APE_OPP.*/
			if (prcmu_qos_add_requirement(PRCMU_QOS_APE_OPP, "mali", 100)) {
				MALI_DEBUG_PRINT(2, ("MALI 100% APE_OPP failed\n"));
				return;
			}
			/*
			* Since the utilization values will be reported higher
			* if DDR_OPP is lowered, we also request 100% DDR_OPP.
			*/
			if (prcmu_qos_add_requirement(PRCMU_QOS_DDR_OPP, "mali", 100)) {
				MALI_DEBUG_PRINT(2, ("MALI 100% DDR_OPP failed\n"));
				return;
			}
			has_requested_low = 0;
		}
	} else {
		if (last_utilization < MALI_HIGH_TO_LOW_LEVEL_UTILIZATION_LIMIT) {
			if (!has_requested_low) {
				/*Remove APE_OPP and DDR_OPP requests*/
				prcmu_qos_remove_requirement(PRCMU_QOS_APE_OPP, "mali");
				prcmu_qos_remove_requirement(PRCMU_QOS_DDR_OPP, "mali");
				MALI_DEBUG_PRINT(5, ("MALI GPU utilization: %u SIGNAL_LOW\n", last_utilization));
				has_requested_low = 1;
			}
		}
	}
	MALI_DEBUG_PRINT(5, ("MALI GPU utilization: %u\n", last_utilization));
}

_mali_osk_errcode_t mali_platform_init(_mali_osk_resource_t *resource)
{
	is_running = 0;
	last_utilization = 0;

	mali_utilization_workqueue = create_singlethread_workqueue("mali_utilization_workqueue");
	if (NULL == mali_utilization_workqueue) {
		MALI_DEBUG_PRINT(2, ("%s: Failed to setup workqueue %s\n", __func__, "v-mali"));
		goto error;
	}
	INIT_WORK(&mali_utilization_work, mali_utilization_function);

	regulator = regulator_get(NULL, "v-mali");
	if (IS_ERR(regulator)) {
		MALI_DEBUG_PRINT(2, ("%s: Failed to get regulator %s\n", __func__, "v-mali"));
		goto error;
	}

	clk_sga = clk_get_sys("mali", NULL);
	if (IS_ERR(clk_sga)) {
		regulator_put(regulator);
		MALI_DEBUG_PRINT(2, ("%s: Failed to get clock %s\n", __func__, "mali"));
		goto error;
	}

	MALI_SUCCESS;
error:
	MALI_DEBUG_PRINT(1, ("SGA initialization failed.\n"));
	MALI_ERROR(_MALI_OSK_ERR_FAULT);
}

_mali_osk_errcode_t mali_platform_deinit(_mali_osk_resource_type_t *type)
{
	destroy_workqueue(mali_utilization_workqueue);
	regulator_put(regulator);
	clk_put(clk_sga);
	MALI_DEBUG_PRINT(2, ("SGA terminated.\n"));
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_powerdown(u32 cores)
{
	if (is_running) {
		clk_disable(clk_sga);
		if (regulator) {
			int ret = regulator_disable(regulator);
			if (ret < 0) {
				MALI_DEBUG_PRINT(2, ("%s: Failed to disable regulator %s\n", __func__, "v-mali"));
				is_running = 0;
				MALI_ERROR(_MALI_OSK_ERR_FAULT);
			}
		}
		is_running = 0;
	}
	MALI_DEBUG_PRINT(4, ("mali_platform_powerdown is_running: %u cores: %u\n", is_running, cores));
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_powerup(u32 cores)
{
	if (!is_running) {
		int ret = regulator_enable(regulator);
		if (ret < 0) {
			MALI_DEBUG_PRINT(2, ("%s: Failed to enable regulator %s\n", __func__, "v-mali"));
			goto error;
		}

		ret = clk_enable(clk_sga);
		if (ret < 0) {
			regulator_disable(regulator);
			MALI_DEBUG_PRINT(2, ("%s: Failed to enable clock %s\n", __func__, "mali"));
			goto error;
		}
		is_running = 1;
	}
	MALI_DEBUG_PRINT(4, ("mali_platform_powerup is_running:%u cores: %u\n", is_running, cores));
	MALI_SUCCESS;
error:
	MALI_DEBUG_PRINT(1, ("Failed to power up.\n"));
	MALI_ERROR(_MALI_OSK_ERR_FAULT);
}

void mali_gpu_utilization_handler(u32 utilization)
{
	last_utilization = utilization;
	/*
	* We should not cancel the potentially not yet run old work
	* in favor of a new work.
	* Since the utilization value will change,
	* the mali_utilization_function will evaluate based on
	* what is the utilization now and not on what it was
	* when it was scheduled.
	*/
	queue_work(mali_utilization_workqueue, &mali_utilization_work);
}



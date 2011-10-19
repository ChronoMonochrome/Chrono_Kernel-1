/*
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 *
 */
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <mach/pm.h>

#ifdef CONFIG_UX500_SUSPEND_STANDBY
static u32 sleep_enabled = 1;
#else
static u32 sleep_enabled;
#endif

#ifdef CONFIG_UX500_SUSPEND_MEM
static u32 deepsleep_enabled = 1;
#else
static u32 deepsleep_enabled;
#endif

static u32 suspend_enabled = 1;

static u32 deepsleeps_done;
static u32 deepsleeps_failed;
static u32 sleeps_done;
static u32 sleeps_failed;
static u32 suspend_count;

#ifdef CONFIG_UX500_SUSPEND_DBG_WAKE_ON_UART
void ux500_suspend_dbg_add_wake_on_uart(void)
{
	irq_set_irq_wake(GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 1);
	irq_set_irq_type(GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN),
		     IRQ_TYPE_EDGE_BOTH);
}

void ux500_suspend_dbg_remove_wake_on_uart(void)
{
	irq_set_irq_wake(GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 0);
}
#endif

bool ux500_suspend_enabled(void)
{
	return suspend_enabled != 0;
}

bool ux500_suspend_sleep_enabled(void)
{
	return sleep_enabled != 0;
}

bool ux500_suspend_deepsleep_enabled(void)
{
	return deepsleep_enabled != 0;
}

void ux500_suspend_dbg_sleep_status(bool is_deepsleep)
{
	enum prcmu_idle_stat prcmu_status;

	prcmu_status = ux500_pm_prcmu_idle_stat();

	if (is_deepsleep) {
		pr_info("Returning from ApDeepSleep. PRCMU ret: 0x%x - %s\n",
			prcmu_status,
			prcmu_status == DEEP_SLEEP_OK ? "Success" : "Fail!");
		if (prcmu_status == DEEP_SLEEP_OK)
			deepsleeps_done++;
		else
			deepsleeps_failed++;
	} else {
		pr_info("Returning from ApSleep. PRCMU ret: 0x%x - %s\n",
			prcmu_status,
			prcmu_status == SLEEP_OK ? "Success" : "Fail!");
		if (prcmu_status == SLEEP_OK)
			sleeps_done++;
		else
			sleeps_failed++;
	}
}

int ux500_suspend_dbg_begin(suspend_state_t state)
{
	suspend_count++;
	return 0;
}

void ux500_suspend_dbg_init(void)
{
	struct dentry *suspend_dir;
	struct dentry *file;

	suspend_dir = debugfs_create_dir("suspend", NULL);
	if (IS_ERR_OR_NULL(suspend_dir))
		return;

	file = debugfs_create_bool("sleep", S_IWUGO | S_IRUGO,
				   suspend_dir,
				   &sleep_enabled);
	if (IS_ERR_OR_NULL(file))
		goto error;

	file = debugfs_create_bool("deepsleep", S_IWUGO | S_IRUGO,
				   suspend_dir,
				   &deepsleep_enabled);
	if (IS_ERR_OR_NULL(file))
		goto error;

	file = debugfs_create_bool("enable", S_IWUGO | S_IRUGO,
				   suspend_dir,
				   &suspend_enabled);
	if (IS_ERR_OR_NULL(file))
		goto error;

	file = debugfs_create_u32("count", S_IRUGO,
				  suspend_dir,
				  &suspend_count);
	if (IS_ERR_OR_NULL(file))
		goto error;

	file = debugfs_create_u32("sleep_count", S_IRUGO,
				  suspend_dir,
				  &sleeps_done);
	if (IS_ERR_OR_NULL(file))
		goto error;

	file = debugfs_create_u32("deepsleep_count", S_IRUGO,
				  suspend_dir,
				  &deepsleeps_done);
	if (IS_ERR_OR_NULL(file))
		goto error;


	file = debugfs_create_u32("sleep_failed", S_IRUGO,
				  suspend_dir,
				  &sleeps_failed);
	if (IS_ERR_OR_NULL(file))
		goto error;

	file = debugfs_create_u32("deepsleep_failed", S_IRUGO,
				  suspend_dir,
				  &deepsleeps_failed);
	if (IS_ERR_OR_NULL(file))
		goto error;

	return;
error:
	if (!IS_ERR_OR_NULL(suspend_dir))
		debugfs_remove_recursive(suspend_dir);
}

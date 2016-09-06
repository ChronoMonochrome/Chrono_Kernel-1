/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Biju Das <biju.das@stericsson.com> for ST-Ericsson
 * Author: Kumar Sanghvi <kumar.sanghvi@stericsson.com> for ST-Ericsson
 * Author: Arun Murthy <arun.murthy@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/netlink.h>
#include <linux/kthread.h>
#include <linux/modem/shrm/shrm.h>
#include <linux/modem/shrm/shrm_driver.h>
#include <linux/modem/shrm/shrm_private.h>
#include <linux/modem/shrm/shrm_net.h>
#include <linux/modem/modem_client.h>
#include <linux/modem/u8500_client.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/mfd/abx500.h>
#include <mach/reboot_reasons.h>
#include <mach/suspend.h>
#include <mach/prcmu-debug.h>
#ifdef CONFIG_U8500_SHRM_ENABLE_FEATURE_SIGNATURE_VERIFICATION
#include <mach/open_modem_shared_memory.h>
#endif

#define L2_HEADER_ISI           0x0
#define L2_HEADER_RPC           0x1
#define L2_HEADER_AUDIO         0x2
#define L2_HEADER_SECURITY      0x3
#define L2_HEADER_COMMON_SIMPLE_LOOPBACK        0xC0
#define L2_HEADER_COMMON_ADVANCED_LOOPBACK      0xC1
#define L2_HEADER_AUDIO_SIMPLE_LOOPBACK         0x80
#define L2_HEADER_AUDIO_ADVANCED_LOOPBACK       0x81
#define L2_HEADER_CIQ           0xC3
#define L2_HEADER_RTC_CALIBRATION               0xC8
#define L2_HEADER_IPCCTRL 0xDC
#define L2_HEADER_IPCDATA 0xDD
#define L2_HEADER_SYSCLK3       0xE6
#define MAX_PAYLOAD 1024
#define MOD_STUCK_TIMEOUT       6
#define FIFO_FULL_TIMEOUT       1
#define PRCM_MOD_AWAKE_STATUS_PRCM_MOD_COREPD_AWAKE     BIT(0)
#define PRCM_MOD_AWAKE_STATUS_PRCM_MOD_AAPD_AWAKE       BIT(1)
#define PRCM_MOD_AWAKE_STATUS_PRCM_MOD_VMODEM_OFF_ISO   BIT(2)
#define PRCM_MOD_PURESET        BIT(0)
#define PRCM_MOD_SW_RESET       BIT(1)

#define PRCM_HOSTACCESS_REQ     0x334
#define PRCM_MOD_AWAKE_STATUS   0x4A0
#define PRCM_MOD_RESETN_VAL     0x204

extern void log_this(u8 pc, char* a, u32 extra1, char* b, u32 extra2);
extern u32 get_host_accessport_val(void);

extern u8 boot_state;
/*
static u8 recieve_common_msg[8*1024];
static u8 recieve_audio_msg[8*1024];
static received_msg_handler rx_common_handler;
static received_msg_handler rx_audio_handler;
static struct hrtimer mod_stuck_timer_0;
static struct hrtimer mod_stuck_timer_1;
static struct hrtimer fifo_full_timer;
struct sock *shrm_nl_sk;
*/
extern struct hrtimer timer;
extern struct shrm_dev *shm_dev;

extern char shrm_common_tx_state;
extern char shrm_common_rx_state;
extern char shrm_audio_tx_state;
extern char shrm_audio_rx_state;
extern atomic_t ac_sleep_disable_count;

void shm_ca_sleep_req_work(struct kthread_work *work)
{
	unsigned long flags;

	dev_dbg(shm_dev->dev, "%s:IRQ_PRCMU_CA_SLEEP\n", __func__);

	local_irq_save(flags);
	preempt_disable();
	if ((boot_state != BOOT_DONE) ||
			(readl(shm_dev->ca_reset_status_rptr))) {
		dev_err(shm_dev->dev, "%s:Modem state reset or unknown\n",
				__func__);
		preempt_enable();
		local_irq_restore(flags);
		return;
	}
	shrm_common_rx_state = SHRM_IDLE;
	shrm_audio_rx_state =  SHRM_IDLE;

	if (!get_host_accessport_val()) {
		dev_err(shm_dev->dev, "%s: host_accessport is low\n", __func__);
		queue_kthread_work(&shm_dev->shm_mod_stuck_kw,
				&shm_dev->shm_mod_reset_req);
		preempt_enable();
		local_irq_restore(flags);
		return;
	}

	log_this(40, NULL, 0, NULL, 0);
	trace_printk("CA_WAKE_ACK\n");
	writel((1<<GOP_CA_WAKE_ACK_BIT),
		shm_dev->intr_base + GOP_SET_REGISTER_BASE);
	preempt_enable();
	local_irq_restore(flags);

	hrtimer_start(&timer, ktime_set(0, 10*NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	if (suspend_sleep_is_blocked()) {
		suspend_unblock_sleep();
	}
	atomic_dec(&ac_sleep_disable_count);
}

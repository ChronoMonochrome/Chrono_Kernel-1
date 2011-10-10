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
#include <linux/workqueue.h>
#include <linux/modem/shrm/shrm.h>
#include <linux/modem/shrm/shrm_driver.h>
#include <linux/modem/shrm/shrm_private.h>
#include <linux/modem/shrm/shrm_net.h>
#include <linux/modem/modem_client.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <mach/reboot_reasons.h>
#include <mach/suspend.h>

#define L2_HEADER_ISI		0x0
#define L2_HEADER_RPC		0x1
#define L2_HEADER_AUDIO		0x2
#define L2_HEADER_SECURITY	0x3
#define L2_HEADER_COMMON_SIMPLE_LOOPBACK	0xC0
#define L2_HEADER_COMMON_ADVANCED_LOOPBACK	0xC1
#define L2_HEADER_AUDIO_SIMPLE_LOOPBACK		0x80
#define L2_HEADER_AUDIO_ADVANCED_LOOPBACK	0x81
#define L2_HEADER_CIQ		0xC3
#define MAX_PAYLOAD 1024

#define PRCM_HOSTACCESS_REQ   (_PRCMU_BASE + 0x334)

static u8 boot_state = BOOT_INIT;
static u8 recieve_common_msg[8*1024];
static u8 recieve_audio_msg[8*1024];
static received_msg_handler rx_common_handler;
static received_msg_handler rx_audio_handler;
static struct hrtimer timer;
struct sock *shrm_nl_sk;

static char shrm_common_tx_state = SHRM_SLEEP_STATE;
static char shrm_common_rx_state = SHRM_SLEEP_STATE;
static char shrm_audio_tx_state = SHRM_SLEEP_STATE;
static char shrm_audio_rx_state = SHRM_SLEEP_STATE;

static atomic_t ac_sleep_disable_count = ATOMIC_INIT(0);
static struct shrm_dev *shm_dev;

/* Spin lock and tasklet declaration */
DECLARE_TASKLET(shm_ca_0_tasklet, shm_ca_msgpending_0_tasklet, 0);
DECLARE_TASKLET(shm_ca_1_tasklet, shm_ca_msgpending_1_tasklet, 0);
DECLARE_TASKLET(shm_ac_read_0_tasklet, shm_ac_read_notif_0_tasklet, 0);
DECLARE_TASKLET(shm_ac_read_1_tasklet, shm_ac_read_notif_1_tasklet, 0);

static DEFINE_MUTEX(ac_state_mutex);

static DEFINE_SPINLOCK(ca_common_lock);
static DEFINE_SPINLOCK(ca_audio_lock);
static DEFINE_SPINLOCK(ca_wake_req_lock);
static DEFINE_SPINLOCK(boot_lock);

enum shrm_nl {
	SHRM_NL_MOD_RESET = 1,
	SHRM_NL_MOD_QUERY_STATE,
	SHRM_NL_USER_MOD_RESET,
	SHRM_NL_STATUS_MOD_ONLINE,
	SHRM_NL_STATUS_MOD_OFFLINE,
};

void shm_mod_reset_req_work(struct work_struct *work)
{
	prcmu_modem_reset();
}

static void shm_ac_sleep_req_work(struct work_struct *work)
{
	mutex_lock(&ac_state_mutex);
	if (atomic_read(&ac_sleep_disable_count) == 0)
		modem_release(shm_dev->modem);
	mutex_unlock(&ac_state_mutex);
}

static void shm_ac_wake_req_work(struct work_struct *work)
{
	mutex_lock(&ac_state_mutex);
	modem_request(shm_dev->modem);
	mutex_unlock(&ac_state_mutex);
}

static u32 get_host_accessport_val(void)
{
	u32 prcm_hostaccess;

	prcm_hostaccess = readl(PRCM_HOSTACCESS_REQ);
	wmb();
	prcm_hostaccess = prcm_hostaccess & 0x01;

	return prcm_hostaccess;
}
static enum hrtimer_restart callback(struct hrtimer *timer)
{
	unsigned long flags;

	spin_lock_irqsave(&ca_wake_req_lock, flags);
	if (((shrm_common_rx_state == SHRM_IDLE) ||
				(shrm_common_rx_state == SHRM_SLEEP_STATE))
			&& ((shrm_common_tx_state == SHRM_IDLE) ||
				(shrm_common_tx_state == SHRM_SLEEP_STATE))
			&& ((shrm_audio_rx_state == SHRM_IDLE)  ||
				(shrm_audio_rx_state == SHRM_SLEEP_STATE))
			&& ((shrm_audio_tx_state == SHRM_IDLE)  ||
				(shrm_audio_tx_state == SHRM_SLEEP_STATE))) {

		shrm_common_rx_state = SHRM_SLEEP_STATE;
		shrm_audio_rx_state = SHRM_SLEEP_STATE;
		shrm_common_tx_state = SHRM_SLEEP_STATE;
		shrm_audio_tx_state = SHRM_SLEEP_STATE;

		queue_work(shm_dev->shm_ac_sleep_wq,
				&shm_dev->shm_ac_sleep_req);

	}
	spin_unlock_irqrestore(&ca_wake_req_lock, flags);

	return HRTIMER_NORESTART;
}

int nl_send_multicast_message(int msg, gfp_t gfp_mask)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	int err;

	/* prepare netlink message */
	skb = alloc_skb(NLMSG_SPACE(MAX_PAYLOAD), gfp_mask);
	if (!skb) {
		dev_err(shm_dev->dev, "%s:alloc_skb failed\n", __func__);
		err = -ENOMEM;
		goto out;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	dev_dbg(shm_dev->dev, "nlh->nlmsg_len = %d\n", nlh->nlmsg_len);

	nlh->nlmsg_pid = 0;  /* from kernel */
	nlh->nlmsg_flags = 0;
	*(int *)NLMSG_DATA(nlh) = msg;
	skb_put(skb, MAX_PAYLOAD);
	/* sender is in group 1<<0 */
	NETLINK_CB(skb).pid = 0;  /* from kernel */
	/* to mcast group 1<<0 */
	NETLINK_CB(skb).dst_group = 1;

	/*multicast the message to all listening processes*/
	err = netlink_broadcast(shrm_nl_sk, skb, 0, 1, gfp_mask);
	dev_dbg(shm_dev->dev, "ret val from nl-multicast = %d\n", err);

out:
	return err;
}

static void nl_send_unicast_message(int dst_pid)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	int err;
	int bt_state;
	unsigned long flags;

	dev_info(shm_dev->dev, "Sending unicast message\n");

	/* prepare the NL message for unicast */
	skb = alloc_skb(NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
	if (!skb) {
		dev_err(shm_dev->dev, "%s:alloc_skb failed\n", __func__);
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	dev_dbg(shm_dev->dev, "nlh->nlmsg_len = %d\n", nlh->nlmsg_len);

	nlh->nlmsg_pid = 0;  /* from kernel */
	nlh->nlmsg_flags = 0;

	spin_lock_irqsave(&boot_lock, flags);
	bt_state = boot_state;
	spin_unlock_irqrestore(&boot_lock, flags);

	if (bt_state == BOOT_DONE)
		*(int *)NLMSG_DATA(nlh) = SHRM_NL_STATUS_MOD_ONLINE;
	else
		*(int *)NLMSG_DATA(nlh) = SHRM_NL_STATUS_MOD_OFFLINE;

	skb_put(skb, MAX_PAYLOAD);
	/* sender is in group 1<<0 */
	NETLINK_CB(skb).pid = 0;  /* from kernel */
	NETLINK_CB(skb).dst_group = 0;

	/*unicast the message to the querying processes*/
	err = netlink_unicast(shrm_nl_sk, skb, dst_pid, MSG_DONTWAIT);
	dev_dbg(shm_dev->dev, "ret val from nl-unicast = %d\n", err);
}


static int check_modem_in_reset(void)
{
	u8 bt_state;
	unsigned long flags;

	spin_lock_irqsave(&boot_lock, flags);
	bt_state = boot_state;
	spin_unlock_irqrestore(&boot_lock, flags);

#ifdef CONFIG_U8500_SHRM_MODEM_SILENT_RESET
	if (bt_state != BOOT_UNKNOWN)
		return 0;
	else
		return -ENODEV;
#else
	/*
	 * this check won't be applicable and won't work correctly
	 * if modem-silent-feature is not enabled
	 * so, simply return 0
	 */
	return 0;
#endif
}

void shm_ca_msgpending_0_tasklet(unsigned long tasklet_data)
{
	struct shrm_dev *shrm = (struct shrm_dev *)tasklet_data;
	u32 reader_local_rptr;
	u32 reader_local_wptr;
	u32 shared_rptr;
	u32 config = 0, version = 0;
	unsigned long flags;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	/* Interprocess locking */
	spin_lock(&ca_common_lock);

	/* Update_reader_local_wptr with shared_wptr */
	update_ca_common_local_wptr(shrm);
	get_reader_pointers(COMMON_CHANNEL, &reader_local_rptr,
				&reader_local_wptr, &shared_rptr);

	set_ca_msg_0_read_notif_send(0);

	if (boot_state == BOOT_DONE) {
		shrm_common_rx_state = SHRM_PTR_FREE;

		if (reader_local_rptr != shared_rptr)
			ca_msg_read_notification_0(shrm);
		if (reader_local_rptr != reader_local_wptr)
			receive_messages_common(shrm);
		get_reader_pointers(COMMON_CHANNEL, &reader_local_rptr,
				&reader_local_wptr, &shared_rptr);
		if (reader_local_rptr == reader_local_wptr)
			shrm_common_rx_state = SHRM_IDLE;
	} else {
		/* BOOT phase.only a BOOT_RESP should be in FIFO */
		if (boot_state != BOOT_INFO_SYNC) {
			if (!read_boot_info_req(shrm, &config, &version)) {
				dev_err(shrm->dev,
						"Unable to read boot state\n");
				return;
			}
			/* SendReadNotification */
			ca_msg_read_notification_0(shrm);
			/*
			 * Check the version number before
			 * sending Boot info response
			 */

			/* send MsgPending notification */
			write_boot_info_resp(shrm, config, version);
			spin_lock_irqsave(&boot_lock, flags);
			boot_state = BOOT_INFO_SYNC;
			spin_unlock_irqrestore(&boot_lock, flags);
			dev_info(shrm->dev, "BOOT_INFO_SYNC\n");
			queue_work(shrm->shm_common_ch_wr_wq,
					&shrm->send_ac_msg_pend_notify_0);
		} else {
			ca_msg_read_notification_0(shrm);
			dev_info(shrm->dev,
				"BOOT_INFO_SYNC\n");
		}
	}
	/* Interprocess locking */
	spin_unlock(&ca_common_lock);
	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

void shm_ca_msgpending_1_tasklet(unsigned long tasklet_data)
{
	struct shrm_dev *shrm = (struct shrm_dev *)tasklet_data;
	u32 reader_local_rptr;
	u32 reader_local_wptr;
	u32 shared_rptr;

	/*
	 * This function is called when CaMsgPendingNotification Trigerred
	 * by CMU. It means that CMU has wrote a message into Ca Audio FIFO
	 */

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown\n",
				__func__);
		return;
	}

	/* Interprocess locking */
	spin_lock(&ca_audio_lock);

	/* Update_reader_local_wptr(with shared_wptr) */
	update_ca_audio_local_wptr(shrm);
	get_reader_pointers(AUDIO_CHANNEL, &reader_local_rptr,
					&reader_local_wptr, &shared_rptr);

	set_ca_msg_1_read_notif_send(0);

	if (boot_state != BOOT_DONE) {
		dev_err(shrm->dev, "Boot Error\n");
		return;
	}
	shrm_audio_rx_state = SHRM_PTR_FREE;
	/* Check we already read the message */
	if (reader_local_rptr != shared_rptr)
		ca_msg_read_notification_1(shrm);
	if (reader_local_rptr != reader_local_wptr)
		receive_messages_audio(shrm);

	get_reader_pointers(AUDIO_CHANNEL, &reader_local_rptr,
			&reader_local_wptr, &shared_rptr);
	if (reader_local_rptr == reader_local_wptr)
		shrm_audio_rx_state = SHRM_IDLE;

	 /* Interprocess locking */
	spin_unlock(&ca_audio_lock);
	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

void shm_ac_read_notif_0_tasklet(unsigned long tasklet_data)
{
	struct shrm_dev *shrm = (struct shrm_dev *)tasklet_data;
	u32 writer_local_rptr;
	u32 writer_local_wptr;
	u32 shared_wptr;
	unsigned long flags;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	/* Update writer_local_rptrwith shared_rptr */
	update_ac_common_local_rptr(shrm);
	get_writer_pointers(COMMON_CHANNEL, &writer_local_rptr,
				&writer_local_wptr, &shared_wptr);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown\n",
				__func__);
		return;
	}

	if (boot_state == BOOT_INFO_SYNC) {
		/* BOOT_RESP sent by APE has been received by CMT */
		spin_lock_irqsave(&boot_lock, flags);
		boot_state = BOOT_DONE;
		spin_unlock_irqrestore(&boot_lock, flags);
		dev_info(shrm->dev, "IPC_ISA BOOT_DONE\n");

		if (shrm->msr_flag) {
			shrm_start_netdev(shrm->ndev);
			shrm->msr_flag = 0;

			/* multicast that modem is online */
			nl_send_multicast_message(SHRM_NL_STATUS_MOD_ONLINE,
					GFP_ATOMIC);
		}

	} else if (boot_state == BOOT_DONE) {
		if (writer_local_rptr != writer_local_wptr) {
			shrm_common_tx_state = SHRM_PTR_FREE;
			queue_work(shrm->shm_common_ch_wr_wq,
					&shrm->send_ac_msg_pend_notify_0);
		} else {
			shrm_common_tx_state = SHRM_IDLE;
			shrm_restart_netdev(shrm->ndev);
		}
	} else {
		dev_err(shrm->dev, "Invalid boot state\n");
	}
	/* start timer here */
	hrtimer_start(&timer, ktime_set(0, 10*NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	atomic_dec(&ac_sleep_disable_count);

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

void shm_ac_read_notif_1_tasklet(unsigned long tasklet_data)
{
	struct shrm_dev *shrm = (struct shrm_dev *)tasklet_data;
	u32 writer_local_rptr;
	u32 writer_local_wptr;
	u32 shared_wptr;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown\n",
				__func__);
		return;
	}

	/* Update writer_local_rptr(with shared_rptr) */
	update_ac_audio_local_rptr(shrm);
	get_writer_pointers(AUDIO_CHANNEL, &writer_local_rptr,
				&writer_local_wptr, &shared_wptr);
	if (boot_state != BOOT_DONE) {
		dev_err(shrm->dev, "Error Case in boot state\n");
		return;
	}
	if (writer_local_rptr != writer_local_wptr) {
		shrm_audio_tx_state = SHRM_PTR_FREE;
		queue_work(shrm->shm_audio_ch_wr_wq,
				&shrm->send_ac_msg_pend_notify_1);
	} else {
		shrm_audio_tx_state = SHRM_IDLE;
	}
	/* start timer here */
	hrtimer_start(&timer, ktime_set(0, 10*NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	atomic_dec(&ac_sleep_disable_count);

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

void shm_ca_sleep_req_work(struct work_struct *work)
{
	dev_dbg(shm_dev->dev, "%s:IRQ_PRCMU_CA_SLEEP\n", __func__);

	shrm_common_rx_state = SHRM_IDLE;
	shrm_audio_rx_state =  SHRM_IDLE;

	writel((1<<GOP_CA_WAKE_ACK_BIT),
		shm_dev->intr_base + GOP_SET_REGISTER_BASE);

	hrtimer_start(&timer, ktime_set(0, 10*NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	suspend_unblock_sleep();
	atomic_dec(&ac_sleep_disable_count);
}

void shm_ca_wake_req_work(struct work_struct *work)
{
	struct shrm_dev *shrm = container_of(work,
			struct shrm_dev, shm_ca_wake_req);

	/* initialize the FIFO Variables */
	if (boot_state == BOOT_INIT)
		shm_fifo_init(shrm);

	mutex_lock(&ac_state_mutex);
	modem_request(shrm->modem);
	mutex_unlock(&ac_state_mutex);

	/* send ca_wake_ack_interrupt to CMU */
	if (!get_host_accessport_val()) {
		dev_crit(shrm->dev, "get_host_accessport failed\n");
		dev_info(shrm->dev, "Initiating a modem reset\n");
		prcmu_modem_reset();
	}
	writel((1<<GOP_CA_WAKE_ACK_BIT),
			shm_dev->intr_base + GOP_SET_REGISTER_BASE);
}
#ifdef CONFIG_U8500_SHRM_MODEM_SILENT_RESET
static int shrm_modem_reset_sequence(void)
{
	int err;
	unsigned long flags;

	hrtimer_cancel(&timer);

	/*
	 * keep the count to 0 so that we can bring down the line
	 * for normal ac-wake and ac-sleep logic
	 */
	atomic_set(&ac_sleep_disable_count, 0);

	/* workaround for MSR */
	queue_work(shm_dev->shm_ac_wake_wq,
			&shm_dev->shm_ac_wake_req);

	/* stop network queue */
	shrm_stop_netdev(shm_dev->ndev);

	/* reset char device queues */
	shrm_char_reset_queues(shm_dev);

	/* reset protocol states */
	shrm_common_tx_state = SHRM_SLEEP_STATE;
	shrm_common_rx_state = SHRM_SLEEP_STATE;
	shrm_audio_tx_state = SHRM_SLEEP_STATE;
	shrm_audio_rx_state = SHRM_SLEEP_STATE;

	/* set the msr flag */
	shm_dev->msr_flag = 1;

	/* multicast that modem is going to reset */
	err = nl_send_multicast_message(SHRM_NL_MOD_RESET, GFP_ATOMIC);

	/* reset the boot state */
	spin_lock_irqsave(&boot_lock, flags);
	boot_state = BOOT_INIT;
	spin_unlock_irqrestore(&boot_lock, flags);

	/* re-enable irqs */
	enable_irq(shm_dev->ac_read_notif_0_irq);
	enable_irq(shm_dev->ac_read_notif_1_irq);
	enable_irq(shm_dev->ca_msg_pending_notif_0_irq);
	enable_irq(shm_dev->ca_msg_pending_notif_1_irq);
	enable_irq(IRQ_PRCMU_CA_WAKE);
	enable_irq(IRQ_PRCMU_CA_SLEEP);

	return err;
}
#endif

static void shrm_modem_reset_callback(unsigned long irq)
{
	dev_err(shm_dev->dev, "Received mod_reset_req interrupt\n");

#ifdef CONFIG_U8500_SHRM_MODEM_SILENT_RESET
	{
		int err;
		dev_info(shm_dev->dev, "Initiating Modem silent reset\n");

		err = shrm_modem_reset_sequence();
		if (err)
			dev_err(shm_dev->dev,
				"Failed multicast of modem reset\n");
	}
#else
	dev_info(shm_dev->dev, "Modem in reset loop, doing System reset\n");

	/* Call the PRCMU reset API */
	prcmu_system_reset(SW_RESET_NO_ARGUMENT);
#endif
}

DECLARE_TASKLET(shrm_sw_reset_callback, shrm_modem_reset_callback,
		IRQ_PRCMU_MODEM_SW_RESET_REQ);

static irqreturn_t shrm_prcmu_irq_handler(int irq, void *data)
{
	struct shrm_dev *shrm = data;
	unsigned long flags;

	switch (irq) {
	case IRQ_PRCMU_CA_WAKE:
		suspend_block_sleep();
		if (shrm->msr_flag)
			atomic_set(&ac_sleep_disable_count, 0);
		atomic_inc(&ac_sleep_disable_count);
		queue_work(shrm->shm_ca_wake_wq, &shrm->shm_ca_wake_req);
		break;
	case IRQ_PRCMU_CA_SLEEP:
		queue_work(shrm->shm_ca_wake_wq, &shrm->shm_ca_sleep_req);
		break;
	case IRQ_PRCMU_MODEM_SW_RESET_REQ:
		/* update the boot_state */
		spin_lock_irqsave(&boot_lock, flags);
		boot_state = BOOT_UNKNOWN;

		/*
		 * put a barrier over here to make sure boot_state is updated
		 * else, it is seen that some of already executing modem
		 * irqs or tasklets fail the protocol checks and will ultimately
		 * try to acces the modem causing system to hang.
		 * This is particularly seen with user-space initiated modem reset
		 */
		wmb();
		spin_unlock_irqrestore(&boot_lock, flags);

		disable_irq_nosync(shrm->ac_read_notif_0_irq);
		disable_irq_nosync(shrm->ac_read_notif_1_irq);
		disable_irq_nosync(shrm->ca_msg_pending_notif_0_irq);
		disable_irq_nosync(shrm->ca_msg_pending_notif_1_irq);
		disable_irq_nosync(IRQ_PRCMU_CA_WAKE);
		disable_irq_nosync(IRQ_PRCMU_CA_SLEEP);

		tasklet_schedule(&shrm_sw_reset_callback);
		break;
	default:
		dev_err(shrm->dev, "%s: => IRQ %d\n", __func__, irq);
		return IRQ_NONE;
	}
	return IRQ_HANDLED;
}

static void send_ac_msg_pend_notify_0_work(struct work_struct *work)
{
	struct shrm_dev *shrm = container_of(work, struct shrm_dev,
			send_ac_msg_pend_notify_0);

	dev_dbg(shrm->dev, "%s IN\n", __func__);
	update_ac_common_shared_wptr(shrm);

	mutex_lock(&ac_state_mutex);
	atomic_inc(&ac_sleep_disable_count);
	modem_request(shrm->modem);
	mutex_unlock(&ac_state_mutex);

	if (!get_host_accessport_val()) {
		dev_crit(shrm->dev, "get_host_accessport failed\n");
		dev_info(shrm->dev, "Initiating a modem reset\n");
		prcmu_modem_reset();
	}

	/* Trigger AcMsgPendingNotification to CMU */
	writel((1<<GOP_COMMON_AC_MSG_PENDING_NOTIFICATION_BIT),
			shrm->intr_base + GOP_SET_REGISTER_BASE);

	if (shrm_common_tx_state == SHRM_PTR_FREE)
		shrm_common_tx_state = SHRM_PTR_BUSY;

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

static void send_ac_msg_pend_notify_1_work(struct work_struct *work)
{
	struct shrm_dev *shrm = container_of(work, struct shrm_dev,
			send_ac_msg_pend_notify_1);

	dev_dbg(shrm->dev, "%s IN\n", __func__);
	/* Update shared_wptr with writer_local_wptr) */
	update_ac_audio_shared_wptr(shrm);

	mutex_lock(&ac_state_mutex);
	atomic_inc(&ac_sleep_disable_count);
	modem_request(shrm->modem);
	mutex_unlock(&ac_state_mutex);

	if (!get_host_accessport_val()) {
		dev_crit(shrm->dev, "get_host_accessport failed\n");
		dev_info(shrm->dev, "Initiating a modem reset\n");
		prcmu_modem_reset();
	}

	/* Trigger AcMsgPendingNotification to CMU */
	writel((1<<GOP_AUDIO_AC_MSG_PENDING_NOTIFICATION_BIT),
			shrm->intr_base + GOP_SET_REGISTER_BASE);

	if (shrm_audio_tx_state == SHRM_PTR_FREE)
		shrm_audio_tx_state = SHRM_PTR_BUSY;

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

void shm_nl_receive(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	int msg;

	dev_dbg(shm_dev->dev, "Received NL msg from user-space\n");

	nlh = (struct nlmsghdr *)skb->data;
	msg = *((int *)(NLMSG_DATA(nlh)));
	switch (msg) {
	case SHRM_NL_MOD_QUERY_STATE:
		dev_info(shm_dev->dev, "mod-query-state from user-space\n");
		nl_send_unicast_message(nlh->nlmsg_pid);
		break;

	case SHRM_NL_USER_MOD_RESET:
		dev_info(shm_dev->dev, "user-space inited mod-reset-req\n");
		dev_info(shm_dev->dev, "PCRMU resets modem\n");
		prcmu_modem_reset();
		break;

	default:
		dev_err(shm_dev->dev, "Invalid NL msg from user-space\n");
		break;
	};
}

int shrm_protocol_init(struct shrm_dev *shrm,
			received_msg_handler common_rx_handler,
			received_msg_handler audio_rx_handler)
{
	int err;

	shm_dev = shrm;
	boot_state = BOOT_INIT;
	dev_info(shrm->dev, "IPC_ISA BOOT_INIT\n");
	rx_common_handler = common_rx_handler;
	rx_audio_handler = audio_rx_handler;
	atomic_set(&ac_sleep_disable_count, 0);

	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = callback;

	shrm->shm_common_ch_wr_wq = create_singlethread_workqueue
		("shm_common_channel_irq");
	if (!shrm->shm_common_ch_wr_wq) {
		dev_err(shrm->dev, "failed to create work queue\n");
		return -ENOMEM;
	}
	shrm->shm_audio_ch_wr_wq = create_singlethread_workqueue
		("shm_audio_channel_irq");
	if (!shrm->shm_audio_ch_wr_wq) {
		dev_err(shrm->dev, "failed to create work queue\n");
		err = -ENOMEM;
		goto free_wq1;
	}
	shrm->shm_ac_wake_wq = create_singlethread_workqueue("shm_ac_wake_req");
	if (!shrm->shm_ac_wake_wq) {
		dev_err(shrm->dev, "failed to create work queue\n");
		err = -ENOMEM;
		goto free_wq2;
	}
	shrm->shm_ca_wake_wq = create_singlethread_workqueue("shm_ca_wake_req");
	if (!shrm->shm_ca_wake_wq) {
		dev_err(shrm->dev, "failed to create work queue\n");
		err = -ENOMEM;
		goto free_wq3;
	}
	shrm->shm_ac_sleep_wq = create_singlethread_workqueue
						("shm_ac_sleep_req");
	if (!shrm->shm_ac_sleep_wq) {
		dev_err(shrm->dev, "failed to create work queue\n");
		err = -ENOMEM;
		goto free_wq4;
	}
	INIT_WORK(&shrm->send_ac_msg_pend_notify_0,
			send_ac_msg_pend_notify_0_work);
	INIT_WORK(&shrm->send_ac_msg_pend_notify_1,
			send_ac_msg_pend_notify_1_work);
	INIT_WORK(&shrm->shm_ca_wake_req, shm_ca_wake_req_work);
	INIT_WORK(&shrm->shm_ca_sleep_req, shm_ca_sleep_req_work);
	INIT_WORK(&shrm->shm_ac_sleep_req, shm_ac_sleep_req_work);
	INIT_WORK(&shrm->shm_ac_wake_req, shm_ac_wake_req_work);
	INIT_WORK(&shrm->shm_mod_reset_req, shm_mod_reset_req_work);

	/* set tasklet data */
	shm_ca_0_tasklet.data = (unsigned long)shrm;
	shm_ca_1_tasklet.data = (unsigned long)shrm;

	err = request_irq(IRQ_PRCMU_CA_SLEEP, shrm_prcmu_irq_handler,
			IRQF_NO_SUSPEND, "ca-sleep", shrm);
	if (err < 0) {
		dev_err(shm_dev->dev, "Failed alloc IRQ_PRCMU_CA_SLEEP.\n");
		goto free_wq5;
	}

	err = request_irq(IRQ_PRCMU_CA_WAKE, shrm_prcmu_irq_handler,
		IRQF_NO_SUSPEND, "ca-wake", shrm);
	if (err < 0) {
		dev_err(shm_dev->dev, "Failed alloc IRQ_PRCMU_CA_WAKE.\n");
		goto drop2;
	}

	err = request_irq(IRQ_PRCMU_MODEM_SW_RESET_REQ, shrm_prcmu_irq_handler,
			IRQF_NO_SUSPEND, "modem-sw-reset-req", shrm);
	if (err < 0) {
		dev_err(shm_dev->dev,
				"Failed alloc IRQ_PRCMU_MODEM_SW_RESET_REQ.\n");
		goto drop1;
	}

#ifdef CONFIG_U8500_SHRM_MODEM_SILENT_RESET
	/* init netlink socket for user-space communication */
	shrm_nl_sk = netlink_kernel_create(NULL, NETLINK_SHRM, 1,
			shm_nl_receive, NULL, THIS_MODULE);

	if (!shrm_nl_sk) {
		dev_err(shm_dev->dev, "netlink socket creation failed\n");
		goto drop;
	}
#endif
	return 0;

#ifdef CONFIG_U8500_SHRM_MODEM_SILENT_RESET
drop:
	free_irq(IRQ_PRCMU_MODEM_SW_RESET_REQ, NULL);
#endif
drop1:
	free_irq(IRQ_PRCMU_CA_WAKE, NULL);
drop2:
	free_irq(IRQ_PRCMU_CA_SLEEP, NULL);
free_wq5:
	destroy_workqueue(shrm->shm_ac_sleep_wq);
free_wq4:
	destroy_workqueue(shrm->shm_ca_wake_wq);
free_wq3:
	destroy_workqueue(shrm->shm_ac_wake_wq);
free_wq2:
	destroy_workqueue(shrm->shm_audio_ch_wr_wq);
free_wq1:
	destroy_workqueue(shrm->shm_common_ch_wr_wq);
	return err;
}

void shrm_protocol_deinit(struct shrm_dev *shrm)
{
	free_irq(IRQ_PRCMU_CA_SLEEP, NULL);
	free_irq(IRQ_PRCMU_CA_WAKE, NULL);
	free_irq(IRQ_PRCMU_MODEM_SW_RESET_REQ, NULL);
	flush_scheduled_work();
	destroy_workqueue(shrm->shm_common_ch_wr_wq);
	destroy_workqueue(shrm->shm_audio_ch_wr_wq);
	destroy_workqueue(shrm->shm_ac_wake_wq);
	destroy_workqueue(shrm->shm_ca_wake_wq);
	destroy_workqueue(shrm->shm_ac_sleep_wq);
	modem_put(shrm->modem);
}

int get_ca_wake_req_state(void)
{
	return ((atomic_read(&ac_sleep_disable_count) > 0) ||
			modem_get_usage(shm_dev->modem));
}

irqreturn_t ca_wake_irq_handler(int irq, void *ctrlr)
{
	struct shrm_dev *shrm = ctrlr;

	dev_dbg(shrm->dev, "%s IN\n", __func__);
	/* initialize the FIFO Variables */
	if (boot_state == BOOT_INIT)
		shm_fifo_init(shrm);

	dev_dbg(shrm->dev, "Inside ca_wake_irq_handler\n");

	/* Clear the interrupt */
	writel((1 << GOP_CA_WAKE_REQ_BIT),
				shrm->intr_base + GOP_CLEAR_REGISTER_BASE);

	/* send ca_wake_ack_interrupt to CMU */
	writel((1 << GOP_CA_WAKE_ACK_BIT),
		shrm->intr_base + GOP_SET_REGISTER_BASE);


	dev_dbg(shrm->dev, "%s OUT\n", __func__);
	return IRQ_HANDLED;
}


irqreturn_t ac_read_notif_0_irq_handler(int irq, void *ctrlr)
{
	struct shrm_dev *shrm = ctrlr;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	shm_ac_read_0_tasklet.data = (unsigned long)shrm;
	tasklet_schedule(&shm_ac_read_0_tasklet);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	/* Clear the interrupt */
	writel((1 << GOP_COMMON_AC_READ_NOTIFICATION_BIT),
			shrm->intr_base + GOP_CLEAR_REGISTER_BASE);

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
	return IRQ_HANDLED;
}

irqreturn_t ac_read_notif_1_irq_handler(int irq, void *ctrlr)
{
	struct shrm_dev *shrm = ctrlr;

	dev_dbg(shrm->dev, "%s IN+\n", __func__);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	shm_ac_read_1_tasklet.data = (unsigned long)shrm;
	tasklet_schedule(&shm_ac_read_1_tasklet);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	/* Clear the interrupt */
	writel((1 << GOP_AUDIO_AC_READ_NOTIFICATION_BIT),
			shrm->intr_base + GOP_CLEAR_REGISTER_BASE);

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
	return IRQ_HANDLED;
}

irqreturn_t ca_msg_pending_notif_0_irq_handler(int irq, void *ctrlr)
{
	struct shrm_dev *shrm = ctrlr;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	tasklet_schedule(&shm_ca_0_tasklet);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	/* Clear the interrupt */
	writel((1 << GOP_COMMON_CA_MSG_PENDING_NOTIFICATION_BIT),
			shrm->intr_base + GOP_CLEAR_REGISTER_BASE);

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
	return IRQ_HANDLED;
}

irqreturn_t ca_msg_pending_notif_1_irq_handler(int irq, void *ctrlr)
{
	struct shrm_dev *shrm = ctrlr;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	tasklet_schedule(&shm_ca_1_tasklet);

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return IRQ_HANDLED;
	}

	/* Clear the interrupt */
	writel((1<<GOP_AUDIO_CA_MSG_PENDING_NOTIFICATION_BIT),
			shrm->intr_base+GOP_CLEAR_REGISTER_BASE);

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
	return IRQ_HANDLED;

}

/**
 * shm_write_msg() - write message to shared memory
 * @shrm:	pointer to the shrm device information structure
 * @l2_header:	L2 header
 * @addr:	pointer to the message
 * @length:	length of the message to be written
 *
 * This function is called from net or char interface driver write operation.
 * Prior to calling this function the message is copied from the user space
 * buffer to the kernel buffer. This function based on the l2 header routes
 * the message to the respective channel and FIFO. Then makes a call to the
 * fifo write function where the message is written to the physical device.
 */
int shm_write_msg(struct shrm_dev *shrm, u8 l2_header,
					void *addr, u32 length)
{
	u8 channel = 0;
	int ret;

	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (boot_state != BOOT_DONE) {
		dev_err(shrm->dev,
			"error:after boot done  call this fn, L2Header = %d\n",
			l2_header);
		ret = -ENODEV;
		goto out;
	}

	if ((l2_header == L2_HEADER_ISI) ||
			(l2_header == L2_HEADER_RPC) ||
			(l2_header == L2_HEADER_SECURITY) ||
			(l2_header == L2_HEADER_COMMON_SIMPLE_LOOPBACK) ||
			(l2_header == L2_HEADER_COMMON_ADVANCED_LOOPBACK) ||
			(l2_header == L2_HEADER_CIQ)) {
		channel = 0;
		if (shrm_common_tx_state == SHRM_SLEEP_STATE)
			shrm_common_tx_state = SHRM_PTR_FREE;
		else if (shrm_common_tx_state == SHRM_IDLE)
			shrm_common_tx_state = SHRM_PTR_FREE;

	} else if ((l2_header == L2_HEADER_AUDIO) ||
			(l2_header == L2_HEADER_AUDIO_SIMPLE_LOOPBACK) ||
			(l2_header == L2_HEADER_AUDIO_ADVANCED_LOOPBACK)) {
		if (shrm_audio_tx_state == SHRM_SLEEP_STATE)
			shrm_audio_tx_state = SHRM_PTR_FREE;
		else if (shrm_audio_tx_state == SHRM_IDLE)
			shrm_audio_tx_state = SHRM_PTR_FREE;

		channel = 1;
	} else {
		ret = -ENODEV;
		goto out;
	}
	ret = shm_write_msg_to_fifo(shrm, channel, l2_header, addr, length);
	if (ret < 0) {
		dev_err(shrm->dev, "write message to fifo failed\n");
		return ret;
	}
	/*
	 * notify only if new msg copied is the only unread one
	 * otherwise it means that reading process is ongoing
	 */
	if (is_the_only_one_unread_message(shrm, channel, length)) {

		/* Send Message Pending Noitication to CMT */
		if (channel == 0)
			queue_work(shrm->shm_common_ch_wr_wq,
					&shrm->send_ac_msg_pend_notify_0);
		else
			queue_work(shrm->shm_audio_ch_wr_wq,
					&shrm->send_ac_msg_pend_notify_1);

	}

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
	return 0;

out:
	return ret;
}

void ca_msg_read_notification_0(struct shrm_dev *shrm)
{
	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (get_ca_msg_0_read_notif_send() == 0) {
		update_ca_common_shared_rptr(shrm);

		if (check_modem_in_reset()) {
			dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
					__func__);
			return;
		}

		/* Trigger CaMsgReadNotification to CMU */
		writel((1 << GOP_COMMON_CA_READ_NOTIFICATION_BIT),
			shrm->intr_base + GOP_SET_REGISTER_BASE);
		set_ca_msg_0_read_notif_send(1);
		shrm_common_rx_state = SHRM_PTR_BUSY;
	}

	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

void ca_msg_read_notification_1(struct shrm_dev *shrm)
{
	dev_dbg(shrm->dev, "%s IN\n", __func__);

	if (get_ca_msg_1_read_notif_send() == 0) {
		update_ca_audio_shared_rptr(shrm);

		if (check_modem_in_reset()) {
			dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
					__func__);
			return;
		}

		/* Trigger CaMsgReadNotification to CMU */
		writel((1<<GOP_AUDIO_CA_READ_NOTIFICATION_BIT),
			shrm->intr_base+GOP_SET_REGISTER_BASE);
		set_ca_msg_1_read_notif_send(1);
		shrm_audio_rx_state = SHRM_PTR_BUSY;
	}
	dev_dbg(shrm->dev, "%s OUT\n", __func__);
}

/**
 * receive_messages_common - receive common channnel msg from
 * CMT(Cellular Mobile Terminal)
 * @shrm:	pointer to shrm device information structure
 *
 * The messages sent from CMT to APE are written to the respective FIFO
 * and an interrupt is triggered by the CMT. This ca message pending
 * interrupt calls this function. This function sends a read notification
 * acknowledgement to the CMT and calls the common channel receive handler
 * where the messsage is copied to the respective(ISI, RPC, SECURIT) queue
 * based on the message l2 header.
 */
void receive_messages_common(struct shrm_dev *shrm)
{
	u8 l2_header;
	u32 len;

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return;
	}

	l2_header = read_one_l2msg_common(shrm, recieve_common_msg, &len);
	/* Send Recieve_Call_back to Upper Layer */
	if (!rx_common_handler) {
		dev_err(shrm->dev, "common_rx_handler is Null\n");
		BUG();
	}
	(*rx_common_handler)(l2_header, &recieve_common_msg, len,
					shrm);
	/* SendReadNotification */
	ca_msg_read_notification_0(shrm);

	while (read_remaining_messages_common()) {
		if (check_modem_in_reset()) {
			dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
					__func__);
			return;
		}

		l2_header = read_one_l2msg_common(shrm, recieve_common_msg,
								&len);
		/* Send Recieve_Call_back to Upper Layer */
		(*rx_common_handler)(l2_header,
					&recieve_common_msg, len,
					shrm);
	}
}

/**
 * receive_messages_audio() - receive audio message from CMT
 * @shrm:	pointer to shrm device information structure
 *
 * The messages sent from CMT to APE are written to the respective FIFO
 * and an interrupt is triggered by the CMT. This ca message pending
 * interrupt calls this function. This function sends a read notification
 * acknowledgement to the CMT and calls the common channel receive handler
 * where the messsage is copied to the audio queue.
 */
void receive_messages_audio(struct shrm_dev *shrm)
{
	u8 l2_header;
	u32 len;

	if (check_modem_in_reset()) {
		dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
				__func__);
		return;
	}

	l2_header = read_one_l2msg_audio(shrm, recieve_audio_msg, &len);
	/* Send Recieve_Call_back to Upper Layer */

	if (!rx_audio_handler) {
		dev_crit(shrm->dev, "audio_rx_handler is Null\n");
		BUG();
	}
	(*rx_audio_handler)(l2_header, &recieve_audio_msg,
					len, shrm);

	/* SendReadNotification */
	ca_msg_read_notification_1(shrm);
	while (read_remaining_messages_audio())	{
		if (check_modem_in_reset()) {
			dev_err(shrm->dev, "%s:Modem state reset or unknown.\n",
					__func__);
			return;
		}

		l2_header = read_one_l2msg_audio(shrm,
						recieve_audio_msg, &len);
		/* Send Recieve_Call_back to Upper Layer */
		(*rx_audio_handler)(l2_header,
					&recieve_audio_msg, len,
					shrm);
	}
}

u8 get_boot_state()
{
	return boot_state;
}

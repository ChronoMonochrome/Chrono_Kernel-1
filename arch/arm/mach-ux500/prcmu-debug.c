/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Martin Persson for ST-Ericsson
 *         Etienne Carriere <etienne.carriere@stericsson.com> for ST-Ericsson
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mfd/dbx500-prcmu.h>

#include <mach/hardware.h>

#define MAX_STATES 5
#define MAX_NAMELEN 16

struct state_history {
	ktime_t start;
	u32 state;
	u32 counter[MAX_STATES];
	u8 opps[MAX_STATES];
	int max_states;
	int req;
	bool reqs[MAX_STATES];
	ktime_t time[MAX_STATES];
	int state_names[MAX_STATES];
	char prefix[MAX_NAMELEN];
	spinlock_t lock;
};

static struct state_history ape_sh = {
	.prefix = "APE",
	.req = PRCMU_QOS_APE_OPP,
	.opps = {APE_50_OPP, APE_100_OPP},
	.state_names = {50, 100},
	.max_states = 2,
};

static struct state_history ddr_sh = {
	.prefix = "DDR",
	.req = PRCMU_QOS_DDR_OPP,
	.opps = {DDR_25_OPP, DDR_50_OPP, DDR_100_OPP},
	.state_names = {25, 50, 100},
	.max_states = 3,
};

static struct state_history arm_sh = {
	.prefix = "ARM",
	.req = PRCMU_QOS_ARM_OPP,
	.opps = {ARM_EXTCLK, ARM_50_OPP, ARM_100_OPP, ARM_MAX_OPP},
	.state_names = {25, 50, 100, 125},
	.max_states = 4,
};

static int ape_voltage_count;

static void log_set(struct state_history *sh, u8 opp)
{
	ktime_t now;
	ktime_t dtime;
	unsigned long flags;
	int state;

	now = ktime_get();
	spin_lock_irqsave(&sh->lock, flags);

	for (state = 0 ; sh->opps[state] != opp; state++)
		;
	BUG_ON(state >= sh->max_states);

	dtime = ktime_sub(now, sh->start);
	sh->time[sh->state] = ktime_add(sh->time[sh->state], dtime);
	sh->start = now;
	sh->counter[sh->state]++;
	sh->state = state;

	spin_unlock_irqrestore(&sh->lock, flags);
}

void prcmu_debug_ape_opp_log(u8 opp)
{
	log_set(&ape_sh, opp);
}

void prcmu_debug_ddr_opp_log(u8 opp)
{
	log_set(&ddr_sh, opp);
}

void prcmu_debug_arm_opp_log(u8 opp)
{
	log_set(&arm_sh, opp);
}

static void log_reset(struct state_history *sh)
{
	unsigned long flags;
	int i;

	pr_info("reset\n");

	spin_lock_irqsave(&sh->lock, flags);
	for (i = 0; i < sh->max_states; i++) {
		sh->counter[i] = 0;
		sh->time[i] = ktime_set(0, 0);
	}

	sh->start = ktime_get();
	spin_unlock_irqrestore(&sh->lock, flags);

}

static ssize_t ape_stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	log_reset(&ape_sh);
	return count;
}

static ssize_t ddr_stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	log_reset(&ddr_sh);
	return count;
}

static ssize_t arm_stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	log_reset(&arm_sh);
	return count;
}

static int log_print(struct seq_file *s, struct state_history *sh)
{
	int i;
	unsigned long flags;
	ktime_t total;
	ktime_t dtime;
	s64 t_ms;
	s64 perc;
	s64 total_ms;

	spin_lock_irqsave(&sh->lock, flags);

	dtime = ktime_sub(ktime_get(), sh->start);

	total = dtime;

	for (i = 0; i < sh->max_states; i++)
		total = ktime_add(total, sh->time[i]);
	total_ms = ktime_to_ms(total);

	for (i = 0; i < sh->max_states; i++) {
		ktime_t t = sh->time[i];
		if (sh->state == i)
			t = ktime_add(t, dtime);

		t_ms = ktime_to_ms(t);
		perc = 100 * t_ms;
		do_div(perc, total_ms);

		seq_printf(s, "%s OPP %d: # %u in %lld ms %d%%\n",
			   sh->prefix, sh->state_names[i],
			   sh->counter[i] + (int)(sh->state == i),
			   t_ms, (u32)perc);

	}
	spin_unlock_irqrestore(&sh->lock, flags);
	return 0;
}

static int ape_stats_print(struct seq_file *s, void *p)
{
	log_print(s, &ape_sh);
	return 0;
}

static int ddr_stats_print(struct seq_file *s, void *p)
{
	log_print(s, &ddr_sh);
	return 0;
}

static int arm_stats_print(struct seq_file *s, void *p)
{
	log_print(s, &arm_sh);
	return 0;
}

static int opp_read(struct seq_file *s, void *p)
{
	int opp;

	struct state_history *sh = (struct state_history *)s->private;

	switch (sh->req) {
	case PRCMU_QOS_DDR_OPP:
		opp = prcmu_get_ddr_opp();
		seq_printf(s, "%s (%d)\n",
			   (opp == DDR_100_OPP) ? "100%" :
			   (opp == DDR_50_OPP) ? "50%" :
			   (opp == DDR_25_OPP) ? "25%" :
			   "unknown", opp);
		break;
	case PRCMU_QOS_APE_OPP:
		opp = prcmu_get_ape_opp();
		seq_printf(s, "%s (%d)\n",
			   (opp == APE_100_OPP) ? "100%" :
			   (opp == APE_50_OPP) ? "50%" :
			   "unknown", opp);
		break;
	case PRCMU_QOS_ARM_OPP:
		opp = prcmu_get_arm_opp();
		seq_printf(s, "%s (%d)\n",
			   (opp == ARM_MAX_OPP) ? "max" :
			   (opp == ARM_MAX_FREQ100OPP) ? "max-freq100" :
			   (opp == ARM_100_OPP) ? "100%" :
			   (opp == ARM_50_OPP) ? "50%" :
			   (opp == ARM_EXTCLK) ? "25% (extclk)" :
			   "unknown", opp);
		break;
	default:
		break;
	}
	return 0;

}

static ssize_t opp_write(struct file *file,
			 const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	long unsigned i;
	int err;
	struct state_history *sh = (struct state_history *)
		((struct seq_file *)file->private_data)->private;

	err = kstrtoul_from_user(user_buf, count, 0, &i);

	if (err)
		return err;

	prcmu_qos_force_opp(sh->req, i);

	pr_info("prcmu debug: forced OPP for %s to %d\n", sh->prefix, (int)i);

	return count;
}

static int cpufreq_delay_read(struct seq_file *s, void *p)
{
	return seq_printf(s, "%lu\n", prcmu_qos_get_cpufreq_opp_delay());
}

static int ape_voltage_read(struct seq_file *s, void *p)
{
	return seq_printf(s, "This reference count only includes "
			  "requests via debugfs.\nCount: %d\n",
			  ape_voltage_count);
}

static ssize_t ape_voltage_write(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	long unsigned i;
	int err;

	err = kstrtoul_from_user(user_buf, count, 0, &i);

	if (err)
		return err;

	switch (i) {
	case 0:
		if (ape_voltage_count == 0)
			pr_info("prcmu debug: reference count is already 0\n");
		else {
			err = prcmu_request_ape_opp_100_voltage(false);
			if (err)
				pr_err("prcmu debug: drop request failed\n");
			else
				ape_voltage_count--;
		}
		break;
	case 1:
		err = prcmu_request_ape_opp_100_voltage(true);
		if (err)
			pr_err("prcmu debug: request failed\n");
		else
			ape_voltage_count++;
		break;
	default:
		pr_info("prcmu debug: value not equal to 0 or 1\n");
	}
	return count;
}

static ssize_t cpufreq_delay_write(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	int err;
	long unsigned i;

	err = kstrtoul_from_user(user_buf, count, 0, &i);

	if (err)
		return err;

	prcmu_qos_set_cpufreq_opp_delay(i);

	pr_info("prcmu debug: changed delay between cpufreq change and QoS "
		 "requirement to %lu.\n", i);

	return count;
}

/* These are only for u8500 */
#define PRCM_AVS_BASE		0x2FC
#define AVS_VBB_RET		0x0
#define AVS_VBB_MAX_OPP		0x1
#define AVS_VBB_100_OPP		0x2
#define AVS_VBB_50_OPP		0x3
#define AVS_VARM_MAX_OPP	0x4
#define AVS_VARM_100_OPP	0x5
#define AVS_VARM_50_OPP		0x6
#define AVS_VARM_RET		0x7
#define AVS_VAPE_100_OPP	0x8
#define AVS_VAPE_50_OPP		0x9
#define AVS_VMOD_100_OPP	0xA
#define AVS_VMOD_50_OPP		0xB
#define AVS_VSAFE		0xC
#define AVS_SIZE		14

static int avs_read(struct seq_file *s, void *p)
{

	u8 avs[AVS_SIZE];
	void __iomem *tcdm_base;

	if (cpu_is_u8500()) {
		tcdm_base = __io_address(U8500_PRCMU_TCDM_BASE);

		memcpy_fromio(avs, tcdm_base + PRCM_AVS_BASE, AVS_SIZE);

		seq_printf(s, "VBB_RET      : 0x%2x\n", avs[AVS_VBB_RET]);
		seq_printf(s, "VBB_MAX_OPP  : 0x%2x\n", avs[AVS_VBB_MAX_OPP]);
		seq_printf(s, "VBB_100_OPP  : 0x%2x\n", avs[AVS_VBB_100_OPP]);
		seq_printf(s, "VBB_50_OPP   : 0x%2x\n", avs[AVS_VBB_50_OPP]);
		seq_printf(s, "VARM_MAX_OPP : 0x%2x\n", avs[AVS_VARM_MAX_OPP]);
		seq_printf(s, "VARM_100_OPP : 0x%2x\n", avs[AVS_VARM_100_OPP]);
		seq_printf(s, "VARM_50_OPP  : 0x%2x\n", avs[AVS_VARM_50_OPP]);
		seq_printf(s, "VARM_RET     : 0x%2x\n", avs[AVS_VARM_RET]);
		seq_printf(s, "VAPE_100_OPP : 0x%2x\n", avs[AVS_VAPE_100_OPP]);
		seq_printf(s, "VAPE_50_OPP  : 0x%2x\n", avs[AVS_VAPE_50_OPP]);
		seq_printf(s, "VMOD_100_OPP : 0x%2x\n", avs[AVS_VMOD_100_OPP]);
		seq_printf(s, "VMOD_50_OPP  : 0x%2x\n", avs[AVS_VMOD_50_OPP]);
		seq_printf(s, "VSAFE        : 0x%2x\n", avs[AVS_VSAFE]);
	} else {
		seq_printf(s, "Only u8500 supported.\n");
	}

	return 0;
}

static int opp_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, opp_read, inode->i_private);
}

static int ape_stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ape_stats_print, inode->i_private);
}

static int ddr_stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ddr_stats_print, inode->i_private);
}

static int arm_stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, arm_stats_print, inode->i_private);
}

static int cpufreq_delay_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, cpufreq_delay_read, inode->i_private);
}

static int ape_voltage_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ape_voltage_read, inode->i_private);
}

static int avs_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, avs_read, inode->i_private);
}

static const struct file_operations opp_fops = {
	.open = opp_open_file,
	.write = opp_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ape_stats_fops = {
	.open = ape_stats_open_file,
	.write = ape_stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ddr_stats_fops = {
	.open = ddr_stats_open_file,
	.write = ddr_stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations arm_stats_fops = {
	.open = arm_stats_open_file,
	.write = arm_stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations cpufreq_delay_fops = {
	.open = cpufreq_delay_open_file,
	.write = cpufreq_delay_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ape_voltage_fops = {
	.open = ape_voltage_open_file,
	.write = ape_voltage_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations avs_fops = {
	.open = avs_open_file,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int setup_debugfs(void)
{
	struct dentry *dir;
	struct dentry *file;

	dir = debugfs_create_dir("prcmu", NULL);
	if (IS_ERR_OR_NULL(dir))
		goto fail;

	file = debugfs_create_file("ape_stats", (S_IRUGO | S_IWUGO),
				   dir, NULL, &ape_stats_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ddr_stats", (S_IRUGO | S_IWUGO),
				   dir, NULL, &ddr_stats_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("arm_stats", (S_IRUGO | S_IWUGO),
				   dir, NULL, &arm_stats_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ape_opp", (S_IRUGO),
				   dir, (void *)&ape_sh,
				   &opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ddr_opp", (S_IRUGO),
				   dir, (void *)&ddr_sh,
				   &opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("arm_opp", (S_IRUGO),
				   dir, (void *)&arm_sh,
				   &opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("opp_cpufreq_delay", (S_IRUGO),
				   dir, NULL, &cpufreq_delay_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ape_voltage", (S_IRUGO),
				   dir, NULL, &ape_voltage_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("avs",
				   (S_IRUGO),
				   dir, NULL, &avs_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	return 0;
fail:
	if (!IS_ERR_OR_NULL(dir))
		debugfs_remove_recursive(dir);

	pr_err("prcmu debug: debugfs entry failed\n");
	return -ENOMEM;
}

static __init int prcmu_debug_init(void)
{
	spin_lock_init(&ape_sh.lock);
	spin_lock_init(&ddr_sh.lock);
	spin_lock_init(&arm_sh.lock);
	ape_sh.start = ktime_get();
	ddr_sh.start = ktime_get();
	arm_sh.start = ktime_get();
	setup_debugfs();
	return 0;
}
late_initcall(prcmu_debug_init);

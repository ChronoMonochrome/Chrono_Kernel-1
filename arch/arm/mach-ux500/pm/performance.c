/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Johan Rudholm <johan.rudholm@stericsson.com>
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com>
 */

#include <linux/kernel.h>
#include <linux/genhd.h>
#include <linux/major.h>
#include <linux/cdev.h>
#include <linux/kernel_stat.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/cpu.h>

#include <mach/irqs.h>

#define WLAN_PROBE_DELAY 3000 /* 3 seconds */
#define WLAN_LIMIT (3000/3) /* If we have more than 1000 irqs per second */

/*
 * MMC TODO:
 * o Develop a more power-aware algorithm
 * o Make the parameters visible through debugfs
 * o Get the value of CONFIG_MMC_BLOCK_MINORS in runtime instead, since
 *   it may be altered by drivers/mmc/card/block.c
 */

/* Sample reads and writes every n ms */
#define PERF_MMC_PROBE_DELAY 1000
/* Read threshold, sectors/second */
#define PERF_MMC_LIMIT_READ 10240
/* Write threshold, sectors/second */
#define PERF_MMC_LIMIT_WRITE 8192
/* Nr of MMC devices */
#define PERF_MMC_HOSTS 8

/*
 * Rescan for new MMC devices every
 * PERF_MMC_PROBE_DELAY * PERF_MMC_RESCAN_CYCLES ms
 */
#define PERF_MMC_RESCAN_CYCLES 10

static struct delayed_work work_mmc;
static struct delayed_work work_wlan_workaround;

static void wlan_load(struct work_struct *work)
{
	int cpu;
	unsigned int num_irqs = 0;
	static unsigned int old_num_irqs = UINT_MAX;

	for_each_online_cpu(cpu)
		num_irqs += kstat_irqs_cpu(IRQ_DB8500_SDMMC1, cpu);

	if ((num_irqs > old_num_irqs) &&
	    (num_irqs - old_num_irqs) > WLAN_LIMIT)
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
					     "wlan", 125);
	else
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
					     "wlan", 25);

	old_num_irqs = num_irqs;

	schedule_delayed_work_on(0,
				 &work_wlan_workaround,
				 msecs_to_jiffies(WLAN_PROBE_DELAY));
}

/*
 * Loop through every CONFIG_MMC_BLOCK_MINORS'th minor device for
 * MMC_BLOCK_MAJOR, get the struct gendisk for each device. Returns
 * nr of found disks. Populate mmc_disks.
 */
static int scan_mmc_devices(struct gendisk *mmc_disks[])
{
	dev_t devnr;
	int i, j = 0, part;
	struct gendisk *mmc_devices[256 / CONFIG_MMC_BLOCK_MINORS];

	memset(&mmc_devices, 0, sizeof(mmc_devices));

	for (i = 0; i * CONFIG_MMC_BLOCK_MINORS < 256; i++) {
		devnr = MKDEV(MMC_BLOCK_MAJOR, i * CONFIG_MMC_BLOCK_MINORS);
		mmc_devices[i] = get_gendisk(devnr, &part);

		/* Invalid capacity of device, do not add to list */
		if (!mmc_devices[i] || !get_capacity(mmc_devices[i]))
			continue;

		mmc_disks[j] = mmc_devices[i];
		j++;

		if (j == PERF_MMC_HOSTS)
			break;
	}

	return j;
}

/*
 * Sample sectors read and written to any MMC devices, update PRCMU
 * qos requirement
 */
static void mmc_load(struct work_struct *work)
{
	static unsigned long long old_sectors_read[PERF_MMC_HOSTS];
	static unsigned long long old_sectors_written[PERF_MMC_HOSTS];
	static struct gendisk *mmc_disks[PERF_MMC_HOSTS];
	static int cycle, nrdisk;
	static bool old_mode;
	unsigned long long sectors;
	bool new_mode = false;
	int i;

	if (!cycle) {
		memset(&mmc_disks, 0, sizeof(mmc_disks));
		nrdisk = scan_mmc_devices(mmc_disks);
		cycle = PERF_MMC_RESCAN_CYCLES;
	}
	cycle--;

	for (i = 0; i < nrdisk; i++) {
		sectors = part_stat_read(&(mmc_disks[i]->part0),
						sectors[READ]);

		if (old_sectors_read[i] &&
			sectors > old_sectors_read[i] &&
			(sectors - old_sectors_read[i]) >
			PERF_MMC_LIMIT_READ)
			new_mode = true;

		old_sectors_read[i] = sectors;
		sectors = part_stat_read(&(mmc_disks[i]->part0),
						sectors[WRITE]);

		if (old_sectors_written[i] &&
			sectors > old_sectors_written[i] &&
			(sectors - old_sectors_written[i]) >
			PERF_MMC_LIMIT_WRITE)
			new_mode = true;

		old_sectors_written[i] = sectors;
	}

	if (!old_mode && new_mode)
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
						"mmc", 125);

	if (old_mode && !new_mode)
		prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
						"mmc", 25);

	old_mode = new_mode;

	schedule_delayed_work(&work_mmc,
				 msecs_to_jiffies(PERF_MMC_PROBE_DELAY));

}

static int __init performance_register(void)
{
	int ret;

	prcmu_qos_add_requirement(PRCMU_QOS_ARM_OPP, "mmc", 25);
	prcmu_qos_add_requirement(PRCMU_QOS_ARM_OPP, "wlan", 25);

	INIT_DELAYED_WORK_DEFERRABLE(&work_wlan_workaround,
				     wlan_load);
	INIT_DELAYED_WORK_DEFERRABLE(&work_mmc, mmc_load);


	ret = schedule_delayed_work(&work_mmc,
				 msecs_to_jiffies(PERF_MMC_PROBE_DELAY));
	if (ret) {
		pr_err("ux500: performance: Fail to schedudle mmc work\n");
		goto out;
	}

	ret = schedule_delayed_work_on(0,
				       &work_wlan_workaround,
				       msecs_to_jiffies(WLAN_PROBE_DELAY));
	if (ret)
		pr_err("ux500: performance: Fail to schedudle wlan work\n");
out:
	return ret;
}
late_initcall(performance_register);

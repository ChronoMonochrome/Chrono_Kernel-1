/*
 * arch/arm/kernel/topology.c
 *
 * Copyright (C) 2011 Linaro Limited.
 * Written by: Vincent Guittot
 *
 * based on arch/sh/kernel/topology.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/node.h>
#include <linux/nodemask.h>
#include <linux/sched.h>
#include <linux/cpumask.h>

#ifdef CONFIG_DEBUG_FS_ENABLED
#include <linux/debugfs.h>
#include <linux/uaccess.h>	/* for copy_from_user */
#endif

#include <asm/cputype.h>
#include <asm/topology.h>

#define MPIDR_SMP_BITMASK (0x3 << 30)
#define MPIDR_SMP_VALUE (0x2 << 30)

#define MPIDR_MT_BITMASK (0x1 << 24)

/*
 * These masks reflect the current use of the affinity levels.
 * The affinity level can be up to 16 bits according to ARM ARM
 */

#define MPIDR_LEVEL0_MASK 0x3
#define MPIDR_LEVEL0_SHIFT 0

#define MPIDR_LEVEL1_MASK 0xF
#define MPIDR_LEVEL1_SHIFT 8

#define MPIDR_LEVEL2_MASK 0xFF
#define MPIDR_LEVEL2_SHIFT 16

struct cputopo_arm cpu_topology[NR_CPUS];

/*
 * cpu topology mask management
 */

static void default_cpu_topology_mask(void);
static void (*set_cpu_topology_mask)(void) = default_cpu_topology_mask;

/*
 * cpu power scale management
 */

/*
 * a per cpu data structure should be better because each cpu is mainly
 * using its own cpu_power even it's not always true because of
 * no_hz_idle_balance
 */
static DEFINE_PER_CPU(unsigned int, cpu_scale);


struct cputopo_scale {
	int scale;
};

static struct cputopo_scale cpu_power[NR_CPUS];

#define CPU_TOPO_MAX_SCALING 2

#define ARM_CORTEX_A9_DEFAULT_SCALE 0
#define ARM_CORTEX_A9_POWER_SCALE 1

/* This table sets the cpu_power scale of a cpu according to the sched_mc mode.
 * The content of this table could be SoC specific so we should add a method to
 * overwrite this default table.
 * TODO: Study how to use DT for setting this table
 */
static unsigned long table_cpu_power[CPU_TOPO_MAX_SCALING] = {
	{1024}, /* default */
	{4096}, /* Power save mode CA9 MP */
};

static void set_power_scale(unsigned int cpuid, unsigned int idx)
{
	cpu_power[cpuid].scale = idx;
	per_cpu(cpu_scale, cpuid) = table_cpu_power[cpu_power[cpuid].scale];
	smp_wmb();
}

static int topo_cpuscale_init(void)
{
	/* Nothing to do right now */
	return 0;
}

/*
 * Update the cpu power
 */

unsigned long arch_scale_freq_power(struct sched_domain *sd, int cpu)
{
	return per_cpu(cpu_scale, cpu);
}

/*
 * default topology function
 */

const struct cpumask *cpu_coregroup_mask(int cpu)
{
	return &cpu_topology[cpu].core_sibling;
}

/*
 * clear cpu topology masks
 */
static void clear_cpu_topology_mask(void)
{
	unsigned int cpuid;
	for_each_possible_cpu(cpuid) {
		struct cputopo_arm *cpuid_topo = &(cpu_topology[cpuid]);
		cpumask_clear(&cpuid_topo->core_sibling);
		cpumask_clear(&cpuid_topo->thread_sibling);
	}
	smp_wmb();
}

/*
 * default_cpu_topology_mask set the core and thread mask as described in the
 * ARM ARM
 */
static void default_cpu_topology_mask(void)
{
	unsigned int cpuid, cpu;

	for_each_possible_cpu(cpuid) {
		struct cputopo_arm *cpuid_topo = &cpu_topology[cpuid];

		for_each_possible_cpu(cpu) {
			struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

			if (cpuid_topo->socket_id == cpu_topo->socket_id) {
				cpumask_set_cpu(cpuid, &cpu_topo->core_sibling);
				if (cpu != cpuid)
					cpumask_set_cpu(cpu,
						&cpuid_topo->core_sibling);

				if (cpuid_topo->core_id == cpu_topo->core_id) {
					cpumask_set_cpu(cpuid,
						&cpu_topo->thread_sibling);
					if (cpu != cpuid)
						cpumask_set_cpu(cpu,
							&cpuid_topo->thread_sibling);
				}
			}
		}

		set_power_scale(cpuid, ARM_CORTEX_A9_DEFAULT_SCALE);
	}
	smp_wmb();
}

/*
 * For Cortex-A9 MPcore, we emulate a multi-package topology in power mode.
 * The goal is to gathers tasks on 1 virtual package
 */
static void power_cpu_topology_mask_CA9(void)
{
	unsigned int cpuid, cpu, idx;

	if (num_possible_cpus() > 2)
		idx = ARM_CORTEX_A9_DEFAULT_SCALE;
	else
		idx = ARM_CORTEX_A9_POWER_SCALE;

	for_each_possible_cpu(cpuid) {
		struct cputopo_arm *cpuid_topo = &cpu_topology[cpuid];

		for_each_possible_cpu(cpu) {
			struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

			if ((cpuid_topo->socket_id == cpu_topo->socket_id)
			&& ((cpuid & 0x1) == (cpu & 0x1))) {
				cpumask_set_cpu(cpuid, &cpu_topo->core_sibling);
				if (cpu != cpuid)
					cpumask_set_cpu(cpu,
						&cpuid_topo->core_sibling);

				if (cpuid_topo->core_id == cpu_topo->core_id) {
					cpumask_set_cpu(cpuid,
						&cpu_topo->thread_sibling);
					if (cpu != cpuid)
						cpumask_set_cpu(cpu,
							&cpuid_topo->thread_sibling);
				}
			}
		}

		set_power_scale(cpuid, idx);
	}
	smp_wmb();
}

#define ARM_FAMILY_MASK 0xFF0FFFF0
#define ARM_CORTEX_A9_FAMILY 0x410FC090

/* update_cpu_topology_policy select a cpu topology policy according to the
 * available cores.
 * TODO: The current version assumes that all cores are exactly the same which
 * might not be true. We need to update it to take into account various
 * configuration among which system with different kind of core.
 */
static int update_cpu_topology_policy(void)
{
	unsigned long cpuid;

	if (sched_mc_power_savings == POWERSAVINGS_BALANCE_NONE) {
		set_cpu_topology_mask = default_cpu_topology_mask;
		return 0;
	}

	cpuid = read_cpuid_id();
	cpuid &= ARM_FAMILY_MASK;

	switch (cpuid) {
	case ARM_CORTEX_A9_FAMILY:
		set_cpu_topology_mask = power_cpu_topology_mask_CA9;
	break;
	default:
		set_cpu_topology_mask = default_cpu_topology_mask;
	break;
	}

	return 0;
}

/*
 * store_cpu_topology is called at boot when only one cpu is running
 * and with the mutex cpu_hotplug.lock locked, when several cpus have booted,
 * which prevents simultaneous write access to cpu_topology array
 */
void store_cpu_topology(unsigned int cpuid)
{
	struct cputopo_arm *cpuid_topo = &cpu_topology[cpuid];
	unsigned int mpidr;

	/* If the cpu topology has been already set, just return */
	if (cpuid_topo->core_id != -1)
		return;

	mpidr = read_cpuid_mpidr();

	/* create cpu topology mapping */
	if ((mpidr & MPIDR_SMP_BITMASK) == MPIDR_SMP_VALUE) {
		/*
		 * This is a multiprocessor system
		 * multiprocessor format & multiprocessor mode field are set
		 */

		if (mpidr & MPIDR_MT_BITMASK) {
			/* core performance interdependency */
			cpuid_topo->thread_id = (mpidr >> MPIDR_LEVEL0_SHIFT)
				& MPIDR_LEVEL0_MASK;
			cpuid_topo->core_id = (mpidr >> MPIDR_LEVEL1_SHIFT)
				& MPIDR_LEVEL1_MASK;
			cpuid_topo->socket_id = (mpidr >> MPIDR_LEVEL2_SHIFT)
				& MPIDR_LEVEL2_MASK;
		} else {
			/* largely independent cores */
			cpuid_topo->thread_id = -1;
			cpuid_topo->core_id = (mpidr >> MPIDR_LEVEL0_SHIFT)
				& MPIDR_LEVEL0_MASK;
			cpuid_topo->socket_id = (mpidr >> MPIDR_LEVEL1_SHIFT)
				& MPIDR_LEVEL1_MASK;
		}
	} else {
		/*
		 * This is an uniprocessor system
		 * we are in multiprocessor format but uniprocessor system
		 * or in the old uniprocessor format
		 */
		cpuid_topo->thread_id = -1;
		cpuid_topo->core_id = 0;
		cpuid_topo->socket_id = -1;
	}

	/*
	 * The core and thread sibling masks will be set during the call of
	 * arch_update_cpu_topology
	 */

////	printk(KERN_INFO "CPU%u: thread %d, cpu %d, socket %d, mpidr %x\n",
////		cpuid, cpu_topology[cpuid].thread_id,
////		cpu_topology[cpuid].core_id,
;
}

/*
 * arch_update_cpu_topology is called by the scheduler before building
 * a new sched_domain hierarchy.
 */
int arch_update_cpu_topology(void)
{

	/* clear core threads mask */
	clear_cpu_topology_mask();

	/* set topology policy */
	update_cpu_topology_policy();

	/* set topology mask and power */
	(*set_cpu_topology_mask)();

	return 1;
}

/*
 * init_cpu_topology is called at boot when only one cpu is running
 * which prevent simultaneous write access to cpu_topology array
 */
void init_cpu_topology(void)
{
	unsigned int cpu;

	/* init core mask */
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *cpu_topo = &(cpu_topology[cpu]);

		cpu_topo->thread_id = -1;
		cpu_topo->core_id =  -1;
		cpu_topo->socket_id = -1;
		cpumask_clear(&cpu_topo->core_sibling);
		cpumask_clear(&cpu_topo->thread_sibling);

		per_cpu(cpu_scale, cpu) = SCHED_POWER_SCALE;

		cpu_power[cpu].scale = ARM_CORTEX_A9_DEFAULT_SCALE;
	}
	smp_wmb();
}

/*
 * debugfs interface for scaling cpu power
 */

#ifdef CONFIG_DEBUG_FS_ENABLED
static struct dentry *topo_debugfs_root;

static ssize_t dbg_write(struct file *file, const char __user *buf,
						size_t size, loff_t *off)
{
	unsigned int *value = file->f_dentry->d_inode->i_private;
	char cdata[128];
	unsigned long tmp;
	unsigned int cpu, scale, freq;

	if (size < (sizeof(cdata)-1)) {
		if (copy_from_user(cdata, buf, size))
			return -EFAULT;
		cdata[size] = 0;
		if (!strict_strtoul(cdata, 10, &tmp)) {
			*value = tmp;

			for_each_online_cpu(cpu) {
				scale = cpu_power[cpu].scale;
				freq = cpu_power[cpu].freq;

				per_cpu(cpu_scale, cpu) =
					table_cpu_power[scale][freq];
				smp_wmb();
			}
		}
		return size;
	}
	return -EINVAL;
}

static ssize_t dbg_read(struct file *file, char __user *buf,
						size_t size, loff_t *off)
{
	unsigned int *value = file->f_dentry->d_inode->i_private;
	char cdata[128];
	unsigned int len;

	len = sprintf(cdata, "%u\n", *value);
	return simple_read_from_buffer(buf, size, off, cdata, len);
}

static const struct file_operations debugfs_fops = {
	.read = dbg_read,
	.write = dbg_write,
};

static struct dentry *topo_debugfs_register(unsigned int cpu,
						struct dentry *parent)
{
	struct dentry *cpu_d, *d;
	char cpu_name[16];

	sprintf(cpu_name, "cpu%u", cpu);

	cpu_d = debugfs_create_dir(cpu_name, parent);
	if (!cpu_d)
		return NULL;

	d = debugfs_create_file("cpu_power", S_IRUGO  | S_IWUGO,
				cpu_d, &per_cpu(cpu_scale, cpu), &debugfs_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file("scale", S_IRUGO | S_IWUGO,
				cpu_d, &cpu_power[cpu].scale, &debugfs_fops);
	if (!d)
		goto err_out;
	d = debugfs_create_file("freq", S_IRUGO,
				cpu_d, &cpu_power[cpu].freq, &debugfs_fops);
	if (!d)
		goto err_out;

	return cpu_d;

err_out:
	debugfs_remove_recursive(cpu_d);
	return NULL;
}

static int __init topo_debugfs_init(void)
{
	struct dentry *d;
	unsigned int cpu;

	d = debugfs_create_dir("cpu_topo", NULL);
	if (!d)
		return -ENOMEM;
	topo_debugfs_root = d;

	for_each_possible_cpu(cpu) {
		d = topo_debugfs_register(cpu, topo_debugfs_root);
		if (d == NULL)
			goto err_out;
	}
	return 0;

err_out:
	debugfs_remove_recursive(topo_debugfs_root);
	return -ENOMEM;
}

late_initcall(topo_debugfs_init);
#endif

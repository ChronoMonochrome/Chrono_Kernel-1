/*
 * drivers/cpufreq/cpufreq_limits_on_suspend.c
 * 
 * Copyright (c) 2014, Shilin Victor <chrono.monochrome@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static bool boot_complete = false;

static bool cpu_freq_limits = false;
static unsigned int screenoff_min_cpufreq = 100000;
static unsigned int screenoff_max_cpufreq = 400000;

static unsigned int screenon_min_cpufreq = 0; // screenon_min_cpufreq and screenon_max_cpufreq uses system values
static unsigned int screenon_max_cpufreq = 0;

static unsigned int restore_screenon_min_cpufreq = 0;// these values will be gotten from system at load
static unsigned int restore_screenon_max_cpufreq = 0; 

bool is_boot_complete(void) {
	return boot_complete;
}

extern bool is_earlysuspended(void);

void cpufreq_limits_update() {
	int new_min, new_max;
	
	bool suspend_state = is_earlysuspended();
	
	if (cpu_freq_limits) {
		/*
		 * since we don't have different cpufreq settings for different CPU cores,
		 * we'll update cpufreq limits only for first core.
		 */
		struct cpufreq_policy *policy = cpufreq_cpu_get(0); 

		new_min = suspend_state ? screenoff_min_cpufreq : screenon_min_cpufreq;
		new_max = suspend_state ? screenoff_max_cpufreq : screenon_max_cpufreq;
		
		policy->min = new_min;
		policy->max = new_max;
		pr_err("[cpufreq_limits] new cpufreqs are %d - %d kHz\n", policy->min, policy->max);
		
		if (restore_screenon_max_cpufreq < screenon_max_cpufreq)
			restore_screenon_max_cpufreq = screenon_max_cpufreq;
		if (restore_screenon_min_cpufreq < screenon_min_cpufreq)
			restore_screenon_min_cpufreq = screenon_min_cpufreq;
	}
}  

static int cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
  
	if (event != CPUFREQ_ADJUST)
		return 0;
	
	if (cpu_freq_limits) {
	  
		struct cpufreq_policy *policy = data;

		bool is_suspend = is_earlysuspended();
		
		/* FIXME: would be better move that code to init function ?   */
		if (!boot_complete) {
		
			if (policy)
				policy->min = 200000;
	
			if (!screenoff_min_cpufreq)
				  screenoff_min_cpufreq = policy->min;
			if (!screenoff_max_cpufreq)
				  screenoff_max_cpufreq = policy->max;
			
			boot_complete = true;
		}
		/*------------------------------------------------------------*/
	  
		if (!is_suspend && (screenon_max_cpufreq != policy->max)) 
			screenon_max_cpufreq = policy->max;
	
		if (!is_suspend && (screenon_min_cpufreq != policy->min)) 
			screenon_min_cpufreq = policy->min;

		if (!is_suspend && (policy->min == screenoff_min_cpufreq)) { 
			  policy->min = restore_screenon_min_cpufreq;
			  pr_err("[cpufreq_limits] new cpufreqs are %d - %d kHz\n", policy->min, policy->max);
		}

		if (!is_suspend && (policy->max == screenoff_max_cpufreq)) { 
			  policy->max = restore_screenon_max_cpufreq;
			  pr_err("[cpufreq_limits] new cpufreqs are %d - %d kHz\n", policy->min, policy->max);
		}
			
	}

	return 0;
}

static struct notifier_block cpufreq_notifier_block = 
{
	.notifier_call = cpufreq_callback,
};

static ssize_t cpufreq_limits_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
	sprintf(buf,  "status: %s\n"
		      "Screen off settings:\n"
		      "min = %d kHz\n"
		      "max = %d kHz\n"
		      "Screen on settings:\n"
		      "min = %d kHz\n"
		      "max = %d kHz\n",
		      cpu_freq_limits ? "on" : "off",
		      screenoff_min_cpufreq,
		      screenoff_max_cpufreq,
		      screenon_min_cpufreq,  
		      screenon_max_cpufreq
	);

	return strlen(buf);
}

static ssize_t cpufreq_limits_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
  	if (!strncmp(&buf[0], "screenon_min=", 13)) {
		if (!sscanf(&buf[13], "%d", &screenon_min_cpufreq))
			goto invalid_input;
	}

	if (!strncmp(&buf[0], "screenon_max=", 13)) {
		if (!sscanf(&buf[13], "%d", &screenon_max_cpufreq))
			goto invalid_input;
	}
  
	if (!strncmp(buf, "on", 2)) {
		cpu_freq_limits = true;
		return count;
	}

	if (!strncmp(buf, "off", 3)) {
		cpu_freq_limits = false;
		return count;
	}
	
	if (!strncmp(&buf[0], "min=", 4)) {
		if (!sscanf(&buf[4], "%d", &screenoff_min_cpufreq))
			goto invalid_input;
	}

	if (!strncmp(&buf[0], "max=", 4)) {
		if (!sscanf(&buf[4], "%d", &screenoff_max_cpufreq))
			goto invalid_input;
	}
	
	return count;

invalid_input:
	pr_err("[cpufreq_limits] invalid input");
	return -EINVAL;
}

static struct kobj_attribute cpufreq_limits_interface = __ATTR(cpufreq_limits_on_suspend, 0644,
									   cpufreq_limits_show,
									   cpufreq_limits_store);

static struct attribute *cpufreq_attrs[] = {
	&cpufreq_limits_interface.attr,
	NULL,
};

static struct attribute_group cpufreq_interface_group = {
	.attrs = cpufreq_attrs,
};

static struct kobject *cpufreq_kobject;

static int cpufreq_limits_driver_init(void)
{
	int ret;

	pr_err("[cpufreq_limits] initialized module with min %d and max %d MHz limits",
					 screenoff_min_cpufreq / 1000,  screenoff_max_cpufreq / 1000
	);
	
	cpufreq_kobject = kobject_create_and_add("cpufreq", kernel_kobj);
	if (!cpufreq_kobject) {
		pr_err("[cpufreq] Failed to create kobject interface\n");
	}

	ret = sysfs_create_group(cpufreq_kobject, &cpufreq_interface_group);
	if (ret) {
		kobject_put(cpufreq_kobject);
	}
	
	cpufreq_register_notifier(&cpufreq_notifier_block, CPUFREQ_POLICY_NOTIFIER);
	
	return ret;
}

static void cpufreq_limits_driver_exit(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	if (screenon_min_cpufreq) {
		policy->min = screenon_min_cpufreq;
		pr_err("[cpufreq] min screenon cpufreq limit restoren -> %d\n", policy->min);
	}
	if (screenon_max_cpufreq) {
		policy->max = screenon_max_cpufreq;
		pr_err("[cpufreq] max screenon cpufreq limit restoren -> %d\n", policy->max);
	}
	
}

module_init(cpufreq_limits_driver_init);
module_exit(cpufreq_limits_driver_exit);

MODULE_AUTHOR("Shilin Victor <chrono.monochrome@gmail.com>");
MODULE_DESCRIPTION("CPUfreq limits on suspend");
MODULE_LICENSE("GPL");

/*
 * Author: Shilin Victor aka ChronoMonochrome <chrono.monochrome@gmail.com>
 *
 * Copyright 2015 Shilin Victor
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h> 

bool pm_sync_active __read_mostly = true;

static ssize_t pm_sync_active_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (pm_sync_active ? 1 : 0));
}

static ssize_t pm_sync_active_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;

	if(sscanf(buf, "%u\n", &data) == 1) {
		if (data == 1) {
			pr_info("%s: pm sync enabled\n", __FUNCTION__);
			pm_sync_active = true;
		}
		else if (data == 0) {
			pr_info("%s: pm sync disabled\n", __FUNCTION__);
			pm_sync_active = false;
		}
		else
			pr_info("%s: bad value: %u\n", __FUNCTION__, data);
	} else
		pr_info("%s: unknown input!\n", __FUNCTION__);

	return count;
}

static ssize_t pm_sync_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "v1.0 (c) ChronoMonochrome\n");
}

static struct kobj_attribute pm_sync_active_attribute = 
	__ATTR(pm_sync_active, 0644,
		pm_sync_active_show,
		pm_sync_active_store);

static struct kobj_attribute pm_sync_version_attribute = 
	__ATTR(pm_sync_version, 0444, pm_sync_version_show, NULL);

static struct attribute *pm_sync_active_attrs[] =
	{
		&pm_sync_active_attribute.attr,
		&pm_sync_version_attribute.attr,
		NULL,
	};

static struct attribute_group pm_sync_active_attr_group =
	{
		.attrs = pm_sync_active_attrs,
	};

static struct kobject *pm_sync_kobj;

static int pm_sync_init(void)
{
	int res = 0;

	pm_sync_kobj = kobject_create_and_add("pm_sync", kernel_kobj);
	if (!pm_sync_kobj) {
		pr_err("%s pm_sync kobject create failed!\n", __FUNCTION__);
		return -ENOMEM;
        }

	res = sysfs_create_group(pm_sync_kobj,
			&pm_sync_active_attr_group);

        if (res) {
		pr_info("%s pm_sync sysfs create failed!\n", __FUNCTION__);
		kobject_put(pm_sync_kobj);
	}
	return res;
}

static void pm_sync_exit(void)
{
	if (pm_sync_kobj != NULL)
		kobject_put(pm_sync_kobj);
}

module_init(pm_sync_init);
module_exit(pm_sync_exit);
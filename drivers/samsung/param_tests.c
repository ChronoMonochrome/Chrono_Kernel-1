/*
 * param.c
 *
 * Parameter read & save driver on param partition.
 *
 * COPYRIGHT(C) Samsung Electronics Co.Ltd. 2006-2010 All Right Reserved.
 *
 * Author: Jeonghwan Min <jeonghwan.min@samsung.com>
 *
 * 2008.02.26. Supprot for BML layer.
 * 2009.12.07. Modified to support for FSR_BML
 * 2010.04.22. Remove FSR_BML
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/file.h>
#include <mach/hardware.h>
#include <mach/sec_param.h>

static bool initialized = false;

#define kloge(fmt, arg...)  printk(KERN_ERR "%s(%d): " fmt "\n" , __func__, __LINE__, ## arg)
#define klogi(fmt, arg...)  printk(KERN_INFO fmt "\n" , ## arg)

/* added by geunyoung for LFS. */
static int load_lfs_param_value(void);
static int save_lfs_param_value(void);

static void late_init(void)
{
	load_lfs_param_value();
	initialized = true;
}

static status_t param_status;

static void set_param_value(int idx, void *value)
{
	int i, str_i;

	klogi("inside set_param_value1 idx = %d, value = %d", idx, *(int*)value);

	for (i = 0; i < MAX_PARAM; i++) {
		if (i < (MAX_PARAM - MAX_STRING_PARAM)) {
			if(param_status.param_int_list.param_list[i].ident == idx) {
				param_status.param_int_list.param_list[i].value = *(int *)value;
			}
		}
		else {
			str_i = (i - (MAX_PARAM - MAX_STRING_PARAM));
			if(param_status.param_str_list[str_i].ident == idx) {
				strlcpy(param_status.param_str_list[str_i].value,
					(char *)value, PARAM_STRING_SIZE);
			}
		}
	}

	save_lfs_param_value();
}

static void get_param_value(int idx, void *value)
{
	int i, str_i;

	for (i = 0 ; i < MAX_PARAM; i++) {
		if (i < (MAX_PARAM - MAX_STRING_PARAM)) {
			if(param_status.param_int_list.param_list[i].ident == idx) {
				*(int *)value = param_status.param_int_list.param_list[i].value;
			}
		}
		else {
			str_i = (i - (MAX_PARAM - MAX_STRING_PARAM));
			if(param_status.param_str_list[str_i].ident == idx) {
				strlcpy((char *)value,
					param_status.param_str_list[str_i].value, PARAM_STRING_SIZE);
			}
		}
	}
}

static char* param_idx_to_str(int idx)
{
	switch (idx) {
	case __LOAD_TESTKERNEL:
		return "LOAD_TESTKERNEL";
	case __BOOT_DELAY:
		return "BOOT_DELAY";
	case __LCD_LEVEL:
		return "LCD_LEVEL";
	case __SWITCH_SEL:
		return "SWITCH_SEL";
	case __PHONE_DEBUG_ON:
		return "PHONE_DEBUG_ON";
	case __LCD_DIM_LEVEL:
		return "LCD_DIM_LEVEL";
	case __LCD_DIM_TIME:
		return "LCD_DIM_TIME";
	case __FORCE_PRERECOVERY:
		return "FORCE_PRERECOVERY";
	case __REBOOT_MODE:
		return "REBOOT_MODE";
	case __NATION_SEL:
		return "NATION_SEL";
	case __DEBUG_LEVEL:
		return "DEBUG_LEVEL";
	case __SET_DEFAULT_PARAM:
		return "SET_DEFAULT_PARAM";
	case __BATT_CAPACITY:
		return "BATT_CAPACITY";
	case __LOAD_KERNEL2:
		return "LOAD_KERNEL2";
	case __FLASH_LOCK_STATUS:
		return "FLASH_LOCK_STATUS";
	case __PARAM_INT_14:
		return "PARAM_INT_14";
	case __VERSION:
		return "VERSION";
	case __CMDLINE:
		return "CMDLINE";
	case __DELTA_LOCATION:
		return "DELTA_LOCATION";
	case __CMDLINE_MODE:
		return "CMDLINE_MODE";
	case __PARAM_STR_4:
		return "PARAM_STR_4";
	}

	return "NULL";
}

static int param_str_to_int(char *param_str)
{
	if (strstr(param_str, "LOAD_TESTKERNEL"))
		return __LOAD_TESTKERNEL;
	else if (strstr(param_str, "BOOT_DELAY"))
		return __BOOT_DELAY;
	else if (strstr(param_str, "LCD_LEVEL"))
		return __LCD_LEVEL;
	else if (strstr(param_str, "SWITCH_SEL"))
		return __SWITCH_SEL;
	else if (strstr(param_str, "PHONE_DEBUG_ON"))
		return __PHONE_DEBUG_ON;
	else if (strstr(param_str, "LCD_DIM_LEVEL"))
		return __LCD_DIM_LEVEL;
	else if (strstr(param_str, "LCD_DIM_TIME"))
		return __LCD_DIM_TIME;
	else if (strstr(param_str, "FORCE_PRERECOVERY"))
		return __FORCE_PRERECOVERY;
	else if (strstr(param_str, "REBOOT_MODE"))
		return __REBOOT_MODE;
	else if (strstr(param_str, "NATION_SEL"))
		return __NATION_SEL;
	else if (strstr(param_str, "DEBUG_LEVEL"))
		return __DEBUG_LEVEL;
	else if (strstr(param_str, "SET_DEFAULT_PARAM"))
		return __SET_DEFAULT_PARAM;
	else if (strstr(param_str, "BATT_CAPACITY"))
		return __BATT_CAPACITY;
	else if (strstr(param_str, "LOAD_KERNEL2"))
		return __LOAD_KERNEL2;
	else if (strstr(param_str, "FLASH_LOCK_STATUS"))
		return __FLASH_LOCK_STATUS;
	else if (strstr(param_str, "PARAM_INT_14"))
		return __PARAM_INT_14;
	else if (strstr(param_str, "VERSION"))
		return __VERSION;
	else if (strstr(param_str, "CMDLINE"))
		return __CMDLINE;
	else if (strstr(param_str, "DELTA_LOCATION"))
		return __DELTA_LOCATION;
	else if (strstr(param_str, "CMDLINE_MODE"))
		return __CMDLINE_MODE;

	return -EINVAL;
}

static inline bool is_typeof_char(int idx)
{
	return (idx >= (MAX_PARAM - MAX_STRING_PARAM));
}

static struct kobject *param_kobject;

#define ATTR_RW(_name)  \
        static struct kobj_attribute _name##_interface = __ATTR(_name, 0644, _name##_show, _name##_store);

#define ATTR_RO(_name)  \
        static struct kobj_attribute _name##_interface = __ATTR(_name, 0444, _name##_show, NULL);

#define ATTR_WO(_name)  \
        static struct kobj_attribute _name##_interface = __ATTR(_name, 0200, _name##_show, NULL);

static ssize_t param_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int count = 0, i;
	signed int value = 0;
	signed char str_val[PARAM_STRING_SIZE];

	if (!initialized)
		late_init();

	//get_param_value(__LOAD_TESTKERNEL, &value);
	//count += sprintf(buf, "%s: %d\n", "__LOAD_TESTKERNEL", value);

	for (i = 1; i < MAX_PARAM - MAX_STRING_PARAM; i++) {
		get_param_value(i, &value);
		count += sprintf(buf, "%s%s: %d (str: %d)\n", buf, param_idx_to_str(i), value, is_typeof_char(i));
	}

	for (; i < MAX_PARAM; i++) {
		get_param_value(i, &str_val);
		count += sprintf(buf, "%s%s: %s (str: %d)\n", buf, param_idx_to_str(i), str_val, is_typeof_char(i));
	}

	return count;
}

#define MAX_PARAM_NAME_LEN 32
#define MAX_PARAM_VALUE_LEN 256

static ssize_t param_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int idx, int_val;
	int space_idx;
	int buf_len;
	char param_str[MAX_PARAM_NAME_LEN], char_val[MAX_PARAM_VALUE_LEN], *char_val_ptr;

	if (!initialized)
		late_init();

	buf_len = strlen(buf);

	char_val_ptr = strstr(buf, " ");
	if (!char_val_ptr) {
		pr_err("%s: not enough arguments (2 required)\n", __func__);
		return -EINVAL;
	}

	char_val_ptr++;

	space_idx = (char_val_ptr - 1) - buf;
	if (space_idx > MAX_PARAM_NAME_LEN || space_idx < 0) {
		pr_err("%s: space_idx: %d\n", __func__, space_idx);
		return -EINVAL;
	}

	memset(param_str, 0, MAX_PARAM_NAME_LEN);
	strncpy(param_str, buf, space_idx);
	//pr_err("%s: copy %d bytes to char_val from str %s", __func__, buf_len - space_idx - 2, char_val_ptr);

	memset(char_val, 0, MAX_PARAM_VALUE_LEN);
	strncpy(char_val, char_val_ptr, buf_len - space_idx - 2);

	idx = param_str_to_int(param_str);
	if (idx < 0) {
		pr_err("%s: invalid inputs: %s\n", __func__, param_str);
		return -EINVAL;
	}

	//pr_err("%s: idx: %d (str: %d)\n", __func__, idx, is_typeof_char(idx));

	//pr_err("%s: write '%s' to '%s'\n", __func__, char_val, param_str);

	if (is_typeof_char(idx)) {
		set_param_value(idx, &char_val);
	} else {
		if (sscanf(char_val, "%d", &int_val) != 1) {
			pr_err("%s: failed to decode val: %s\n", __func__, char_val);
			return -EINVAL;
		}

		set_param_value(idx, &int_val);
	}

	return count;
}
ATTR_RW(param);

static struct attribute *param_attrs[] = {
	&param_interface.attr,
	NULL,
};

static struct attribute_group param_interface_group = {
	.attrs = param_attrs,
};

static int param_init(void)
{
	int ret;

        param_kobject = kobject_create_and_add("param", kernel_kobj);
        if (!param_kobject)
                pr_err("[param] Failed to create kobject interface\n");

        ret = sysfs_create_group(param_kobject, &param_interface_group);
        if (ret)
                kobject_put(param_kobject);


	return 0;

}

static void param_exit(void)
{
	klogi("param_exit");
}

module_init(param_init);
module_exit(param_exit);

/* added by geunyoung for LFS. */
#define PARAM_FILE_NAME	"/mnt/.lfs/param.blk"
#define PARAM_RD	0
#define PARAM_WR	1

static int lfs_param_op(int dir, int flags)
{
	struct file *filp;
	mm_segment_t fs;

	int ret;

	filp = filp_open(PARAM_FILE_NAME, flags, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __FUNCTION__,
				PTR_ERR(filp));

		return -1;
	}

	fs = get_fs();
	set_fs(get_ds());

	if (dir == PARAM_RD)
		ret = filp->f_op->read(filp, (char __user *)&param_status,
				sizeof(param_status), &filp->f_pos);
	else
		ret = filp->f_op->write(filp, (char __user *)&param_status,
				sizeof(param_status), &filp->f_pos);

	set_fs(fs);
	filp_close(filp, NULL);

	return ret;
}

static int load_lfs_param_value(void)
{
	int ret;

	ret = lfs_param_op(PARAM_RD, O_RDONLY);

	if (ret == sizeof(param_status)) {
		pr_info("%s: param.blk read successfully. (size: %d)\n", __FUNCTION__, ret);
	}

	return ret;
}

static int save_lfs_param_value(void)
{
	int ret;

	ret = lfs_param_op(PARAM_WR, O_RDWR|O_SYNC);

	if (ret == sizeof(param_status)) {
		pr_info("%s: param.blk write successfully. (size: %d)\n", __FUNCTION__, ret);
	}

	return 0;
}


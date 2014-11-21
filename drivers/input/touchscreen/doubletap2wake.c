/*
 * drivers/input/touchscreen/doubletap2wake.c
 *
 *
 * Copyright (c) 2012, Dennis Rassmann <showp1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <asm-generic/cputime.h>
#include <linux/input/doubletap2wake.h>
#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE_WAKELOCK
#include <linux/wakelock.h>
#endif

/* Tuneables */
/* Tuneables */
#define DT2W_DEBUG		0
#define DT2W_DEFAULT		0

#define DT2W_PWRKEY_DUR		60
#define DT2W_FEATHER		200
#define DT2W_TIME		700

/* Resources */
int dt2w_switch = DT2W_DEFAULT;
static cputime64_t tap_time_pre = 0;
static int touch_nr = 0, x_pre = 0, y_pre = 0;
static bool scr_suspended = false, exec_count = true;
static struct input_dev * doubletap2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);

#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE_WAKELOCK
static bool use_wakelock = true;
static struct wake_lock dt2w_wake_lock;
#endif

/* Read cmdline for dt2w */
static int __init read_dt2w_cmdline(char *dt2w)
{
	if (strcmp(dt2w, "1") == 0) {
		printk(KERN_INFO "[cmdline_dt2w]: DoubleTap2Wake enabled. | dt2w='%s'", dt2w);
		dt2w_switch = 1;
	} else if (strcmp(dt2w, "0") == 0) {
		printk(KERN_INFO "[cmdline_dt2w]: DoubleTap2Wake disabled. | dt2w='%s'", dt2w);
		dt2w_switch = 0;
	} else {
		printk(KERN_INFO "[cmdline_dt2w]: No valid input found. Going with default: | dt2w='%u'", dt2w_switch);
	}
	return 1;
}
__setup("dt2w=", read_dt2w_cmdline);

/* PowerKey setter */
void doubletap2wake_setdev(struct input_dev * input_device) {
	doubletap2wake_pwrdev = input_device;
	return;
}
EXPORT_SYMBOL(doubletap2wake_setdev);

/* PowerKey work func */
static void doubletap2wake_presspwr(struct work_struct * doubletap2wake_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
                return;
	input_event(doubletap2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(doubletap2wake_pwrdev, EV_SYN, 0, 0);
	msleep(DT2W_PWRKEY_DUR);
	input_event(doubletap2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(doubletap2wake_pwrdev, EV_SYN, 0, 0);
	msleep(DT2W_PWRKEY_DUR);
        mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(doubletap2wake_presspwr_work, doubletap2wake_presspwr);

/* PowerKey trigger */
void doubletap2wake_pwrtrigger(void) {
	schedule_work(&doubletap2wake_presspwr_work);
        return;
}

void doubletap2wake_reset(void)
{
	exec_count = true;
	touch_nr = 0;
	tap_time_pre = 0;
	x_pre = 0;
	y_pre = 0;
}

static unsigned int calc_feather(int coord, int prev_coord) {
	int calc_coord = 0;
	calc_coord = coord-prev_coord;
	if (calc_coord < 0)
		calc_coord = calc_coord * (-1);
	return calc_coord;
}

/* Sweep2wake main function */
void detect_doubletap2wake(int x, int y, bool st)
{
	bool single_touch = st;


#if DT2W_DEBUG
        pr_info("[doubletap2wake]: x,y(%4d,%4d) single:%s\n",
                x, y, (single_touch) ? "true" : "false");
#endif
	if ((!single_touch) && (dt2w_switch > 0) && (exec_count) && (scr_suspended)) {
		if (touch_nr == 0) {
			tap_time_pre = ktime_to_ms(ktime_get());
			x_pre = x;
			y_pre = y;
			touch_nr++;
		} else if (touch_nr == 1) {
			if ((calc_feather(x, x_pre) < DT2W_FEATHER) &&
			    (calc_feather(y, y_pre) < DT2W_FEATHER) &&
			    ((ktime_to_ms(ktime_get())-tap_time_pre) < DT2W_TIME))
				touch_nr++;
			else
				doubletap2wake_reset();
				/* Meticulus:
				 * A disqualifing 2nd tap should also be a qualifing
				 * first tap.
				 */
				detect_doubletap2wake(x, y, single_touch);
				return;
		} else {
			tap_time_pre = ktime_to_ms(ktime_get());
			x_pre = x;
			y_pre = y;
			touch_nr++;
		}
		if ((touch_nr > 1)) {
			pr_info("[doubletap2wake]:ON\n");
			exec_count = false;
			doubletap2wake_pwrtrigger();
			doubletap2wake_reset();
		}
	}
}

/*
 * INIT / EXIT stuff below here
 */

void dt2w_set_scr_suspended(bool suspended)
{
	scr_suspended = suspended;
	doubletap2wake_reset();
}

static int set_enable(const char *val, struct kernel_param *kp)
{
	int max_tries = 10; 
	int tries = 0;
	if(scr_suspended){
		/*
		 * Meticulus:
		 * We can't change the "enable" while the screen is off because
		 * it causes unbalanced irq enable/disable requests. So
		 * I'm waking the screen and then setting it.
		 */
		if(DT2W_DEBUG)	
			printk("dt2w: cant enable/disable while screen is off! Waking...\n");

		doubletap2wake_pwrtrigger();

		while(scr_suspended && tries <= max_tries){
			msleep(200);
			tries = tries + 1;
		}
	}
	if(strcmp(val, "1") >= 0 || strcmp(val, "true") >= 0){
		dt2w_switch = 1;
		if(DT2W_DEBUG)
			printk("dt2w: enabled\n");

#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE_WAKELOCK
		if(use_wakelock && !wake_lock_active(&dt2w_wake_lock)){
			wake_lock(&dt2w_wake_lock);
			if(DT2W_DEBUG)
				printk("dt2w: wake lock enabled\n");
		}
#endif
	}
	else if(strcmp(val, "0") >= 0 || strcmp(val, "false") >= 0){
		dt2w_switch = 0;
		if(DT2W_DEBUG)
			printk("dt2w: disabled\n");
#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE_WAKELOCK
		if(wake_lock_active(&dt2w_wake_lock)){
			wake_unlock(&dt2w_wake_lock);
		}
#endif

	}else {
		printk("dt2w: invalid input '%s' for 'enable'; use 1 or 0\n", val);
	}

	return 0;
}

module_param_call(enable, set_enable, param_get_int, &dt2w_switch, 0664);

#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE_WAKELOCK

static int set_use_wakelock(const char *val, struct kernel_param *kp){

	if(strcmp(val, "1") >= 0 || strcmp(val, "true") >= 0){
		use_wakelock = true;
		if(use_wakelock && !wake_lock_active(&dt2w_wake_lock)){
			wake_lock(&dt2w_wake_lock);
			if(DT2W_DEBUG)
				printk("dt2w: wake lock enabled\n");
		}
	}
	else if(strcmp(val, "0") >= 0 || strcmp(val, "false") >= 0){
		use_wakelock = false;
		if(wake_lock_active(&dt2w_wake_lock)){
			wake_unlock(&dt2w_wake_lock);
		}

	}else {
		printk("dt2w: invalid input '%s' for 'use_wakelock'; use 1 or 0\n", val);
	}
	return 0;

}

module_param_call(use_wakelock, set_use_wakelock, param_get_bool, &use_wakelock, 0664);
#endif

static int __init doubletap2wake_init(void)
{
#ifdef CONFIG_TOUCHSCREEN_DOUBLETAP2WAKE_WAKELOCK
	wake_lock_init(&dt2w_wake_lock, WAKE_LOCK_SUSPEND, "dt2w_kernel_wake_lock");
#endif
	pr_info("[doubletap2wake]: %s done\n", __func__);
	return 0;
}

static void __exit doubletap2wake_exit(void)
{
	return;
}

module_init(doubletap2wake_init);
module_exit(doubletap2wake_exit);

MODULE_DESCRIPTION("DoubleTap2wake");
MODULE_LICENSE("GPLv2");


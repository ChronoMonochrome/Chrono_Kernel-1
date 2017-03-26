/*
 * drivers/input/touchscreen/sweep2wake.c
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
#include <linux/input/sweep2wake.h>
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_WAKELOCK
#include <linux/wakelock.h>
#endif

/* Tuneables */
#define DEBUG                   0
#define S2W_Y_LIMIT             800 /* screen height */
#define S2W_X_MAX               400 /* screen width */
#define S2W_X_B1                100 /* First x that must be reached in the "swipe" */
#define S2W_X_B2                300 /* Second x that must be reached in the "swipe" */
#define S2W_X_FINAL             50  /* Final criteria for complete swipe; must reach screen width - this */
#define S2W_PWRKEY_DUR          60  /* Milliseconds to "press" power key */

/* Resources */
int s2w_switch = 0;
static bool scr_suspended = false, exec_count = true;
static bool scr_on_touch = false, barrier[2] = {false, false};
static struct input_dev * sweep2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_WAKELOCK
static bool use_wakelock = true;
static struct wake_lock s2w_wake_lock;
#endif

/* Read cmdline for s2w */
static int __init read_s2w_cmdline(char *s2w)
{
	if (strcmp(s2w, "1") == 0) {
		printk(KERN_INFO "[cmdline_s2w]: Sweep2Wake enabled. | s2w='%s'", s2w);
		s2w_switch = 1;
	} else if (strcmp(s2w, "0") == 0) {
		printk(KERN_INFO "[cmdline_s2w]: Sweep2Wake disabled. | s2w='%s'", s2w);
		s2w_switch = 0;
	} else {
		printk(KERN_INFO "[cmdline_s2w]: No valid input found. Going with default: | s2w='%u'", s2w_switch);
	}
	return 1;
}
__setup("s2w=", read_s2w_cmdline);

/* PowerKey setter */
void sweep2wake_setdev(struct input_dev * input_device) {
	sweep2wake_pwrdev = input_device;
	return;
}
EXPORT_SYMBOL(sweep2wake_setdev);

/* PowerKey work func */
static void sweep2wake_presspwr(struct work_struct * sweep2wake_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
                return;
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
        mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(sweep2wake_presspwr_work, sweep2wake_presspwr);

/* PowerKey trigger */
void sweep2wake_pwrtrigger(void) {
	schedule_work(&sweep2wake_presspwr_work);
        return;
}

void s2w_reset(void)
{
	barrier[0] = false;
	barrier[1] = false;
	exec_count = true;
}

/* Sweep2wake main function */
void detect_sweep2wake(int x, int y, bool st)
{
        int prevx = 0, nextx = 0;
        bool single_touch = st;
	
	if(!single_touch){
		s2w_reset();
		return;
	}
#if DEBUG
        pr_info("[sweep2wake]: x,y(%4d,%4d) single:%s\n",
                x, y, (single_touch) ? "true" : "false");
#endif
	//left->right
	if ((single_touch) && (scr_suspended == true) && (s2w_switch > 0)) {
		prevx = 0;
		nextx = S2W_X_B1;
		if ((barrier[0] == true) ||
		   ((x > prevx) &&
		    (x < nextx) &&
		    (y > 0))) {
			prevx = nextx;
			nextx = S2W_X_B2;
			barrier[0] = true;
			if ((barrier[1] == true) ||
			   ((x > prevx) &&
			    (x < nextx) &&
			    (y > 0))) {
				prevx = nextx;
				barrier[1] = true;
				if ((x > prevx) &&
				    (y > 0)) {
					if (x > (S2W_X_MAX - S2W_X_FINAL)) {
						if (exec_count) {
							printk(KERN_INFO "[sweep2wake]: ON");
							sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
	//right->left
	} else if ((single_touch) && (scr_suspended == false) && (s2w_switch > 0)) {
		scr_on_touch=true;
		prevx = (S2W_X_MAX - S2W_X_FINAL);
		nextx = S2W_X_B2;
		if ((barrier[0] == true) ||
		   ((x < prevx) &&
		    (x > nextx) &&
		    (y > S2W_Y_LIMIT))) {
			prevx = nextx;
			nextx = S2W_X_B1;
			barrier[0] = true;
			if ((barrier[1] == true) ||
			   ((x < prevx) &&
			    (x > nextx) &&
			    (y > S2W_Y_LIMIT))) {
				prevx = nextx;
				barrier[1] = true;
				if ((x < prevx) &&
				    (y > S2W_Y_LIMIT)) {
					if (x < S2W_X_FINAL) {
						if (exec_count) {
							printk(KERN_INFO "[sweep2wake]: OFF");
							sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
	}
}

/*
 * INIT / EXIT stuff below here
 */

void s2w_set_scr_suspended(bool suspended)
{
	scr_suspended = suspended;
	s2w_reset();
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
		if(DEBUG)	
			printk("s2w: cant enable/disable while screen is off! Waking...\n");

		sweep2wake_pwrtrigger();

		while(scr_suspended && tries <= max_tries){
			msleep(200);
			tries = tries + 1;
		}
	}
	if(strcmp(val, "1") >= 0 || strcmp(val, "true") >= 0){
		s2w_switch = 1;
		if(DEBUG)
			printk("s2w: enabled\n");

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_WAKELOCK
		if(use_wakelock && !wake_lock_active(&s2w_wake_lock)){
			wake_lock(&s2w_wake_lock);
			if(DEBUG)
				printk("s2w: wake lock enabled\n");
		}
#endif
	}
	else if(strcmp(val, "0") >= 0 || strcmp(val, "false") >= 0){
		s2w_switch = 0;
		if(DEBUG)
			printk("s2w: disabled\n");
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_WAKELOCK
		if(wake_lock_active(&s2w_wake_lock)){
			wake_unlock(&s2w_wake_lock);
		}
#endif

	}else {
		printk("s2w: invalid input '%s' for 'enable'; use 1 or 0\n", val);
	}

	return 0;
}

module_param_call(enable, set_enable, param_get_int, &s2w_switch, 0664);

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_WAKELOCK

static int set_use_wakelock(const char *val, struct kernel_param *kp){

	if(strcmp(val, "1") >= 0 || strcmp(val, "true") >= 0){
		use_wakelock = true;
		if(use_wakelock && !wake_lock_active(&s2w_wake_lock)){
			wake_lock(&s2w_wake_lock);
			if(DEBUG)
				printk("s2w: wake lock enabled\n");
		}
	}
	else if(strcmp(val, "0") >= 0 || strcmp(val, "false") >= 0){
		use_wakelock = false;
		if(wake_lock_active(&s2w_wake_lock)){
			wake_unlock(&s2w_wake_lock);
		}

	}else {
		printk("s2w: invalid input '%s' for 'use_wakelock'; use 1 or 0\n", val);
	}
	return 0;

}

module_param_call(use_wakelock, set_use_wakelock, param_get_bool, &use_wakelock, 0664);
#endif

static int __init sweep2wake_init(void)
{
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_WAKELOCK
	wake_lock_init(&s2w_wake_lock, WAKE_LOCK_SUSPEND, "s2w_kernel_wake_lock");
#endif
	pr_info("[sweep2wake]: %s done\n", __func__);
	return 0;
}

static void __exit sweep2wake_exit(void)
{
	return;
}

module_init(sweep2wake_init);
module_exit(sweep2wake_exit);

MODULE_DESCRIPTION("Sweep2wake");
MODULE_LICENSE("GPLv2");


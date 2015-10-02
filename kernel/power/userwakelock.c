/* kernel/power/userwakelock.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
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

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#ifdef CONFIG_USER_WAKELOCK_DONT_LOCK
#include <linux/string.h>
#endif

#include "power.h"

enum {
	DEBUG_FAILURE	= BIT(0),
	DEBUG_ERROR	= BIT(1),
	DEBUG_NEW	= BIT(2),
	DEBUG_ACCESS	= BIT(3),
	DEBUG_LOOKUP	= BIT(4),
};
static int debug_mask = DEBUG_FAILURE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_MUTEX(tree_lock);

struct user_wake_lock {
	struct rb_node		node;
	struct wake_lock	wake_lock;
	char			name[0];
};
struct rb_root user_wake_locks;

#ifdef CONFIG_USER_WAKELOCK_DONT_LOCK
#define MAX_USERLIST_WAKELOCKS		100

/*
 * Data struct for the wakelocks in user defined list
 */
struct wakelock_userlist_t {
	uint enabled;
	char *names[MAX_USERLIST_WAKELOCKS];
	int names_count;
};

static struct wakelock_userlist_t wakelock_blacklist;		/* User defined wakelocks blacklist */
static struct wakelock_userlist_t wakelock_whitelist;		/* User defined wakelocks whitelist */

/*
 * Checks if a wakelock name is inside a user defined list
 */
static bool is_in_wakelock_userlist(char *wakelock_name, struct wakelock_userlist_t *wakelock_userlist)
{
	int i = 0;

	/* If  the wakelock list is not empty, then check if 
	 * the passed wakelock name is contained inside it */
	if (wakelock_userlist->names_count > 0) {
		for (i = 0; i < wakelock_userlist->names_count; i++) {
			if (strstr(wakelock_userlist->names[i], wakelock_name) != NULL)
				return true; /* The wakelock name is inside the list */
		}
	}

	return false; /* The wakelock name is not contained inside the list */
}

/*
 * Checks if a wakelock name is inside a blacklist
 */
static bool is_in_wakelock_blacklist(char *wakelock_name)
{
	return is_in_wakelock_userlist(wakelock_name, &wakelock_blacklist);
}

/*
 * Checks if a wakelock name is inside a whitelist
 */
static bool is_in_wakelock_whitelist(char *wakelock_name)
{
	return is_in_wakelock_userlist(wakelock_name, &wakelock_whitelist);
}

/*
 * Check if a do not lock feature is enabled
 */

static bool is_wakelock_list_enabled(struct wakelock_userlist_t *wakelock_userlist)
{
	return wakelock_userlist->enabled;
}

static bool is_wakelock_blacklist_enabled(void)
{
	return is_wakelock_list_enabled(&wakelock_blacklist);
}

static bool is_wakelock_whitelist_enabled(void)
{
	return is_wakelock_list_enabled(&wakelock_whitelist);
}
#endif

static struct user_wake_lock *lookup_wake_lock_name(
	const char *buf, int allocate, long *timeoutptr)
{
	struct rb_node **p = &user_wake_locks.rb_node;
	struct rb_node *parent = NULL;
	struct user_wake_lock *l;
	int diff;
	u64 timeout;
	int name_len;
	const char *arg;

	/* Find length of lock name and start of optional timeout string */
	arg = buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		goto bad_arg;
	while (isspace(*arg))
		arg++;

	/* Process timeout string */
	if (timeoutptr && *arg) {
		timeout = simple_strtoull(arg, (char **)&arg, 0);
		while (isspace(*arg))
			arg++;
		if (*arg)
			goto bad_arg;
		/* convert timeout from nanoseconds to jiffies > 0 */
		timeout += (NSEC_PER_SEC / HZ) - 1;
		do_div(timeout, (NSEC_PER_SEC / HZ));
		if (timeout <= 0)
			timeout = 1;
		*timeoutptr = timeout;
	} else if (*arg)
		goto bad_arg;
	else if (timeoutptr)
		*timeoutptr = 0;

	/* Lookup wake lock in rbtree */
	while (*p) {
		parent = *p;
		l = rb_entry(parent, struct user_wake_lock, node);
		diff = strncmp(buf, l->name, name_len);
		if (!diff && l->name[name_len])
			diff = -1;
		if (debug_mask & DEBUG_ERROR)
			pr_info("lookup_wake_lock_name: compare %.*s %s %d\n",
				name_len, buf, l->name, diff);

		if (diff < 0)
			p = &(*p)->rb_left;
		else if (diff > 0)
			p = &(*p)->rb_right;
		else
			return l;
	}

	/* Allocate and add new wakelock to rbtree */
	if (!allocate) {
		if (debug_mask & DEBUG_ERROR)
			pr_info("lookup_wake_lock_name: %.*s not found\n",
				name_len, buf);
		return ERR_PTR(-EINVAL);
	}
	l = kzalloc(sizeof(*l) + name_len + 1, GFP_KERNEL);
	if (l == NULL) {
		if (debug_mask & DEBUG_FAILURE)
			pr_err("lookup_wake_lock_name: failed to allocate "
				"memory for %.*s\n", name_len, buf);
		return ERR_PTR(-ENOMEM);
	}
	memcpy(l->name, buf, name_len);
	if (debug_mask & DEBUG_NEW)
		pr_info("lookup_wake_lock_name: new wake lock %s\n", l->name);
	wake_lock_init(&l->wake_lock, WAKE_LOCK_SUSPEND, l->name);
	rb_link_node(&l->node, parent, p);
	rb_insert_color(&l->node, &user_wake_locks);
	return l;

bad_arg:
	if (debug_mask & DEBUG_ERROR)
		pr_info("lookup_wake_lock_name: wake lock, %.*s, bad arg, %s\n",
			name_len, buf, arg);
	return ERR_PTR(-EINVAL);
}

bool is_any_user_wakelock_active(void)
{
	struct rb_node *n;
	struct user_wake_lock *l;
	bool res = false;

	mutex_lock(&tree_lock);

        for (n = rb_first(&user_wake_locks); n != NULL; n = rb_next(n)) {
                l = rb_entry(n, struct user_wake_lock, node);
                if (wake_lock_active(&l->wake_lock)) {
			res = true;
			break;
		}
        }

        mutex_unlock(&tree_lock);

	return res;
}

ssize_t wake_lock_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;
	struct rb_node *n;
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);

	for (n = rb_first(&user_wake_locks); n != NULL; n = rb_next(n)) {
		l = rb_entry(n, struct user_wake_lock, node);
		if (wake_lock_active(&l->wake_lock))
			s += scnprintf(s, end - s, "%s ", l->name);
	}
	s += scnprintf(s, end - s, "\n");

	mutex_unlock(&tree_lock);
	return (s - buf);
}

ssize_t wake_lock_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	long timeout;
	struct user_wake_lock *l;
#ifdef CONFIG_USER_WAKELOCK_DONT_LOCK
	bool is_whitelist_enabled, is_blacklist_enabled, is_in_whitelist, is_in_blacklist, preferred_blacklist;
#endif

	mutex_lock(&tree_lock);
	l = lookup_wake_lock_name(buf, 1, &timeout);
	if (IS_ERR(l)) {
		n = PTR_ERR(l);
		goto bad_name;
	}

	if (debug_mask & DEBUG_ACCESS)
		pr_info("wake_lock_store: %s, timeout %ld\n", l->name, timeout);

#ifdef CONFIG_USER_WAKELOCK_DONT_LOCK
	is_whitelist_enabled = is_wakelock_whitelist_enabled();
	is_blacklist_enabled = is_wakelock_blacklist_enabled();
	is_in_whitelist = is_in_wakelock_whitelist(l->name);
	is_in_blacklist = is_in_wakelock_blacklist(l->name);
	preferred_blacklist = (strstr(l->name, "PowerManagerService.WakeLocks") != NULL);

	if (is_whitelist_enabled &&  is_in_whitelist ||
	     is_blacklist_enabled && !is_in_blacklist ||
	     !is_whitelist_enabled && !is_blacklist_enabled &&
	     !preferred_blacklist) {
#endif
		if (timeout)
			wake_lock_timeout(&l->wake_lock, timeout);
		else
			wake_lock(&l->wake_lock);
#ifdef CONFIG_USER_WAKELOCK_DONT_LOCK
	} else {
		if (debug_mask & DEBUG_ACCESS)
			pr_err("wake_lock_store: wakelock %s is blocked\n", l->name);
	}
#endif
bad_name:
	mutex_unlock(&tree_lock);
	return n;
}


ssize_t wake_unlock_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;
	struct rb_node *n;
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);

	for (n = rb_first(&user_wake_locks); n != NULL; n = rb_next(n)) {
		l = rb_entry(n, struct user_wake_lock, node);
		if (!wake_lock_active(&l->wake_lock))
			s += scnprintf(s, end - s, "%s ", l->name);
	}
	s += scnprintf(s, end - s, "\n");

	mutex_unlock(&tree_lock);
	return (s - buf);
}

ssize_t wake_unlock_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);
	l = lookup_wake_lock_name(buf, 0, NULL);
	if (IS_ERR(l)) {
		n = PTR_ERR(l);
		goto not_found;
	}

	if (debug_mask & DEBUG_ACCESS)
		pr_info("wake_unlock_store: %s\n", l->name);

	wake_unlock(&l->wake_lock);
not_found:
	mutex_unlock(&tree_lock);
	return n;
}

#ifdef CONFIG_USER_WAKELOCK_DONT_LOCK
module_param_named(wakelock_blacklist, wakelock_blacklist.enabled, uint, S_IRUGO | S_IWUSR);
module_param_array_named(wakelock_blacklist_names, wakelock_blacklist.names, charp,
			 &wakelock_blacklist.names_count, S_IRUGO | S_IWUSR);

module_param_named(wakelock_whitelist, wakelock_whitelist.enabled, uint, S_IRUGO | S_IWUSR);
module_param_array_named(wakelock_whitelist_names, wakelock_whitelist.names, charp,
			 &wakelock_whitelist.names_count, S_IRUGO | S_IWUSR);
#endif


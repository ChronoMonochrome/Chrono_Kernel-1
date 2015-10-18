#ifdef CONFIG_GOD_MODE
#include <linux/god_mode.h>
#endif
/*  linux/fs/ext3/namei.h
 *
 * Copyright (C) 2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
*/

extern struct dentry *ext3_get_parent(struct dentry *child);

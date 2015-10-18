#ifdef CONFIG_GOD_MODE
#include <linux/god_mode.h>
#endif
/*
 * super.h
 */

int befs_load_sb(struct super_block *sb, befs_super_block * disk_sb);

int befs_check_sb(struct super_block *sb);


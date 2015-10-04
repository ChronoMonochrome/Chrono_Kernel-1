/*
 * Provide a default dump_stack() function for architectures
 * which don't implement their own.
 */

#include <linux/kernel.h>
#include <linux/module.h>

void dump_stack(void)
{
//	printk(KERN_NOTICE
;
}

EXPORT_SYMBOL(dump_stack);

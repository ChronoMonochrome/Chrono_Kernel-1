/*
 * Provide a default dump_stack() function for architectures
 * which don't implement their own.
 */

#include <linux/kernel.h>
#include <linux/export.h>

void dump_stack(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_NOTICE
		"This architecture does not implement dump_stack()\n");
#else
	;
#endif
}

EXPORT_SYMBOL(dump_stack);

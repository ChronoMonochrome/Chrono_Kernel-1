/*
 *  linux/arch/arm/kernel/early_printk.c
 *
 *  Copyright (C) 2009 Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>

extern void printch(int);

static void early_write(const char *s, unsigned n)
{
	while (n-- > 0) {
		if (*s == '\n')
			printch('\r');
		printch(*s);
		s++;
	}
}

static void early_console_write(struct console *con, const char *s, unsigned n)
{
	early_write(s, n);
}

static struct console early_console = {
	.name =		"earlycon",
	.write =	early_console_write,
	.flags =	CON_PRINTBUFFER | CON_BOOT,
	.index =	-1,
};

#ifdef CONFIG_DEBUG_PRINTK
asmlinkage void early_printk(const char *fmt, ...)
{
	char buf[512];
#else
asmlinkage void early_;
#endif
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vscnprintf(buf, sizeof(buf), fmt, ap);
	early_write(buf, n);
	va_end(ap);
}

#ifdef CONFIG_DEBUG_PRINTK
static int __init setup_early_printk(char *buf)
{
	register_console(&early_console);
#else
static int __init setup_early_;
#endif
	return 0;
}

early_param("earlyprintk", setup_early_printk);

/*
 *  Copyright (c) 1996-2001 Vojtech Pavlik
 */

/*
 * This is just a very simple driver that can dump the data
 * out of the joystick port into the syslog ...
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Simunkova 1594, Prague 8, 182 00 Czech Republic
 */

#include <linux/module.h>
#include <linux/gameport.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>

#define DRIVER_DESC	"Gameport data dumper module"

MODULE_AUTHOR("Vojtech Pavlik <vojtech@ucw.cz>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define BUF_SIZE 256

struct joydump {
	unsigned int time;
	unsigned char data;
};

static int joydump_connect(struct gameport *gameport, struct gameport_driver *drv)
{
	struct joydump *buf;	/* all entries */
	struct joydump *dump, *prev;	/* one entry each */
	int axes[4], buttons;
	int i, j, t, timeout;
	unsigned long flags;
	unsigned char u;

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "joydump: ,------------------ START ----------------.\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "joydump: | Dumping: %30s |\n", gameport->phys);
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "joydump: | Speed: %28d kHz |\n", gameport->speed);
#else
	;
#endif

	if (gameport_open(gameport, drv, GAMEPORT_MODE_RAW)) {

#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "joydump: | Raw mode not available - trying cooked.    |\n");
#else
		;
#endif

		if (gameport_open(gameport, drv, GAMEPORT_MODE_COOKED)) {

#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "joydump: | Cooked not available either. Failing.   |\n");
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "joydump: `------------------- END -----------------'\n");
#else
			;
#endif
			return -ENODEV;
		}

		gameport_cooked_read(gameport, axes, &buttons);

		for (i = 0; i < 4; i++)
#ifdef CONFIG_DEBUG_PRINTK
			printk(KERN_INFO "joydump: | Axis %d: %4d.                           |\n", i, axes[i]);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "joydump: | Buttons %02x.                             |\n", buttons);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "joydump: `------------------- END -----------------'\n");
#else
		;
#endif
	}

	timeout = gameport_time(gameport, 10000); /* 10 ms */

	buf = kmalloc(BUF_SIZE * sizeof(struct joydump), GFP_KERNEL);
	if (!buf) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "joydump: no memory for testing\n");
#else
		;
#endif
		goto jd_end;
	}
	dump = buf;
	t = 0;
	i = 1;

	local_irq_save(flags);

	u = gameport_read(gameport);

	dump->data = u;
	dump->time = t;
	dump++;

	gameport_trigger(gameport);

	while (i < BUF_SIZE && t < timeout) {

		dump->data = gameport_read(gameport);

		if (dump->data ^ u) {
			u = dump->data;
			dump->time = t;
			i++;
			dump++;
		}
		t++;
	}

	local_irq_restore(flags);

/*
 * Dump data.
 */

	t = i;
	dump = buf;
	prev = dump;

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "joydump: >------------------ DATA -----------------<\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "joydump: | index: %3d delta: %3d us data: ", 0, 0);
#else
	;
#endif
	for (j = 7; j >= 0; j--)
#ifdef CONFIG_DEBUG_PRINTK
		printk("%d", (dump->data >> j) & 1);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(" |\n");
#else
	;
#endif
	dump++;

	for (i = 1; i < t; i++, dump++, prev++) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "joydump: | index: %3d delta: %3d us data: ",
			i, dump->time - prev->time);
#else
		;
#endif
		for (j = 7; j >= 0; j--)
#ifdef CONFIG_DEBUG_PRINTK
			printk("%d", (dump->data >> j) & 1);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(" |\n");
#else
		;
#endif
	}
	kfree(buf);

jd_end:
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "joydump: `------------------- END -----------------'\n");
#else
	;
#endif

	return 0;
}

static void joydump_disconnect(struct gameport *gameport)
{
	gameport_close(gameport);
}

static struct gameport_driver joydump_drv = {
	.driver		= {
		.name	= "joydump",
	},
	.description	= DRIVER_DESC,
	.connect	= joydump_connect,
	.disconnect	= joydump_disconnect,
};

module_gameport_driver(joydump_drv);

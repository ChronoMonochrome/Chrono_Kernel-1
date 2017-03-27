/*
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING. If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Test sub-page read and write on MTD device.
 * Author: Adrian Hunter <ext-adrian.hunter@nokia.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>
#include <linux/sched.h>

#define PRINT_PREF KERN_INFO "mtd_subpagetest: "

static int dev;
module_param(dev, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");

static struct mtd_info *mtd;
static unsigned char *writebuf;
static unsigned char *readbuf;
static unsigned char *bbt;

static int subpgsize;
static int bufsize;
static int ebcnt;
static int pgcnt;
static int errcnt;
static unsigned long next = 1;

static inline unsigned int simple_rand(void)
{
	next = next * 1103515245 + 12345;
	return (unsigned int)((next / 65536) % 32768);
}

static inline void simple_srand(unsigned long seed)
{
	next = seed;
}

static void set_random_data(unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		buf[i] = simple_rand();
}

static inline void clear_data(unsigned char *buf, size_t len)
{
	memset(buf, 0, len);
}

static int erase_eraseblock(int ebnum)
{
	int err;
	struct erase_info ei;
	loff_t addr = ebnum * mtd->erasesize;

	memset(&ei, 0, sizeof(struct erase_info));
	ei.mtd  = mtd;
	ei.addr = addr;
	ei.len  = mtd->erasesize;

	err = mtd->erase(mtd, &ei);
	if (err) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error %d while erasing EB %d\n", err, ebnum);
#else
		;
#endif
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "some erase error occurred at EB %d\n",
		       ebnum);
#else
		;
#endif
		return -EIO;
	}

	return 0;
}

static int erase_whole_device(void)
{
	int err;
	unsigned int i;

#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "erasing whole device\n");
#else
	;
#endif
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = erase_eraseblock(i);
		if (err)
			return err;
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "erased %u eraseblocks\n", i);
#else
	;
#endif
	return 0;
}

static int write_eraseblock(int ebnum)
{
	size_t written = 0;
	int err = 0;
	loff_t addr = ebnum * mtd->erasesize;

	set_random_data(writebuf, subpgsize);
	err = mtd->write(mtd, addr, subpgsize, &written, writebuf);
	if (unlikely(err || written != subpgsize)) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: write failed at %#llx\n",
		       (long long)addr);
#else
		;
#endif
		if (written != subpgsize) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "  write size: %#x\n", subpgsize);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "  written: %#zx\n", written);
#else
			;
#endif
		}
		return err ? err : -1;
	}

	addr += subpgsize;

	set_random_data(writebuf, subpgsize);
	err = mtd->write(mtd, addr, subpgsize, &written, writebuf);
	if (unlikely(err || written != subpgsize)) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: write failed at %#llx\n",
		       (long long)addr);
#else
		;
#endif
		if (written != subpgsize) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "  write size: %#x\n", subpgsize);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "  written: %#zx\n", written);
#else
			;
#endif
		}
		return err ? err : -1;
	}

	return err;
}

static int write_eraseblock2(int ebnum)
{
	size_t written = 0;
	int err = 0, k;
	loff_t addr = ebnum * mtd->erasesize;

	for (k = 1; k < 33; ++k) {
		if (addr + (subpgsize * k) > (ebnum + 1) * mtd->erasesize)
			break;
		set_random_data(writebuf, subpgsize * k);
		err = mtd->write(mtd, addr, subpgsize * k, &written, writebuf);
		if (unlikely(err || written != subpgsize * k)) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "error: write failed at %#llx\n",
			       (long long)addr);
#else
			;
#endif
			if (written != subpgsize) {
#ifdef CONFIG_DEBUG_PRINTK
				printk(PRINT_PREF "  write size: %#x\n",
				       subpgsize * k);
#else
				;
#endif
#ifdef CONFIG_DEBUG_PRINTK
				printk(PRINT_PREF "  written: %#08zx\n",
				       written);
#else
				;
#endif
			}
			return err ? err : -1;
		}
		addr += subpgsize * k;
	}

	return err;
}

static void print_subpage(unsigned char *p)
{
	int i, j;

	for (i = 0; i < subpgsize; ) {
		for (j = 0; i < subpgsize && j < 32; ++i, ++j)
#ifdef CONFIG_DEBUG_PRINTK
			printk("%02x", *p++);
#else
			;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk("\n");
#else
		;
#endif
	}
}

static int verify_eraseblock(int ebnum)
{
	size_t read = 0;
	int err = 0;
	loff_t addr = ebnum * mtd->erasesize;

	set_random_data(writebuf, subpgsize);
	clear_data(readbuf, subpgsize);
	read = 0;
	err = mtd->read(mtd, addr, subpgsize, &read, readbuf);
	if (unlikely(err || read != subpgsize)) {
		if (err == -EUCLEAN && read == subpgsize) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "ECC correction at %#llx\n",
			       (long long)addr);
#else
			;
#endif
			err = 0;
		} else {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "error: read failed at %#llx\n",
			       (long long)addr);
#else
			;
#endif
			return err ? err : -1;
		}
	}
	if (unlikely(memcmp(readbuf, writebuf, subpgsize))) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: verify failed at %#llx\n",
		       (long long)addr);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "------------- written----------------\n");
#else
		;
#endif
		print_subpage(writebuf);
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "------------- read ------------------\n");
#else
		;
#endif
		print_subpage(readbuf);
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "-------------------------------------\n");
#else
		;
#endif
		errcnt += 1;
	}

	addr += subpgsize;

	set_random_data(writebuf, subpgsize);
	clear_data(readbuf, subpgsize);
	read = 0;
	err = mtd->read(mtd, addr, subpgsize, &read, readbuf);
	if (unlikely(err || read != subpgsize)) {
		if (err == -EUCLEAN && read == subpgsize) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "ECC correction at %#llx\n",
			       (long long)addr);
#else
			;
#endif
			err = 0;
		} else {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "error: read failed at %#llx\n",
			       (long long)addr);
#else
			;
#endif
			return err ? err : -1;
		}
	}
	if (unlikely(memcmp(readbuf, writebuf, subpgsize))) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: verify failed at %#llx\n",
		       (long long)addr);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "------------- written----------------\n");
#else
		;
#endif
		print_subpage(writebuf);
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "------------- read ------------------\n");
#else
		;
#endif
		print_subpage(readbuf);
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "-------------------------------------\n");
#else
		;
#endif
		errcnt += 1;
	}

	return err;
}

static int verify_eraseblock2(int ebnum)
{
	size_t read = 0;
	int err = 0, k;
	loff_t addr = ebnum * mtd->erasesize;

	for (k = 1; k < 33; ++k) {
		if (addr + (subpgsize * k) > (ebnum + 1) * mtd->erasesize)
			break;
		set_random_data(writebuf, subpgsize * k);
		clear_data(readbuf, subpgsize * k);
		read = 0;
		err = mtd->read(mtd, addr, subpgsize * k, &read, readbuf);
		if (unlikely(err || read != subpgsize * k)) {
			if (err == -EUCLEAN && read == subpgsize * k) {
#ifdef CONFIG_DEBUG_PRINTK
				printk(PRINT_PREF "ECC correction at %#llx\n",
				       (long long)addr);
#else
				;
#endif
				err = 0;
			} else {
#ifdef CONFIG_DEBUG_PRINTK
				printk(PRINT_PREF "error: read failed at "
				       "%#llx\n", (long long)addr);
#else
				;
#endif
				return err ? err : -1;
			}
		}
		if (unlikely(memcmp(readbuf, writebuf, subpgsize * k))) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "error: verify failed at %#llx\n",
			       (long long)addr);
#else
			;
#endif
			errcnt += 1;
		}
		addr += subpgsize * k;
	}

	return err;
}

static int verify_eraseblock_ff(int ebnum)
{
	uint32_t j;
	size_t read = 0;
	int err = 0;
	loff_t addr = ebnum * mtd->erasesize;

	memset(writebuf, 0xff, subpgsize);
	for (j = 0; j < mtd->erasesize / subpgsize; ++j) {
		clear_data(readbuf, subpgsize);
		read = 0;
		err = mtd->read(mtd, addr, subpgsize, &read, readbuf);
		if (unlikely(err || read != subpgsize)) {
			if (err == -EUCLEAN && read == subpgsize) {
#ifdef CONFIG_DEBUG_PRINTK
				printk(PRINT_PREF "ECC correction at %#llx\n",
				       (long long)addr);
#else
				;
#endif
				err = 0;
			} else {
#ifdef CONFIG_DEBUG_PRINTK
				printk(PRINT_PREF "error: read failed at "
				       "%#llx\n", (long long)addr);
#else
				;
#endif
				return err ? err : -1;
			}
		}
		if (unlikely(memcmp(readbuf, writebuf, subpgsize))) {
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "error: verify 0xff failed at "
			       "%#llx\n", (long long)addr);
#else
			;
#endif
			errcnt += 1;
		}
		addr += subpgsize;
	}

	return err;
}

static int verify_all_eraseblocks_ff(void)
{
	int err;
	unsigned int i;

#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "verifying all eraseblocks for 0xff\n");
#else
	;
#endif
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = verify_eraseblock_ff(i);
		if (err)
			return err;
		if (i % 256 == 0)
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "verified up to eraseblock %u\n", i);
#else
			;
#endif
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "verified %u eraseblocks\n", i);
#else
	;
#endif
	return 0;
}

static int is_block_bad(int ebnum)
{
	loff_t addr = ebnum * mtd->erasesize;
	int ret;

	ret = mtd->block_isbad(mtd, addr);
	if (ret)
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "block %d is bad\n", ebnum);
#else
		;
#endif
	return ret;
}

static int scan_for_bad_eraseblocks(void)
{
	int i, bad = 0;

	bbt = kzalloc(ebcnt, GFP_KERNEL);
	if (!bbt) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: cannot allocate memory\n");
#else
		;
#endif
		return -ENOMEM;
	}

#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "scanning for bad eraseblocks\n");
#else
	;
#endif
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "scanned %d eraseblocks, %d are bad\n", i, bad);
#else
	;
#endif
	return 0;
}

static int __init mtd_subpagetest_init(void)
{
	int err = 0;
	uint32_t i;
	uint64_t tmp;

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "=================================================\n");
#else
	;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "MTD device: %d\n", dev);
#else
	;
#endif

	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: cannot get MTD device\n");
#else
		;
#endif
		return err;
	}

	if (mtd->type != MTD_NANDFLASH) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "this test requires NAND flash\n");
#else
		;
#endif
		goto out;
	}

	subpgsize = mtd->writesize >> mtd->subpage_sft;
	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;

#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "MTD device size %llu, eraseblock size %u, "
	       "page size %u, subpage size %u, count of eraseblocks %u, "
	       "pages per eraseblock %u, OOB size %u\n",
	       (unsigned long long)mtd->size, mtd->erasesize,
	       mtd->writesize, subpgsize, ebcnt, pgcnt, mtd->oobsize);
#else
	;
#endif

	err = -ENOMEM;
	bufsize = subpgsize * 32;
	writebuf = kmalloc(bufsize, GFP_KERNEL);
	if (!writebuf) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: cannot allocate memory\n");
#else
		;
#endif
		goto out;
	}
	readbuf = kmalloc(bufsize, GFP_KERNEL);
	if (!readbuf) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error: cannot allocate memory\n");
#else
		;
#endif
		goto out;
	}

	err = scan_for_bad_eraseblocks();
	if (err)
		goto out;

	err = erase_whole_device();
	if (err)
		goto out;

#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "writing whole device\n");
#else
	;
#endif
	simple_srand(1);
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = write_eraseblock(i);
		if (unlikely(err))
			goto out;
		if (i % 256 == 0)
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "written up to eraseblock %u\n", i);
#else
			;
#endif
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "written %u eraseblocks\n", i);
#else
	;
#endif

	simple_srand(1);
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "verifying all eraseblocks\n");
#else
	;
#endif
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = verify_eraseblock(i);
		if (unlikely(err))
			goto out;
		if (i % 256 == 0)
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "verified up to eraseblock %u\n", i);
#else
			;
#endif
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "verified %u eraseblocks\n", i);
#else
	;
#endif

	err = erase_whole_device();
	if (err)
		goto out;

	err = verify_all_eraseblocks_ff();
	if (err)
		goto out;

	/* Write all eraseblocks */
	simple_srand(3);
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "writing whole device\n");
#else
	;
#endif
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = write_eraseblock2(i);
		if (unlikely(err))
			goto out;
		if (i % 256 == 0)
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "written up to eraseblock %u\n", i);
#else
			;
#endif
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "written %u eraseblocks\n", i);
#else
	;
#endif

	/* Check all eraseblocks */
	simple_srand(3);
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "verifying all eraseblocks\n");
#else
	;
#endif
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = verify_eraseblock2(i);
		if (unlikely(err))
			goto out;
		if (i % 256 == 0)
#ifdef CONFIG_DEBUG_PRINTK
			printk(PRINT_PREF "verified up to eraseblock %u\n", i);
#else
			;
#endif
		cond_resched();
	}
#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "verified %u eraseblocks\n", i);
#else
	;
#endif

	err = erase_whole_device();
	if (err)
		goto out;

	err = verify_all_eraseblocks_ff();
	if (err)
		goto out;

#ifdef CONFIG_DEBUG_PRINTK
	printk(PRINT_PREF "finished with %d errors\n", errcnt);
#else
	;
#endif

out:
	kfree(bbt);
	kfree(readbuf);
	kfree(writebuf);
	put_mtd_device(mtd);
	if (err)
#ifdef CONFIG_DEBUG_PRINTK
		printk(PRINT_PREF "error %d occurred\n", err);
#else
		;
#endif
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "=================================================\n");
#else
	;
#endif
	return err;
}
module_init(mtd_subpagetest_init);

static void __exit mtd_subpagetest_exit(void)
{
	return;
}
module_exit(mtd_subpagetest_exit);

MODULE_DESCRIPTION("Subpage test module");
MODULE_AUTHOR("Adrian Hunter");
MODULE_LICENSE("GPL");

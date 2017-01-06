#ifdef CONFIG_GOD_MODE
#include <linux/god_mode.h>
#endif
/* Updated: Karl MacMillan <kmacmillan@tresys.com>
 *
 *	Added conditional policy language extensions
 *
 *  Updated: Hewlett-Packard <paul@paul-moore.com>
 *
 *	Added support for the policy capability bitmap
 *
 * Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2003 - 2004 Tresys Technology, LLC
 * Copyright (C) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 */

#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/security.h>
#include <linux/major.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>
#include <linux/audit.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/ctype.h>

/* selinuxfs pseudo filesystem for exporting the security policy API.
   Based on the proc code and the fs/nfsd/nfsctl.c code. */

#include "flask.h"
#include "avc.h"
#include "avc_ss.h"
#include "security.h"
#include "objsec.h"
#include "conditional.h"

/* Policy capability filenames */
static char *policycap_names[] = {
	"network_peer_controls",
	"open_perms"
};

unsigned int selinux_checkreqprot = CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE;

static int __init checkreqprot_setup(char *str)
{
	unsigned long checkreqprot;
	if (!strict_strtoul(str, 0, &checkreqprot))
		selinux_checkreqprot = checkreqprot ? 1 : 0;
	return 1;
}
__setup("checkreqprot=", checkreqprot_setup);

static DEFINE_MUTEX(sel_mutex);

/* global data for booleans */
static struct dentry *bool_dir;
static int bool_num;
static char **bool_pending_names;
static int *bool_pending_values;

/* global data for classes */
static struct dentry *class_dir;
static unsigned long last_class_ino;

static char policy_opened;

/* global data for policy capabilities */
static struct dentry *policycap_dir;

/* Check whether a task is allowed to use a security operation. */
static int task_has_security(struct task_struct *tsk,
			     u32 perms)
{
	const struct task_security_struct *tsec;
	u32 sid = 0;

	rcu_read_lock();
	tsec = __task_cred(tsk)->security;
	if (tsec)
		sid = tsec->sid;
	rcu_read_unlock();
	if (!tsec)
		return -EACCES;

	return avc_has_perm(sid, SECINITSID_SECURITY,
			    SECCLASS_SECURITY, perms, NULL);
}

enum sel_inos {
	SEL_ROOT_INO = 2,
	SEL_LOAD,	/* load policy */
	SEL_ENFORCE,	/* get or set enforcing status */
	SEL_CONTEXT,	/* validate context */
	SEL_ACCESS,	/* compute access decision */
	SEL_CREATE,	/* compute create labeling decision */
	SEL_RELABEL,	/* compute relabeling decision */
	SEL_USER,	/* compute reachable user contexts */
	SEL_POLICYVERS,	/* return policy version for this kernel */
	SEL_COMMIT_BOOLS, /* commit new boolean values */
	SEL_MLS,	/* return if MLS policy is enabled */
	SEL_DISABLE,	/* disable SELinux until next reboot */
	SEL_MEMBER,	/* compute polyinstantiation membership decision */
	SEL_CHECKREQPROT, /* check requested protection, not kernel-applied one */
	SEL_COMPAT_NET,	/* whether to use old compat network packet controls */
	SEL_REJECT_UNKNOWN, /* export unknown reject handling to userspace */
	SEL_DENY_UNKNOWN, /* export unknown deny handling to userspace */
	SEL_STATUS,	/* export current status using mmap() */
	SEL_POLICY,	/* allow userspace to read the in kernel policy */
	SEL_INO_NEXT,	/* The next inode number to use */
};

static unsigned long sel_last_ino = SEL_INO_NEXT - 1;

#define SEL_INITCON_INO_OFFSET		0x01000000
#define SEL_BOOL_INO_OFFSET		0x02000000
#define SEL_CLASS_INO_OFFSET		0x04000000
#define SEL_POLICYCAP_INO_OFFSET	0x08000000
#define SEL_INO_MASK			0x00ffffff

#define TMPBUFLEN	12
static ssize_t sel_read_enforce(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	return 0;
}

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
static ssize_t sel_write_enforce(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)

{
	return 0;
}
#else
#define sel_write_enforce NULL
#endif

static const struct file_operations sel_enforce_ops = {
	.read		= sel_read_enforce,
	.write		= sel_write_enforce,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_handle_unknown(struct file *filp, char __user *buf,
					size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_handle_unknown_ops = {
	.read		= sel_read_handle_unknown,
	.llseek		= generic_file_llseek,
};

static int sel_open_handle_status(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t sel_read_handle_status(struct file *filp, char __user *buf,
				      size_t count, loff_t *ppos)
{
	return 0;
}

static int sel_mmap_handle_status(struct file *filp,
				  struct vm_area_struct *vma)
{
	return 0;
}

static const struct file_operations sel_handle_status_ops = {
	.open		= sel_open_handle_status,
	.read		= sel_read_handle_status,
	.mmap		= sel_mmap_handle_status,
	.llseek		= generic_file_llseek,
};

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
static ssize_t sel_write_disable(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)

{
	char *page = NULL;
	ssize_t length;
	int new_value;

	length = -ENOMEM;
	if (count >= PAGE_SIZE)
		goto out;

	/* No partial writes. */
	length = -EINVAL;
	if (*ppos != 0)
		goto out;

	length = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		goto out;

	length = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	length = -EINVAL;
	if (sscanf(page, "%d", &new_value) != 1)
		goto out;

	if (new_value) {
		length = selinux_disable();
		if (length)
			goto out;
		audit_log(current->audit_context, GFP_KERNEL, AUDIT_MAC_STATUS,
			"selinux=0 auid=%u ses=%u",
			audit_get_loginuid(current),
			audit_get_sessionid(current));
	}

	length = count;
out:
	free_page((unsigned long) page);
	return length;
}
#else
#define sel_write_disable NULL
#endif

static const struct file_operations sel_disable_ops = {
	.write		= sel_write_disable,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_policyvers(struct file *filp, char __user *buf,
				   size_t count, loff_t *ppos)
{
	char tmpbuf[TMPBUFLEN];
	ssize_t length;

	length = scnprintf(tmpbuf, TMPBUFLEN, "%u", POLICYDB_VERSION_MAX);
	return simple_read_from_buffer(buf, count, ppos, tmpbuf, length);
}

static const struct file_operations sel_policyvers_ops = {
	.read		= sel_read_policyvers,
	.llseek		= generic_file_llseek,
};

/* declaration for sel_write_load */
static int sel_make_bools(void);
static int sel_make_classes(void);
static int sel_make_policycap(void);

/* declaration for sel_make_class_dirs */
static struct dentry *sel_make_dir(struct dentry *dir, const char *name,
			unsigned long *ino);

static ssize_t sel_read_mls(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_mls_ops = {
	.read		= sel_read_mls,
	.llseek		= generic_file_llseek,
};

struct policy_load_memory {
	size_t len;
	void *data;
};

static int sel_open_policy(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sel_release_policy(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t sel_read_policy(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	return 0;
}

static int sel_mmap_policy_fault(struct vm_area_struct *vma,
				 struct vm_fault *vmf)
{
	return 0;
}

static struct vm_operations_struct sel_mmap_policy_ops = {
	.fault = sel_mmap_policy_fault,
	.page_mkwrite = sel_mmap_policy_fault,
};

static int sel_mmap_policy(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

static const struct file_operations sel_policy_ops = {
	.open		= sel_open_policy,
	.read		= sel_read_policy,
	.mmap		= sel_mmap_policy,
	.release	= sel_release_policy,
};

static ssize_t sel_write_load(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)

{
	return 0;
}

static const struct file_operations sel_load_ops = {
	.write		= sel_write_load,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_write_context(struct file *file, char *buf, size_t size)
{
	return 0;
}

static ssize_t sel_read_checkreqprot(struct file *filp, char __user *buf,
				     size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t sel_write_checkreqprot(struct file *file, const char __user *buf,
				      size_t count, loff_t *ppos)
{
	return 0;
}
static const struct file_operations sel_checkreqprot_ops = {
	.read		= sel_read_checkreqprot,
	.write		= sel_write_checkreqprot,
	.llseek		= generic_file_llseek,
};

/*
 * Remaining nodes use transaction based IO methods like nfsd/nfsctl.c
 */
static ssize_t sel_write_access(struct file *file, char *buf, size_t size);
static ssize_t sel_write_create(struct file *file, char *buf, size_t size);
static ssize_t sel_write_relabel(struct file *file, char *buf, size_t size);
static ssize_t sel_write_user(struct file *file, char *buf, size_t size);
static ssize_t sel_write_member(struct file *file, char *buf, size_t size);

static ssize_t (*write_op[])(struct file *, char *, size_t) = {
	[SEL_ACCESS] = sel_write_access,
	[SEL_CREATE] = sel_write_create,
	[SEL_RELABEL] = sel_write_relabel,
	[SEL_USER] = sel_write_user,
	[SEL_MEMBER] = sel_write_member,
	[SEL_CONTEXT] = sel_write_context,
};

static ssize_t selinux_transaction_write(struct file *file, const char __user *buf, size_t size, loff_t *pos)
{
	return 0;
}

static const struct file_operations transaction_ops = {
	.write		= selinux_transaction_write,
	.read		= simple_transaction_read,
	.release	= simple_transaction_release,
	.llseek		= generic_file_llseek,
};

/*
 * payload - write methods
 * If the method has a response, the response should be put in buf,
 * and the length returned.  Otherwise return 0 or and -error.
 */

static ssize_t sel_write_access(struct file *file, char *buf, size_t size)
{
	return 0;
}

static ssize_t sel_write_create(struct file *file, char *buf, size_t size)
{
	return 0;
}

static ssize_t sel_write_relabel(struct file *file, char *buf, size_t size)
{
	return 0;
}

static ssize_t sel_write_user(struct file *file, char *buf, size_t size)
{
	return 0;
}

static ssize_t sel_write_member(struct file *file, char *buf, size_t size)
{
	return 0;
}

static struct inode *sel_make_inode(struct super_block *sb, int mode)
{
	return 0;
}

static ssize_t sel_read_bool(struct file *filep, char __user *buf,
			     size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t sel_write_bool(struct file *filep, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_bool_ops = {
	.read		= sel_read_bool,
	.write		= sel_write_bool,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_commit_bools_write(struct file *filep,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_commit_bools_ops = {
	.write		= sel_commit_bools_write,
	.llseek		= generic_file_llseek,
};

static void sel_remove_entries(struct dentry *de)
{
}

#define BOOL_DIR_NAME "booleans"

static int sel_make_bools(void)
{
	return 0;
}

#define NULL_FILE_NAME "null"

struct dentry *selinux_null;

static ssize_t sel_read_avc_cache_threshold(struct file *filp, char __user *buf,
					    size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t sel_write_avc_cache_threshold(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)

{
	return 0;
}

static ssize_t sel_read_avc_hash_stats(struct file *filp, char __user *buf,
				       size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_avc_cache_threshold_ops = {
	.read		= sel_read_avc_cache_threshold,
	.write		= sel_write_avc_cache_threshold,
	.llseek		= generic_file_llseek,
};

static const struct file_operations sel_avc_hash_stats_ops = {
	.read		= sel_read_avc_hash_stats,
	.llseek		= generic_file_llseek,
};

#ifdef CONFIG_SECURITY_SELINUX_AVC_STATS
static struct avc_cache_stats *sel_avc_get_stat_idx(loff_t *idx)
{
	int cpu;

	for (cpu = *idx; cpu < nr_cpu_ids; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*idx = cpu + 1;
		return &per_cpu(avc_cache_stats, cpu);
	}
	return NULL;
}

static void *sel_avc_stats_seq_start(struct seq_file *seq, loff_t *pos)
{
	loff_t n = *pos - 1;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	return sel_avc_get_stat_idx(&n);
}

static void *sel_avc_stats_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return sel_avc_get_stat_idx(pos);
}

static int sel_avc_stats_seq_show(struct seq_file *seq, void *v)
{
	struct avc_cache_stats *st = v;

	if (v == SEQ_START_TOKEN)
		seq_printf(seq, "lookups hits misses allocations reclaims "
			   "frees\n");
	else {
		unsigned int lookups = st->lookups;
		unsigned int misses = st->misses;
		unsigned int hits = lookups - misses;
		seq_printf(seq, "%u %u %u %u %u %u\n", lookups,
			   hits, misses, st->allocations,
			   st->reclaims, st->frees);
	}
	return 0;
}

static void sel_avc_stats_seq_stop(struct seq_file *seq, void *v)
{ }

static const struct seq_operations sel_avc_cache_stats_seq_ops = {
	.start		= sel_avc_stats_seq_start,
	.next		= sel_avc_stats_seq_next,
	.show		= sel_avc_stats_seq_show,
	.stop		= sel_avc_stats_seq_stop,
};

static int sel_open_avc_cache_stats(struct inode *inode, struct file *file)
{
	return seq_open(file, &sel_avc_cache_stats_seq_ops);
}

static const struct file_operations sel_avc_cache_stats_ops = {
	.open		= sel_open_avc_cache_stats,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif

static int sel_make_avc_files(struct dentry *dir)
{
	int i;
	static struct tree_descr files[] = {
		{ "cache_threshold",
		  &sel_avc_cache_threshold_ops, S_IRUGO|S_IWUSR },
		{ "hash_stats", &sel_avc_hash_stats_ops, S_IRUGO },
#ifdef CONFIG_SECURITY_SELINUX_AVC_STATS
		{ "cache_stats", &sel_avc_cache_stats_ops, S_IRUGO },
#endif
	};

	for (i = 0; i < ARRAY_SIZE(files); i++) {
		struct inode *inode;
		struct dentry *dentry;

		dentry = d_alloc_name(dir, files[i].name);
		if (!dentry)
			return -ENOMEM;

		inode = sel_make_inode(dir->d_sb, S_IFREG|files[i].mode);
		if (!inode)
			return -ENOMEM;

		inode->i_fop = files[i].ops;
		inode->i_ino = ++sel_last_ino;
		d_add(dentry, inode);
	}

	return 0;
}

static ssize_t sel_read_initcon(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_initcon_ops = {
	.read		= sel_read_initcon,
	.llseek		= generic_file_llseek,
};

static int sel_make_initcon_files(struct dentry *dir)
{
	return 0;
}

static inline unsigned int sel_div(unsigned long a, unsigned long b)
{
	return a / b - (a % b < 0);
}

static inline unsigned long sel_class_to_ino(u16 class)
{
	return (class * (SEL_VEC_MAX + 1)) | SEL_CLASS_INO_OFFSET;
}

static inline u16 sel_ino_to_class(unsigned long ino)
{
	return sel_div(ino & SEL_INO_MASK, SEL_VEC_MAX + 1);
}

static inline unsigned long sel_perm_to_ino(u16 class, u32 perm)
{
	return (class * (SEL_VEC_MAX + 1) + perm) | SEL_CLASS_INO_OFFSET;
}

static inline u32 sel_ino_to_perm(unsigned long ino)
{
	return (ino & SEL_INO_MASK) % (SEL_VEC_MAX + 1);
}

static ssize_t sel_read_class(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_class_ops = {
	.read		= sel_read_class,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_perm(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_perm_ops = {
	.read		= sel_read_perm,
	.llseek		= generic_file_llseek,
};

static ssize_t sel_read_policycap(struct file *file, char __user *buf,
				  size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations sel_policycap_ops = {
	.read		= sel_read_policycap,
	.llseek		= generic_file_llseek,
};

static int sel_make_perm_files(char *objclass, int classvalue,
				struct dentry *dir)
{
	return 0;
}

static int sel_make_class_dir_entries(char *classname, int index,
					struct dentry *dir)
{
	return 0;
}

static void sel_remove_classes(void)
{
}

static int sel_make_classes(void)
{
	return 0;
}

static int sel_make_policycap(void)
{
	return 0;
}

static struct dentry *sel_make_dir(struct dentry *dir, const char *name,
			unsigned long *ino)
{
	struct dentry *dentry = d_alloc_name(dir, name);
	struct inode *inode;

	if (!dentry)
		return ERR_PTR(-ENOMEM);

	inode = sel_make_inode(dir->d_sb, S_IFDIR | S_IRUGO | S_IXUGO);
	if (!inode) {
		dput(dentry);
		return ERR_PTR(-ENOMEM);
	}

	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	inode->i_ino = ++(*ino);
	/* directory inodes start off with i_nlink == 2 (for "." entry) */
	inc_nlink(inode);
	d_add(dentry, inode);
	/* bump link count on parent directory, too */
	inc_nlink(dir->d_inode);

	return dentry;
}

static int sel_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret;
	struct dentry *dentry;
	struct inode *inode;
	struct inode_security_struct *isec;

	static struct tree_descr selinux_files[] = {
		[SEL_LOAD] = {"load", &sel_load_ops, S_IRUSR|S_IWUSR},
		[SEL_ENFORCE] = {"enforce", &sel_enforce_ops, S_IRUGO|S_IWUSR},
		[SEL_CONTEXT] = {"context", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_ACCESS] = {"access", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_CREATE] = {"create", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_RELABEL] = {"relabel", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_USER] = {"user", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_POLICYVERS] = {"policyvers", &sel_policyvers_ops, S_IRUGO},
		[SEL_COMMIT_BOOLS] = {"commit_pending_bools", &sel_commit_bools_ops, S_IWUSR},
		[SEL_MLS] = {"mls", &sel_mls_ops, S_IRUGO},
		[SEL_DISABLE] = {"disable", &sel_disable_ops, S_IWUSR},
		[SEL_MEMBER] = {"member", &transaction_ops, S_IRUGO|S_IWUGO},
		[SEL_CHECKREQPROT] = {"checkreqprot", &sel_checkreqprot_ops, S_IRUGO|S_IWUSR},
		[SEL_REJECT_UNKNOWN] = {"reject_unknown", &sel_handle_unknown_ops, S_IRUGO},
		[SEL_DENY_UNKNOWN] = {"deny_unknown", &sel_handle_unknown_ops, S_IRUGO},
		[SEL_STATUS] = {"status", &sel_handle_status_ops, S_IRUGO},
		[SEL_POLICY] = {"policy", &sel_policy_ops, S_IRUSR},
		/* last one */ {""}
	};
	ret = simple_fill_super(sb, SELINUX_MAGIC, selinux_files);
	if (ret)
		goto err;

	bool_dir = sel_make_dir(sb->s_root, BOOL_DIR_NAME, &sel_last_ino);
	if (IS_ERR(bool_dir)) {
		ret = PTR_ERR(bool_dir);
		bool_dir = NULL;
		goto err;
	}

	ret = -ENOMEM;
	dentry = d_alloc_name(sb->s_root, NULL_FILE_NAME);
	if (!dentry)
		goto err;

	ret = -ENOMEM;
	inode = sel_make_inode(sb, S_IFCHR | S_IRUGO | S_IWUGO);
	if (!inode)
		goto err;

	inode->i_ino = ++sel_last_ino;
	isec = (struct inode_security_struct *)inode->i_security;
	isec->sid = SECINITSID_DEVNULL;
	isec->sclass = SECCLASS_CHR_FILE;
	isec->initialized = 1;

	init_special_inode(inode, S_IFCHR | S_IRUGO | S_IWUGO, MKDEV(MEM_MAJOR, 3));
	d_add(dentry, inode);
	selinux_null = dentry;

	dentry = sel_make_dir(sb->s_root, "avc", &sel_last_ino);
	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);
		goto err;
	}

	ret = sel_make_avc_files(dentry);
	if (ret)
		goto err;

	dentry = sel_make_dir(sb->s_root, "initial_contexts", &sel_last_ino);
	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);
		goto err;
	}

	ret = sel_make_initcon_files(dentry);
	if (ret)
		goto err;

	class_dir = sel_make_dir(sb->s_root, "class", &sel_last_ino);
	if (IS_ERR(class_dir)) {
		ret = PTR_ERR(class_dir);
		class_dir = NULL;
		goto err;
	}

	policycap_dir = sel_make_dir(sb->s_root, "policy_capabilities", &sel_last_ino);
	if (IS_ERR(policycap_dir)) {
		ret = PTR_ERR(policycap_dir);
		policycap_dir = NULL;
		goto err;
	}
	return 0;
err:
	printk(KERN_ERR "SELinux: %s:  failed while creating inodes\n",
		__func__);
	return ret;
}

static struct dentry *sel_mount(struct file_system_type *fs_type,
		      int flags, const char *dev_name, void *data)
{
	return mount_single(fs_type, flags, data, sel_fill_super);
}

static struct file_system_type sel_fs_type = {
	.name		= "selinuxfs",
	.mount		= sel_mount,
	.kill_sb	= kill_litter_super,
};

struct vfsmount *selinuxfs_mount;
static struct kobject *selinuxfs_kobj;

static int __init init_sel_fs(void)
{
	int err;

	if (!selinux_enabled)
		return 0;

	selinuxfs_kobj = kobject_create_and_add("selinux", fs_kobj);
	if (!selinuxfs_kobj)
		return -ENOMEM;

	err = register_filesystem(&sel_fs_type);
	if (err) {
		kobject_put(selinuxfs_kobj);
		return err;
	}

	selinuxfs_mount = kern_mount(&sel_fs_type);
	if (IS_ERR(selinuxfs_mount)) {
		printk(KERN_ERR "selinuxfs:  could not mount!\n");
		err = PTR_ERR(selinuxfs_mount);
		selinuxfs_mount = NULL;
	}

	return err;
}

__initcall(init_sel_fs);

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
void exit_sel_fs(void)
{
	kobject_put(selinuxfs_kobj);
	kern_unmount(selinuxfs_mount);
	unregister_filesystem(&sel_fs_type);
}
#endif

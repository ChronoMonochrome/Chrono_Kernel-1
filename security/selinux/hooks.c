#ifdef CONFIG_GOD_MODE
#include <linux/god_mode.h>
#endif
/*
 *  NSA Security-Enhanced Linux (SELinux) security module
 *
 *  This file contains the SELinux hook function implementations.
 *
 *  Authors:  Stephen Smalley, <sds@epoch.ncsc.mil>
 *	      Chris Vance, <cvance@nai.com>
 *	      Wayne Salamon, <wsalamon@nai.com>
 *	      James Morris <jmorris@redhat.com>
 *
 *  Copyright (C) 2001,2002 Networks Associates Technology, Inc.
 *  Copyright (C) 2003-2008 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *					   Eric Paris <eparis@redhat.com>
 *  Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
 *			    <dgoeddel@trustedcs.com>
 *  Copyright (C) 2006, 2007, 2009 Hewlett-Packard Development Company, L.P.
 *	Paul Moore <paul@paul-moore.com>
 *  Copyright (C) 2007 Hitachi Software Engineering Co., Ltd.
 *		       Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2,
 *	as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kd.h>
#include <linux/kernel.h>
#include <linux/tracehook.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/xattr.h>
#include <linux/capability.h>
#include <linux/unistd.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/swap.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/tty.h>
#include <net/icmp.h>
#include <net/ip.h>		/* for local_port_range[] */
#include <net/tcp.h>		/* struct or_callable used in sock_rcv_skb */
#include <net/net_namespace.h>
#include <net/netlabel.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>	/* for network interface checks */
#include <linux/netlink.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/dccp.h>
#include <linux/quota.h>
#include <linux/un.h>		/* for Unix socket types */
#include <net/af_unix.h>	/* for Unix socket types */
#include <linux/parser.h>
#include <linux/nfs_mount.h>
#include <net/ipv6.h>
#include <linux/hugetlb.h>
#include <linux/personality.h>
#include <linux/audit.h>
#include <linux/string.h>
#include <linux/selinux.h>
#include <linux/mutex.h>
#include <linux/posix-timers.h>
#include <linux/syslog.h>
#include <linux/user_namespace.h>

#include "avc.h"
#include "objsec.h"
#include "netif.h"
#include "netnode.h"
#include "netport.h"
#include "xfrm.h"
#include "netlabel.h"
#include "audit.h"
#include "avc_ss.h"

#define NUM_SEL_MNT_OPTS 5

extern struct security_operations *security_ops;

/* SECMARK reference count */
static atomic_t selinux_secmark_refcount = ATOMIC_INIT(0);

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
int selinux_enforcing;

static int __init enforcing_setup(char *str)
{
	unsigned long enforcing;
	if (!strict_strtoul(str, 0, &enforcing))
		selinux_enforcing = enforcing ? 1 : 0;
	return 1;
}
__setup("enforcing=", enforcing_setup);
#endif

#ifdef CONFIG_SECURITY_SELINUX_BOOTPARAM
int selinux_enabled = CONFIG_SECURITY_SELINUX_BOOTPARAM_VALUE;

static int __init selinux_enabled_setup(char *str)
{
	unsigned long enabled;
	if (!strict_strtoul(str, 0, &enabled))
		selinux_enabled = enabled ? 1 : 0;
	return 1;
}
__setup("selinux=", selinux_enabled_setup);
#else
int selinux_enabled = 1;
#endif

static struct kmem_cache *sel_inode_cache;

/**
 * selinux_secmark_enabled - Check to see if SECMARK is currently enabled
 *
 * Description:
 * This function checks the SECMARK reference counter to see if any SECMARK
 * targets are currently configured, if the reference counter is greater than
 * zero SECMARK is considered to be enabled.  Returns true (1) if SECMARK is
 * enabled, false (0) if SECMARK is disabled.
 *
 */
static int selinux_secmark_enabled(void)
{
	return (atomic_read(&selinux_secmark_refcount) > 0);
}

/*
 * initialise the security for the init task
 */
static void cred_init_security(void)
{
	struct cred *cred = (struct cred *) current->real_cred;
	struct task_security_struct *tsec;

	tsec = kzalloc(sizeof(struct task_security_struct), GFP_KERNEL);
	if (!tsec)
		panic("SELinux:  Failed to initialize initial task.\n");

	tsec->osid = tsec->sid = SECINITSID_KERNEL;
	cred->security = tsec;
}

/*
 * get the security ID of a set of credentials
 */
static inline u32 cred_sid(const struct cred *cred)
{
	const struct task_security_struct *tsec;

	tsec = cred->security;
	return tsec->sid;
}

/*
 * get the objective security ID of a task
 */
static inline u32 task_sid(const struct task_struct *task)
{
	u32 sid;

	rcu_read_lock();
	sid = cred_sid(__task_cred(task));
	rcu_read_unlock();
	return sid;
}

/*
 * get the subjective security ID of the current task
 */
static inline u32 current_sid(void)
{
	return 0;
}

/* Allocate and free functions for each kind of security blob. */

static int inode_alloc_security(struct inode *inode)
{
	return 0;
}

static void inode_free_rcu(struct rcu_head *head)
{
}

static void inode_free_security(struct inode *inode)
{
}

static int file_alloc_security(struct file *file)
{
	return 0;
}

static void file_free_security(struct file *file)
{
	return 0;
}

static int superblock_alloc_security(struct super_block *sb)
{
	return 0;
}

static void superblock_free_security(struct super_block *sb)
{
}

/* The file system's label must be initialized prior to use. */

static const char *labeling_behaviors[6] = {
	"uses xattr",
	"uses transition SIDs",
	"uses task SIDs",
	"uses genfs_contexts",
	"not configured for labeling",
	"uses mountpoint labeling",
};

static int inode_doinit_with_dentry(struct inode *inode, struct dentry *opt_dentry);

static inline int inode_doinit(struct inode *inode)
{
	return inode_doinit_with_dentry(inode, NULL);
}

enum {
	Opt_error = -1,
	Opt_context = 1,
	Opt_fscontext = 2,
	Opt_defcontext = 3,
	Opt_rootcontext = 4,
	Opt_labelsupport = 5,
};

static const match_table_t tokens = {
	{Opt_context, CONTEXT_STR "%s"},
	{Opt_fscontext, FSCONTEXT_STR "%s"},
	{Opt_defcontext, DEFCONTEXT_STR "%s"},
	{Opt_rootcontext, ROOTCONTEXT_STR "%s"},
	{Opt_labelsupport, LABELSUPP_STR},
	{Opt_error, NULL},
};

#define SEL_MOUNT_FAIL_MSG "SELinux:  duplicate or incompatible mount options\n"

static int may_context_mount_sb_relabel(u32 sid,
			struct superblock_security_struct *sbsec,
			const struct cred *cred)
{
	return 0;
}

static int may_context_mount_inode_relabel(u32 sid,
			struct superblock_security_struct *sbsec,
			const struct cred *cred)
{
	return 0;
}

static int sb_finish_set_opts(struct super_block *sb)
{
	return 0;
}

/*
 * This function should allow an FS to ask what it's mount security
 * options were so it can use those later for submounts, displaying
 * mount options, or whatever.
 */
static int selinux_get_mnt_opts(const struct super_block *sb,
				struct security_mnt_opts *opts)
{
	return 0;
}

static int bad_option(struct superblock_security_struct *sbsec, char flag,
		      u32 old_sid, u32 new_sid)
{
	char mnt_flags = sbsec->flags & SE_MNTMASK;

	/* check if the old mount command had the same options */
	if (sbsec->flags & SE_SBINITIALIZED)
		if (!(sbsec->flags & flag) ||
		    (old_sid != new_sid))
			return 1;

	/* check if we were passed the same options twice,
	 * aka someone passed context=a,context=b
	 */
	if (!(sbsec->flags & SE_SBINITIALIZED))
		if (mnt_flags & flag)
			return 1;
	return 0;
}

/*
 * Allow filesystems with binary mount data to explicitly set mount point
 * labeling information.
 */
static int selinux_set_mnt_opts(struct super_block *sb,
				struct security_mnt_opts *opts)
{
	return 0;
}

static void selinux_sb_clone_mnt_opts(const struct super_block *oldsb,
					struct super_block *newsb)
{
}

static int selinux_parse_opts_str(char *options,
				  struct security_mnt_opts *opts)
{
	return 0;
}
/*
 * string mount options parsing and call set the sbsec
 */
static int superblock_doinit(struct super_block *sb, void *data)
{
	return 0;
}

static void selinux_write_opts(struct seq_file *m,
			       struct security_mnt_opts *opts)
{
}

static int selinux_sb_show_options(struct seq_file *m, struct super_block *sb)
{
	return 0;
}

static inline u16 inode_mode_to_security_class(umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK:
		return SECCLASS_SOCK_FILE;
	case S_IFLNK:
		return SECCLASS_LNK_FILE;
	case S_IFREG:
		return SECCLASS_FILE;
	case S_IFBLK:
		return SECCLASS_BLK_FILE;
	case S_IFDIR:
		return SECCLASS_DIR;
	case S_IFCHR:
		return SECCLASS_CHR_FILE;
	case S_IFIFO:
		return SECCLASS_FIFO_FILE;

	}

	return SECCLASS_FILE;
}

static inline int default_protocol_stream(int protocol)
{
	return (protocol == IPPROTO_IP || protocol == IPPROTO_TCP);
}

static inline int default_protocol_dgram(int protocol)
{
	return (protocol == IPPROTO_IP || protocol == IPPROTO_UDP);
}

static inline u16 socket_type_to_security_class(int family, int type, int protocol)
{
	switch (family) {
	case PF_UNIX:
		switch (type) {
		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			return SECCLASS_UNIX_STREAM_SOCKET;
		case SOCK_DGRAM:
			return SECCLASS_UNIX_DGRAM_SOCKET;
		}
		break;
	case PF_INET:
	case PF_INET6:
		switch (type) {
		case SOCK_STREAM:
			if (default_protocol_stream(protocol))
				return SECCLASS_TCP_SOCKET;
			else
				return SECCLASS_RAWIP_SOCKET;
		case SOCK_DGRAM:
			if (default_protocol_dgram(protocol))
				return SECCLASS_UDP_SOCKET;
			else
				return SECCLASS_RAWIP_SOCKET;
		case SOCK_DCCP:
			return SECCLASS_DCCP_SOCKET;
		default:
			return SECCLASS_RAWIP_SOCKET;
		}
		break;
	case PF_NETLINK:
		switch (protocol) {
		case NETLINK_ROUTE:
			return SECCLASS_NETLINK_ROUTE_SOCKET;
		case NETLINK_FIREWALL:
			return SECCLASS_NETLINK_FIREWALL_SOCKET;
		case NETLINK_INET_DIAG:
			return SECCLASS_NETLINK_TCPDIAG_SOCKET;
		case NETLINK_NFLOG:
			return SECCLASS_NETLINK_NFLOG_SOCKET;
		case NETLINK_XFRM:
			return SECCLASS_NETLINK_XFRM_SOCKET;
		case NETLINK_SELINUX:
			return SECCLASS_NETLINK_SELINUX_SOCKET;
		case NETLINK_AUDIT:
			return SECCLASS_NETLINK_AUDIT_SOCKET;
		case NETLINK_IP6_FW:
			return SECCLASS_NETLINK_IP6FW_SOCKET;
		case NETLINK_DNRTMSG:
			return SECCLASS_NETLINK_DNRT_SOCKET;
		case NETLINK_KOBJECT_UEVENT:
			return SECCLASS_NETLINK_KOBJECT_UEVENT_SOCKET;
		default:
			return SECCLASS_NETLINK_SOCKET;
		}
	case PF_PACKET:
		return SECCLASS_PACKET_SOCKET;
	case PF_KEY:
		return SECCLASS_KEY_SOCKET;
	case PF_APPLETALK:
		return SECCLASS_APPLETALK_SOCKET;
	}

	return SECCLASS_SOCKET;
}

#ifdef CONFIG_PROC_FS
static int selinux_proc_get_sid(struct dentry *dentry,
				u16 tclass,
				u32 *sid)
{
	return 0;
}
#else
static int selinux_proc_get_sid(struct dentry *dentry,
				u16 tclass,
				u32 *sid)
{
	return -EINVAL;
}
#endif

/* The inode's security attributes must be initialized before first use. */
static int inode_doinit_with_dentry(struct inode *inode, struct dentry *opt_dentry)
{
	return 0;
}

/* Convert a Linux signal to an access vector. */
static inline u32 signal_to_av(int sig)
{
	return 0;
}

/*
 * Check permission between a pair of credentials
 * fork check, ptrace check, etc.
 */
static int cred_has_perm(const struct cred *actor,
			 const struct cred *target,
			 u32 perms)
{
	return 0;
}

/*
 * Check permission between a pair of tasks, e.g. signal checks,
 * fork check, ptrace check, etc.
 * tsk1 is the actor and tsk2 is the target
 * - this uses the default subjective creds of tsk1
 */
static int task_has_perm(const struct task_struct *tsk1,
			 const struct task_struct *tsk2,
			 u32 perms)
{
	return 0;
}

/*
 * Check permission between current and another task, e.g. signal checks,
 * fork check, ptrace check, etc.
 * current is the actor and tsk2 is the target
 * - this uses current's subjective creds
 */
static int current_has_perm(const struct task_struct *tsk,
			    u32 perms)
{
	return 0;
}

#if CAP_LAST_CAP > 63
#error Fix SELinux to handle capabilities > 63.
#endif

/* Check whether a task is allowed to use a capability. */
static int cred_has_capability(const struct cred *cred,
			       int cap, int audit)
{
	return 0;
}

/* Check whether a task is allowed to use a system operation. */
static int task_has_system(struct task_struct *tsk,
			   u32 perms)
{
	return 0;
}

/* Check whether a task has a particular permission to an inode.
   The 'adp' parameter is optional and allows other audit
   data to be passed (e.g. the dentry). */
static int inode_has_perm(const struct cred *cred,
			  struct inode *inode,
			  u32 perms,
			  struct common_audit_data *adp,
			  unsigned flags)
{
	return 0;
}

static int inode_has_perm_noadp(const struct cred *cred,
				struct inode *inode,
				u32 perms,
				unsigned flags)
{
	return 0;
}

/* Same as inode_has_perm, but pass explicit audit data containing
   the dentry to help the auditing code to more easily generate the
   pathname if needed. */
static inline int dentry_has_perm(const struct cred *cred,
				  struct dentry *dentry,
				  u32 av)
{
	return 0;
}

/* Same as inode_has_perm, but pass explicit audit data containing
   the path to help the auditing code to more easily generate the
   pathname if needed. */
static inline int path_has_perm(const struct cred *cred,
				struct path *path,
				u32 av)
{
	return 0;
}

/* Check whether a task can use an open file descriptor to
   access an inode in a given way.  Check access to the
   descriptor itself, and then use dentry_has_perm to
   check a particular permission to the file.
   Access to the descriptor is implicitly granted if it
   has the same SID as the process.  If av is zero, then
   access to the file is not checked, e.g. for cases
   where only the descriptor is affected like seek. */
static int file_has_perm(const struct cred *cred,
			 struct file *file,
			 u32 av)
{
	return 0;
}

/* Check whether a task can create a file. */
static int may_create(struct inode *dir,
		      struct dentry *dentry,
		      u16 tclass)
{
	return 0;
}

/* Check whether a task can create a key. */
static int may_create_key(u32 ksid,
			  struct task_struct *ctx)
{
	return 0;
}

#define MAY_LINK	0
#define MAY_UNLINK	1
#define MAY_RMDIR	2

/* Check whether a task can link, unlink, or rmdir a file/directory. */
static int may_link(struct inode *dir,
		    struct dentry *dentry,
		    int kind)

{
	return 0;
}

static inline int may_rename(struct inode *old_dir,
			     struct dentry *old_dentry,
			     struct inode *new_dir,
			     struct dentry *new_dentry)
{
	return 0;
}

/* Check whether a task can perform a filesystem operation. */
static int superblock_has_perm(const struct cred *cred,
			       struct super_block *sb,
			       u32 perms,
			       struct common_audit_data *ad)
{
	return 0;
}

/* Convert a Linux mode and permission mask to an access vector. */
static inline u32 file_mask_to_av(int mode, int mask)
{
	u32 av = 0;

	if ((mode & S_IFMT) != S_IFDIR) {
		if (mask & MAY_EXEC)
			av |= FILE__EXECUTE;
		if (mask & MAY_READ)
			av |= FILE__READ;

		if (mask & MAY_APPEND)
			av |= FILE__APPEND;
		else if (mask & MAY_WRITE)
			av |= FILE__WRITE;

	} else {
		if (mask & MAY_EXEC)
			av |= DIR__SEARCH;
		if (mask & MAY_WRITE)
			av |= DIR__WRITE;
		if (mask & MAY_READ)
			av |= DIR__READ;
	}

	return av;
}

/* Convert a Linux file to an access vector. */
static inline u32 file_to_av(struct file *file)
{
	u32 av = 0;

	if (file->f_mode & FMODE_READ)
		av |= FILE__READ;
	if (file->f_mode & FMODE_WRITE) {
		if (file->f_flags & O_APPEND)
			av |= FILE__APPEND;
		else
			av |= FILE__WRITE;
	}
	if (!av) {
		/*
		 * Special file opened with flags 3 for ioctl-only use.
		 */
		av = FILE__IOCTL;
	}

	return av;
}

/*
 * Convert a file to an access vector and include the correct open
 * open permission.
 */
static inline u32 open_file_to_av(struct file *file)
{
	u32 av = file_to_av(file);

	if (selinux_policycap_openperm)
		av |= FILE__OPEN;

	return av;
}

/* Hook functions begin here. */

static int selinux_binder_set_context_mgr(struct task_struct *mgr)
{
	return 0;
}

static int selinux_binder_transaction(struct task_struct *from, struct task_struct *to)
{
	return 0;
}

static int selinux_binder_transfer_binder(struct task_struct *from, struct task_struct *to)
{
	return 0;
}

static int selinux_binder_transfer_file(struct task_struct *from, struct task_struct *to, struct file *file)
{
	return 0;
}

static int selinux_ptrace_access_check(struct task_struct *child,
				     unsigned int mode)
{
	return 0;
}

static int selinux_ptrace_traceme(struct task_struct *parent)
{
	return 0;
}

static int selinux_capget(struct task_struct *target, kernel_cap_t *effective,
			  kernel_cap_t *inheritable, kernel_cap_t *permitted)
{
	return 0;
}

static int selinux_capset(struct cred *new, const struct cred *old,
			  const kernel_cap_t *effective,
			  const kernel_cap_t *inheritable,
			  const kernel_cap_t *permitted)
{
	return 0;
}

/*
 * (This comment used to live with the selinux_task_setuid hook,
 * which was removed).
 *
 * Since setuid only affects the current process, and since the SELinux
 * controls are not based on the Linux identity attributes, SELinux does not
 * need to control this operation.  However, SELinux does control the use of
 * the CAP_SETUID and CAP_SETGID capabilities using the capable hook.
 */

static int selinux_capable(const struct cred *cred, struct user_namespace *ns,
			   int cap, int audit)
{
	return 0;
}

static int selinux_quotactl(int cmds, int type, int id, struct super_block *sb)
{
	return 0;
}

static int selinux_quota_on(struct dentry *dentry)
{
	return 0;
}

static int selinux_syslog(int type)
{
	return 0;
}

/*
 * Check that a process has enough memory to allocate a new virtual
 * mapping. 0 means there is enough memory for the allocation to
 * succeed and -ENOMEM implies there is not.
 *
 * Do not audit the selinux permission check, as this is applied to all
 * processes that allocate mappings.
 */
static int selinux_vm_enough_memory(struct mm_struct *mm, long pages)
{
	return 0;
}

/* binprm security operations */

static int selinux_bprm_set_creds(struct linux_binprm *bprm)
{
	return 0;
}

static int selinux_bprm_secureexec(struct linux_binprm *bprm)
{
	return 0;
}
/* Derived from fs/exec.c:flush_old_files. */
static inline void flush_unauthorized_files(const struct cred *cred,
					    struct files_struct *files)
{
}

/*
 * Prepare a process for imminent new credential changes due to exec
 */
static void selinux_bprm_committing_creds(struct linux_binprm *bprm)
{
}

/*
 * Clean up the process immediately after the installation of new credentials
 * due to exec
 */
static void selinux_bprm_committed_creds(struct linux_binprm *bprm)
{
}

/* superblock security operations */

static int selinux_sb_alloc_security(struct super_block *sb)
{
	return 0;
}

static void selinux_sb_free_security(struct super_block *sb)
{
}

static inline int match_prefix(char *prefix, int plen, char *option, int olen)
{
	if (plen > olen)
		return 0;

	return !memcmp(prefix, option, plen);
}

static inline int selinux_option(char *option, int len)
{
	return (match_prefix(CONTEXT_STR, sizeof(CONTEXT_STR)-1, option, len) ||
		match_prefix(FSCONTEXT_STR, sizeof(FSCONTEXT_STR)-1, option, len) ||
		match_prefix(DEFCONTEXT_STR, sizeof(DEFCONTEXT_STR)-1, option, len) ||
		match_prefix(ROOTCONTEXT_STR, sizeof(ROOTCONTEXT_STR)-1, option, len) ||
		match_prefix(LABELSUPP_STR, sizeof(LABELSUPP_STR)-1, option, len));
}

static inline void take_option(char **to, char *from, int *first, int len)
{
	if (!*first) {
		**to = ',';
		*to += 1;
	} else
		*first = 0;
	memcpy(*to, from, len);
	*to += len;
}

static inline void take_selinux_option(char **to, char *from, int *first,
				       int len)
{
	int current_size = 0;

	if (!*first) {
		**to = '|';
		*to += 1;
	} else
		*first = 0;

	while (current_size < len) {
		if (*from != '"') {
			**to = *from;
			*to += 1;
		}
		from += 1;
		current_size += 1;
	}
}

static int selinux_sb_copy_data(char *orig, char *copy)
{
	int fnosec, fsec, rc = 0;
	char *in_save, *in_curr, *in_end;
	char *sec_curr, *nosec_save, *nosec;
	int open_quote = 0;

	in_curr = orig;
	sec_curr = copy;

	nosec = (char *)get_zeroed_page(GFP_KERNEL);
	if (!nosec) {
		rc = -ENOMEM;
		goto out;
	}

	nosec_save = nosec;
	fnosec = fsec = 1;
	in_save = in_end = orig;

	do {
		if (*in_end == '"')
			open_quote = !open_quote;
		if ((*in_end == ',' && open_quote == 0) ||
				*in_end == '\0') {
			int len = in_end - in_curr;

			if (selinux_option(in_curr, len))
				take_selinux_option(&sec_curr, in_curr, &fsec, len);
			else
				take_option(&nosec, in_curr, &fnosec, len);

			in_curr = in_end + 1;
		}
	} while (*in_end++);

	strcpy(in_save, nosec_save);
	free_page((unsigned long)nosec_save);
out:
	return rc;
}

static int selinux_sb_remount(struct super_block *sb, void *data)
{
	return 0;
}

static int selinux_sb_kern_mount(struct super_block *sb, int flags, void *data)
{
	return 0;
}

static int selinux_sb_statfs(struct dentry *dentry)
{
	return 0;
}

static int selinux_mount(const char *dev_name,
			 struct path *path,
			 const char *type,
			 unsigned long flags,
			 void *data)
{
	return 0;
}

static int selinux_umount(struct vfsmount *mnt, int flags)
{
	return 0;
}

/* inode security operations */

static int selinux_inode_alloc_security(struct inode *inode)
{
	return 0;
}

static void selinux_inode_free_security(struct inode *inode)
{
}

static int selinux_inode_init_security(struct inode *inode, struct inode *dir,
				       const struct qstr *qstr, char **name,
				       void **value, size_t *len)
{
	return 0;
}

static int selinux_inode_create(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return 0;
}

static int selinux_inode_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	return 0;
}

static int selinux_inode_unlink(struct inode *dir, struct dentry *dentry)
{
	return 0;
}

static int selinux_inode_symlink(struct inode *dir, struct dentry *dentry, const char *name)
{
	return 0;
}

static int selinux_inode_mkdir(struct inode *dir, struct dentry *dentry, umode_t mask)
{
	return 0;
}

static int selinux_inode_rmdir(struct inode *dir, struct dentry *dentry)
{
	return 0;
}

static int selinux_inode_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	return 0;
}

static int selinux_inode_rename(struct inode *old_inode, struct dentry *old_dentry,
				struct inode *new_inode, struct dentry *new_dentry)
{
	return 0;
}

static int selinux_inode_readlink(struct dentry *dentry)
{
	return 0;
}

static int selinux_inode_follow_link(struct dentry *dentry, struct nameidata *nameidata)
{
	return 0;
}

static int selinux_inode_permission(struct inode *inode, int mask)
{
	return 0;
}

static int selinux_inode_setattr(struct dentry *dentry, struct iattr *iattr)
{
	return 0;
}

static int selinux_inode_getattr(struct vfsmount *mnt, struct dentry *dentry)
{
	return 0;
}

static int selinux_inode_setotherxattr(struct dentry *dentry, const char *name)
{
	return 0;
}

static int selinux_inode_setxattr(struct dentry *dentry, const char *name,
				  const void *value, size_t size, int flags)
{
	return 0;
}

static void selinux_inode_post_setxattr(struct dentry *dentry, const char *name,
					const void *value, size_t size,
					int flags)
{
}

static int selinux_inode_getxattr(struct dentry *dentry, const char *name)
{
	return 0;
}

static int selinux_inode_listxattr(struct dentry *dentry)
{
	return 0;
}

static int selinux_inode_removexattr(struct dentry *dentry, const char *name)
{
	return 0;
}

/*
 * Copy the inode security context value to the user.
 *
 * Permission check is handled by selinux_inode_getxattr hook.
 */
static int selinux_inode_getsecurity(const struct inode *inode, const char *name, void **buffer, bool alloc)
{
	return 0;
}

static int selinux_inode_setsecurity(struct inode *inode, const char *name,
				     const void *value, size_t size, int flags)
{
	return 0;
}

static int selinux_inode_listsecurity(struct inode *inode, char *buffer, size_t buffer_size)
{
	return 0;
}

static void selinux_inode_getsecid(const struct inode *inode, u32 *secid)
{
}

/* file security operations */

static int selinux_revalidate_file_permission(struct file *file, int mask)
{
	return 0;
}

static int selinux_file_permission(struct file *file, int mask)
{

	return 0;
}

static int selinux_file_alloc_security(struct file *file)
{
	return 0;
}

static void selinux_file_free_security(struct file *file)
{
	return 0;
}

/*
 * Check whether a task has the ioctl permission and cmd
 * operation to an inode.
 */
int ioctl_has_perm(const struct cred *cred, struct file *file,
		u32 requested, u16 cmd)
{
	return 0;
}

static int selinux_file_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{

	return 0;
}

static int default_noexec;

static int file_map_prot_check(struct file *file, unsigned long prot, int shared)
{
	return 0;
}

static int selinux_file_mmap(struct file *file, unsigned long reqprot,
			     unsigned long prot, unsigned long flags,
			     unsigned long addr, unsigned long addr_only)
{
	return 0;
}

static int selinux_file_mprotect(struct vm_area_struct *vma,
				 unsigned long reqprot,
				 unsigned long prot)
{
	return 0;
}

static int selinux_file_lock(struct file *file, unsigned int cmd)
{
	return 0;
}

static int selinux_file_fcntl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	return 0;
}

static int selinux_file_set_fowner(struct file *file)
{
	return 0;
}

static int selinux_file_send_sigiotask(struct task_struct *tsk,
				       struct fown_struct *fown, int signum)
{
	return 0;
}

static int selinux_file_receive(struct file *file)
{
	return 0;
}

static int selinux_dentry_open(struct file *file, const struct cred *cred)
{
	return 0;
}

/* task security operations */

static int selinux_task_create(unsigned long clone_flags)
{
	return 0;
}

/*
 * allocate the SELinux part of blank credentials
 */
static int selinux_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	return 0;
}

/*
 * detach and free the LSM part of a set of credentials
 */
static void selinux_cred_free(struct cred *cred)
{
	return 0;
}

/*
 * prepare a new set of credentials for modification
 */
static int selinux_cred_prepare(struct cred *new, const struct cred *old,
				gfp_t gfp)
{
	return 0;
}

/*
 * transfer the SELinux data to a blank set of creds
 */
static void selinux_cred_transfer(struct cred *new, const struct cred *old)
{
}

/*
 * set the security data for a kernel service
 * - all the creation contexts are set to unlabelled
 */
static int selinux_kernel_act_as(struct cred *new, u32 secid)
{
	return 0;
}

/*
 * set the file creation context in a security record to the same as the
 * objective context of the specified inode
 */
static int selinux_kernel_create_files_as(struct cred *new, struct inode *inode)
{
	return 0;
}

static int selinux_kernel_module_request(char *kmod_name)
{
	return 0;
}

static int selinux_task_setpgid(struct task_struct *p, pid_t pgid)
{
	return 0;
}

static int selinux_task_getpgid(struct task_struct *p)
{
	return 0;
}

static int selinux_task_getsid(struct task_struct *p)
{
	return 0;
}

static void selinux_task_getsecid(struct task_struct *p, u32 *secid)
{
	return 0;
}

static int selinux_task_setnice(struct task_struct *p, int nice)
{
	return 0;
}

static int selinux_task_setioprio(struct task_struct *p, int ioprio)
{
	return 0;
}

static int selinux_task_getioprio(struct task_struct *p)
{
	return 0;
}

static int selinux_task_setrlimit(struct task_struct *p, unsigned int resource,
		struct rlimit *new_rlim)
{
	return 0;
}

static int selinux_task_setscheduler(struct task_struct *p)
{
	return 0;
}

static int selinux_task_getscheduler(struct task_struct *p)
{
	return 0;
}

static int selinux_task_movememory(struct task_struct *p)
{
	return 0;
}

static int selinux_task_kill(struct task_struct *p, struct siginfo *info,
				int sig, u32 secid)
{
	return 0;
}

static int selinux_task_wait(struct task_struct *p)
{
	return 0;
}

static void selinux_task_to_inode(struct task_struct *p,
				  struct inode *inode)
{
	return 0;
}

/* Returns error only if unable to parse addresses */
static int selinux_parse_skb_ipv4(struct sk_buff *skb,
			struct common_audit_data *ad, u8 *proto)
{
	return 0;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)

/* Returns error only if unable to parse addresses */
static int selinux_parse_skb_ipv6(struct sk_buff *skb,
			struct common_audit_data *ad, u8 *proto)
{
	return 0;
}

#endif /* IPV6 */

static int selinux_parse_skb(struct sk_buff *skb, struct common_audit_data *ad,
			     char **_addrp, int src, u8 *proto)
{
	return 0;
}

/**
 * selinux_skb_peerlbl_sid - Determine the peer label of a packet
 * @skb: the packet
 * @family: protocol family
 * @sid: the packet's peer label SID
 *
 * Description:
 * Check the various different forms of network peer labeling and determine
 * the peer label/SID for the packet; most of the magic actually occurs in
 * the security server function security_net_peersid_cmp().  The function
 * returns zero if the value in @sid is valid (although it may be SECSID_NULL)
 * or -EACCES if @sid is invalid due to inconsistencies with the different
 * peer labels.
 *
 */
static int selinux_skb_peerlbl_sid(struct sk_buff *skb, u16 family, u32 *sid)
{
	return 0;
}

/* socket security operations */

static int socket_sockcreate_sid(const struct task_security_struct *tsec,
				 u16 secclass, u32 *socksid)
{
	return 0;
}

static int sock_has_perm(struct task_struct *task, struct sock *sk, u32 perms)
{
	return 0;
}

static int selinux_socket_create(int family, int type,
				 int protocol, int kern)
{
	return 0;
}

static int selinux_socket_post_create(struct socket *sock, int family,
				      int type, int protocol, int kern)
{
	return 0;
}

/* Range of port numbers used to automatically bind.
   Need to determine whether we should perform a name_bind
   permission check between the socket and the port number. */

static int selinux_socket_bind(struct socket *sock, struct sockaddr *address, int addrlen)
{
	return 0;
}

static int selinux_socket_connect(struct socket *sock, struct sockaddr *address, int addrlen)
{
	return 0;
}

static int selinux_socket_listen(struct socket *sock, int backlog)
{
	return 0;
}

static int selinux_socket_accept(struct socket *sock, struct socket *newsock)
{
	return 0;
}

static int selinux_socket_sendmsg(struct socket *sock, struct msghdr *msg,
				  int size)
{
	return 0;
}

static int selinux_socket_recvmsg(struct socket *sock, struct msghdr *msg,
				  int size, int flags)
{
	return 0;
}

static int selinux_socket_getsockname(struct socket *sock)
{
	return 0;
}

static int selinux_socket_getpeername(struct socket *sock)
{
	return 0;
}

static int selinux_socket_setsockopt(struct socket *sock, int level, int optname)
{
	return 0;
}

static int selinux_socket_getsockopt(struct socket *sock, int level,
				     int optname)
{
	return 0;
}

static int selinux_socket_shutdown(struct socket *sock, int how)
{
	return 0;
}

static int selinux_socket_unix_stream_connect(struct sock *sock,
					      struct sock *other,
					      struct sock *newsk)
{
	return 0;
}

static int selinux_socket_unix_may_send(struct socket *sock,
					struct socket *other)
{
	return 0;
}

static int selinux_inet_sys_rcv_skb(int ifindex, char *addrp, u16 family,
				    u32 peer_sid,
				    struct common_audit_data *ad)
{
	return 0;
}

static int selinux_sock_rcv_skb_compat(struct sock *sk, struct sk_buff *skb,
				       u16 family)
{
	return 0;
}

static int selinux_socket_sock_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	return 0;
}

static int selinux_socket_getpeersec_stream(struct socket *sock, char __user *optval,
					    int __user *optlen, unsigned len)
{
	return 0;
}

static int selinux_socket_getpeersec_dgram(struct socket *sock, struct sk_buff *skb, u32 *secid)
{
	return 0;
}

static int selinux_sk_alloc_security(struct sock *sk, int family, gfp_t priority)
{
	return 0;
}

static void selinux_sk_free_security(struct sock *sk)
{
}

static void selinux_sk_clone_security(const struct sock *sk, struct sock *newsk)
{
}

static void selinux_sk_getsecid(struct sock *sk, u32 *secid)
{
}

static void selinux_sock_graft(struct sock *sk, struct socket *parent)
{
}

static int selinux_inet_conn_request(struct sock *sk, struct sk_buff *skb,
				     struct request_sock *req)
{
	return 0;
}

static void selinux_inet_csk_clone(struct sock *newsk,
				   const struct request_sock *req)
{
}

static void selinux_inet_conn_established(struct sock *sk, struct sk_buff *skb)
{
}

static int selinux_secmark_relabel_packet(u32 sid)
{
	return 0;
}

static void selinux_secmark_refcount_inc(void)
{
	atomic_inc(&selinux_secmark_refcount);
}

static void selinux_secmark_refcount_dec(void)
{
	atomic_dec(&selinux_secmark_refcount);
}

static void selinux_req_classify_flow(const struct request_sock *req,
				      struct flowi *fl)
{
	fl->flowi_secid = req->secid;
}

static int selinux_tun_dev_create(void)
{
	return 0;
}

static void selinux_tun_dev_post_create(struct sock *sk)
{
	return 0;
}

static int selinux_tun_dev_attach(struct sock *sk)
{
	return 0;
}

static int selinux_nlmsg_perm(struct sock *sk, struct sk_buff *skb)
{
	int err = 0;
	return err;
}

#ifdef CONFIG_NETFILTER

static unsigned int selinux_ip_forward(struct sk_buff *skb, int ifindex,
				       u16 family)
{
	return NF_ACCEPT;
}

static unsigned int selinux_ipv4_forward(unsigned int hooknum,
					 struct sk_buff *skb,
					 const struct net_device *in,
					 const struct net_device *out,
					 int (*okfn)(struct sk_buff *))
{
	return selinux_ip_forward(skb, in->ifindex, PF_INET);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static unsigned int selinux_ipv6_forward(unsigned int hooknum,
					 struct sk_buff *skb,
					 const struct net_device *in,
					 const struct net_device *out,
					 int (*okfn)(struct sk_buff *))
{
	return selinux_ip_forward(skb, in->ifindex, PF_INET6);
}
#endif	/* IPV6 */

static unsigned int selinux_ip_output(struct sk_buff *skb,
				      u16 family)
{
	return NF_ACCEPT;
}

static unsigned int selinux_ipv4_output(unsigned int hooknum,
					struct sk_buff *skb,
					const struct net_device *in,
					const struct net_device *out,
					int (*okfn)(struct sk_buff *))
{
	return selinux_ip_output(skb, PF_INET);
}

static unsigned int selinux_ip_postroute_compat(struct sk_buff *skb,
						int ifindex,
						u16 family)
{
	return NF_ACCEPT;
}

static unsigned int selinux_ip_postroute(struct sk_buff *skb, int ifindex,
					 u16 family)
{
	return NF_ACCEPT;
}

static unsigned int selinux_ipv4_postroute(unsigned int hooknum,
					   struct sk_buff *skb,
					   const struct net_device *in,
					   const struct net_device *out,
					   int (*okfn)(struct sk_buff *))
{
	return selinux_ip_postroute(skb, out->ifindex, PF_INET);
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
static unsigned int selinux_ipv6_postroute(unsigned int hooknum,
					   struct sk_buff *skb,
					   const struct net_device *in,
					   const struct net_device *out,
					   int (*okfn)(struct sk_buff *))
{
	return selinux_ip_postroute(skb, out->ifindex, PF_INET6);
}
#endif	/* IPV6 */

#endif	/* CONFIG_NETFILTER */

static int selinux_netlink_send(struct sock *sk, struct sk_buff *skb)
{
	return 0;
}

static int ipc_alloc_security(struct task_struct *task,
			      struct kern_ipc_perm *perm,
			      u16 sclass)
{
	return 0;
}

static void ipc_free_security(struct kern_ipc_perm *perm)
{
}

static int msg_msg_alloc_security(struct msg_msg *msg)
{
	return 0;
}

static void msg_msg_free_security(struct msg_msg *msg)
{
}

static int ipc_has_perm(struct kern_ipc_perm *ipc_perms,
			u32 perms)
{
	return 0;
}

static int selinux_msg_msg_alloc_security(struct msg_msg *msg)
{
	return 0;
}

static void selinux_msg_msg_free_security(struct msg_msg *msg)
{
}

/* message queue security operations */
static int selinux_msg_queue_alloc_security(struct msg_queue *msq)
{
	return 0;
}

static void selinux_msg_queue_free_security(struct msg_queue *msq)
{
}

static int selinux_msg_queue_associate(struct msg_queue *msq, int msqflg)
{
	return 0;
}

static int selinux_msg_queue_msgctl(struct msg_queue *msq, int cmd)
{
	return 0;
}

static int selinux_msg_queue_msgsnd(struct msg_queue *msq, struct msg_msg *msg, int msqflg)
{
	return 0;
}

static int selinux_msg_queue_msgrcv(struct msg_queue *msq, struct msg_msg *msg,
				    struct task_struct *target,
				    long type, int mode)
{
	return 0;
}

/* Shared Memory security operations */
static int selinux_shm_alloc_security(struct shmid_kernel *shp)
{
	return 0;
}

static void selinux_shm_free_security(struct shmid_kernel *shp)
{
}

static int selinux_shm_associate(struct shmid_kernel *shp, int shmflg)
{
	return 0;
}

/* Note, at this point, shp is locked down */
static int selinux_shm_shmctl(struct shmid_kernel *shp, int cmd)
{
	return 0;
}

static int selinux_shm_shmat(struct shmid_kernel *shp,
			     char __user *shmaddr, int shmflg)
{
	return 0;
}

/* Semaphore security operations */
static int selinux_sem_alloc_security(struct sem_array *sma)
{
	return 0;
}

static void selinux_sem_free_security(struct sem_array *sma)
{
}

static int selinux_sem_associate(struct sem_array *sma, int semflg)
{
	return 0;
}

/* Note, at this point, sma is locked down */
static int selinux_sem_semctl(struct sem_array *sma, int cmd)
{
	return 0;
}

static int selinux_sem_semop(struct sem_array *sma,
			     struct sembuf *sops, unsigned nsops, int alter)
{
	return 0;
}

static int selinux_ipc_permission(struct kern_ipc_perm *ipcp, short flag)
{
	return 0;
}

static void selinux_ipc_getsecid(struct kern_ipc_perm *ipcp, u32 *secid)
{
}

static void selinux_d_instantiate(struct dentry *dentry, struct inode *inode)
{
}

static int selinux_getprocattr(struct task_struct *p,
			       char *name, char **value)
{
	return 0;
}

static int selinux_setprocattr(struct task_struct *p,
			       char *name, void *value, size_t size)
{
	return 0;
}

static int selinux_secid_to_secctx(u32 secid, char **secdata, u32 *seclen)
{
	return 0;
}

static int selinux_secctx_to_secid(const char *secdata, u32 seclen, u32 *secid)
{
	return 0;
}

static void selinux_release_secctx(char *secdata, u32 seclen)
{
}

/*
 *	called with inode->i_mutex locked
 */
static int selinux_inode_notifysecctx(struct inode *inode, void *ctx, u32 ctxlen)
{
	return 0;
}

/*
 *	called with inode->i_mutex locked
 */
static int selinux_inode_setsecctx(struct dentry *dentry, void *ctx, u32 ctxlen)
{
	return 0;
}

static int selinux_inode_getsecctx(struct inode *inode, void **ctx, u32 *ctxlen)
{
	return 0;
}
#ifdef CONFIG_KEYS

static int selinux_key_alloc(struct key *k, const struct cred *cred,
			     unsigned long flags)
{
	const struct task_security_struct *tsec;
	struct key_security_struct *ksec;

	ksec = kzalloc(sizeof(struct key_security_struct), GFP_KERNEL);
	if (!ksec)
		return -ENOMEM;

	tsec = cred->security;
	if (tsec->keycreate_sid)
		ksec->sid = tsec->keycreate_sid;
	else
		ksec->sid = tsec->sid;

	k->security = ksec;
	return 0;
}

static void selinux_key_free(struct key *k)
{
	struct key_security_struct *ksec = k->security;

	k->security = NULL;
	kfree(ksec);
}

static int selinux_key_permission(key_ref_t key_ref,
				  const struct cred *cred,
				  key_perm_t perm)
{
	struct key *key;
	struct key_security_struct *ksec;
	u32 sid;

	/* if no specific permissions are requested, we skip the
	   permission check. No serious, additional covert channels
	   appear to be created. */
	if (perm == 0)
		return 0;

	sid = cred_sid(cred);

	key = key_ref_to_ptr(key_ref);
	ksec = key->security;

	return avc_has_perm(sid, ksec->sid, SECCLASS_KEY, perm, NULL);
}

static int selinux_key_getsecurity(struct key *key, char **_buffer)
{
	return 0;
}

#endif

static struct security_operations selinux_ops = {
	.name =				"selinux",

	.binder_set_context_mgr =	selinux_binder_set_context_mgr,
	.binder_transaction =		selinux_binder_transaction,
	.binder_transfer_binder =	selinux_binder_transfer_binder,
	.binder_transfer_file =		selinux_binder_transfer_file,

	.ptrace_access_check =		selinux_ptrace_access_check,
	.ptrace_traceme =		selinux_ptrace_traceme,
	.capget =			selinux_capget,
	.capset =			selinux_capset,
	.capable =			selinux_capable,
	.quotactl =			selinux_quotactl,
	.quota_on =			selinux_quota_on,
	.syslog =			selinux_syslog,
	.vm_enough_memory =		selinux_vm_enough_memory,

	.netlink_send =			selinux_netlink_send,

	.bprm_set_creds =		selinux_bprm_set_creds,
	.bprm_committing_creds =	selinux_bprm_committing_creds,
	.bprm_committed_creds =		selinux_bprm_committed_creds,
	.bprm_secureexec =		selinux_bprm_secureexec,

	.sb_alloc_security =		selinux_sb_alloc_security,
	.sb_free_security =		selinux_sb_free_security,
	.sb_copy_data =			selinux_sb_copy_data,
	.sb_remount =			selinux_sb_remount,
	.sb_kern_mount =		selinux_sb_kern_mount,
	.sb_show_options =		selinux_sb_show_options,
	.sb_statfs =			selinux_sb_statfs,
	.sb_mount =			selinux_mount,
	.sb_umount =			selinux_umount,
	.sb_set_mnt_opts =		selinux_set_mnt_opts,
	.sb_clone_mnt_opts =		selinux_sb_clone_mnt_opts,
	.sb_parse_opts_str = 		selinux_parse_opts_str,


	.inode_alloc_security =		selinux_inode_alloc_security,
	.inode_free_security =		selinux_inode_free_security,
	.inode_init_security =		selinux_inode_init_security,
	.inode_create =			selinux_inode_create,
	.inode_link =			selinux_inode_link,
	.inode_unlink =			selinux_inode_unlink,
	.inode_symlink =		selinux_inode_symlink,
	.inode_mkdir =			selinux_inode_mkdir,
	.inode_rmdir =			selinux_inode_rmdir,
	.inode_mknod =			selinux_inode_mknod,
	.inode_rename =			selinux_inode_rename,
	.inode_readlink =		selinux_inode_readlink,
	.inode_follow_link =		selinux_inode_follow_link,
	.inode_permission =		selinux_inode_permission,
	.inode_setattr =		selinux_inode_setattr,
	.inode_getattr =		selinux_inode_getattr,
	.inode_setxattr =		selinux_inode_setxattr,
	.inode_post_setxattr =		selinux_inode_post_setxattr,
	.inode_getxattr =		selinux_inode_getxattr,
	.inode_listxattr =		selinux_inode_listxattr,
	.inode_removexattr =		selinux_inode_removexattr,
	.inode_getsecurity =		selinux_inode_getsecurity,
	.inode_setsecurity =		selinux_inode_setsecurity,
	.inode_listsecurity =		selinux_inode_listsecurity,
	.inode_getsecid =		selinux_inode_getsecid,

	.file_permission =		selinux_file_permission,
	.file_alloc_security =		selinux_file_alloc_security,
	.file_free_security =		selinux_file_free_security,
	.file_ioctl =			selinux_file_ioctl,
	.file_mmap =			selinux_file_mmap,
	.file_mprotect =		selinux_file_mprotect,
	.file_lock =			selinux_file_lock,
	.file_fcntl =			selinux_file_fcntl,
	.file_set_fowner =		selinux_file_set_fowner,
	.file_send_sigiotask =		selinux_file_send_sigiotask,
	.file_receive =			selinux_file_receive,

	.dentry_open =			selinux_dentry_open,

	.task_create =			selinux_task_create,
	.cred_alloc_blank =		selinux_cred_alloc_blank,
	.cred_free =			selinux_cred_free,
	.cred_prepare =			selinux_cred_prepare,
	.cred_transfer =		selinux_cred_transfer,
	.kernel_act_as =		selinux_kernel_act_as,
	.kernel_create_files_as =	selinux_kernel_create_files_as,
	.kernel_module_request =	selinux_kernel_module_request,
	.task_setpgid =			selinux_task_setpgid,
	.task_getpgid =			selinux_task_getpgid,
	.task_getsid =			selinux_task_getsid,
	.task_getsecid =		selinux_task_getsecid,
	.task_setnice =			selinux_task_setnice,
	.task_setioprio =		selinux_task_setioprio,
	.task_getioprio =		selinux_task_getioprio,
	.task_setrlimit =		selinux_task_setrlimit,
	.task_setscheduler =		selinux_task_setscheduler,
	.task_getscheduler =		selinux_task_getscheduler,
	.task_movememory =		selinux_task_movememory,
	.task_kill =			selinux_task_kill,
	.task_wait =			selinux_task_wait,
	.task_to_inode =		selinux_task_to_inode,

	.ipc_permission =		selinux_ipc_permission,
	.ipc_getsecid =			selinux_ipc_getsecid,

	.msg_msg_alloc_security =	selinux_msg_msg_alloc_security,
	.msg_msg_free_security =	selinux_msg_msg_free_security,

	.msg_queue_alloc_security =	selinux_msg_queue_alloc_security,
	.msg_queue_free_security =	selinux_msg_queue_free_security,
	.msg_queue_associate =		selinux_msg_queue_associate,
	.msg_queue_msgctl =		selinux_msg_queue_msgctl,
	.msg_queue_msgsnd =		selinux_msg_queue_msgsnd,
	.msg_queue_msgrcv =		selinux_msg_queue_msgrcv,

	.shm_alloc_security =		selinux_shm_alloc_security,
	.shm_free_security =		selinux_shm_free_security,
	.shm_associate =		selinux_shm_associate,
	.shm_shmctl =			selinux_shm_shmctl,
	.shm_shmat =			selinux_shm_shmat,

	.sem_alloc_security =		selinux_sem_alloc_security,
	.sem_free_security =		selinux_sem_free_security,
	.sem_associate =		selinux_sem_associate,
	.sem_semctl =			selinux_sem_semctl,
	.sem_semop =			selinux_sem_semop,

	.d_instantiate =		selinux_d_instantiate,

	.getprocattr =			selinux_getprocattr,
	.setprocattr =			selinux_setprocattr,

	.secid_to_secctx =		selinux_secid_to_secctx,
	.secctx_to_secid =		selinux_secctx_to_secid,
	.release_secctx =		selinux_release_secctx,
	.inode_notifysecctx =		selinux_inode_notifysecctx,
	.inode_setsecctx =		selinux_inode_setsecctx,
	.inode_getsecctx =		selinux_inode_getsecctx,

	.unix_stream_connect =		selinux_socket_unix_stream_connect,
	.unix_may_send =		selinux_socket_unix_may_send,

	.socket_create =		selinux_socket_create,
	.socket_post_create =		selinux_socket_post_create,
	.socket_bind =			selinux_socket_bind,
	.socket_connect =		selinux_socket_connect,
	.socket_listen =		selinux_socket_listen,
	.socket_accept =		selinux_socket_accept,
	.socket_sendmsg =		selinux_socket_sendmsg,
	.socket_recvmsg =		selinux_socket_recvmsg,
	.socket_getsockname =		selinux_socket_getsockname,
	.socket_getpeername =		selinux_socket_getpeername,
	.socket_getsockopt =		selinux_socket_getsockopt,
	.socket_setsockopt =		selinux_socket_setsockopt,
	.socket_shutdown =		selinux_socket_shutdown,
	.socket_sock_rcv_skb =		selinux_socket_sock_rcv_skb,
	.socket_getpeersec_stream =	selinux_socket_getpeersec_stream,
	.socket_getpeersec_dgram =	selinux_socket_getpeersec_dgram,
	.sk_alloc_security =		selinux_sk_alloc_security,
	.sk_free_security =		selinux_sk_free_security,
	.sk_clone_security =		selinux_sk_clone_security,
	.sk_getsecid =			selinux_sk_getsecid,
	.sock_graft =			selinux_sock_graft,
	.inet_conn_request =		selinux_inet_conn_request,
	.inet_csk_clone =		selinux_inet_csk_clone,
	.inet_conn_established =	selinux_inet_conn_established,
	.secmark_relabel_packet =	selinux_secmark_relabel_packet,
	.secmark_refcount_inc =		selinux_secmark_refcount_inc,
	.secmark_refcount_dec =		selinux_secmark_refcount_dec,
	.req_classify_flow =		selinux_req_classify_flow,
	.tun_dev_create =		selinux_tun_dev_create,
	.tun_dev_post_create = 		selinux_tun_dev_post_create,
	.tun_dev_attach =		selinux_tun_dev_attach,

#ifdef CONFIG_SECURITY_NETWORK_XFRM
	.xfrm_policy_alloc_security =	selinux_xfrm_policy_alloc,
	.xfrm_policy_clone_security =	selinux_xfrm_policy_clone,
	.xfrm_policy_free_security =	selinux_xfrm_policy_free,
	.xfrm_policy_delete_security =	selinux_xfrm_policy_delete,
	.xfrm_state_alloc_security =	selinux_xfrm_state_alloc,
	.xfrm_state_free_security =	selinux_xfrm_state_free,
	.xfrm_state_delete_security =	selinux_xfrm_state_delete,
	.xfrm_policy_lookup =		selinux_xfrm_policy_lookup,
	.xfrm_state_pol_flow_match =	selinux_xfrm_state_pol_flow_match,
	.xfrm_decode_session =		selinux_xfrm_decode_session,
#endif

#ifdef CONFIG_KEYS
	.key_alloc =			selinux_key_alloc,
	.key_free =			selinux_key_free,
	.key_permission =		selinux_key_permission,
	.key_getsecurity =		selinux_key_getsecurity,
#endif

#ifdef CONFIG_AUDIT
	.audit_rule_init =		selinux_audit_rule_init,
	.audit_rule_known =		selinux_audit_rule_known,
	.audit_rule_match =		selinux_audit_rule_match,
	.audit_rule_free =		selinux_audit_rule_free,
#endif
};

static __init int selinux_init(void)
{
	if (!security_module_enable(&selinux_ops)) {
		selinux_enabled = 0;
		return 0;
	}

	if (!selinux_enabled) {
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_INFO "SELinux:  Disabled at boot.\n");
#else
		;
#endif
		return 0;
	}

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "SELinux:  Initializing.\n");
#else
	;
#endif

	/* Set the security state for the initial task. */
	cred_init_security();

	default_noexec = !(VM_DATA_DEFAULT_FLAGS & VM_EXEC);

	sel_inode_cache = kmem_cache_create("selinux_inode_security",
					    sizeof(struct inode_security_struct),
					    0, SLAB_PANIC, NULL);
	//avc_init();

	if (register_security(&selinux_ops))
		panic("SELinux: Unable to register with kernel.\n");

	if (selinux_enforcing)
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "SELinux:  Starting in enforcing mode\n");
#else
		;
#endif
	else
#ifdef CONFIG_DEBUG_PRINTK
		printk(KERN_DEBUG "SELinux:  Starting in permissive mode\n");
#else
		;
#endif

	return 0;
}

static void delayed_superblock_init(struct super_block *sb, void *unused)
{
	superblock_doinit(sb, NULL);
}

void selinux_complete_init(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "SELinux:  Completing initialization.\n");
#else
	;
#endif

	/* Set up any superblocks initialized prior to the policy load. */
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "SELinux:  Setting up existing superblocks.\n");
#else
	;
#endif
	iterate_supers(delayed_superblock_init, NULL);
}

/* SELinux requires early initialization in order to label
   all processes and objects when they are created. */
security_initcall(selinux_init);

#if defined(CONFIG_NETFILTER)

static struct nf_hook_ops selinux_ipv4_ops[] = {
	{
		.hook =		selinux_ipv4_postroute,
		.owner =	THIS_MODULE,
		.pf =		PF_INET,
		.hooknum =	NF_INET_POST_ROUTING,
		.priority =	NF_IP_PRI_SELINUX_LAST,
	},
	{
		.hook =		selinux_ipv4_forward,
		.owner =	THIS_MODULE,
		.pf =		PF_INET,
		.hooknum =	NF_INET_FORWARD,
		.priority =	NF_IP_PRI_SELINUX_FIRST,
	},
	{
		.hook =		selinux_ipv4_output,
		.owner =	THIS_MODULE,
		.pf =		PF_INET,
		.hooknum =	NF_INET_LOCAL_OUT,
		.priority =	NF_IP_PRI_SELINUX_FIRST,
	}
};

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)

static struct nf_hook_ops selinux_ipv6_ops[] = {
	{
		.hook =		selinux_ipv6_postroute,
		.owner =	THIS_MODULE,
		.pf =		PF_INET6,
		.hooknum =	NF_INET_POST_ROUTING,
		.priority =	NF_IP6_PRI_SELINUX_LAST,
	},
	{
		.hook =		selinux_ipv6_forward,
		.owner =	THIS_MODULE,
		.pf =		PF_INET6,
		.hooknum =	NF_INET_FORWARD,
		.priority =	NF_IP6_PRI_SELINUX_FIRST,
	}
};

#endif	/* IPV6 */

static int __init selinux_nf_ip_init(void)
{
	int err = 0;

	if (!selinux_enabled)
		goto out;

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "SELinux:  Registering netfilter hooks\n");
#else
	;
#endif

	err = nf_register_hooks(selinux_ipv4_ops, ARRAY_SIZE(selinux_ipv4_ops));
	if (err)
		panic("SELinux: nf_register_hooks for IPv4: error %d\n", err);

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	err = nf_register_hooks(selinux_ipv6_ops, ARRAY_SIZE(selinux_ipv6_ops));
	if (err)
		panic("SELinux: nf_register_hooks for IPv6: error %d\n", err);
#endif	/* IPV6 */

out:
	return err;
}

__initcall(selinux_nf_ip_init);

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
static void selinux_nf_ip_exit(void)
{
#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_DEBUG "SELinux:  Unregistering netfilter hooks\n");
#else
	;
#endif

	nf_unregister_hooks(selinux_ipv4_ops, ARRAY_SIZE(selinux_ipv4_ops));
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	nf_unregister_hooks(selinux_ipv6_ops, ARRAY_SIZE(selinux_ipv6_ops));
#endif	/* IPV6 */
}
#endif

#else /* CONFIG_NETFILTER */

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
#define selinux_nf_ip_exit()
#endif

#endif /* CONFIG_NETFILTER */

#ifdef CONFIG_SECURITY_SELINUX_DISABLE
static int selinux_disabled;

int selinux_disable(void)
{
#if 0
	if (ss_initialized) {
		/* Not permitted after initial policy load. */
		return -EINVAL;
	}
#endif
	if (selinux_disabled) {
		/* Only do this once. */
		return -EINVAL;
	}

#ifdef CONFIG_DEBUG_PRINTK
	printk(KERN_INFO "SELinux:  Disabled at runtime.\n");
#else
	;
#endif

	selinux_disabled = 1;
	selinux_enabled = 0;

	reset_security_ops();

	/* Try to destroy the avc node cache */
	avc_disable();

	/* Unregister netfilter hooks. */
	selinux_nf_ip_exit();

	/* Unregister selinuxfs. */
	exit_sel_fs();

	return 0;
}
#endif

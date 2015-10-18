#ifdef CONFIG_GOD_MODE
#include <linux/god_mode.h>
#endif
#ifndef OCFS2_MMAP_H
#define OCFS2_MMAP_H

int ocfs2_mmap(struct file *file, struct vm_area_struct *vma);

#endif  /* OCFS2_MMAP_H */

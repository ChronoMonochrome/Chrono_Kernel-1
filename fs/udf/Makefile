#
# Makefile for the linux udf-filesystem routines.
#

CFLAGS_MODULE += $(DISABLE_LTO)
CFLAGS_namei.o += -fno-strict-aliasing
CFLAGS_namei.o += -fno-strict-aliasing
CFLAGS_super.o += -fno-strict-aliasing
CFLAGS_MODULE_super.o += -fno-strict-aliasing

obj-$(CONFIG_UDF_FS) += udf.o

udf-objs     := balloc.o dir.o file.o ialloc.o inode.o lowlevel.o namei.o \
		partition.o super.o truncate.o symlink.o \
		directory.o misc.o udftime.o unicode.o

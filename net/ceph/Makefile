#
# Makefile for CEPH filesystem.
#
obj-$(CONFIG_CEPH_LIB) += libceph.o

libceph-y := ceph_common.o messenger.o msgpool.o buffer.o pagelist.o \
	mon_client.o \
	osd_client.o osdmap.o crush/crush.o crush/mapper.o crush/hash.o \
	debugfs.o \
	auth.o auth_none.o \
	crypto.o armor.o \
	auth_x.o \
	ceph_fs.o ceph_strings.o ceph_hash.o \
	pagevec.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


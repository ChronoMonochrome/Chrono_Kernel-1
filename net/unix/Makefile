#
# Makefile for the Linux unix domain socket layer.
#

obj-$(CONFIG_UNIX)	+= unix.o

unix-y			:= af_unix.o garbage.o
unix-$(CONFIG_SYSCTL)	+= sysctl_net_unix.o

obj-$(CONFIG_UNIX_DIAG)	+= unix_diag.o
unix_diag-y		:= diag.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)

#
# Makefile for the Linux NET/ROM layer.
#

obj-$(CONFIG_NETROM) += netrom.o

netrom-y		:= af_netrom.o nr_dev.o nr_in.o nr_loopback.o \
			   nr_out.o nr_route.o nr_subr.o nr_timer.o
netrom-$(CONFIG_SYSCTL)	+= sysctl_net_netrom.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


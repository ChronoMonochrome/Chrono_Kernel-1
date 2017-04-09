#
# Makefile for PTP 1588 clock support.
#

ptp-y					:= ptp_clock.o ptp_chardev.o ptp_sysfs.o
obj-$(CONFIG_PTP_1588_CLOCK)		+= ptp.o
obj-$(CONFIG_PTP_1588_CLOCK_IXP46X)	+= ptp_ixp46x.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


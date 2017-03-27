#
#  Makefile for the Linux Controller Area Network core.
#

obj-$(CONFIG_CAN)	+= can.o
can-y			:= af_can.o proc.o

obj-$(CONFIG_CAN_RAW)	+= can-raw.o
can-raw-y		:= raw.o

obj-$(CONFIG_CAN_BCM)	+= can-bcm.o
can-bcm-y		:= bcm.o

obj-$(CONFIG_CAN_GW)	+= can-gw.o
can-gw-y		:= gw.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)

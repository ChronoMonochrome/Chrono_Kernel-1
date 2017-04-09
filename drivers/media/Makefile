#
# Makefile for the kernel multimedia device drivers.
#

media-objs	:= media-device.o media-devnode.o media-entity.o

ifeq ($(CONFIG_MEDIA_CONTROLLER),y)
  obj-$(CONFIG_MEDIA_SUPPORT) += media.o
endif

obj-y += common/ rc/ video/

obj-$(CONFIG_VIDEO_DEV) += radio/
obj-$(CONFIG_DVB_CORE)  += dvb/
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


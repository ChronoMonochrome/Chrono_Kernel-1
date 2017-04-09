#
# Makefile for the kernel Sensor device drivers.
#

# Object files in subdirectories

obj-$(CONFIG_PHONE_MODEMCTL)	+= modemctl/
obj-$(CONFIG_PHONE_ONEDRAM)		+= onedram/
obj-$(CONFIG_PHONE_SVNET)		+= svnet/
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


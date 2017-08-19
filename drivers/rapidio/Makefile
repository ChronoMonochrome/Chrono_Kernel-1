#
# Makefile for RapidIO interconnect services
#
obj-y += rio.o rio-access.o rio-driver.o rio-scan.o rio-sysfs.o

obj-$(CONFIG_RAPIDIO)		+= switches/

subdir-ccflags-$(CONFIG_RAPIDIO_DEBUG) := -DDEBUG
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


#
# Makefile for the linux kernel.
#
obj-$(CONFIG_DMI)		+= dmi_scan.o
obj-$(CONFIG_DMI_SYSFS)		+= dmi-sysfs.o
obj-$(CONFIG_EDD)		+= edd.o
obj-$(CONFIG_EFI_VARS)		+= efivars.o
obj-$(CONFIG_EFI_PCDP)		+= pcdp.o
obj-$(CONFIG_DELL_RBU)          += dell_rbu.o
obj-$(CONFIG_DCDBAS)		+= dcdbas.o
obj-$(CONFIG_DMIID)		+= dmi-id.o
obj-$(CONFIG_ISCSI_IBFT_FIND)	+= iscsi_ibft_find.o
obj-$(CONFIG_ISCSI_IBFT)	+= iscsi_ibft.o
obj-$(CONFIG_FIRMWARE_MEMMAP)	+= memmap.o
obj-$(CONFIG_SIGMA)		+= sigma.o

obj-$(CONFIG_GOOGLE_FIRMWARE)	+= google/
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


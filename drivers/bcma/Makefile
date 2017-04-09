bcma-y					+= main.o scan.o core.o
bcma-y					+= driver_chipcommon.o driver_chipcommon_pmu.o
bcma-y					+= driver_pci.o
bcma-$(CONFIG_BCMA_HOST_PCI)		+= host_pci.o
obj-$(CONFIG_BCMA)			+= bcma.o

ccflags-$(CONFIG_BCMA_DEBUG)		:= -DDEBUG
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


# core
ssb-y					+= main.o scan.o
ssb-$(CONFIG_SSB_EMBEDDED)		+= embedded.o
ssb-$(CONFIG_SSB_SPROM)			+= sprom.o

# host support
ssb-$(CONFIG_SSB_PCIHOST)		+= pci.o pcihost_wrapper.o
ssb-$(CONFIG_SSB_PCMCIAHOST)		+= pcmcia.o
ssb-$(CONFIG_SSB_SDIOHOST)		+= sdio.o

# built-in drivers
ssb-y					+= driver_chipcommon.o
ssb-y					+= driver_chipcommon_pmu.o
ssb-$(CONFIG_SSB_DRIVER_MIPS)		+= driver_mipscore.o
ssb-$(CONFIG_SSB_DRIVER_EXTIF)		+= driver_extif.o
ssb-$(CONFIG_SSB_DRIVER_PCICORE)	+= driver_pcicore.o
ssb-$(CONFIG_SSB_DRIVER_GIGE)		+= driver_gige.o

# b43 pci-ssb-bridge driver
# Not strictly a part of SSB, but kept here for convenience
ssb-$(CONFIG_SSB_B43_PCI_BRIDGE)	+= b43_pci_bridge.o

obj-$(CONFIG_SSB)			+= ssb.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


#
# Makefile for the Linux IEEE 1394 implementation
#

firewire-core-y += core-card.o core-cdev.o core-device.o \
                   core-iso.o core-topology.o core-transaction.o
firewire-ohci-y += ohci.o
firewire-sbp2-y += sbp2.o
firewire-net-y  += net.o

obj-$(CONFIG_FIREWIRE)      += firewire-core.o
obj-$(CONFIG_FIREWIRE_OHCI) += firewire-ohci.o
obj-$(CONFIG_FIREWIRE_SBP2) += firewire-sbp2.o
obj-$(CONFIG_FIREWIRE_NET)  += firewire-net.o
obj-$(CONFIG_FIREWIRE_NOSY) += nosy.o
obj-$(CONFIG_PROVIDE_OHCI1394_DMA_INIT) += init_ohci1394_dma.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


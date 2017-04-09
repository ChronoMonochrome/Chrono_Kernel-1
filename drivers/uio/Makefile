obj-$(CONFIG_UIO)	+= uio.o
obj-$(CONFIG_UIO_CIF)	+= uio_cif.o
obj-$(CONFIG_UIO_PDRV)	+= uio_pdrv.o
obj-$(CONFIG_UIO_PDRV_GENIRQ)	+= uio_pdrv_genirq.o
obj-$(CONFIG_UIO_AEC)	+= uio_aec.o
obj-$(CONFIG_UIO_SERCOS3)	+= uio_sercos3.o
obj-$(CONFIG_UIO_PCI_GENERIC)	+= uio_pci_generic.o
obj-$(CONFIG_UIO_NETX)	+= uio_netx.o
obj-$(CONFIG_UIO_PRUSS)         += uio_pruss.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


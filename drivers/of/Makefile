obj-y = base.o
obj-$(CONFIG_OF_FLATTREE) += fdt.o
obj-$(CONFIG_OF_PROMTREE) += pdt.o
obj-$(CONFIG_OF_ADDRESS)  += address.o
obj-$(CONFIG_OF_IRQ)    += irq.o
obj-$(CONFIG_OF_DEVICE) += device.o platform.o
obj-$(CONFIG_OF_GPIO)   += gpio.o
obj-$(CONFIG_OF_I2C)	+= of_i2c.o
obj-$(CONFIG_OF_NET)	+= of_net.o
obj-$(CONFIG_OF_SPI)	+= of_spi.o
obj-$(CONFIG_OF_MDIO)	+= of_mdio.o
obj-$(CONFIG_OF_PCI)	+= of_pci.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


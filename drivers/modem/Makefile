obj-$(CONFIG_MODEM)       := modem_access.o
obj-$(CONFIG_MODEM_U8500) += modem_u8500.o
obj-$(CONFIG_U8500_SHRM)  += shrm/
obj-$(CONFIG_MODEM_M6718) += modem_m6718.o
obj-$(CONFIG_MODEM_M6718_SPI) += m6718_spi/
obj-$(CONFIG_MODEM_U5500_MCDD)  += mcdd.o
obj-$(CONFIG_U8500_KERNEL_CLIENT)	+= u8500_client.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

CFLAGS_modem_access.o += $(AFLAGS_THUMB2)
CFLAGS_modem_u8500.o += $(AFLAGS_THUMB2)

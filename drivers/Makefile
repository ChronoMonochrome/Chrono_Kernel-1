#
# Makefile for the Linux kernel device drivers.
#
# 15 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/gpio/
else
obj-y += gpio/
endif

obj-$(CONFIG_PCI)		+= pci/
obj-$(CONFIG_PARISC)		+= parisc/
obj-$(CONFIG_RAPIDIO)		+= rapidio/

ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/video/
else
obj-y += video/
endif

obj-y				+= idle/
obj-$(CONFIG_ACPI)		+= acpi/
obj-$(CONFIG_SFI)		+= sfi/
# PnP must come after ACPI since it will eventually need to check if acpi
# was used and do nothing if so
obj-$(CONFIG_PNP)		+= pnp/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_ARM_AMBA)		+= ../u8500/drivers/amba/
else
obj-$(CONFIG_ARM_AMBA)		+= amba/
endif

# Many drivers will want to use DMA so this has to be made available
# really early.
ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_DMA_ENGINE) += ../u8500/drivers/dma/
else
obj-$(CONFIG_DMA_ENGINE) += dma/
endif

obj-$(CONFIG_VIRTIO)		+= virtio/
obj-$(CONFIG_XEN)		+= xen/

# regulators early, since some subsystems rely on them to initialize
ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_REGULATOR) += ../u8500/drivers/regulator/
else
obj-$(CONFIG_REGULATOR) += regulator/
endif

# tty/ comes before char/ so that the VT console is the boot-time
# default.
obj-y				+= tty/

ifdef CONFIG_ARCH_U8500
obj-y				+= ../u8500/drivers/tty/
endif

obj-y				+= char/

ifdef CONFIG_ARCH_U8500
obj-y				+= ../u8500/drivers/char/
endif

# gpu/ comes after char for AGP vs DRM startup
ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/gpu/
else
obj-y += gpu/
endif

obj-$(CONFIG_CONNECTOR)		+= connector/

# i810fb and intelfb depend on char/agp/
obj-$(CONFIG_FB_I810)           += video/i810/
obj-$(CONFIG_FB_INTEL)          += video/intelfb/

obj-$(CONFIG_PARPORT)		+= parport/
obj-y				+= base/

ifdef CONFIG_ARCH_U8500
obj-y				+= ../u8500/drivers/base/
endif

obj-y				+= block/

ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/misc/
else
obj-y += misc/
endif

ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/mfd/
else
obj-y += mfd/
endif

obj-y				+= nfc/
obj-$(CONFIG_NUBUS)		+= nubus/
obj-y				+= macintosh/
obj-$(CONFIG_IDE)		+= ide/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_SCSI)		+= ../u8500/drivers/scsi/
else
obj-$(CONFIG_SCSI)		+= drivers/scsi/
endif

obj-$(CONFIG_ATA)		+= ata/
obj-$(CONFIG_TARGET_CORE)	+= target/
obj-$(CONFIG_MTD)		+= mtd/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_SPI) += ../u8500/drivers/spi/
else
obj-$(CONFIG_SPI) += spi/
endif

ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/hsi/
else
obj-y += hsi/
endif

obj-y				+= net/

ifdef CONFIG_ARCH_U8500
obj-y				+= ../u8500/drivers/net/
endif

obj-$(CONFIG_ATM)		+= atm/
obj-$(CONFIG_FUSION)		+= message/
obj-y				+= firewire/
obj-$(CONFIG_UIO)		+= uio/
obj-y				+= cdrom/
obj-y				+= auxdisplay/
obj-$(CONFIG_PCCARD)		+= pcmcia/
obj-$(CONFIG_DIO)		+= dio/
obj-$(CONFIG_SBUS)		+= sbus/
obj-$(CONFIG_ZORRO)		+= zorro/
obj-$(CONFIG_MAC)		+= macintosh/
obj-$(CONFIG_ATA_OVER_ETH)	+= block/aoe/
obj-$(CONFIG_PARIDE) 		+= block/paride/
obj-$(CONFIG_TC)		+= tc/
obj-$(CONFIG_UWB)		+= uwb/
ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_USB_OTG_UTILS)	+= ../u8500/drivers/usb/
obj-$(CONFIG_USB)		+= ../u8500/drivers/usb/
obj-$(CONFIG_PCI)		+= ../u8500/drivers/usb/
obj-$(CONFIG_USB_GADGET)	+= ../u8500/drivers/usb/
else
obj-$(CONFIG_USB_OTG_UTILS)	+= usb/
obj-$(CONFIG_USB)		+= usb/
obj-$(CONFIG_PCI)		+= usb/
obj-$(CONFIG_USB_GADGET)	+= usb/
endif
obj-$(CONFIG_SERIO)		+= input/serio/
obj-$(CONFIG_GAMEPORT)		+= input/gameport/

obj-$(CONFIG_INPUT)		+= input/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_INPUT)		+= ../u8500/drivers/input/
endif

obj-$(CONFIG_I2O)		+= message/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_RTC_LIB) += ../u8500/drivers/rtc/
else
obj-$(CONFIG_RTC_LIB) += rtc/
endif

ifdef CONFIG_ARCH_U8500
obj-y += ../u8500/drivers/i2c/ ../u8500/drivers/media/
else
obj-y += i2c/ media/
endif

obj-$(CONFIG_PPS)		+= pps/
obj-$(CONFIG_PTP_1588_CLOCK)	+= ptp/
obj-$(CONFIG_W1)		+= w1/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_POWER_SUPPLY) += ../u8500/drivers/power/
else
obj-$(CONFIG_POWER_SUPPLY) += power/
endif

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_HWMON) += ../u8500/drivers/hwmon/
else
obj-$(CONFIG_HWMON) += hwmon/
endif

obj-$(CONFIG_THERMAL)		+= thermal/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_WATCHDOG) += ../u8500/drivers/watchdog/
else
obj-$(CONFIG_WATCHDOG) += watchdog/
endif

obj-$(CONFIG_PHONE)		+= telephony/
obj-$(CONFIG_MD)		+= md/
obj-$(CONFIG_BT)		+= bluetooth/
obj-$(CONFIG_ACCESSIBILITY)	+= accessibility/
obj-$(CONFIG_ISDN)		+= isdn/
obj-$(CONFIG_EDAC)		+= edac/
obj-$(CONFIG_MCA)		+= mca/
obj-$(CONFIG_EISA)		+= eisa/
obj-y				+= lguest/
obj-$(CONFIG_CPU_FREQ)		+= cpufreq/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_CPU_IDLE)		+= ../u8500/drivers/cpuidle/
else
obj-$(CONFIG_CPU_IDLE)		+= cpuidle/
endif

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_MMC)		+= ../u8500/drivers/mmc/
else
obj-$(CONFIG_MMC)		+= mmc/
endif

obj-$(CONFIG_MEMSTICK)		+= memstick/
obj-y				+= leds/
obj-$(CONFIG_NEW_SENSORS)	+= sensor/
obj-$(CONFIG_SWITCH)		+= switch/
obj-$(CONFIG_INFINIBAND)	+= infiniband/
obj-$(CONFIG_SGI_SN)		+= sn/
obj-y				+= firmware/
obj-$(CONFIG_CRYPTO)		+= crypto/
obj-$(CONFIG_SUPERH)		+= sh/
obj-$(CONFIG_ARCH_SHMOBILE)	+= sh/
ifndef CONFIG_ARCH_USES_GETTIMEOFFSET

ifdef CONFIG_ARCH_U8500
obj-y				+= ../u8500/drivers/clocksource/
else
obj-y				+= clocksource/
endif

endif
obj-$(CONFIG_DCA)		+= dca/
obj-$(CONFIG_HID)		+= hid/
obj-$(CONFIG_PPC_PS3)		+= ps3/
obj-$(CONFIG_OF)		+= of/
obj-$(CONFIG_SSB)		+= ssb/
obj-$(CONFIG_BCMA)		+= bcma/
obj-$(CONFIG_VHOST_NET)		+= vhost/
obj-$(CONFIG_VLYNQ)		+= vlynq/
obj-$(CONFIG_STAGING)		+= staging/

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_STAGING)		+= ../u8500/drivers/staging/
endif

obj-y				+= platform/
obj-y				+= ieee802154/
#common clk code
ifdef CONFIG_ARCH_U8500
obj-y				+= ../u8500/drivers/clk/
else
obj-y				+= clk/
endif
obj-$(CONFIG_TEE_SUPPORT)	+= tee/
obj-$(CONFIG_MODEM)		+= modem/

obj-$(CONFIG_HWSPINLOCK)	+= hwspinlock/
obj-y				+= samsung/
obj-$(CONFIG_SAMSUNG_PHONE_SVNET)	+= phone_svn/
obj-$(CONFIG_AUTHENTEC_VPNCLIENT_INTERCEPTOR) += interceptor/

obj-$(CONFIG_BATTERY_SAMSUNG)	+= battery/

ifeq (1, $(shell if [ -d ../../../../../../kernel/drivers/exfat ]; then echo 1; fi))
obj-y   += exfat/
endif

ifdef CONFIG_ARCH_U8500
obj-$(CONFIG_BCM4330)	+= ../u8500/drivers/bcmdhd/
obj-$(CONFIG_BCM4334)	+= ../u8500/drivers/bcmdhd/
endif

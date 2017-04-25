#
# Makefile for the Linux kernel device drivers.
#
# 15 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

obj-y				+= gpio/
#obj-$(CONFIG_ARM_AMBA)		+= amba/
# Many drivers will want to use DMA so this has to be made available
# really early.
obj-$(CONFIG_DMA_ENGINE)	+= dma/

# regulators early, since some subsystems rely on them to initialize
#obj-$(CONFIG_REGULATOR)		+= regulator/

# tty/ comes before char/ so that the VT console is the boot-time
# default.
#obj-y				+= tty/
#obj-y				+= char/

# gpu/ comes after char for AGP vs DRM startup
#obj-y				+= gpu/
obj-y				+= base/ misc/
#mfd/

obj-y				+= net/
#obj-$(CONFIG_USB_OTG_UTILS)	+= usb/
#obj-$(CONFIG_USB)		+= usb/
#obj-$(CONFIG_PCI)		+= usb/
#obj-$(CONFIG_USB_GADGET)	+= usb/
#obj-$(CONFIG_INPUT)		+= input/
#obj-$(CONFIG_RTC_LIB)		+= rtc/
#obj-y				+= i2c/
#obj-$(CONFIG_POWER_SUPPLY)	+= power/
#obj-$(CONFIG_HWMON)		+= hwmon/
#obj-$(CONFIG_WATCHDOG)		+= watchdog/
#obj-$(CONFIG_CPU_FREQ)		+= cpufreq/
#obj-$(CONFIG_CPU_IDLE)		+= cpuidle/
#obj-$(CONFIG_UX500_MMC)		+= mmc/
#obj-$(CONFIG_NEW_SENSORS)	+= sensor/
ifndef CONFIG_ARCH_USES_GETTIMEOFFSET
obj-y				+= clocksource/
endif
#obj-$(CONFIG_STAGING)		+= staging/
#common clk code
obj-$(CONFIG_TEE_SUPPORT)	+= tee/
obj-$(CONFIG_MODEM)		+= modem/

obj-y				+= samsung/
obj-$(CONFIG_SAMSUNG_PHONE_SVNET)	+= phone_svn/

#obj-$(CONFIG_BATTERY_SAMSUNG)	+= battery/

#
# Makefile for the linux kernel, U8500 machine.
#

obj-y				:= clock.o cpu.o devices.o \
				   devices-common.o \
				   id.o pins.o usb.o timer.o reboot_reasons.o
obj-y				+= pm/
obj-$(CONFIG_CACHE_L2X0)	+= cache-l2x0.o
ifeq ($(CONFIG_UX500_SOC_DB5500), y)
obj-$(CONFIG_UX500_SOC_DBX500)	+= cpu-db5500.o dma-db5500.o prcmu-db5500.o \
				   devices-db5500.o
endif
ifeq ($(CONFIG_UX500_SOC_DB8500), y)
obj-$(CONFIG_UX500_SOC_DBX500)	+= cpu-db8500.o devices-db8500.o \
				   prcmu-db8500.o clock-db8500.o \
				   regulator-db8500.o
endif

obj-$(CONFIG_MACH_MOP500)	+= board-mop500.o board-mop500-sdi.o \
				board-mop500-regulators.o \
				board-mop500-uib.o board-mop500-stuib.o \
				board-mop500-u8500uib.o board-mop500-pins.o \
				board-mop500-mcde.o \
				board-mop500-msp.o \
				board-pins-sleep-force.o \
				board-mop500-wlan.o
obj-$(CONFIG_MACH_U5500)	+= board-u5500.o board-u5500-sdi.o \
				   board-u5500-pins.o
obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o
obj-$(CONFIG_U5500_MODEM_IRQ)	+= modem-irq-db5500.o
obj-$(CONFIG_U5500_MBOX)	+= mbox-db5500.o
obj-$(CONFIG_TEE_UX500)	+= tee_ux500.o
obj-$(CONFIG_TEE_SVP)		+= tee_service_svp.o
obj-$(CONFIG_TEE_SVP)		+= tee_ta_start_modem_svp.o
obj-$(CONFIG_DB8500_MLOADER) 	+= mloader-db8500.o
obj-$(CONFIG_UX500_L2X0_PREFETCH_CTRL)	+= l2x0-prefetch.o

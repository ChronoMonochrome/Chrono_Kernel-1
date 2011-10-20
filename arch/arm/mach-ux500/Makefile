#
# Makefile for the linux kernel, UX500 machine.
#

obj-y				:= clock.o cpu.o devices.o dcache.o \
				   devices-common.o id.o pins.o \
				   usb.o reboot_reasons.o timer.o \
				   uart-db8500.o clock-debug.o
obj-y				+= pm/
obj-$(CONFIG_CACHE_L2X0)	+= cache-l2x0.o


ifeq ($(CONFIG_UX500_SOC_DB5500), y)
obj-$(CONFIG_UX500_SOC_DBX500)	+= cpu-db5500.o dma-db5500.o \
				   devices-db5500.o clock-db5500.o
obj-$(CONFIG_TOUCHSCREEN_CYTTSP_SPI)    += board-u5500-cyttsp.o
endif
ifeq ($(CONFIG_UX500_SOC_DB8500), y)

obj-$(CONFIG_UX500_SOC_DBX500)	+= cpu-db8500.o devices-db8500.o \
				   clock-db8500.o dma-db8500.o
endif
obj-$(CONFIG_MACH_MOP500)	+= board-mop500.o board-mop500-sdi.o \
				board-mop500-regulators.o \
				board-mop500-uib.o board-mop500-stuib.o \
				board-mop500-u8500uib.o board-mop500-pins.o \
				board-mop500-mcde.o \
				board-mop500-msp.o board-mop500-bm.o \
				board-pins-sleep-force.o \
				board-mop500-wlan.o \
				board-mop500-cyttsp.o
obj-$(CONFIG_SENSORS1P_MOP)	+= sensors1p.o
obj-$(CONFIG_MACH_U5500)	+= board-u5500.o board-u5500-sdi.o \
				   board-u5500-mcde.o \
				   board-u5500-regulators.o \
				   board-u5500-pins.o \
				   board-u5500-wlan.o
obj-$(CONFIG_U5500_MMIO)	+= board-u5500-mmio.o
obj-$(CONFIG_U8500_MMIO)	+= board-mop500-mmio.o
obj-$(CONFIG_SMP)		+= platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)	+= hotplug.o
obj-$(CONFIG_U5500_MODEM_IRQ)	+= modem-irq-db5500.o
obj-$(CONFIG_TEE_UX500) 	+= tee_ux500.o product.o
obj-$(CONFIG_TEE_SVP)		+= tee_service_svp.o
obj-$(CONFIG_TEE_SVP)		+= tee_ta_start_modem_svp.o
obj-$(CONFIG_DB8500_MLOADER) 	+= mloader-db8500.o
obj-$(CONFIG_U5500_MLOADER)	+= mloader-db5500.o
obj-$(CONFIG_HWMEM)			+= hwmem-int.o
obj-$(CONFIG_UX500_L2X0_PREFETCH_CTRL)	+= l2x0-prefetch.o
obj-$(CONFIG_AB5500_BM)			+= board-u5500-bm.o
obj-$(CONFIG_DBX500_PRCMU_DEBUG)	+= prcmu-debug.o

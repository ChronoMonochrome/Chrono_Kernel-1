#
# Makefile for the linux kernel, UX500 machine.
#

obj-y				:= clock.o cpu.o devices.o \
				   devices-common.o id.o pins.o \
				   usb.o reboot_reasons.o timer.o \
				   uart-db8500.o clock-debug.o
obj-y				+= pm/ test/
obj-$(CONFIG_CACHE_L2X0)	+= cache-l2x0.o
obj-$(CONFIG_CPU_IDLE) 		+= cpuidle.o


ifeq ($(CONFIG_UX500_SOC_DB5500), y)
obj-$(CONFIG_UX500_SOC_DBX500)	+= cpu-db5500.o dma-db5500.o \
				   devices-db5500.o clock-db5500.o
board-mcde-objs			+= board-u5500-mcde.o
endif
ifeq ($(CONFIG_UX500_SOC_DB8500), y)
obj-$(CONFIG_UX500_SOC_DBX500)	+= cpu-db8500.o devices-db8500.o \
				   clock-db8500.o dma-db8500.o cpu-db9500.o
board-mcde-objs			+= board-mop500-mcde.o
endif
obj-$(CONFIG_MACH_MOP500)	+= board-mop500.o board-mop500-sdi.o \
				board-mop500-regulators.o \
				board-mop500-uib.o board-mop500-stuib.o \
				board-mop500-u8500uib.o board-mop500-pins.o \
				board-mop500-bm.o \
				board-pins-sleep-force.o
obj-$(CONFIG_ANDROID_STE_TIMED_VIBRA)	+= board-mop500-vibra.o
obj-$(CONFIG_MACH_U5500)	+= board-u5500.o board-u5500-sdi.o \
				   board-u5500-regulators.o \
				   board-u5500-pins.o
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
obj-$(CONFIG_UX500_DEBUG_HWREG) 	+= hwreg.o
obj-$(CONFIG_HWMEM)			+= hwmem-int.o
obj-$(CONFIG_UX500_L2X0_PREFETCH_CTRL)	+= l2x0-prefetch.o
obj-$(CONFIG_AB5500_BM)			+= board-u5500-bm.o
obj-$(CONFIG_DBX500_PRCMU_DEBUG)	+= prcmu-debug.o
obj-$(CONFIG_UX500_DB_DUMP)		+= dbx500_dump.o

obj-$(CONFIG_HWMEM) 		+= dcache.o

ifdef CONFIG_STM_TRACE
obj-$(CONFIG_MACH_MOP500)	+= board-mop500-stm.o
endif
ifdef CONFIG_SENSORS_LSM303DLH
obj-$(CONFIG_MACH_MOP500)	+= board-mop500-sensors.o
endif
obj-$(CONFIG_FB_MCDE)		+= board-mcde.o
ifdef CONFIG_STM_MSP_SPI
obj-$(CONFIG_MACH_MOP500) 	+= board-mop500-msp.o
endif
ifdef CONFIG_CW1200
obj-$(CONFIG_MACH_MOP500) 	+= board-mop500-wlan.o
obj-$(CONFIG_MACH_U5500)	+= board-u5500-wlan.o
endif
ifdef CONFIG_TOUCHSCREEN_CYTTSP_SPI
obj-$(CONFIG_MACH_MOP500) 	+= board-mop500-cyttsp.o
obj-$(CONFIG_MACH_U5500)	+= board-u5500-cyttsp.o
endif

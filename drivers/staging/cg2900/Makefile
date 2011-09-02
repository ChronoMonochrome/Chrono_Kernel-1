#
# Makefile for ST-Ericsson CG2900 connectivity combo controller
#

ccflags-y :=					\
	-Idrivers/staging/cg2900/include \
	-Iarch/arm/mach-ux500

obj-$(CONFIG_CG2900)	+= devices-cg2900.o
ifeq ($(CONFIG_UX500_SOC_DB8500), y)
obj-$(CONFIG_CG2900)		+= devices-cg2900-u8500.o
obj-$(CONFIG_CG2900)	+= board-mop500-cg2900.o
endif
ifeq ($(CONFIG_UX500_SOC_DB5500), y)
obj-$(CONFIG_CG2900)		+= devices-cg2900-u5500.o
obj-$(CONFIG_CG2900)	+= board-u5500-cg2900.o
endif

obj-y				+= mfd/
obj-y				+= bluetooth/

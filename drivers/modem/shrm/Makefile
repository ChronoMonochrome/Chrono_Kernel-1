#
# Makefile for SHRM drivers
#

CFLAGS_shrm_protocol.o += -fno-strict-aliasing

ifdef CONFIG_PHONET
u8500_shrm-objs := 	modem_shrm_driver.o shrm_fifo.o shrm_protocol.o
else
u8500_shrm-objs := 	shrm_driver.o shrm_fifo.o shrm_protocol.o
endif

obj-$(CONFIG_U8500_SHRM)	+= u8500_shrm.o

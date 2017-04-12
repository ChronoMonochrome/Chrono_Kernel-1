#
# Makefile for SHRM drivers
#

ifdef CONFIG_PHONET
u8500_shrm-objs := 	modem_shrm_driver.o shrm_fifo.o shrm_protocol.o
else
u8500_shrm-objs := 	shrm_driver.o shrm_fifo.o shrm_protocol.o
endif

obj-$(CONFIG_U8500_SHRM)	+= u8500_shrm.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

CFLAGS_fifo.o += $(AFLAGS_THUMB2)
CFLAGS_modem_shrm_driver.o += $(AFLAGS_THUMB2)

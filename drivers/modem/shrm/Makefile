#
# Makefile for SHRM drivers
#

ifdef CONFIG_PHONET
#u8500_shrm-objs := 	modem_shrm_driver.o shrm_fifo.o shrm_protocol.o test.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

AFLAGS_modem_shrm_driver.o += $(AFLAGS_THUMB2)
CFLAGS_modem_shrm_driver.o += $(CFLAGS_THUMB2)

AFLAGS_shrm_fifo.o += $(AFLAGS_THUMB2)
CFLAGS_shrm_fifo.o += $(CFLAGS_THUMB2)

AFLAGS_shrm_protocol.o += $(AFLAGS_THUMB2)
CFLAGS_shrm_protocol.o += $(CFLAGS_THUMB2)

else
u8500_shrm-objs := 	shrm_driver.o shrm_fifo.o shrm_protocol.o
endif

#obj-$(CONFIG_U8500_SHRM) += test.o

u8500_shrm.o: $(src)/u8500_shrm.o
	cp $(src)/u8500_shrm.o $(obj)/u8500_shrm.o

obj-$(CONFIG_U8500_SHRM)	+= u8500_shrm.o


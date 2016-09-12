#
# Makefile for the SuperH specific drivers.
#
obj-y	:= intc/

obj-$(CONFIG_HAVE_CLK)		+= clk/
obj-$(CONFIG_MAPLE)		+= maple/
obj-$(CONFIG_SUPERHYWAY)	+= superhyway/
obj-$(CONFIG_GENERIC_GPIO)	+= pfc.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


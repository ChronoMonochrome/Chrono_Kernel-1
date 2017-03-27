#
# Makefile for the Linux IrDA protocol layer.
#

obj-$(CONFIG_IRDA) += irda.o
obj-$(CONFIG_IRLAN) += irlan/
obj-$(CONFIG_IRNET) += irnet/
obj-$(CONFIG_IRCOMM) += ircomm/

irda-y := iriap.o iriap_event.o irlmp.o irlmp_event.o irlmp_frame.o \
          irlap.o irlap_event.o irlap_frame.o timer.o qos.o irqueue.o \
          irttp.o irda_device.o irias_object.o wrapper.o af_irda.o \
	  discovery.o parameters.o irnetlink.o irmod.o
irda-$(CONFIG_PROC_FS) += irproc.o
irda-$(CONFIG_SYSCTL) += irsysctl.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


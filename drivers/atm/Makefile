#
# Makefile for the Linux network (ATM) device drivers.
#

fore_200e-y	:= fore200e.o

obj-$(CONFIG_ATM_ZATM)		+= zatm.o uPD98402.o
obj-$(CONFIG_ATM_NICSTAR)	+= nicstar.o
obj-$(CONFIG_ATM_AMBASSADOR)	+= ambassador.o
obj-$(CONFIG_ATM_HORIZON)	+= horizon.o
obj-$(CONFIG_ATM_IA)		+= iphase.o suni.o
obj-$(CONFIG_ATM_FORE200E)	+= fore_200e.o
obj-$(CONFIG_ATM_ENI)		+= eni.o suni.o
obj-$(CONFIG_ATM_IDT77252)	+= idt77252.o
obj-$(CONFIG_ATM_SOLOS)		+= solos-pci.o

ifeq ($(CONFIG_ATM_NICSTAR_USE_SUNI),y)
  obj-$(CONFIG_ATM_NICSTAR)	+= suni.o
endif
ifeq ($(CONFIG_ATM_NICSTAR_USE_IDT77105),y)
  obj-$(CONFIG_ATM_NICSTAR)	+= idt77105.o
endif
ifeq ($(CONFIG_ATM_IDT77252_USE_SUNI),y)
  obj-$(CONFIG_ATM_IDT77252)	+= suni.o
endif

obj-$(CONFIG_ATM_DUMMY)		+= adummy.o
obj-$(CONFIG_ATM_TCP)		+= atmtcp.o
obj-$(CONFIG_ATM_FIRESTREAM)	+= firestream.o
obj-$(CONFIG_ATM_LANAI)		+= lanai.o

obj-$(CONFIG_ATM_HE)		+= he.o
ifeq ($(CONFIG_ATM_HE_USE_SUNI),y)
  obj-$(CONFIG_ATM_HE)		+= suni.o
endif
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


#
# Makefile for the Linux 802.x protocol layers.
#

# Check the p8022 selections against net/core/Makefile.
obj-$(CONFIG_LLC)	+= p8022.o psnap.o
obj-$(CONFIG_TR)	+= p8022.o psnap.o tr.o
obj-$(CONFIG_NET_FC)	+=                 fc.o
obj-$(CONFIG_FDDI)	+=                 fddi.o
obj-$(CONFIG_HIPPI)	+=                 hippi.o
obj-$(CONFIG_IPX)	+= p8022.o psnap.o p8023.o
obj-$(CONFIG_ATALK)	+= p8022.o psnap.o
obj-$(CONFIG_STP)	+= stp.o
obj-$(CONFIG_GARP)	+= garp.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


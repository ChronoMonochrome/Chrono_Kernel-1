#
# Makefile for the Linux Plug-and-Play Support.
#

obj-y		:= pnp.o

pnp-y		:= core.o card.o driver.o resource.o manager.o support.o interface.o quirks.o

obj-$(CONFIG_PNPACPI)		+= pnpacpi/
obj-$(CONFIG_PNPBIOS)		+= pnpbios/
obj-$(CONFIG_ISAPNP)		+= isapnp/

# pnp_system_init goes after pnpacpi/pnpbios init
pnp-y				+= system.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


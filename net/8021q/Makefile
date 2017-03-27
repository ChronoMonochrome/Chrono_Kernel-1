#
# Makefile for the Linux VLAN layer.
#
obj-$(subst m,y,$(CONFIG_VLAN_8021Q))	+= vlan_core.o
obj-$(CONFIG_VLAN_8021Q)		+= 8021q.o

8021q-y					:= vlan.o vlan_dev.o vlan_netlink.o
8021q-$(CONFIG_VLAN_8021Q_GVRP)		+= vlan_gvrp.o
8021q-$(CONFIG_PROC_FS)			+= vlanproc.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


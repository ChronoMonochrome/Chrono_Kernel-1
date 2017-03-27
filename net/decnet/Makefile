
obj-$(CONFIG_DECNET) += decnet.o

decnet-y := af_decnet.o dn_nsp_in.o dn_nsp_out.o \
	    dn_route.o dn_dev.o dn_neigh.o dn_timer.o
decnet-$(CONFIG_DECNET_ROUTER) += dn_fib.o dn_rules.o dn_table.o
decnet-y += sysctl_net_decnet.o

obj-$(CONFIG_NETFILTER) += netfilter/

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


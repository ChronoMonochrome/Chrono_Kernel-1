<<<<<<< HEAD
obj-$(CONFIG_IEEE802154) += ieee802154.o af_802154.o
obj-$(CONFIG_IEEE802154_6LOWPAN) += 6lowpan.o

ieee802154-y := netlink.o nl-mac.o nl-phy.o nl_policy.o wpan-class.o
af_802154-y := af_ieee802154.o raw.o dgram.o
=======
obj-$(CONFIG_IEEE802154) +=	ieee802154.o af_802154.o
ieee802154-y		:= netlink.o nl-mac.o nl-phy.o nl_policy.o wpan-class.o
af_802154-y		:= af_ieee802154.o raw.o dgram.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)

>>>>>>> 697ef77... Thumb2: squashed changes

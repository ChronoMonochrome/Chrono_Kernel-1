# tagging formats
obj-$(CONFIG_NET_DSA_TAG_DSA) += tag_dsa.o
obj-$(CONFIG_NET_DSA_TAG_EDSA) += tag_edsa.o
obj-$(CONFIG_NET_DSA_TAG_TRAILER) += tag_trailer.o

# switch drivers
obj-$(CONFIG_NET_DSA_MV88E6XXX) += mv88e6xxx.o
obj-$(CONFIG_NET_DSA_MV88E6060) += mv88e6060.o
obj-$(CONFIG_NET_DSA_MV88E6123_61_65) += mv88e6123_61_65.o
obj-$(CONFIG_NET_DSA_MV88E6131) += mv88e6131.o

# the core
obj-$(CONFIG_NET_DSA) += dsa.o slave.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


obj-$(CONFIG_PS3_VUART) += ps3-vuart.o
obj-$(CONFIG_PS3_PS3AV) += ps3av_mod.o
ps3av_mod-y		:= ps3av.o ps3av_cmd.o
obj-$(CONFIG_PPC_PS3) += sys-manager-core.o
obj-$(CONFIG_PS3_SYS_MANAGER) += ps3-sys-manager.o
obj-$(CONFIG_PS3_STORAGE) += ps3stor_lib.o
obj-$(CONFIG_PS3_LPM) += ps3-lpm.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


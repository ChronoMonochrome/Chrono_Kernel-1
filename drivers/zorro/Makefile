#
# Makefile for the Zorro bus specific drivers.
#

obj-$(CONFIG_ZORRO)	+= zorro.o zorro-driver.o zorro-sysfs.o names.o
obj-$(CONFIG_PROC_FS)	+= proc.o

hostprogs-y 		:= gen-devlist

# Files generated that shall be removed upon make clean
clean-files := devlist.h

# Dependencies on generated files need to be listed explicitly
$(obj)/names.o: $(obj)/devlist.h

# And that's how to generate them
quiet_cmd_devlist = DEVLIST $@
      cmd_devlist = ( cd $(obj); ./gen-devlist ) < $<
$(obj)/devlist.h: $(src)/zorro.ids $(obj)/gen-devlist
	$(call cmd,devlist)

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


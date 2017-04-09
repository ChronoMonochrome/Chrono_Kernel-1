obj-$(CONFIG_UWB)		+= uwb.o
obj-$(CONFIG_UWB_WHCI)		+= umc.o whci.o whc-rc.o
obj-$(CONFIG_UWB_HWA)		+= hwa-rc.o
obj-$(CONFIG_UWB_I1480U)	+= i1480/

uwb-objs :=		\
	address.o	\
	allocator.o	\
	beacon.o	\
	driver.o	\
	drp.o		\
	drp-avail.o	\
	drp-ie.o	\
	est.o		\
	ie.o		\
	ie-rcv.o	\
	lc-dev.o	\
	lc-rc.o		\
	neh.o		\
	pal.o		\
	radio.o		\
	reset.o		\
	rsv.o		\
	scan.o		\
	uwb-debug.o	\
	uwbd.o

umc-objs :=		\
	umc-bus.o	\
	umc-dev.o	\
	umc-drv.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


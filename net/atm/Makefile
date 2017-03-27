#
# Makefile for the ATM Protocol Families.
#

atm-y		:= addr.o pvc.o signaling.o svc.o ioctl.o common.o atm_misc.o raw.o resources.o atm_sysfs.o
mpoa-objs	:= mpc.o mpoa_caches.o mpoa_proc.o

obj-$(CONFIG_ATM) += atm.o
obj-$(CONFIG_ATM_CLIP) += clip.o
obj-$(CONFIG_ATM_BR2684) += br2684.o
atm-$(CONFIG_PROC_FS) += proc.o

obj-$(CONFIG_ATM_LANE) += lec.o
obj-$(CONFIG_ATM_MPOA) += mpoa.o
obj-$(CONFIG_PPPOATM) += pppoatm.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


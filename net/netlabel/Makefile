#
# Makefile for the NetLabel subsystem.
#

# base objects
obj-y	:= netlabel_user.o netlabel_kapi.o
obj-y	+= netlabel_domainhash.o netlabel_addrlist.o

# management objects
obj-y	+= netlabel_mgmt.o

# protocol modules
obj-y	+= netlabel_unlabeled.o
obj-y	+= netlabel_cipso_v4.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


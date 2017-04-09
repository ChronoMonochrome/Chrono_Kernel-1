#
# Copyright (C) ST-Ericsson SA 2010
# Author: Martin Hovang (martin.xm.hovang@stericsson.com)
# License terms: GNU General Public License (GPL) version 2
#

obj-$(CONFIG_TEE_SUPPORT) += tee_service.o
obj-$(CONFIG_TEE_SUPPORT) += tee_driver.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


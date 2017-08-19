#
# Makefile for the kernel Sensor device drivers.
#

# Object files in subdirectories

obj-$(CONFIG_SENSORS_ALPS)		+= alps/
obj-$(CONFIG_PROXIMITY)			+= proximity/
obj-$(CONFIG_ORIENTATION)		+= orientation/
obj-$(CONFIG_MPU_SENSORS_MPU3050)	+= mpu3050/
obj-$(CONFIG_COMPASS)			+= compass/
obj-$(CONFIG_ORIENTATION)		+= orientation/
obj-$(CONFIG_SENSORS_CORE)		+= sensors_core.o
#obj-$(CONFIG_SENSORS_MMC328X) += mpu3050/compass/mmc328x.o
#obj-$(CONFIG_ECOMPASS) +=mpu3050/compass/mecs.o

AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)


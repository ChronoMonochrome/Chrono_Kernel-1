#
# Makefile for the kernel MemoryStick device drivers.
#

subdir-ccflags-$(CONFIG_MEMSTICK_DEBUG) := -DDEBUG

obj-$(CONFIG_MEMSTICK)		+= core/
obj-$(CONFIG_MEMSTICK)		+= host/


include $(srctree)/scripts/Makefile_build.thumb2

#
# Makefile for cpuidle.
#

obj-y += cpuidle.o driver.o governor.o sysfs.o governors/

obj-$(CONFIG_DBX500_CPUIDLE_DEBUG) 	+= cpuidle-dbx500_dbg.o
#KBUILD_CFLAGS += -DDEBUG

include $(srctree)/scripts/Makefile_build.thumb2

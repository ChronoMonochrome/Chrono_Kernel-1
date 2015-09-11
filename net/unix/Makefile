#
# Makefile for the Linux unix domain socket layer.
#

KBUILD_CFLAGS += -fno-strict-aliasing

obj-$(CONFIG_UNIX)	+= unix.o

unix-y			:= af_unix.o garbage.o
unix-$(CONFIG_SYSCTL)	+= sysctl_net_unix.o

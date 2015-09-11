#
# Makefile for the Linux Ethernet layer.
#

KBUILD_CFLAGS += -fno-strict-aliasing
CFLAGS_MODULE += -fno-strict-aliasing

obj-y					+= eth.o
obj-$(subst m,y,$(CONFIG_IPX))		+= pe2.o
obj-$(subst m,y,$(CONFIG_ATALK))	+= pe2.o

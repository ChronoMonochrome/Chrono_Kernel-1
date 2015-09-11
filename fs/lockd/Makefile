#
# Makefile for the linux lock manager stuff
#

CFLAGS_MODULE += $(DISABLE_LTO)
CFLAGS_svclock.o += -fno-strict-aliasing
CFLAGS_MODULE_svclock.o += -fno-strict-aliasing

obj-$(CONFIG_LOCKD) += lockd.o

lockd-objs-y := clntlock.o clntproc.o clntxdr.o host.o svc.o svclock.o \
	        svcshare.o svcproc.o svcsubs.o mon.o xdr.o grace.o
lockd-objs-$(CONFIG_LOCKD_V4) += clnt4xdr.o xdr4.o svc4proc.o
lockd-objs		      := $(lockd-objs-y)

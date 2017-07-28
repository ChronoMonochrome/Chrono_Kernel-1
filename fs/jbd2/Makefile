#
# Makefile for the linux journaling routines.
#

obj-$(CONFIG_JBD2) += jbd2.o

jbd2-objs := transaction.o commit.o recovery.o checkpoint.o revoke.o journal.o
include $(srctree)/scripts/Makefile_build.thumb2

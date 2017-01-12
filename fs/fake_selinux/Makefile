#
# Makefile for the Linux fat filesystem support.
#

obj-m += fake_selinux.o
fake_selinux-y :=  selinuxfs.o status.o services.o

#obj-$(CONFIG_VFAT_FS) += vfat.o
#obj-$(CONFIG_MSDOS_FS) += msdos.o
#fat-y := cache.o dir.o fatent.o file.o inode.o misc.o
#vfat-y := namei_vfat.o
#msdos-y := namei_msdos.o

ccflags-m := -Ifs/fake_selinux/selinux -DSELINUX_MODULE
ccflags-y := -Ifs/fake_selinux/selinux

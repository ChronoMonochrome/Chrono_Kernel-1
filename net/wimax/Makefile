
obj-$(CONFIG_WIMAX)		+= wimax.o

wimax-y :=		\
	id-table.o	\
	op-msg.o	\
	op-reset.o	\
	op-rfkill.o	\
	op-state-get.o	\
	stack.o

wimax-$(CONFIG_DEBUG_FS) += debugfs.o



include $(srctree)/scripts/Makefile_build.thumb2

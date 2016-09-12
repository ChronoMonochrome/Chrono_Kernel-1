#
# linux/arch/arm/obj/Makefile
#
# Copyright (C) 1995-2000 Russell King
#
lib-y := dummy.o
# memcpy-lib.o memmove-lib.o

obj-y		:= backtrace.o changebit.o csumipv6.o csumpartial.o   \
		   csumpartialcopy.o csumpartialcopyuser.o clearbit.o \
		   delay.o findbit.o memchr.o

# Build memcpy and memmove only if GLIBC versions are not used
#ifneq ($(CONFIG_ARM_USE_GLIBC_MEMCOPY),y)
#  obj-y		+= memcpy.o memmove.o
#endif

obj-y		+= memset.o memzero.o setbit.o                        \
		   strchr.o strrchr.o                                 \
		   testchangebit.o testclearbit.o testsetbit.o        \
		   ashldi3.o ashrdi3.o lshrdi3.o muldi3.o             \
		   ucmpdi2.o lib1funcs.o div64.o                       \
		   io-readsb.o io-writesb.o io-readsl.o io-writesl.o

mmu-y	:= clear_user.o copy_page.o getuser.o putuser.o

# the code in uaccess.S is not preemption safe and
# probably faster on ARMv3 only
ifeq ($(CONFIG_PREEMPT),y)
  mmu-y	+= copy_from_user.o copy_to_user.o
else
ifneq ($(CONFIG_CPU_32v3),y)
  mmu-y	+= copy_from_user.o copy_to_user.o
else
  mmu-y	+= uaccess.o
endif
endif

# using obj_ here won't override already available weak symbols
obj-$(CONFIG_UACCESS_WITH_MEMCPY) += uaccess_with_memcpy.o

obj-$(CONFIG_MMU) += $(mmu-y)

ifeq ($(CONFIG_CPU_32v3),y)
  obj-y	+= io-readsw-armv3.o io-writesw-armv3.o
else
  obj-y	+= io-readsw-armv4.o io-writesw-armv4.o
endif

obj-$(CONFIG_ARCH_RPC)		+= ecard.o io-acorn.o floppydma.o
obj-$(CONFIG_ARCH_SHARK)	+= io-shark.o

$(obj)/csumpartialcopy.o:	$(obj)/csumpartialcopygeneric.S
$(obj)/csumpartialcopyuser.o:	$(obj)/csumpartialcopygeneric.S

ifeq ($(CONFIG_KERNEL_MODE_NEON),y)
  NEON_FLAGS			:= -mfloat-abi=softfp -mfpu=neon
  CFLAGS_xor-neon.o		+= $(NEON_FLAGS)
  obj-$(CONFIG_XOR_BLOCKS)	+= xor-neon.o
endif

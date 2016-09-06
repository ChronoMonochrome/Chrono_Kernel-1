obj-$(CONFIG_DBX500_CONTEXT)            += context_arm.o

KBUILD_AFLAGS := -D__ASSEMBLY__  -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -mno-unaligned-access -D__LINUX_ARM_ARCH__=7 -mcpu=cortex-a9

#
# Power save related files
#
obj-y := pm.o runtime.o
obj-y += context/

obj-$(CONFIG_DBX500_PRCMU_QOS_POWER)	+= prcmu-qos-power.o
obj-$(CONFIG_DBX500_CONTEXT) 		+= context.o context-db8500.o
obj-$(CONFIG_DBX500_CPUIDLE) 		+= timer.o
obj-$(CONFIG_UX500_SUSPEND)		+= suspend.o
obj-$(CONFIG_UX500_SUSPEND_DBG)		+= suspend_dbg.o
obj-$(CONFIG_UX500_PM_PERFORMANCE)	+= performance.o
obj-$(CONFIG_UX500_USECASE_GOVERNOR)	+= usecase_gov.o

#KBUILD_CFLAGS := -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -mno-unaligned-access -D__LINUX_ARM_ARCH__=7 -mcpu=cortex-a9
#AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=arm,-Wa$(comma)-mauto-it)
#AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
#CFLAGS_ARM   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
#AFLAGS_ARM   :=$(CFLAGS_ARM) -Wa$(comma)-marm


#$(warning AFLAGS_context_arm.o $(AFLAGS_context_arm.o))
#AFLAGS_context_arm.o += -marm -Wa,-mimplicit-it=arm -Wa,-mwarn-deprecated
#-Wa,-marm
#AFLAGS_REMOVE_context_arm.o = -mthumb -Wa,-mimplicit-it=always -Wa,-mno-warn-deprecated -Wa,-mthumb

KBUILD_VERBOSE = 1
  quiet =
  Q =

export quiet Q KBUILD_VERBOSE

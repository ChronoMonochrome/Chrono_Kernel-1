#
# Copyright (C) 2010-2011 ARM Limited. All rights reserved.
# 
# This program is free software and is provided to you under the terms of the GNU General Public License version 2
# as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
# 
# A copy of the licence is included with the program, and can also be obtained from Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

OSKOS=linux
FILES_PREFIX=

ifneq ($(KBUILD_EXTMOD),)
DRIVER_DIR=$(KBUILD_EXTMOD)
else
src?=.
srctree?=.
DRIVER_DIR?=$(srctree)/$(src)
M?=$(DRIVER_DIR)
endif
MALI_RELEASE_NAME=$(shell cat $(DRIVER_DIR)/.version 2> /dev/null)
include $(DRIVER_DIR)/Makefile.platform
include $(DRIVER_DIR)/Makefile.common

# set up defaults if not defined by the user
ARCH ?= arm
USING_MMU ?= 1
USING_UMP ?= 0
USING_HWMEM ?= 0
USING_OS_MEMORY ?= 0
USING_PMM ?= 0
USING_GPU_UTILIZATION ?= 0
USING_MALI_RUN_TIME_PM ?= 0
USING_MALI_PMM_TESTSUITE ?= 0
OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB ?= 6
USING_PROFILING ?= 0
TIMESTAMP ?= default
BUILD ?= debug
TARGET_PLATFORM ?= default

ifeq ($(USING_UMP),1)
ifneq ($(USING_HWMEM),1)
	UMP_SYMVERS_FILE = ../ump/Module.symvers
	KBUILD_EXTRA_SYMBOLS = $(KBUILD_EXTMOD)/$(UMP_SYMVERS_FILE)
endif
endif

# Check if a Mali Core sub module should be enabled, true or false returned
submodule_enabled = $(shell gcc $(DEFINES) -E $1/arch/config.h | grep type | grep -c $(2))

# linux build system integration
ifneq ($(KERNELRELEASE),)
# Inside the kernel build system

# This conditional makefile exports the global definition ARM_INTERNAL_BUILD. Customer releases will not include arm_internal.mak
-include ../../../arm_internal.mak

# Set up our defines, which will be passed to gcc
DEFINES += -DUSING_OS_MEMORY=$(USING_OS_MEMORY)
DEFINES += -DUSING_MMU=$(USING_MMU)
DEFINES += -DUSING_UMP=$(USING_UMP)
DEFINES += -DUSING_HWMEM=$(USING_HWMEM)
DEFINES += -D_MALI_OSK_SPECIFIC_INDIRECT_MMAP
DEFINES += -DMALI_TIMELINE_PROFILING_ENABLED=$(USING_PROFILING)
DEFINES += -DMALI_POWER_MGMT_TEST_SUITE=$(USING_MALI_PMM_TESTSUITE)
DEFINES += -DMALI_PMM_RUNTIME_JOB_CONTROL_ON=$(USING_MALI_RUN_TIME_PM)
DEFINES += -DMALI_STATE_TRACKING=1
DEFINES += -DMALI_OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB=$(OS_MEMORY_KERNEL_BUFFER_SIZE_IN_MB)

ifneq ($(call submodule_enabled, $M, PMU),0)
        MALI_PLATFORM_FILE = platform/mali400-pmu/mali_platform.c
else
	MALI_PLATFORM_FILE = platform/$(TARGET_PLATFORM)/mali_platform.c
endif

DEFINES += -DUSING_MALI_PMM=$(USING_PMM)
DEFINES += -DMALI_GPU_UTILIZATION=$(USING_GPU_UTILIZATION)

ifeq ($(BUILD), debug)
DEFINES += -DDEBUG
endif
DEFINES += -DSVN_REV=$(SVN_REV)
DEFINES += -DSVN_REV_STRING=\"$(SVN_REV)\"

# Linux has its own mmap cleanup handlers (see mali_kernel_mem_mmu.c)
DEFINES += -DMALI_UKK_HAS_IMPLICIT_MMAP_CLEANUP

ifeq ($(USING_UMP),1)
	DEFINES += -DMALI_USE_UNIFIED_MEMORY_PROVIDER=1
	EXTRA_CFLAGS += -I$(DRIVER_DIR)/../../../include/ump
else
	DEFINES += -DMALI_USE_UNIFIED_MEMORY_PROVIDER=0
endif

# Use our defines when compiling
EXTRA_CFLAGS += $(DEFINES) -I$(DRIVER_DIR) -I$(DRIVER_DIR)/common -I$(DRIVER_DIR)/linux -I$(DRIVER_DIR)/platform

# For customer releases the Linux Device Drivers will be provided as ARM proprietary and GPL releases:
# The ARM proprietary product will only include the license/proprietary directory
# The GPL product will only include the license/gpl directory

ifeq ($(wildcard $(DRIVER_DIR)/linux/license/gpl/*),)
EXTRA_CFLAGS += -I$(DRIVER_DIR)/linux/license/proprietary
else
EXTRA_CFLAGS += -I$(DRIVER_DIR)/linux/license/gpl
endif
EXTRA_CFLAGS += -I$(DRIVER_DIR)/common/pmm

# Source files which always are included in a build
SRC = \
	common/mali_kernel_core.c \
	linux/mali_kernel_linux.c \
	$(OSKOS)/mali_osk_indir_mmap.c \
	common/mali_kernel_rendercore.c \
	common/mali_kernel_descriptor_mapping.c \
	common/mali_kernel_vsync.c \
	linux/mali_ukk_vsync.c \
	$(MALI_PLATFORM_FILE) \
	$(OSKFILES) \
	$(UKKFILES)
	#__malidrv_build_info.c
	
ifeq ($(USING_PROFILING),1)
SRC += \
	common/mali_kernel_profiling.c \
	timestamp-$(TIMESTAMP)/mali_timestamp.c
EXTRA_CFLAGS += -I$(DRIVER_DIR)/timestamp-$(TIMESTAMP)
endif

# Selecting files to compile by parsing the config file

ifeq ($(USING_PMM),1)
SRC += \
	common/pmm/mali_pmm.c \
	common/pmm/mali_pmm_policy.c \
	common/pmm/mali_pmm_policy_alwayson.c \
	common/pmm/mali_pmm_policy_jobcontrol.c \
	common/pmm/mali_pmm_state.c \
	linux/mali_kernel_pm.c \
	linux/mali_osk_pm.c \
	linux/mali_device_pause_resume.c 
endif

ifeq ($(USING_GPU_UTILIZATION),1)
SRC += \
	common/mali_kernel_utilization.c
endif

ifneq ($(call submodule_enabled, $M, MALI400PP),0)
	# Mali-400 PP in use
	EXTRA_CFLAGS += -DUSING_MALI400
	SRC += common/mali_kernel_MALI200.c
endif

ifneq ($(call submodule_enabled, $M, MALI400GP),0)
	# Mali-400 GP in use
	SRC += common/mali_kernel_GP2.c
endif

ifneq ($(call submodule_enabled, $M, MALI300PP),0)
	# Mali-400 PP in use
	EXTRA_CFLAGS += -DUSING_MALI400
	SRC += common/mali_kernel_MALI200.c
endif

ifneq ($(call submodule_enabled, $M, MALI300GP),0)
	# Mali-400 GP in use
	SRC += common/mali_kernel_GP2.c
endif

ifneq ($(call submodule_enabled, $M, MALI200),0)
	# Mali200 in use
	EXTRA_CFLAGS += -DUSING_MALI200
	SRC += common/mali_kernel_MALI200.c
endif

ifneq ($(call submodule_enabled, $M, MALIGP2),0)
	# MaliGP2 in use
	SRC += common/mali_kernel_GP2.c
endif

ifneq ($(call submodule_enabled, $M, MMU),0)
	# Mali MMU in use
	SRC += common/mali_kernel_mem_mmu.c common/mali_kernel_memory_engine.c common/mali_block_allocator.c common/mali_kernel_mem_os.c
else
	# No Mali MMU in use
	SRC += common/mali_kernel_mem_buddy.c
endif

ifneq ($(call submodule_enabled, $M, MALI400L2)$(),0)
	# Mali Level2 cache in use
	EXTRA_CFLAGS += -DUSING_MALI400_L2_CACHE
	SRC += common/mali_kernel_l2_cache.c
endif

ifneq ($(call submodule_enabled, $M, MALI300L2)$(),0)
	# Mali Level2 cache in use
	EXTRA_CFLAGS += -DUSING_MALI400_L2_CACHE
	SRC += common/mali_kernel_l2_cache.c
endif

ifeq ($(USING_HWMEM),1)
	# HWMEM used as backend for UMP api
	EXTRA_CFLAGS += -I$(DRIVER_DIR)/../ump/common
	SRC += platform/ux500/ump_kernel_api_hwmem.c
endif

# Target build file
MODULE:=mali.ko

# Tell the Linux build system from which .o file to create the kernel module
obj-$(CONFIG_GPU_MALI) := $(MODULE:.ko=.o)
# Tell the Linux build system to enable building of our .c files
$(MODULE:.ko=-y) := $(SRC:.c=.o)

else
# Outside the kernel build system

# Get any user defined KDIR-<names> or maybe even a hardcoded KDIR
-include KDIR_CONFIGURATION

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build

ifeq ($(ARCH), arm)
	# when compiling for ARM we're cross compiling
	export CROSS_COMPILE ?= arm-none-linux-gnueabi-
	# default to Virtex5
	CONFIG ?= pb-virtex5-m200
else
	# Compiling for the host
	CONFIG ?= $(shell uname -m)
endif

# default cpu to select
CPU ?= pb11mp

# look up KDIR based om CPU selection
KDIR ?= $(KDIR-$(CPU))

# validate lookup result
ifeq ($(KDIR),)
$(error No KDIR found for platform $(CPU))
endif

# report detected/selected settings
ifdef ARM_INTERNAL_BUILD
$(warning Config $(CONFIG))
$(warning Host CPU $(CPU))
$(warning MMU $(USING_MMU))
$(warning OS_MEMORY $(USING_OS_MEMORY))
endif

# Validate selected config
ifneq ($(shell [ -d $(DRIVER_DIR)/arch-$(CONFIG) ] && [ -f $(DRIVER_DIR)/arch-$(CONFIG)/config.h ] && echo "OK"), OK)
$(warning Current directory is $(shell pwd))
$(error No configuration found for config $(CONFIG). Check that $(DRIVER_DIR)/arch-$(CONFIG)/config.h exists)
else
# Link arch to the selected arch-config directory
$(shell [ -L $(DRIVER_DIR)/arch ] && rm $(DRIVER_DIR)/arch)
$(shell ln -sf $(DRIVER_DIR)/arch-$(CONFIG) $(DRIVER_DIR)/arch)
$(shell touch $(DRIVER_DIR)/arch/config.h)

# Register number of Mali400 cores for routing test jobs to the right Mali-400
ifeq ($CONFIG, pb-virtex5-m400-1)
VERSION_STRINGS += USING_MALI400_PP_CORES=1
endif
ifeq ($CONFIG, pb-virtex5-m400-2)
VERSION_STRINGS += USING_MALI400_PP_CORES=2
endif
ifeq ($CONFIG, pb-virtex5-m400-3)
VERSION_STRINGS += USING_MALI400_PP_CORES=3
endif
ifeq ($CONFIG, pb-virtex5-m400-4)
VERSION_STRINGS += USING_MALI400_PP_CORES=4
endif

endif

# Filename to use when patching. Original will be saved as this
PATCH_BACKUP_FILE:=config_makefile_patch_backup.h

ifdef ARM_INTERNAL_BUILD
$(warning Looking for previous backup)
endif
# Look for previous backup
shell_output:=$(shell [ -f arch/$(PATCH_BACKUP_FILE) ] && mv -vf arch/$(PATCH_BACKUP_FILE) arch/config.h  && echo "Patch backup restored")
ifneq ($(shell_output),)
ifdef ARM_INTERNAL_BUILD
$(warning $(shell_output))
endif
endif

ifdef PATCH
ifdef ARM_INTERNAL_BUILD
$(warning Patching using: $(PATCH) )
endif
ifneq ($(shell [ -f arch/$(PATCH).diff ] && echo "OK"), OK)
# The patch file does not exist
shell_output:=$(shell echo "Possible PATCH arguments are:" ; find arch/ -name "*.diff" |sed -r 's/.*\/(.*)\.diff/\1/')
ifdef ARM_INTERNAL_BUILD
$(warning $(shell_output))
endif
$(error Could not find file arch-$(CONFIG)/$(PATCH).diff )
else
# Patch file found, do patching
shell_output:=$(shell cp -f arch/config.h arch/$(PATCH_BACKUP_FILE) && patch --no-backup-if-mismatch -st -p0 arch/config.h -i arch/$(PATCH).diff && echo "OK")
ifneq ($(shell_output), OK)
$(warning Output from failed patch: $(shell_output))
$(shell mv -f arch/$(PATCH_BACKUP_FILE) arch/config.h)
$(error Could not patch file arch-$(CONFIG)/config.h with arch-$(CONFIG)/$(PATCH).diff.)
endif
endif
endif

# Extend common version-string
VERSION_STRINGS += BUILD=$(shell echo $(BUILD) | tr a-z A-Z)
VERSION_STRINGS += CPU=$(CPU)
VERSION_STRINGS += USING_MMU=$(USING_MMU)
VERSION_STRINGS += USING_UMP=$(USING_UMP)
VERSION_STRINGS += USING_HWMEM=$(USING_HWMEM)
VERSION_STRINGS += USING_PMM=$(USING_PMM)
VERSION_STRINGS += USING_MALI200=$(call submodule_enabled, ., MALI200)
VERSION_STRINGS += USING_MALI400=$(call submodule_enabled, ., MALI400)
VERSION_STRINGS += USING_MALI400_L2_CACHE=$(call submodule_enabled, ., MALI400L2)
VERSION_STRINGS += USING_GP2=$(call submodule_enabled, ., MALIGP2)
VERSION_STRINGS += KDIR=$(KDIR)

all: make-build-info-file $(UMP_SYMVERS_FILE)
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) CONFIG_GPU_MALI=m modules
	@([ "$(PATCH)" != "" ] && [ -f arch/$(PATCH_BACKUP_FILE) ] && mv -f arch/$(PATCH_BACKUP_FILE) arch/config.h  && echo "Patch backup restored") || echo No Unpatching needed
	@rm -f $(FILES_PREFIX)__malidrv_build_info.c $(FILES_PREFIX)__malidrv_build_info.o

clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) clean
	@([ "$(PATCH)" != "" ] && [ -f arch/$(PATCH_BACKUP_FILE) ] && mv -f arch/$(PATCH_BACKUP_FILE) arch/config.h  && echo "Patch backup restored") || echo No Unpatching needed

make-build-info-file:
	@echo 'char *__malidrv_build_info(void) { return "malidrv: $(VERSION_STRINGS)";}' > $(FILES_PREFIX)__malidrv_build_info.c

kernelrelease:
	$(MAKE) -C $(KDIR) kernelrelease

# end of outside kernel build system block
endif

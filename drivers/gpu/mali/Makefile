MALI_SUBFOLDER := mali400ko/driver/src/devicedrv/mali
UMP_SUBFOLDER := mali400ko/driver/src/devicedrv/ump
MALI_FOLDER := $(srctree)/$(src)/$(MALI_SUBFOLDER)
UMP_FOLDER := $(srctree)/$(src)/$(UMP_SUBFOLDER)
ifeq ($(shell [ -d $(MALI_FOLDER) ] && echo "OK"), OK)
obj-$(CONFIG_GPU_MALI)  += $(MALI_SUBFOLDER)/
else
$(warning WARNING: mali: Could not find $(MALI_FOLDER) - mali device driver will not be built)
obj-n += ./
endif

ifeq ($(shell [ -d $(UMP_FOLDER) ] && echo "OK"), OK)
obj-$(CONFIG_UMP) += $(UMP_SUBFOLDER)/
else
$(warning WARNING: mali: Could not find $(UMP_FOLDER) - mali device driver will not be built)
obj-n += ./
endif

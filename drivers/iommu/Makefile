obj-$(CONFIG_IOMMU_API) += iommu.o
obj-$(CONFIG_MSM_IOMMU) += msm_iommu.o msm_iommu_dev.o
obj-$(CONFIG_AMD_IOMMU) += amd_iommu.o amd_iommu_init.o
obj-$(CONFIG_AMD_IOMMU_V2) += amd_iommu_v2.o
obj-$(CONFIG_DMAR_TABLE) += dmar.o
obj-$(CONFIG_INTEL_IOMMU) += iova.o intel-iommu.o
obj-$(CONFIG_IRQ_REMAP) += intr_remapping.o
obj-$(CONFIG_OMAP_IOMMU) += omap-iommu.o
obj-$(CONFIG_OMAP_IOVMM) += omap-iovmm.o
obj-$(CONFIG_OMAP_IOMMU_DEBUG) += omap-iommu-debug.o
obj-$(CONFIG_TEGRA_IOMMU_GART) += tegra-gart.o
obj-$(CONFIG_TEGRA_IOMMU_SMMU) += tegra-smmu.o
AFLAGS_AUTOIT   :=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
AFLAGS_NOWARN   :=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)
CFLAGS_THUMB2   :=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_THUMB2   :=$(CFLAGS_THUMB2) -Wa$(comma)-mthumb

KBUILD_AFLAGS += $(AFLAGS_THUMB2)
KBUILD_CFLAGS += $(CFLAGS_THUMB2)

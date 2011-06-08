# Ux500 Platform Support

ifdef CONFIG_SND_SOC_UX500_DEBUG
CFLAGS_ux500_pcm.o := -DDEBUG
CFLAGS_ux500_msp_dai.o := -DDEBUG
CFLAGS_ux500_ab3550.o := -DDEBUG
CFLAGS_ux500_ab8500.o := -DDEBUG
CFLAGS_ux500_av8100.o := -DDEBUG
CFLAGS_ux500_cg29xx.o := -DDEBUG
endif

ifdef CONFIG_SND_SOC_UX500_AB3550
snd-soc-ux500-ab3550-objs := ux500_ab3550.o
obj-$(CONFIG_SND_SOC_UX500_AB3550) += ux500_ab3550.o
endif

ifdef CONFIG_SND_SOC_UX500_AB5500
snd-soc-ux500-ab5500-objs := ux500_ab5500.o
obj-$(CONFIG_SND_SOC_UX500_AB5500) += ux500_ab5500.o
endif

ifdef CONFIG_SND_SOC_UX500_AB8500
snd-soc-ux500-ab8500-objs := ux500_ab8500.o
obj-$(CONFIG_SND_SOC_UX500_AB8500) += ux500_ab8500.o
endif

ifdef CONFIG_SND_SOC_UX500_AV8100
snd-soc-ux500-av8100-objs := ux500_av8100.o
obj-$(CONFIG_SND_SOC_UX500_AV8100) += ux500_av8100.o
endif

ifdef CONFIG_SND_SOC_UX500_CG29XX
snd-soc-ux500-cg29xx-objs := ux500_cg29xx.o
obj-$(CONFIG_SND_SOC_UX500_CG29XX) += ux500_cg29xx.o
endif

snd-soc-ux500-objs := ux500_pcm.o ux500_msp_dai.o

ifdef CONFIG_UX500_SOC_DB8500
snd-soc-ux500-objs += u8500.o
endif

ifdef CONFIG_UX500_SOC_DB5500
snd-soc-ux500-objs += u5500.o
endif

obj-$(CONFIG_UX500_SOC_DBX500) += snd-soc-ux500.o

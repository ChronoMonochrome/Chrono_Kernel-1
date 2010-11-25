# Ux500 Platform Support

ifdef CONFIG_SND_SOC_UX500_DEBUG
CFLAGS_ux500_pcm.o := -DDEBUG
CFLAGS_ux500_msp_dai.o := -DDEBUG
CFLAGS_ux500_av8100.o := -DDEBUG
endif

ifdef CONFIG_SND_SOC_UX500_AB3550
snd-soc-ux500-av8100-objs := ux500_ab3550.o
obj-$(CONFIG_SND_SOC_UX500_AB3550) += ux500_ab3550.o
endif

ifdef CONFIG_SND_SOC_UX500_AV8100
snd-soc-ux500-av8100-objs := ux500_av8100.o
obj-$(CONFIG_SND_SOC_UX500_AV8100) += ux500_av8100.o
endif

ifdef CONFIG_SND_SOC_UX500_CG29XX
snd-soc-ux500-av8100-objs := ux500_cg29xx.o
obj-$(CONFIG_SND_SOC_UX500_CG29XX) += ux500_cg29xx.o
endif

#ifdef UX500_SOC_DB8500
snd-soc-u8500-objs := u8500.o
obj-y += ux500_pcm.o ux500_msp_dai.o snd-soc-u8500.o
#endif

ifdef UX500_SOC_DB5500
snd-soc-u8500-objs := u5500.o
obj-$(UX500_SOC_DB5500) += ux500_pcm.o ux500_msp_dai.o snd-soc-u5500.o
endif


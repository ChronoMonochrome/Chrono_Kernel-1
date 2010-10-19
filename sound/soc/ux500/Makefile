# Ux500 Platform Support

ifdef CONFIG_SND_SOC_UX500_DEBUG
CFLAGS_av8100_audio.o := -DDEBUG
CFLAGS_ab3550.o := -DDEBUG
CFLAGS_cg29xx.o := -DDEBUG
CFLAGS_ux500_pcm.o := -DDEBUG
CFLAGS_ux500_msp_dai.o := -DDEBUG
CFLAGS_ux500_av8100.o := -DDEBUG
endif

snd-soc-ux500-objs := ux500_pcm.o ux500_msp_dai.o
snd-soc-ux500-ab3550-objs := ux500_ab3550.o
snd-soc-ux500-cg29xx-objs := ux500_cg29xx.o
snd-soc-ux500-av8100-objs := ux500_av8100.o

obj-$(CONFIG_SND_SOC_UX500) += snd-soc-ux500.o
obj-$(CONFIG_SND_SOC_UX500_AB3550) += snd-soc-ux500-ab3550.o
obj-$(CONFIG_SND_SOC_UX500_CG29XX) += snd-soc-ux500-cg29xx.o
obj-$(CONFIG_SND_SOC_UX500_AV8100) += snd-soc-ux500-av8100.o

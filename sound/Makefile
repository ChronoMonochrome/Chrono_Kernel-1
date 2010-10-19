# Makefile for the Linux sound card driver
#

obj-$(CONFIG_SOUND) += soundcore.o
obj-$(CONFIG_SOUND_PRIME) += sound_firmware.o
obj-$(CONFIG_SOUND_PRIME) += oss/
obj-$(CONFIG_DMASOUND) += oss/
obj-$(CONFIG_SND) += core/ i2c/ drivers/ isa/ pci/ ppc/ arm/ sh/ synth/ usb/ \
	firewire/ sparc/ spi/ parisc/ pcmcia/ mips/ soc/ atmel/
obj-$(CONFIG_SND_AOA) += aoa/

# This one must be compilable even if sound is configured out
obj-$(CONFIG_AC97_BUS) += ac97_bus.o

ifeq ($(CONFIG_SND),y)
  obj-y += last.o
endif

obj-$(CONFIG_U8500_ACODEC) += u8500mod_acodec.o
ifeq ($(CONFIG_U8500_AB8500_CUT10),y)
  u8500mod_acodec-objs := u8500_acodec_ab8500.o ab8500_codec_v1_0.o
else
  u8500mod_acodec-objs := u8500_acodec_ab8500.o ab8500_codec.o
endif

soundcore-objs  := sound_core.o

#
# Makefile for nfc devices
#

obj-$(CONFIG_PN544_HCI_NFC)	+= pn544/
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_NFC_WILINK)	+= nfcwilink.o
obj-$(CONFIG_NFC_MEI_PHY)	+= mei_phy.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG

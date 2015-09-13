#
# Arch-specific CryptoAPI modules.
#

obj-$(CONFIG_CRYPTO_AES_ARM) += aes-arm.o
obj-$(CONFIG_CRYPTO_AES_ARM_BS) += aes-arm-bs.o
obj-$(CONFIG_CRYPTO_SHA1_ARM) += sha1-arm.o
obj-$(CONFIG_CRYPTO_SHA1_ARM_NEON) += sha1-arm-neon.o
obj-$(CONFIG_CRYPTO_SHA512_ARM_NEON) += sha512-arm-neon.o

#aes-arm-y	:= aes-armv4.o aes_glue.o
#aes-arm-bs-y	:= aesbs-core.o aesbs-glue.o
sha1-arm-y	:= sha1-armv4-large.o sha1_glue.o
sha1-arm-neon-y	:= sha1-armv7-neon.o sha1_neon_glue.o
sha512-arm-neon-y := sha512-armv7-neon.o sha512_neon_glue.o

aes-arm.o: $(src)/aes-arm.o
	cp $(src)/aes-arm.o $(obj)/aes-arm.o
aes-arm-bs.o: $(src)/aes-arm-bs.o
	cp $(src)/aes-arm-bs.o $(obj)/aes-arm-bs.o
#sha1-arm.o: $(src)/sha1-arm.o
#	cp $(src)/sha1-arm.o $(obj)/sha1-arm.o
#sha1-arm-neon.o: $(src)/sha1-arm-neon.o
#	cp $(src)/sha1-arm-neon.o $(obj)/sha1-arm-neon.o 
#sha512-arm-neon.o: $(src)/sha512-arm-neon.o
#	cp $(src)/sha512-arm-neon.o $(obj)/sha512-arm-neon.o
#aesbs-core.o: $(src)/aesbs-core.o
#	cp $(src)/aesbs-core.o $(obj)/aesbs-core.o
#built-in.o: $(src)/built-in.o
#	cp $(src)/built-in.o $(obj)/built-in.o


ifeq ($(CONFIG_CRYPTO_DEV_UX500_DEBUG_INFO),y)
        EXTRA_CFLAGS += -D__DEBUG
else
        EXTRA_CFLAGS += -D__RELEASE
endif

obj-$(CONFIG_CRYPTO_DEV_UX500_HASH) += u8500_hash.o
u8500_hash-objs :=  hash_core.o

#
# Makefile for multifunction miscellaneous devices
#

obj-$(CONFIG_AB5500_CORE)	+= ab5500-core.o ab5500-power.o
obj-$(CONFIG_MFD_STMPE)		+= stmpe.o
obj-$(CONFIG_STMPE_I2C)		+= stmpe-i2c.o
obj-$(CONFIG_STMPE_SPI)		+= stmpe-spi.o

obj-$(CONFIG_ABX500_CORE)	+= abx500-core.o
obj-$(CONFIG_AB3100_CORE)	+= ab3100-core.o
obj-$(CONFIG_AB3100_OTP)	+= ab3100-otp.o
obj-$(CONFIG_AB3550_CORE)	+= ab3550-core.o
obj-$(CONFIG_AB8500_CORE)	+= ab8500-core.o ab8500-sysctrl.o
obj-$(CONFIG_AB8500_DEBUG)	+= ab8500-debugfs.o
obj-$(CONFIG_AB8500_DENC)	+= ab8500-denc.o
obj-$(CONFIG_AB8500_GPADC)	+= ab8500-gpadc.o
obj-$(CONFIG_DBX500_PRCMU)      += dbx500-prcmu.o
obj-$(CONFIG_MFD_DB8500_PRCMU)	+= db8500-prcmu.o
obj-$(CONFIG_MFD_DBX540_PRCMU)  += dbx540-prcmu.o
# ab8500-i2c need to come after db8500-prcmu (which provides the channel)
obj-$(CONFIG_AB8500_I2C_CORE)	+= ab8500-i2c.o
obj-$(CONFIG_MFD_DB5500_PRCMU)	+= db5500-prcmu.o
obj-$(CONFIG_AB5500_GPADC)	+= ab5500-gpadc.o

# generic gpio support: platform drivers, dedicated expander chips, etc
ccflags-$(CONFIG_DEBUG_GPIO)	+= -DDEBUG

obj-$(CONFIG_GPIOLIB)           += gpiolib.o
obj-$(CONFIG_UX500_GPIO_AB8500)	+= gpio-ab8500.o
obj-$(CONFIG_PLAT_NOMADIK)+= gpio-nomadik.o

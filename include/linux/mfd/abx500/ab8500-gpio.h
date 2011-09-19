/*
 * Copyright ST-Ericsson 2010.
 *
 * Author: Bibek Basu <bibek.basu@stericsson.com>
 * Licensed under GPLv2.
 */

#ifndef _AB8500_GPIO_H
#define _AB8500_GPIO_H

/*
 * Platform data to register a block: only the initial gpio/irq number.
 * Array sizes are large enough to contain all AB8500 and AB9540 GPIO
 * registers.
 */

struct ab8500_gpio_platform_data {
	int gpio_base;
	u32 irq_base;
	u8  config_reg[8];
	u8  config_direction[6];
	u8  config_pullups[6];
};

enum ab8500_pin {
	AB8500_PIN_GPIO1 = 0,
	AB8500_PIN_GPIO2,
	AB8500_PIN_GPIO3,
	AB8500_PIN_GPIO4,
	AB8500_PIN_GPIO5,
	AB8500_PIN_GPIO6,
	AB8500_PIN_GPIO7,
	AB8500_PIN_GPIO8,
	AB8500_PIN_GPIO9,
	AB8500_PIN_GPIO10,
	AB8500_PIN_GPIO11,
	AB8500_PIN_GPIO12,
	AB8500_PIN_GPIO13,
	AB8500_PIN_GPIO14,
	AB8500_PIN_GPIO15,
	AB8500_PIN_GPIO16,
	AB8500_PIN_GPIO17,
	AB8500_PIN_GPIO18,
	AB8500_PIN_GPIO19,
	AB8500_PIN_GPIO20,
	AB8500_PIN_GPIO21,
	AB8500_PIN_GPIO22,
	AB8500_PIN_GPIO23,
	AB8500_PIN_GPIO24,
	AB8500_PIN_GPIO25,
	AB8500_PIN_GPIO26,
	AB8500_PIN_GPIO27,
	AB8500_PIN_GPIO28,
	AB8500_PIN_GPIO29,
	AB8500_PIN_GPIO30,
	AB8500_PIN_GPIO31,
	AB8500_PIN_GPIO32,
	AB8500_PIN_GPIO33,
	AB8500_PIN_GPIO34,
	AB8500_PIN_GPIO35,
	AB8500_PIN_GPIO36,
	AB8500_PIN_GPIO37,
	AB8500_PIN_GPIO38,
	AB8500_PIN_GPIO39,
	AB8500_PIN_GPIO40,
	AB8500_PIN_GPIO41,
	AB8500_PIN_GPIO42,
};

int ab8500_config_pulldown(struct device *dev,
				enum ab8500_pin gpio, bool enable);

int ab8500_gpio_config_select(struct device *dev,
				enum ab8500_pin gpio, bool gpio_select);

int ab8500_gpio_config_get_select(struct device *dev,
				enum ab8500_pin	gpio, bool *gpio_select);

#endif /* _AB8500_GPIO_H */

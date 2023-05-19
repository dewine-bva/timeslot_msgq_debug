#ifndef TEST_GPIO_H_
#define TEST_GPIO_H_

#include <zephyr/drivers/gpio.h>

int init_test_gpios();

void disable_gpio_work(gpio_pin_t pin);

#endif // TEST_GPIO_H_
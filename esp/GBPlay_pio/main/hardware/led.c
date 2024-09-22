#include <driver/gpio.h>

#include "led.h"

#define LED_GPIO 2

void led_initialize()
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    led_set_state(false);
}

void led_set_state(bool is_on)
{
    gpio_set_level(LED_GPIO, is_on);
}

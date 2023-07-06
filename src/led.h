#pragma once

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

#define LED_M0_R GPIO6
#define LED_M0_B GPIO7
#define LED_M1_R GPIO8
#define LED_M1_B GPIO9

void led_init(void);

static inline void led_set(const uint16_t led) {gpio_set(GPIOC, led);}
static inline void led_clear(const uint16_t led) {gpio_clear(GPIOC, led);}
static inline void led_toggle(const uint16_t led) {gpio_toggle(GPIOC, led);}

#pragma once

#include <libopencm3/stm32/gpio.h>

typedef enum {
	DIR_HALT,
	DIR_FWD,
	DIR_REV,

	DIR_COUNT
} direction_t;

int output_init(void);

int output_enable(int channel);
int output_disable(int channel);

int output_direction(int channel, direction_t direction);

int output_speed(int channel, int speed);

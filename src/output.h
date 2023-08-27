#pragma once

#include <libopencm3/stm32/gpio.h>
#include "led.h"

#define MAX_MOTOR_VAL 1000
#define MIN_MOTOR_VAL (-MAX_MOTOR_VAL)
#define NUM_OUTPUTS 2

typedef struct {
    bool enabled;
    int16_t value;
    bool in_fault;
    uint16_t current;
} output_t;
// defined in output.c
extern output_t output_data[];

void output_init(void);

void output_set_power(uint8_t output_num, int16_t output_val);
bool output_enabled(uint8_t output_num);
int16_t output_get_output(uint8_t output_num);
void output_disable(uint8_t output_num);
uint16_t output_get_current(uint8_t output_num);
void check_output_faults(void);
void outputs_reset(void);

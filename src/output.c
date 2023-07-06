#include "output.h"
#include "led.h"

#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/gpio.h>

#define MOTOR_SPEED_COEFF 2

static const struct {
    uint32_t INa;
    uint32_t INb;
    uint32_t ENa;
    uint32_t ENb;
    uint32_t PWM;
    enum tim_oc_id timer_chan;
} output_pins[] = {{
    .INa = GPIO12,
    .INb = GPIO13,
    .ENa = GPIO14,
    .ENb = GPIO15,
    .PWM = GPIO_TIM2_CH2,
    .timer_chan = TIM_OC2
}, {
    .INa = GPIO8,
    .INb = GPIO9,
    .ENa = GPIO10,
    .ENb = GPIO11,
    .PWM = GPIO_TIM2_CH1_ETR,
    .timer_chan = TIM_OC1
}};

output_t output_data[NUM_OUTPUTS] = { 0 };

static void setup_gpio(void) {
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        // setup ctrl gpio
        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, output_pins[i].INa);
        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, output_pins[i].INb);
        // Enable pins are pulled low by the H-bridge when in fault
        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, output_pins[i].ENa);
        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, output_pins[i].ENb);
        output_disable(i);

        // setup pwm pins
        gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, output_pins[i].PWM);
    }
}

void output_init(void) {
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        output_data[i].enabled = false;
        output_data[i].value = 0;
        output_data[i].in_fault = false;
        output_data[i].current = 0;
    }
    setup_gpio();

    rcc_periph_clock_enable(RCC_TIM2);
    // Run the timer at 24MHz
    timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    // Don't prescaler the timer clock, results in a ~12kHz PWM
    timer_set_prescaler(TIM2, 1);
    // Configure period to match available motor value range
    timer_set_period(TIM2, (MOTOR_SPEED_COEFF * MAX_MOTOR_VAL));

    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        // Configure positive polarity output compare
        timer_set_oc_mode(TIM2, output_pins[i].timer_chan, TIM_OCM_PWM1);
        timer_set_oc_polarity_high(TIM2, output_pins[i].timer_chan);

        // Enable output compare outputs to physical pins
        timer_enable_oc_preload(TIM2, output_pins[i].timer_chan);
        timer_enable_oc_output(TIM2, output_pins[i].timer_chan);
    }

    timer_enable_preload(TIM2);
    timer_enable_counter(TIM2);
}

void output_set_power(uint8_t output_num, int16_t output_val) {
    if (!(output_num < NUM_OUTPUTS)) {
        // skip invalid output numbers
        return;
    }
    if (output_val < MIN_MOTOR_VAL || output_val > MAX_MOTOR_VAL) {
        // skip invalid output values
        return;
    }

    if (!output_data[output_num].enabled) {
        // enable output if it wasn't previously
        gpio_set(GPIOB, output_pins[output_num].ENa);
        gpio_set(GPIOB, output_pins[output_num].ENb);

        output_data[output_num].enabled = true;
    }

    // set direction
    if (output_val > 0) {  // forward
        gpio_set(GPIOB, output_pins[output_num].INa);
        gpio_clear(GPIOB, output_pins[output_num].INb);

        // set speed
        timer_set_oc_value(TIM2, output_pins[output_num].timer_chan, output_val * MOTOR_SPEED_COEFF);
    } else if (output_val < 0) {  // reverse
        gpio_clear(GPIOB, output_pins[output_num].INa);
        gpio_set(GPIOB, output_pins[output_num].INb);

        // set speed
        timer_set_oc_value(TIM2, output_pins[output_num].timer_chan, -output_val * MOTOR_SPEED_COEFF);
    } else if (output_val == 0) {  // brake
        gpio_clear(GPIOB, output_pins[output_num].INa);
        gpio_clear(GPIOB, output_pins[output_num].INb);
    }

    // store set speed
    output_data[output_num].value = output_val;
}

bool output_enabled(uint8_t output_num) {
    if (!(output_num < NUM_OUTPUTS)) {
        // skip invalid output numbers
        return false;
    }

    return output_data[output_num].enabled;
}

int8_t output_get_output(uint8_t output_num) {
    if (!(output_num < NUM_OUTPUTS)) {
        // skip invalid output numbers
        return 0;
    }

    if (output_data[output_num].enabled) {
        return output_data[output_num].value;
    } else {
        return 0;
    }
}

void output_disable(uint8_t output_num) {
    if (!(output_num < NUM_OUTPUTS)) {
        // skip invalid output numbers
        return;
    }

    output_data[output_num].enabled = false;

    gpio_clear(GPIOB, output_pins[output_num].ENa);
    gpio_clear(GPIOB, output_pins[output_num].ENb);
}

uint16_t output_get_current(uint8_t output_num) {
    if (!(output_num < NUM_OUTPUTS)) {
        // skip invalid output numbers
        return 0;
    }

    return output_data[output_num].current;
}

void check_output_faults(void) {
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        if (output_data[i].enabled) {
            // The h-bridge drives the enable pin low when a fault occurs
            if (
                (!gpio_get(GPIOB, output_pins[i].ENa))
                || (!gpio_get(GPIOB, output_pins[i].ENb))
            ) {
                // A fault has occured on the output
                output_data[i].in_fault = true;
                led_set((i == 0)?(LED_M0_R):(LED_M1_R));
            } else {
                output_data[i].in_fault = false;
                led_clear((i == 0)?(LED_M0_R):(LED_M1_R));
            }
        } else {
            output_data[i].in_fault = false;
            led_clear((i == 0)?(LED_M0_R):(LED_M1_R));
        }
    }
}

void outputs_reset(void) {
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        output_disable(i);
        output_data[i].in_fault = false;
        output_data[i].current = 0;
    }

    led_clear(LED_M0_R);
    led_clear(LED_M0_B);
    led_clear(LED_M1_R);
    led_clear(LED_M1_B);
}

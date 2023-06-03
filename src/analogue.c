#include "analogue.h"
#include "led.h"
#include "output.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dbgmcu.h>

uint16_t input_voltage = 0;

static void init_adc_timer(void) {
    rcc_periph_clock_enable(RCC_TIM1);

    timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    // 24MHz/1k = 24kHz timer clock. /1 gives 24kHz sampling frequency
    timer_set_period(TIM1, 1000);
    timer_set_prescaler(TIM1, 1);

    // Configure OC1 to generate an event on match
    timer_set_oc_mode(TIM1, TIM_OC1, TIM_OCM_PWM1);
    timer_enable_oc_preload(TIM1, TIM_OC1);
    timer_set_oc_value(TIM1, TIM_OC1, 1);

    // Enable trigger output for triggering ADC scan run
    timer_set_master_mode(TIM1, TIM_CR2_MMS_COMPARE_OC1REF);

    timer_enable_preload(TIM1);

    /* Halt the ADC timer while debugging */
    DBGMCU_CR |= DBGMCU_CR_TIM1_STOP;
}

static void init_adc(void) {
    gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO1);  // 12V
    gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO3);  // M0 CS
    gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO0);  // M1 CS

    rcc_periph_clock_enable(RCC_ADC1);
    rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV2);

    // Disable ADC during configuration
    adc_power_off(ADC1);

    // Extend the conversion times to reduce noise impact
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);
    adc_set_right_aligned(ADC1);

    // Enable an interrupt after each scan run
    nvic_enable_irq(NVIC_ADC1_2_IRQ);
    nvic_set_priority(NVIC_ADC1_2_IRQ, 1);
    adc_enable_eoc_interrupt(ADC1);

    // Enable scan runs on timer 1 trigger output
    adc_enable_external_trigger_injected(ADC1, ADC_CR2_JEXTSEL_TIM1_TRGO);
    adc_enable_scan_mode(ADC1);

    // Configure the channels to be sampled in each scan run
    // the outputs will be in the ADC_JDRx registers
    uint8_t channel[] = {9, 13, 10};
    adc_set_injected_sequence(ADC1, 3, channel);

    adc_power_on(ADC1);

    // Wait >1.3us for ADC to be ready to perform conversions
    __asm__(  // wait 40 clock cycles @ 24MHz = ~1.6us
        "nop;nop;nop;nop;nop;"
        "nop;nop;nop;nop;nop;"  // 10
        "nop;nop;nop;nop;nop;"
        "nop;nop;nop;nop;nop;"  // 20
        "nop;nop;nop;nop;nop;"
        "nop;nop;nop;nop;nop;"  // 30
        "nop;nop;nop;nop;nop;"
        "nop;nop;nop;nop;nop;"  // 40
    );
    adc_reset_calibration(ADC1);
    adc_calibrate(ADC1);
}

void analogue_init(void) {
    init_adc_timer();
    init_adc();

    timer_enable_counter(TIM1);
}

static uint16_t convert_to_ma(uint16_t current_raw) {
    // voltage_mv = code * vref/4096
    // current_ma = (voltage_mv/rshunt) * Igain

    // current_ma = code * 3300/(4096 * 1100) * 7000
    return (uint16_t)((((uint32_t)current_raw * 2625) >> 9) & 0xffff);
}

static uint16_t convert_to_mv(uint16_t voltage_raw) {
    // meas_voltage_mv = code * vref/4096
    // voltage_mv = meas_voltage_mv * (R1 + R2)/(R2)

    // voltage_mv = (code * 3300/4096) * 5400/1100
    return (uint16_t)((((uint32_t)voltage_raw * 2025) >> 9) & 0xffff);
}

static uint16_t decay_filter(uint16_t in_sample, uint16_t prev_out_sample) {
    // Low-pass single-pole IIR filter, decay=0.972
    // y(n) = b1*x[n] + a0*y[n-1], a0 = decay, b1 = (1 - a0)
    int32_t decay = 256 - 249;  // 1 - 0.972 shifted 8 left
    int32_t intermediary = ((int32_t)in_sample - (int32_t)prev_out_sample);
    return (uint16_t)(prev_out_sample + ((decay * intermediary) >> 8));
}

void adc1_2_isr(void) {
    ADC1_SR = 0;
    check_output_faults();

    input_voltage = convert_to_mv(adc_read_injected(ADC1, 1));  // 12V
    uint16_t m0_current = convert_to_ma((uint16_t)(adc_read_injected(ADC1, 2) & 0xffff));  // M0 CS
    uint16_t m1_current = convert_to_ma((uint16_t)(adc_read_injected(ADC1, 3) & 0xffff));  // M1 CS

    output_data[0].current = decay_filter(m0_current, output_data[0].current);
    output_data[1].current = decay_filter(m1_current, output_data[1].current);

    // Light blue LEDs when the outputs are drawing more than 5 amps
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        if (output_data[i].current > 5000) {
            led_set((i == 0)?(LED_M0_B):(LED_M1_B));
        } else {
            led_clear((i == 0)?(LED_M0_B):(LED_M1_B));
        }
    }
}

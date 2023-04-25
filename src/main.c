#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>

#include "led.h"
#include "output.h"
#include "usart.h"
#include "analogue.h"

uint32_t *top_of_ram = ((uint32_t *)0x20001FF0);
#define BOOTLOADER_MAGIC 0xFACEBEE5

static void init(void) {
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_24MHZ]);

    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPAEN);
    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPBEN);
    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPCEN);
    led_init();
    output_init();
    usart_init();
    //analogue_init();

    led_set(LED_M0_R);
}

static void print_version(void) {
    printf("MCV4B:" FW_VER "\n");
}

static void enter_bootloader(void) {
    printf("Entering bootloader\n");
    *top_of_ram = BOOTLOADER_MAGIC;
    scb_reset_system();
}

typedef enum {
    STATE_INIT,
    STATE_SPEED0,
    STATE_SPEED1
} state_t;

static void fsm(int c) {
    int8_t i = c - 128;

    static state_t state = STATE_INIT;
    switch (state) {
        case STATE_INIT:
            switch (i) {
                case -128:
                    state = STATE_INIT;
                    break;
                case -127:
                    state = STATE_INIT;
                    print_version();
                    break;
                case -126:
                    state = STATE_SPEED0;
                    break;
                case -125:
                    state = STATE_SPEED1;
                    break;
                case -124:
                    enter_bootloader();
                    break;
                default:
                    state = STATE_INIT;
                    break;
            }
            break;
        case STATE_SPEED0:
            state = STATE_INIT;
            if (i != -128) {
                set_output(0, i);
            }
            break;
        case STATE_SPEED1:
            state = STATE_INIT;
            if (i != -128) {
                set_output(1, i);
            }
            break;
        default:
            state = STATE_INIT;
            break;
    }
}


int main(void) {
    /* Check to see if we should jump into the bootloader */
    if (*top_of_ram == BOOTLOADER_MAGIC) {
        *top_of_ram = 0;
        __asm__ __volatile__(
            "ldr r0, =0x1FFFF000;"  // load bootloader address into r0
            "ldr sp,[r0, #0];"      // Load bootloader address into stack pointer
            "ldr r0,[r0, #4];"      // Load bootloader start address into r0 (addr+4)
            "bx r0;"                // Jump to address in r0
        );
    }

    init();

    while (1) {
        int c = usart_get_char();
        fsm(c);
    }

    return 0;
}

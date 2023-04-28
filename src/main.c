#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/cm3/scb.h>

#include "led.h"
#include "output.h"
#include "usart.h"
#include "analogue.h"
#include "msg_handler.h"


static void init(void) {
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_24MHZ]);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    led_init();
    output_init();
    usart_init();
    analogue_init();

    // Configure watchdog. Period: 50ms
    iwdg_set_period_ms(50);
    iwdg_start();
}

static void enter_bootloader(void) {
    *top_of_ram = 0;  // reset bootloader signature so we don't repeatedly enter bootloader
    __asm__ __volatile__(
        "ldr r0, =0x1FFFF000;"  // load bootloader address into r0
        "ldr sp,[r0, #0];"      // Load bootloader address into stack pointer
        "ldr r0,[r0, #4];"      // Load bootloader start address into r0 (addr+4)
        "bx r0;"                // Jump to address in r0
    );
}

int main(void) {
    /* Check to see if we should jump into the bootloader */
    if (*top_of_ram == BOOTLOADER_MAGIC) {
        enter_bootloader();
    }

    init();

    while (1) {
        iwdg_reset();
        uint16_t c = usart_get_char();
        if (c & 0x100) {  // skip parity errors
            continue;
        }
        process_received_data((char)(c & 0xff));

        if (*top_of_ram == BOOTLOADER_MAGIC) {
            scb_reset_system();  // reset MCU to enter bootloader
        }
    }
    return 0;
}

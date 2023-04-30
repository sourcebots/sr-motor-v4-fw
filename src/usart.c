#include "usart.h"

#include <stdint.h>
#include <string.h>
#include <libopencm3/stm32/iwdg.h>
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/usart.h"

void usart_init(void) {
    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_USART1EN);

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);

    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
    usart_set_mode(USART1, USART_MODE_TX_RX);

    usart_enable(USART1);
}

uint16_t usart_get_char(void) {
    // Wait until the data is ready to be received.
    while ((USART_SR(USART1) & USART_SR_RXNE) == 0)iwdg_reset();
    return usart_recv(USART1);
}

int usart_send_string(char* str) {
    uint16_t len = strlen(str);

    for (uint16_t i=0; i<len; i++) {
        iwdg_reset();
        if (str[i] == '\0') {
            usart_send_blocking(USART1, '\n');
        } else {
            usart_send_blocking(USART1, str[i]);
        }
    }

    return len;
}

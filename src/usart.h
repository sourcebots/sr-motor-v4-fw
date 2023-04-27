#pragma once

#include <stdint.h>

void usart_init(void);

uint16_t usart_get_char(void);
int usart_send_string(char* str);

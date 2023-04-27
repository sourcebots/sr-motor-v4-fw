#pragma once

extern uint32_t* top_of_ram;  // defined in msg_handler.c
#define BOOTLOADER_MAGIC 0xFACEBEE5

void process_received_data(char new_data);
void handle_msg(char* buf, char* response, int max_len);
void enter_bootloader_next_cycle(void);

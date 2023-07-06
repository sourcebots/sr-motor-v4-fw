#pragma once

#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/f1/bkp.h>

#define bootloader_flag (BKP_DR1 & 0xFFFF)
#define BOOTLOADER_SIGNATURE 0xBEE5

void process_received_data(char new_data);
void handle_msg(char* buf, char* response, int max_len);
void enter_bootloader_next_cycle(void);

static inline void set_bootloader_signature(uint32_t signature) {
    pwr_disable_backup_domain_write_protect();
    // The backup registers are not cleared on reset
    // They are 16 bit registers and register 1 is used to signal when
    // the bootloader should be entered
    BKP_DR1 = signature;
    pwr_enable_backup_domain_write_protect();
}

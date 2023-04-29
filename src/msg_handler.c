#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "msg_handler.h"
#include "output.h"
#include "analogue.h"
#include "usart.h"

#define BOARD_NAME_SHORT "MCv4B"
#define MSG_MAXLEN 64
#define USB_BUFFER_SIZE 64

uint32_t* top_of_ram = ((uint32_t *)0x20001FF0);
const char serialnum[] = "XXXXXXXXXXXXXXX";
char msg_buffer[MSG_MAXLEN];
int current_msg_len = 0;

static char* itoa(int value, char* string);

static void append_str(char* dest, const char* src, int dest_max_len) {
    strncat(dest, src, dest_max_len - strlen(dest));
}
static char* get_next_arg(char* response, const char* err_msg, int max_len) {
    char* next_arg = strtok(NULL, ":");
    if (next_arg == NULL) {
        strncat(response, err_msg, max_len);
        return NULL;
    }
    return next_arg;
}

void process_received_data(char new_data) {
    if (new_data == '\n') {
        msg_buffer[current_msg_len] = '\0'; // add null terminator to make it a string

        char response_buffer[USB_BUFFER_SIZE];

        handle_msg(msg_buffer, response_buffer, (USB_BUFFER_SIZE - 2));
        current_msg_len = 0;

        uint16_t resp_len = strlen(response_buffer);
        response_buffer[resp_len++] = '\n';
        response_buffer[resp_len] = '\0';
        usart_send_string(response_buffer);
    } else if (new_data == '\r') {
        // Drop carriage returns
    } else {
        msg_buffer[current_msg_len] = new_data;
        current_msg_len++;

        // drop a full buffer without newlines
        if (current_msg_len == (MSG_MAXLEN - 1)) {
            current_msg_len = 0;
        }
    }
}

void handle_msg(char* buf, char* response, int max_len) {
    // max_len is the maximum length of the string that can be fitted in buf
    // so the buffer must be at least max_len+1 long
    char temp_str[12] = {0};  // for doing itoa conversions
    response[0] = '\0';  // make a blank string

    char* next_arg = strtok(buf, ":");
    if (strcmp(next_arg, "MOT") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing motor number", max_len);
        if(next_arg == NULL) {return;}

        unsigned long int output_num;

        if (isdigit((int)next_arg[0])) {
            output_num = strtoul(next_arg, NULL, 10);
            // bounds check
            if (output_num >= 2) {
                append_str(response, "NACK:Invalid motor number", max_len);
                return;
            }
        } else {
            append_str(response, "NACK:Missing motor number", max_len);
            return;
        }

        next_arg = get_next_arg(response, "NACK:Missing motor command", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "SET") == 0) {
            next_arg = get_next_arg(response, "NACK:Missing motor power", max_len);
            if(next_arg == NULL) {return;}
            if (!(isdigit((int)next_arg[0]) || (next_arg[0] == '-'))) {
                append_str(response, "NACK:Invalid motor power", max_len);
                return;
            }

            long int output_val = strtol(next_arg, NULL, 10);

            // bounds check
            if (output_val < MIN_MOTOR_VAL || output_val > MAX_MOTOR_VAL) {
                append_str(response, "NACK:Invalid motor power", max_len);
                return;
            }
            // Set motor power
            output_set_power((uint8_t) output_num, (int16_t) output_val);

            append_str(response, "ACK", max_len);
            return;
        } else if (strcmp(next_arg, "GET?") == 0) {
            // Get motor value
            append_str(response, output_enabled((uint8_t)output_num)?"1":"0", max_len);
            append_str(response, ":", max_len);
            append_str(response, itoa(output_get_output((uint8_t)output_num), temp_str), max_len);
            return;
        } else if (strcmp(next_arg, "DISABLE") == 0) {
            // Disable motor
            output_disable((uint8_t)output_num);

            append_str(response, "ACK", max_len);
            return;
        } else if (strcmp(next_arg, "I?") == 0) {
            append_str(response, itoa(output_get_current((uint8_t)output_num), temp_str), max_len);
            return;
        } else {
            append_str(response, "NACK:Unknown motor command", max_len);
            return;
        }
    } else if (strcmp(next_arg, "*IDN?") == 0) {
        // Identifier string: manufacturer, board name, asset tag, version
        append_str(response, "Student Robotics:" BOARD_NAME_SHORT ":", max_len);
        append_str(response, serialnum, max_len);
        append_str(response, ":" FW_VER, max_len);
        return;
    } else if (strcmp(next_arg, "*STATUS?") == 0) {
        append_str(response, (output_data[0].in_fault)?"1":"0", max_len);
        append_str(response, ",", max_len);
        append_str(response, (output_data[1].in_fault)?"1":"0", max_len);
        append_str(response, ":", max_len);
        append_str(response, itoa(input_voltage, temp_str), max_len);
        return;
    } else if (strcmp(next_arg, "*RESET") == 0) {
        outputs_reset();

        append_str(response, "ACK", max_len);
        return;
    } else if (strcmp(next_arg, "*SYS") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing system command", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "BOOTLOADER") == 0) {
            // enter bootloader after sending ack
            enter_bootloader_next_cycle();

            append_str(response, "ACK\n\n", max_len);
            return;
        }

        append_str(response, "NACK:Invalid system command", max_len);
        return;
    } else if (strcmp(next_arg, "ECHO") == 0) {
        next_arg = strtok(NULL, ":");

        append_str(response, next_arg, max_len);
        return;
    } else {
        append_str(response, "NACK:Unknown command: '", max_len);
        append_str(response, next_arg, max_len);
        append_str(response, "'", max_len);
        return;
    }

    // This should be unreachable
    append_str(response, "NACK:Unknown error", max_len);
    return;
}

static char* itoa(int value, char* string) {
    // string must be a buffer of at least 12 chars
    // including stdio.h to get sprintf overflows the rom
    char tmp[11];
    char* tmp_ptr = tmp;
    char* sp = string;
    unsigned int digit;
    unsigned int remaining;
    bool sign;

    if ( string == NULL ) {
        return 0;
    }

    sign = (value < 0);
    if (sign) {
        remaining = -value;
    } else {
        remaining = (unsigned int)value;
    }

    while (remaining || tmp_ptr == tmp) {
        digit = remaining % 10;
        remaining /= 10;
        *tmp_ptr = digit + '0';
        tmp_ptr++;
    }

    if (sign) {
        *sp = '-';
        sp++;
    }

    // string is in reverse at this point
    while (tmp_ptr > tmp) {
        tmp_ptr--;
        *sp = *tmp_ptr;
        sp++;
    }
    *sp = '\0';

    return string;
}

void enter_bootloader_next_cycle(void) {
    // Set the signature to enter bootloader at next reset
    // the main loop will trigger a reset at the end of this transaction
    *top_of_ram = BOOTLOADER_MAGIC;
}

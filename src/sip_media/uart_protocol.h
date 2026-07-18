#ifndef WIBOX_UART_PROTOCOL_H
#define WIBOX_UART_PROTOCOL_H

#include <stddef.h>

typedef enum {
    UART_CODE_UNKNOWN = 0,
    UART_CODE_ALARM_REPORT,
    UART_CODE_CMD_RESET,
    UART_CODE_STA_TO_AP,
    UART_CODE_START_CALL,
    UART_CODE_HANG_UP_0,
    UART_CODE_HANG_UP_1,
    UART_CODE_PHYSICAL_HANDSET_ANSWERED,
    UART_CODE_PUSH_STATE_0,
    UART_CODE_PUSH_STATE_1,
    UART_CODE_MCU_STATE_0,
    UART_CODE_MCU_STATE_1,
    UART_CODE_CMD_DOWN_LONG_1,
    UART_CODE_CMD_DOWN_LONG_2
} uart_code_t;

typedef struct {
    uart_code_t code;
    const char *name;
    const char *event_type;
    unsigned char bytes[4];
} uart_code_def_t;

const uart_code_def_t *uart_protocol_find(const unsigned char frame[4]);
int uart_protocol_parse_control_frame(const char *input, unsigned char frame[4]);
void uart_protocol_format_bytes(const unsigned char *data, size_t len,
                                char *out, size_t out_size);

#endif

#include "uart_protocol.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    static const unsigned char frames[][4] = {
        {0xFB, 0x11, 0x00, 0x1C}, {0xFB, 0x20, 0x00, 0x2B},
        {0xFB, 0x21, 0x00, 0x2C},
        {0xFB, 0x14, 0x01, 0x20}, {0xFB, 0x13, 0x00, 0x1E},
        {0xFB, 0x13, 0x01, 0x1F}, {0xFB, 0x23, 0x00, 0x2E},
        {0xFB, 0x19, 0x00, 0x24}, {0xFB, 0x19, 0x01, 0x25},
        {0xFB, 0x16, 0x00, 0x21}, {0xFB, 0x16, 0x01, 0x22},
        {0xFB, 0x24, 0x01, 0x30}, {0xFB, 0x24, 0x02, 0x31}
    };
    unsigned char parsed[4];
    unsigned char unknown[4] = {0xFB, 0x7f, 0x00, 0x00};
    char formatted[32];
    char small[5];
    size_t i;

    CHECK(uart_protocol_find(NULL) == NULL);
    for (i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        const uart_code_def_t *def = uart_protocol_find(frames[i]);
        CHECK(def != NULL);
        CHECK(def->code == (uart_code_t)(i + 1));
        CHECK(def->name[0] != '\0');
        CHECK(def->event_type[0] != '\0');
    }
    CHECK(uart_protocol_find(unknown) == NULL);

    CHECK(uart_protocol_parse_control_frame("UART FB 11 00 1C", parsed) == 0);
    CHECK(memcmp(parsed, frames[0], 4) == 0);
    CHECK(uart_protocol_parse_control_frame("UART:0xFB-0X23-00-2e", parsed) == 0);
    CHECK(memcmp(parsed, frames[6], 4) == 0);
    CHECK(uart_protocol_parse_control_frame("UART \\xFB \\x13 \\x01 \\x1F  ", parsed) == 0);
    CHECK(memcmp(parsed, frames[5], 4) == 0);
    CHECK(uart_protocol_parse_control_frame(NULL, parsed) == -1);
    CHECK(uart_protocol_parse_control_frame("UART FB 11 00 1C", NULL) == -1);
    CHECK(uart_protocol_parse_control_frame("NOPE FB 11 00 1C", parsed) == -1);
    CHECK(uart_protocol_parse_control_frame("UART FB 11 00", parsed) == -1);
    CHECK(uart_protocol_parse_control_frame("UART FB 11 00 GG", parsed) == -1);
    CHECK(uart_protocol_parse_control_frame("UART \\qFB 11 00 1C", parsed) == -1);
    CHECK(uart_protocol_parse_control_frame("UART FB 11 00 1C FF", parsed) == -1);
    CHECK(uart_protocol_parse_control_frame("UART", parsed) == -1);

    uart_protocol_format_bytes(frames[0], 4, formatted, sizeof(formatted));
    CHECK(strcmp(formatted, "FB 11 00 1C") == 0);
    uart_protocol_format_bytes(NULL, 4, formatted, sizeof(formatted));
    CHECK(strcmp(formatted, "") == 0);
    uart_protocol_format_bytes(frames[0], 4, small, sizeof(small));
    CHECK(strcmp(small, "FB") == 0);
    uart_protocol_format_bytes(frames[0], 0, formatted, sizeof(formatted));
    CHECK(strcmp(formatted, "") == 0);
    uart_protocol_format_bytes(frames[0], 4, formatted, 0);
    uart_protocol_format_bytes(frames[0], 4, NULL, 0);

    printf("RESULT uart_protocol PASS frames=%zu\n",
           sizeof(frames) / sizeof(frames[0]));
    return 0;
}

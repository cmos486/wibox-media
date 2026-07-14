#include "intercom.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int access_result;
static int open_result = 42;
static ssize_t write_result = 4;
static int close_count;
static int publish_count;
static int publish_contract_failed;
static unsigned char last_write[4];
static unsigned char last_publish[4];
static char last_path[128];
static char last_event[64];
static char last_alias[64];

int __real_access(const char *path, int mode);
int __real_open(const char *path, int flags, ...);
ssize_t __real_write(int fd, const void *buffer, size_t len);
int __real_close(int fd);

int __wrap_access(const char *path, int mode)
{
    (void)mode;
    if (strcmp(path, "/dev/ttySGK1") != 0) {
        return __real_access(path, mode);
    }
    snprintf(last_path, sizeof(last_path), "%s", path);
    if (access_result != 0) errno = ENOENT;
    return access_result;
}

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list args;

    if (strcmp(path, "/dev/ttySGK1") != 0) {
        if (flags & O_CREAT) {
            va_start(args, flags);
            mode = va_arg(args, mode_t);
            va_end(args);
            return __real_open(path, flags, mode);
        }
        return __real_open(path, flags);
    }
    snprintf(last_path, sizeof(last_path), "%s", path);
    if (open_result < 0) errno = ENODEV;
    return open_result;
}

ssize_t __wrap_write(int fd, const void *buffer, size_t len)
{
    if (fd != 42) {
        return __real_write(fd, buffer, len);
    }
    if (buffer && len >= sizeof(last_write)) {
        memcpy(last_write, buffer, sizeof(last_write));
    }
    if (write_result < 0) errno = EIO;
    return write_result;
}

int __wrap_close(int fd)
{
    if (fd != 42) {
        return __real_close(fd);
    }
    close_count++;
    return 0;
}

void mqtt_publish_uart_event_ex(const char *event_type, const char *alias,
                                const char *direction,
                                const unsigned char *raw, size_t raw_len,
                                int param, int known)
{
    if (raw_len != 4 || strcmp(direction, "out") != 0 ||
        !raw || param != raw[2] || known != 1) {
        publish_contract_failed = 1;
        return;
    }
    snprintf(last_event, sizeof(last_event), "%s", event_type);
    snprintf(last_alias, sizeof(last_alias), "%s", alias);
    memcpy(last_publish, raw, sizeof(last_publish));
    publish_count++;
}

int main(void)
{
    static const struct {
        intercom_cmd_t command;
        unsigned char frame[4];
        const char *event;
        const char *alias;
    } cases[] = {
        {INTERCOM_CMD_UNLOCK_DOOR,        {0xFB, 0x12, 0x01, 0x1E}, "unlock_door",        "UNLOCK_DOOR"},
        {INTERCOM_CMD_START_CALL,         {0xFB, 0x14, 0x01, 0x20}, "start_call",         "START_CALL"},
        {INTERCOM_CMD_STOP_CALL,          {0xFB, 0x14, 0x00, 0x1F}, "stop_call",          "STOP_CALL"},
        {INTERCOM_CMD_ENABLE_PUSH_STATE,  {0xFB, 0x19, 0x01, 0x25}, "enable_push_state",  "ENABLE_PUSH_STATE"},
        {INTERCOM_CMD_DISABLE_PUSH_STATE, {0xFB, 0x19, 0x00, 0x24}, "disable_push_state", "DISABLE_PUSH_STATE"},
        {INTERCOM_CMD_F1_ON,              {0xFB, 0x17, 0x01, 0x23}, "f1_on",              "F1_ON"},
        {INTERCOM_CMD_F1_OFF,             {0xFB, 0x17, 0x00, 0x22}, "f1_off",             "F1_OFF"}
    };
    size_t i;

    intercom_cleanup();
    CHECK(intercom_send_command(INTERCOM_CMD_START_CALL) == -1);
    access_result = -1;
    CHECK(intercom_init() == 0);
    CHECK(strcmp(last_path, "/dev/ttySGK1") == 0);
    CHECK(intercom_send_command((intercom_cmd_t)99) == -1);

    open_result = -1;
    CHECK(intercom_send_command(INTERCOM_CMD_START_CALL) == -1);
    open_result = 42;
    write_result = 3;
    CHECK(intercom_send_command(INTERCOM_CMD_START_CALL) == -1);
    CHECK(close_count == 1);

    write_result = 4;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int previous_publish = publish_count;
        CHECK(intercom_send_command(cases[i].command) == 0);
        CHECK(memcmp(last_write, cases[i].frame, 4) == 0);
        CHECK(memcmp(last_publish, cases[i].frame, 4) == 0);
        CHECK(strcmp(last_event, cases[i].event) == 0);
        CHECK(strcmp(last_alias, cases[i].alias) == 0);
        CHECK(strcmp(last_path, "/dev/ttySGK1") == 0);
        CHECK(publish_count == previous_publish + 1);
    }

    intercom_cleanup();
    CHECK(intercom_send_command(INTERCOM_CMD_UNLOCK_DOOR) == -1);
    CHECK(publish_contract_failed == 0);
    printf("RESULT intercom_hw_mock PASS commands=%zu\n",
           sizeof(cases) / sizeof(cases[0]));
    return 0;
}

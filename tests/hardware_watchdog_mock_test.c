#include "hardware_watchdog.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

enum failure_mode {
    FAIL_NONE,
    FAIL_OPEN,
    FAIL_SETTIMEOUT,
    FAIL_ENABLE,
    FAIL_KEEPALIVE_BOTH,
    FAIL_KEEPALIVE_IOCTL
};

static enum failure_mode failure;
static int actual_timeout;
static int keepalive_count;
static int disarm_count;
static int magic_close_count;
static int close_count;
static int fake_clock_enabled;
static unsigned long long fake_now_ms;
static int clock_call_count;

int __real_open(const char *path, int flags, ...);
int __real_fcntl(int fd, int command, ...);
ssize_t __real_write(int fd, const void *buffer, size_t length);
int __real_close(int fd);
int __real_clock_gettime(clockid_t clock_id, struct timespec *value);

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list args;

    if (strcmp(path, "/dev/test-watchdog") == 0) {
        if (failure == FAIL_OPEN) {
            errno = ENOENT;
            return -1;
        }
        return 77;
    }
    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
        return __real_open(path, flags, mode);
    }
    return __real_open(path, flags);
}

int __wrap_fcntl(int fd, int command, ...)
{
    va_list args;
    long value = 0;

    if (fd == 77) {
        return command == F_GETFD ? 0 : 0;
    }
    if (command == F_GETFD || command == F_GETFL) {
        return __real_fcntl(fd, command);
    }
    va_start(args, command);
    value = va_arg(args, long);
    va_end(args);
    return __real_fcntl(fd, command, value);
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    void *argument;
    va_list args;

    va_start(args, request);
    argument = va_arg(args, void *);
    va_end(args);
    if (fd != 77) {
        errno = ENOTTY;
        return -1;
    }
    if (request == WDIOC_SETTIMEOUT) {
        if (failure == FAIL_SETTIMEOUT) {
            errno = EIO;
            return -1;
        }
        if (actual_timeout > 0) *(int *)argument = actual_timeout;
        return 0;
    }
    if (request == WDIOC_SETOPTIONS) {
        int options = *(int *)argument;
        if (options == WDIOS_ENABLECARD && failure == FAIL_ENABLE) {
            errno = EIO;
            return -1;
        }
        if (options == WDIOS_DISABLECARD) disarm_count++;
        return 0;
    }
    if (request == WDIOC_KEEPALIVE) {
        if (failure == FAIL_KEEPALIVE_BOTH || failure == FAIL_KEEPALIVE_IOCTL) {
            errno = EIO;
            return -1;
        }
        __atomic_add_fetch(&keepalive_count, 1, __ATOMIC_SEQ_CST);
        return 0;
    }
    errno = ENOTTY;
    return -1;
}

ssize_t __wrap_write(int fd, const void *buffer, size_t length)
{
    const char *bytes = (const char *)buffer;

    if (fd != 77) return __real_write(fd, buffer, length);
    if (length == 1 && bytes[0] == '\0') {
        if (failure == FAIL_KEEPALIVE_BOTH) {
            errno = EIO;
            return -1;
        }
        __atomic_add_fetch(&keepalive_count, 1, __ATOMIC_SEQ_CST);
        return 1;
    }
    if (length == 1 && bytes[0] == 'V') {
        magic_close_count++;
        return 1;
    }
    return -1;
}

int __wrap_close(int fd)
{
    if (fd != 77) return __real_close(fd);
    close_count++;
    return 0;
}

int __wrap_clock_gettime(clockid_t clock_id, struct timespec *value)
{
    if (!fake_clock_enabled || clock_id != CLOCK_MONOTONIC) {
        return __real_clock_gettime(clock_id, value);
    }
    {
        unsigned long long now = __atomic_load_n(&fake_now_ms, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&clock_call_count, 1, __ATOMIC_SEQ_CST);
        value->tv_sec = (time_t)(now / 1000ULL);
        value->tv_nsec = (long)((now % 1000ULL) * 1000000ULL);
    }
    return 0;
}

static void reset_mock(void)
{
    failure = FAIL_NONE;
    actual_timeout = 0;
    __atomic_store_n(&keepalive_count, 0, __ATOMIC_SEQ_CST);
    disarm_count = 0;
    magic_close_count = 0;
    close_count = 0;
    fake_clock_enabled = 0;
    __atomic_store_n(&fake_now_ms, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&clock_call_count, 0, __ATOMIC_SEQ_CST);
}

int main(void)
{
    const char *guard = "/tmp/wibox-firmware-update-critical";
    FILE *fp;
    int fed;
    int attempts;

    unlink(guard);
    reset_mock();
    CHECK(hardware_watchdog_start(0, "/dev/test-watchdog", 30, 5) == 0);
    CHECK(hardware_watchdog_start(1, NULL, 30, 5) == -1);
    CHECK(hardware_watchdog_start(1, "", 30, 5) == -1);
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 4, 1) == -1);
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 0) == -1);
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 10, 5) == -1);

    fp = fopen(guard, "w");
    CHECK(fp != NULL);
    fputs("state=PREPARE\n", fp);
    fclose(fp);
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 5) == 0);
    CHECK(close_count == 0);
    unlink(guard);

    failure = FAIL_OPEN;
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 5) == -1);
    reset_mock();
    failure = FAIL_SETTIMEOUT;
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 5) == -1);
    CHECK(disarm_count == 1 && close_count == 1);
    reset_mock();
    actual_timeout = 2;
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 1) == -1);
    CHECK(disarm_count == 1 && close_count == 1);
    reset_mock();
    failure = FAIL_ENABLE;
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 5) == -1);
    CHECK(disarm_count == 1 && close_count == 1);
    reset_mock();
    failure = FAIL_KEEPALIVE_BOTH;
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 5) == -1);
    CHECK(disarm_count == 1 && close_count == 1);

    reset_mock();
    fake_clock_enabled = 1;
    __atomic_store_n(&fake_now_ms, 1000, __ATOMIC_SEQ_CST);
    failure = FAIL_KEEPALIVE_IOCTL;
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 5, 1) == 0);
    CHECK(__atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST) == 1);
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 5, 1) == 0);
    for (attempts = 0;
         attempts < 50 &&
         __atomic_load_n(&clock_call_count, __ATOMIC_SEQ_CST) < 3;
         attempts++) {
        usleep(10000);
    }
    CHECK(__atomic_load_n(&clock_call_count, __ATOMIC_SEQ_CST) >= 3);
    __atomic_store_n(&fake_now_ms, 2200, __ATOMIC_SEQ_CST);
    for (attempts = 0;
         attempts < 50 &&
         __atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST) < 2;
         attempts++) {
        usleep(10000);
    }
    CHECK(__atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST) >= 2);
    hardware_watchdog_heartbeat();
    fed = __atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST);
    __atomic_store_n(&fake_now_ms, 5000, __ATOMIC_SEQ_CST);
    usleep(150000);
    CHECK(__atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST) == fed);
    __atomic_store_n(&fake_now_ms, 5100, __ATOMIC_SEQ_CST);
    hardware_watchdog_heartbeat();
    for (attempts = 0;
         attempts < 50 &&
         __atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST) <= fed;
         attempts++) {
        usleep(10000);
    }
    CHECK(__atomic_load_n(&keepalive_count, __ATOMIC_SEQ_CST) > fed);
    hardware_watchdog_stop(1);
    CHECK(disarm_count == 1 && close_count == 1);
    hardware_watchdog_stop(1);

    reset_mock();
    CHECK(hardware_watchdog_start(1, "/dev/test-watchdog", 30, 5) == 0);
    hardware_watchdog_stop(0);
    CHECK(disarm_count == 0 && close_count == 1);
    unlink(guard);
    printf("RESULT hardware_watchdog_mock PASS\n");
    return 0;
}

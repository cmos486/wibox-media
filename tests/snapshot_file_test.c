#include "snapshot_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

enum failure_mode {
    FAIL_NONE,
    FAIL_WRITE,
    FAIL_FSYNC,
    FAIL_CLOSE,
    FAIL_RENAME
};

static enum failure_mode failure;
static int snapshot_fd = -1;
static int partial_write;
static int interrupted_write;

int __real_open(const char *path, int flags, ...);
ssize_t __real_write(int fd, const void *data, size_t size);
int __real_fsync(int fd);
int __real_close(int fd);
int __real_rename(const char *old_path, const char *new_path);

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list args;
    int fd;

    if (flags & O_CREAT) {
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
        fd = __real_open(path, flags, mode);
    } else {
        fd = __real_open(path, flags);
    }
    if (fd >= 0 && strstr(path, "wibox-snapshot-file-test") &&
        strstr(path, ".tmp")) {
        snapshot_fd = fd;
    }
    return fd;
}

ssize_t __wrap_write(int fd, const void *data, size_t size)
{
    if (fd == snapshot_fd && interrupted_write) {
        interrupted_write = 0;
        errno = EINTR;
        return -1;
    }
    if (fd == snapshot_fd && failure == FAIL_WRITE) {
        errno = EIO;
        return -1;
    }
    if (fd == snapshot_fd && partial_write && size > 1) {
        partial_write = 0;
        return __real_write(fd, data, size / 2);
    }
    return __real_write(fd, data, size);
}

int __wrap_fsync(int fd)
{
    if (fd == snapshot_fd && failure == FAIL_FSYNC) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}

int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (fd == snapshot_fd) {
        snapshot_fd = -1;
        if (failure == FAIL_CLOSE) {
            errno = EIO;
            return -1;
        }
    }
    return result;
}

int __wrap_rename(const char *old_path, const char *new_path)
{
    if (failure == FAIL_RENAME) {
        errno = EIO;
        return -1;
    }
    return __real_rename(old_path, new_path);
}

static void reset_failure(void)
{
    failure = FAIL_NONE;
    snapshot_fd = -1;
    partial_write = 0;
    interrupted_write = 0;
}

int main(void)
{
    static const unsigned char jpeg[] = {0xff, 0xd8, 1, 2, 3, 0xff, 0xd9};
    static const unsigned char replacement[] = {0xff, 0xd8, 9, 8, 0xff, 0xd9};
    const char *path = "/tmp/wibox-snapshot-file-test.jpg";
    char temporary[256];
    unsigned char readback[16];
    char long_path[300];
    FILE *input;

    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    unlink(path);
    unlink(temporary);
    CHECK(snapshot_file_write_atomic(NULL, jpeg, sizeof(jpeg)) == -1);
    CHECK(snapshot_file_write_atomic("", jpeg, sizeof(jpeg)) == -1);
    CHECK(snapshot_file_write_atomic(path, NULL, sizeof(jpeg)) == -1);
    CHECK(snapshot_file_write_atomic(path, jpeg, 3) == -1);
    CHECK(snapshot_file_write_atomic(path, jpeg, sizeof(jpeg)) == 0);
    CHECK(access(path, R_OK) == 0 && access(temporary, F_OK) != 0);

    CHECK(snapshot_file_write_atomic(path, replacement, sizeof(replacement)) == 0);
    input = fopen(path, "rb");
    CHECK(input != NULL);
    CHECK(fread(readback, 1, sizeof(readback), input) == sizeof(replacement));
    fclose(input);
    CHECK(memcmp(readback, replacement, sizeof(replacement)) == 0);

    unlink(path);
    partial_write = 1;
    interrupted_write = 1;
    CHECK(snapshot_file_write_atomic(path, jpeg, sizeof(jpeg)) == 0);
    CHECK(access(path, R_OK) == 0 && access(temporary, F_OK) != 0);

    reset_failure();
    unlink(path);
    failure = FAIL_WRITE;
    CHECK(snapshot_file_write_atomic(path, jpeg, sizeof(jpeg)) == -1);
    CHECK(access(path, F_OK) != 0 && access(temporary, F_OK) != 0);

    reset_failure();
    failure = FAIL_FSYNC;
    CHECK(snapshot_file_write_atomic(path, jpeg, sizeof(jpeg)) == -1);
    CHECK(access(path, F_OK) != 0 && access(temporary, F_OK) != 0);

    reset_failure();
    failure = FAIL_CLOSE;
    CHECK(snapshot_file_write_atomic(path, jpeg, sizeof(jpeg)) == -1);
    CHECK(access(path, F_OK) != 0 && access(temporary, F_OK) != 0);

    reset_failure();
    failure = FAIL_RENAME;
    CHECK(snapshot_file_write_atomic(path, jpeg, sizeof(jpeg)) == -1);
    CHECK(access(path, F_OK) != 0 && access(temporary, F_OK) != 0);
    reset_failure();

    memset(long_path, 'x', sizeof(long_path));
    long_path[0] = '/';
    long_path[sizeof(long_path) - 1] = '\0';
    CHECK(snapshot_file_write_atomic(long_path, jpeg, sizeof(jpeg)) == -1);
    CHECK(snapshot_file_write_atomic("/tmp/no-such-wibox-dir/image.jpg",
                                     jpeg, sizeof(jpeg)) == -1);
    unlink(path);
    unlink(temporary);
    printf("RESULT snapshot_file PASS\n");
    return 0;
}

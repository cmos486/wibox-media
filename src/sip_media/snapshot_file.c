#include "snapshot_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int snapshot_file_write_atomic(const char *path,
                               const uint8_t *data,
                               size_t size)
{
    char temporary_path[256];
    size_t offset = 0;
    int output_fd = -1;
    int path_len;

    if (!path || !path[0] || !data || size < 4) {
        return -1;
    }
    path_len = snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path);
    if (path_len <= 0 || (size_t)path_len >= sizeof(temporary_path)) {
        return -1;
    }
    output_fd = open(temporary_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        return -1;
    }
    while (offset < size) {
        ssize_t written = write(output_fd, data + offset, size - offset);
        if (written > 0) {
            offset += (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        close(output_fd);
        unlink(temporary_path);
        return -1;
    }
    if (fsync(output_fd) != 0) {
        close(output_fd);
        unlink(temporary_path);
        return -1;
    }
    if (close(output_fd) != 0) {
        unlink(temporary_path);
        return -1;
    }
    if (rename(temporary_path, path) != 0) {
        unlink(temporary_path);
        return -1;
    }
    return 0;
}

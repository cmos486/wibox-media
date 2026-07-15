#ifndef WIBOX_SNAPSHOT_FILE_H
#define WIBOX_SNAPSHOT_FILE_H

#include <stddef.h>
#include <stdint.h>

int snapshot_file_write_atomic(const char *path,
                               const uint8_t *data,
                               size_t size);

#endif

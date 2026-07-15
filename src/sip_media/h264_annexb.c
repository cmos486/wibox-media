#include "h264_annexb.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t *find_start_code(const uint8_t *cursor,
                                      const uint8_t *end,
                                      int *start_code_len)
{
    while (cursor < end) {
        if ((size_t)(end - cursor) >= 3 &&
            cursor[0] == 0 && cursor[1] == 0 && cursor[2] == 1) {
            *start_code_len = 3;
            return cursor;
        }
        if ((size_t)(end - cursor) >= 4 &&
            cursor[0] == 0 && cursor[1] == 0 &&
            cursor[2] == 0 && cursor[3] == 1) {
            *start_code_len = 4;
            return cursor;
        }
        cursor++;
    }
    return NULL;
}

void h264_nal_cache_clear(h264_nal_cache_t *cache)
{
    if (!cache) {
        return;
    }
    free(cache->data);
    cache->data = NULL;
    cache->len = 0;
}

static int nal_cache_set(h264_nal_cache_t *cache,
                         const uint8_t *nal, size_t nal_len)
{
    uint8_t *copy;

    if (!cache) {
        return 0;
    }
    copy = malloc(nal_len);
    if (!copy) {
        return -1;
    }
    memcpy(copy, nal, nal_len);
    free(cache->data);
    cache->data = copy;
    cache->len = nal_len;
    return 0;
}

static void scan_nal(const uint8_t *nal, size_t nal_len,
                     h264_annexb_scan_t *scan,
                     h264_nal_cache_t *sps_cache,
                     h264_nal_cache_t *pps_cache)
{
    int nal_type;

    if (!nal || nal_len == 0) {
        return;
    }
    nal_type = nal[0] & 0x1f;
    if (scan->first_nal < 0) {
        scan->first_nal = nal_type;
    }
    scan->last_nal = nal_type;
    scan->nal_count++;
    if (nal_type == 7) {
        scan->has_sps = 1;
        (void)nal_cache_set(sps_cache, nal, nal_len);
    } else if (nal_type == 8) {
        scan->has_pps = 1;
        (void)nal_cache_set(pps_cache, nal, nal_len);
    } else if (nal_type == 5) {
        scan->has_idr = 1;
        scan->has_slice = 1;
    } else if (nal_type == 1) {
        scan->has_slice = 1;
    }
}

int h264_annexb_scan(const uint8_t *data, size_t len,
                     h264_annexb_scan_t *scan,
                     h264_nal_cache_t *sps_cache,
                     h264_nal_cache_t *pps_cache)
{
    const uint8_t *end;
    const uint8_t *start_code;
    int start_code_len = 0;

    if (!scan) {
        return -1;
    }
    memset(scan, 0, sizeof(*scan));
    scan->first_nal = -1;
    scan->last_nal = -1;
    if (!data || len == 0) {
        return -1;
    }

    end = data + len;
    start_code = find_start_code(data, end, &start_code_len);
    if (!start_code) {
        scan_nal(data, len, scan, sps_cache, pps_cache);
        return scan->first_nal;
    }

    while (start_code) {
        const uint8_t *nal = start_code + start_code_len;
        const uint8_t *next = find_start_code(nal, end, &start_code_len);
        const uint8_t *nal_end = next ? next : end;

        while (nal_end > nal && nal_end[-1] == 0) {
            nal_end--;
        }
        scan_nal(nal, (size_t)(nal_end - nal), scan,
                 sps_cache, pps_cache);
        start_code = next;
    }
    return scan->first_nal;
}

static int packetize_nal(const uint8_t *nal, size_t nal_len,
                         size_t max_payload, int marker,
                         h264_payload_writer_fn writer, void *context)
{
    uint8_t *fragment;
    size_t position;

    if (nal_len <= max_payload) {
        return writer(context, nal, nal_len, marker);
    }
    if (max_payload < 3) {
        return -1;
    }

    fragment = malloc(max_payload);
    if (!fragment) {
        return -1;
    }
    fragment[0] = (nal[0] & 0xe0) | 28;
    position = 1;
    while (position < nal_len) {
        size_t chunk = nal_len - position;
        int first = position == 1;
        int last;

        if (chunk > max_payload - 2) {
            chunk = max_payload - 2;
        }
        last = position + chunk == nal_len;
        fragment[1] = (first ? 0x80 : 0) | (last ? 0x40 : 0) |
                      (nal[0] & 0x1f);
        memcpy(fragment + 2, nal + position, chunk);
        if (writer(context, fragment, chunk + 2, marker && last) != 0) {
            free(fragment);
            return -1;
        }
        position += chunk;
    }
    free(fragment);
    return 0;
}

int h264_annexb_packetize(const uint8_t *data, size_t len,
                          size_t max_payload,
                          h264_payload_writer_fn writer,
                          void *writer_context)
{
    const uint8_t *end;
    const uint8_t *start_code;
    const uint8_t *pending = NULL;
    size_t pending_len = 0;
    int start_code_len = 0;
    int sent = 0;

    if (!data || len == 0 || !writer || max_payload == 0) {
        return -1;
    }
    end = data + len;
    start_code = find_start_code(data, end, &start_code_len);
    if (!start_code) {
        return packetize_nal(data, len, max_payload, 1,
                             writer, writer_context) == 0 ? 1 : -1;
    }

    while (start_code) {
        const uint8_t *nal = start_code + start_code_len;
        const uint8_t *next = find_start_code(nal, end, &start_code_len);
        const uint8_t *nal_end = next ? next : end;

        while (nal_end > nal && nal_end[-1] == 0) {
            nal_end--;
        }
        if (nal_end > nal) {
            if (pending && packetize_nal(pending, pending_len, max_payload, 0,
                                         writer, writer_context) != 0) {
                return -1;
            }
            if (pending) {
                sent++;
            }
            pending = nal;
            pending_len = (size_t)(nal_end - nal);
        }
        start_code = next;
    }
    if (!pending) {
        return 0;
    }
    if (packetize_nal(pending, pending_len, max_payload, 1,
                      writer, writer_context) != 0) {
        return -1;
    }
    return sent + 1;
}

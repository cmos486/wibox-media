#ifndef WIBOX_H264_ANNEXB_H
#define WIBOX_H264_ANNEXB_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t len;
} h264_nal_cache_t;

typedef struct {
    int first_nal;
    int last_nal;
    int has_sps;
    int has_pps;
    int has_idr;
    int has_slice;
    int nal_count;
} h264_annexb_scan_t;

typedef int (*h264_payload_writer_fn)(void *context,
                                      const uint8_t *payload,
                                      size_t payload_len,
                                      int marker);

int h264_annexb_scan(const uint8_t *data, size_t len,
                     h264_annexb_scan_t *scan,
                     h264_nal_cache_t *sps_cache,
                     h264_nal_cache_t *pps_cache);

int h264_annexb_packetize(const uint8_t *data, size_t len,
                          size_t max_payload,
                          h264_payload_writer_fn writer,
                          void *writer_context);

void h264_nal_cache_clear(h264_nal_cache_t *cache);

#endif

#include "h264_annexb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

typedef struct {
    unsigned char payload[16][32];
    size_t length[16];
    int marker[16];
    int count;
    int fail_at;
} packet_capture_t;

static int fail_next_malloc;

void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size)
{
    if (fail_next_malloc) {
        fail_next_malloc = 0;
        return NULL;
    }
    return __real_malloc(size);
}

static int capture_packet(void *context, const uint8_t *payload,
                          size_t payload_len, int marker)
{
    packet_capture_t *capture = context;

    if (capture->fail_at >= 0 && capture->count == capture->fail_at) {
        return -1;
    }
    if (capture->count >= 16 || payload_len > sizeof(capture->payload[0])) {
        return -1;
    }
    memcpy(capture->payload[capture->count], payload, payload_len);
    capture->length[capture->count] = payload_len;
    capture->marker[capture->count] = marker;
    capture->count++;
    return 0;
}

static void reset_capture(packet_capture_t *capture)
{
    memset(capture, 0, sizeof(*capture));
    capture->fail_at = -1;
}

int main(void)
{
    static const uint8_t raw_sps[] = {0x67, 0x42, 0x00, 0x1f};
    static const uint8_t raw_pps[] = {0x68, 0xce};
    static const uint8_t raw_idr[] = {0x65, 0xaa};
    static const uint8_t raw_slice[] = {0x61, 0xbb};
    static const uint8_t empty_annexb[] = {0x00, 0x00, 0x01};
    static const uint8_t trailing_annexb[] = {
        0x00, 0x00, 0x01, 0x61, 0x11, 0x00, 0x00,
        0x00, 0x00, 0x01
    };
    static const uint8_t mixed[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x01, 0x00,
        0x00, 0x01, 0x68, 0xce, 0x00,
        0x00, 0x00, 0x01, 0x65, 0xaa, 0xbb, 0x00, 0x00
    };
    static const uint8_t two_nals[] = {
        0x00, 0x00, 0x01, 0x61, 0x11,
        0x00, 0x00, 0x00, 0x01, 0x65, 0x22
    };
    uint8_t large_nal[14];
    h264_annexb_scan_t scan;
    h264_nal_cache_t sps = {0};
    h264_nal_cache_t pps = {0};
    h264_nal_cache_t failed_cache = {0};
    packet_capture_t capture;

    CHECK(h264_annexb_scan(NULL, 0, NULL, NULL, NULL) == -1);
    CHECK(h264_annexb_scan(NULL, 0, &scan, NULL, NULL) == -1);
    CHECK(scan.first_nal == -1 && scan.nal_count == 0);
    CHECK(h264_annexb_scan(raw_sps, sizeof(raw_sps), &scan, &sps, &pps) == 7);
    CHECK(scan.first_nal == 7 && scan.last_nal == 7 && scan.nal_count == 1);
    CHECK(scan.has_sps && !scan.has_pps && !scan.has_idr && !scan.has_slice);
    CHECK(sps.len == sizeof(raw_sps) && memcmp(sps.data, raw_sps, sps.len) == 0);
    CHECK(h264_annexb_scan(raw_pps, sizeof(raw_pps), &scan, NULL, &pps) == 8);
    CHECK(scan.has_pps && !scan.has_slice);
    CHECK(h264_annexb_scan(raw_idr, sizeof(raw_idr), &scan, NULL, NULL) == 5);
    CHECK(scan.has_idr && scan.has_slice);
    CHECK(h264_annexb_scan(raw_slice, sizeof(raw_slice), &scan, NULL, NULL) == 1);
    CHECK(!scan.has_idr && scan.has_slice);
    CHECK(h264_annexb_scan(empty_annexb, sizeof(empty_annexb), &scan,
                           NULL, NULL) == -1);
    CHECK(scan.nal_count == 0);
    fail_next_malloc = 1;
    CHECK(h264_annexb_scan(raw_sps, sizeof(raw_sps), &scan,
                           &failed_cache, NULL) == 7);
    CHECK(failed_cache.data == NULL && failed_cache.len == 0);

    CHECK(h264_annexb_scan(mixed, sizeof(mixed), &scan, &sps, &pps) == 7);
    CHECK(scan.first_nal == 7 && scan.last_nal == 5 && scan.nal_count == 3);
    CHECK(scan.has_sps && scan.has_pps && scan.has_idr && scan.has_slice);
    CHECK(sps.len == 3 && sps.data[0] == 0x67);
    CHECK(pps.len == 2 && pps.data[0] == 0x68);

    reset_capture(&capture);
    CHECK(h264_annexb_packetize(raw_sps, sizeof(raw_sps), 8,
                                capture_packet, &capture) == 1);
    CHECK(capture.count == 1 && capture.marker[0] == 1);
    CHECK(capture.length[0] == sizeof(raw_sps));

    reset_capture(&capture);
    CHECK(h264_annexb_packetize(two_nals, sizeof(two_nals), 8,
                                capture_packet, &capture) == 2);
    CHECK(capture.count == 2 && capture.marker[0] == 0 && capture.marker[1] == 1);
    CHECK(capture.payload[0][0] == 0x61 && capture.payload[1][0] == 0x65);

    reset_capture(&capture);
    CHECK(h264_annexb_packetize(trailing_annexb, sizeof(trailing_annexb), 8,
                                capture_packet, &capture) == 1);
    CHECK(capture.count == 1 && capture.marker[0] == 1);

    reset_capture(&capture);
    CHECK(h264_annexb_packetize(empty_annexb, sizeof(empty_annexb), 8,
                                capture_packet, &capture) == 0);

    memset(large_nal, 0x5a, sizeof(large_nal));
    large_nal[0] = 0x65;
    reset_capture(&capture);
    CHECK(h264_annexb_packetize(large_nal, sizeof(large_nal), 6,
                                capture_packet, &capture) == 1);
    CHECK(capture.count == 4);
    CHECK(capture.payload[0][0] == 0x7c && capture.payload[0][1] == 0x85);
    CHECK((capture.payload[1][1] & 0xc0) == 0);
    CHECK((capture.payload[3][1] & 0x40) != 0 && capture.marker[3] == 1);
    CHECK(capture.marker[0] == 0 && capture.marker[1] == 0);

    reset_capture(&capture);
    capture.fail_at = 1;
    CHECK(h264_annexb_packetize(large_nal, sizeof(large_nal), 6,
                                capture_packet, &capture) == -1);
    CHECK(h264_annexb_packetize(NULL, 0, 6, capture_packet, &capture) == -1);
    CHECK(h264_annexb_packetize(raw_sps, sizeof(raw_sps), 0,
                                capture_packet, &capture) == -1);
    CHECK(h264_annexb_packetize(large_nal, sizeof(large_nal), 2,
                                capture_packet, &capture) == -1);
    reset_capture(&capture);
    capture.fail_at = 0;
    CHECK(h264_annexb_packetize(two_nals, sizeof(two_nals), 8,
                                capture_packet, &capture) == -1);
    reset_capture(&capture);
    capture.fail_at = 1;
    CHECK(h264_annexb_packetize(two_nals, sizeof(two_nals), 8,
                                capture_packet, &capture) == -1);
    reset_capture(&capture);
    fail_next_malloc = 1;
    CHECK(h264_annexb_packetize(large_nal, sizeof(large_nal), 6,
                                capture_packet, &capture) == -1);

    h264_nal_cache_clear(&sps);
    h264_nal_cache_clear(&pps);
    h264_nal_cache_clear(&failed_cache);
    h264_nal_cache_clear(&sps);
    h264_nal_cache_clear(NULL);
    CHECK(sps.data == NULL && sps.len == 0 && pps.data == NULL && pps.len == 0);
    printf("RESULT h264_annexb PASS packets=%d\n", capture.count);
    return 0;
}

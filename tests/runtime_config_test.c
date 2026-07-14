#include "runtime_config.h"

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
    char uri[64];
    char tiny[5];
    char long_uri[96];

    CHECK(clamp_video_bitrate(-1) == 512);
    CHECK(clamp_video_bitrate(512) == 512);
    CHECK(clamp_video_bitrate(639) == 512);
    CHECK(clamp_video_bitrate(640) == 768);
    CHECK(clamp_video_bitrate(4096) == 4096);
    CHECK(clamp_video_bitrate(5000) == 4096);
    CHECK(clamp_video_gop_n(0) == 25);
    CHECK(clamp_video_gop_n(1) == 5);
    CHECK(clamp_video_gop_n(25) == 25);
    CHECK(clamp_video_gop_n(121) == 120);
    CHECK(clamp_video_idr_interval(0) == 1);
    CHECK(clamp_video_idr_interval(5) == 5);
    CHECK(clamp_video_idr_interval(11) == 10);
    CHECK(clamp_video_brc_mode(-1) == 0);
    CHECK(clamp_video_brc_mode(3) == 3);
    CHECK(clamp_video_brc_mode(4) == 0);
    CHECK(clamp_video_rtsp_periodic_idr_ms(0) == 0);
    CHECK(clamp_video_rtsp_periodic_idr_ms(1) == 500);
    CHECK(clamp_video_rtsp_periodic_idr_ms(500) == 500);
    CHECK(clamp_video_rtsp_periodic_idr_ms(10001) == 10000);
    CHECK(clamp_outgoing_call_timeout(0) == 10);
    CHECK(clamp_outgoing_call_timeout(12) == 10);
    CHECK(clamp_outgoing_call_timeout(13) == 15);
    CHECK(clamp_outgoing_call_timeout(120) == 120);
    CHECK(clamp_outgoing_call_timeout(121) == 120);
    CHECK(clamp_ring_snapshot_delay(-1) == 0);
    CHECK(clamp_ring_snapshot_delay(0) == 0);
    CHECK(clamp_ring_snapshot_delay(249) == 0);
    CHECK(clamp_ring_snapshot_delay(250) == 500);
    CHECK(clamp_ring_snapshot_delay(4749) == 4500);
    CHECK(clamp_ring_snapshot_delay(4750) == 5000);
    CHECK(clamp_ring_snapshot_delay(5001) == 5000);
    CHECK(clamp_recording_max_seconds(0) == 30);
    CHECK(clamp_recording_max_seconds(12) == 12);
    CHECK(clamp_recording_max_seconds(31) == 30);

    CHECK(normalize_sip_target_uri("  sip:1000@example.test:5060 \n", uri,
                                   sizeof(uri)) == 0);
    CHECK(strcmp(uri, "sip:1000@example.test:5060") == 0);
    CHECK(normalize_sip_target_uri(NULL, uri, sizeof(uri)) == -1);
    CHECK(normalize_sip_target_uri("sip:x", NULL, sizeof(uri)) == -1);
    CHECK(normalize_sip_target_uri("sip:x", uri, 0) == -1);
    CHECK(normalize_sip_target_uri("", uri, sizeof(uri)) == -1);
    CHECK(normalize_sip_target_uri("sip:", uri, sizeof(uri)) == -1);
    CHECK(normalize_sip_target_uri("http:x", uri, sizeof(uri)) == -1);
    CHECK(normalize_sip_target_uri("sip:a b", uri, sizeof(uri)) == -1);
    CHECK(normalize_sip_target_uri("sip:x", tiny, sizeof(tiny)) == -1);
    memset(long_uri, 'a', sizeof(long_uri));
    memcpy(long_uri, "sip:", 4);
    long_uri[sizeof(long_uri) - 1] = '\0';
    CHECK(normalize_sip_target_uri(long_uri, uri, sizeof(uri)) == -1);

    printf("RESULT runtime_config PASS\n");
    return 0;
}

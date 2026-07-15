#include "video_worker.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

enum video_bridge_mode {
    VIDEO_BRIDGE_MODE_RTP = 0,
    VIDEO_BRIDGE_MODE_SNAPSHOT = 1,
};

struct video_bridge_options {
    enum video_bridge_mode mode;
    const char *remote_ip;
    int remote_port;
    int local_port;
    int payload_type;
    int bitrate_kbps;
    int gop_n;
    int idr_interval;
    int brc_mode;
    int rtsp_periodic_idr_ms;
    const char *dumpfile;
    long long dump_limit_bytes;
    int rtp_sink_fd;
    int rtp_control_fd;
    const char *snapshot_path;
    int jpeg_quality;
};

static struct video_bridge_options captured;
static int bridge_calls;
static int bridge_result;

int video_bridge_run_options(const struct video_bridge_options *options)
{
    captured = *options;
    bridge_calls++;
    return bridge_result;
}

int main(void)
{
    video_encoder_tuning_t tuning = {30, 2, 1, 1000};

    bridge_result = 7;
    CHECK(video_worker_run("192.0.2.10", 9002, 8002, 97, 3072,
                           &tuning, "/tmp/video.h264", 123456, 11, 12) == 7);
    CHECK(bridge_calls == 1);
    CHECK(captured.mode == VIDEO_BRIDGE_MODE_RTP);
    CHECK(strcmp(captured.remote_ip, "192.0.2.10") == 0);
    CHECK(captured.remote_port == 9002 && captured.local_port == 8002);
    CHECK(captured.payload_type == 97 && captured.bitrate_kbps == 3072);
    CHECK(captured.gop_n == 30 && captured.idr_interval == 2);
    CHECK(captured.brc_mode == 1 && captured.rtsp_periodic_idr_ms == 1000);
    CHECK(strcmp(captured.dumpfile, "/tmp/video.h264") == 0);
    CHECK(captured.dump_limit_bytes == 123456);
    CHECK(captured.rtp_sink_fd == 11 && captured.rtp_control_fd == 12);

    bridge_result = 0;
    CHECK(video_worker_run(NULL, 0, 8002, 96, 4096, NULL,
                           NULL, 0, -1, -1) == 0);
    CHECK(bridge_calls == 2);
    CHECK(captured.gop_n == 0 && captured.idr_interval == 0);
    CHECK(captured.brc_mode == 0 && captured.rtsp_periodic_idr_ms == 0);

    CHECK(video_snapshot_capture(NULL, 90) == 1);
    CHECK(video_snapshot_capture("", 90) == 1);
    CHECK(bridge_calls == 2);
    bridge_result = 3;
    CHECK(video_snapshot_capture("/tmp/snapshot.jpg", 85) == 3);
    CHECK(bridge_calls == 3);
    CHECK(captured.mode == VIDEO_BRIDGE_MODE_SNAPSHOT);
    CHECK(strcmp(captured.snapshot_path, "/tmp/snapshot.jpg") == 0);
    CHECK(captured.jpeg_quality == 85);
    printf("RESULT video_worker_mock PASS\n");
    return 0;
}

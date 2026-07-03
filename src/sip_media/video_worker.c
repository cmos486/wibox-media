#include "video_worker.h"

#include <stdio.h>

#define main video_bridge_main
#include "../video_rtp_bridge/video_rtp_bridge.c"
#undef main

int video_worker_run(const char* remote_ip, int remote_port,
                     int local_port, int payload_type,
                     int bitrate_kbps,
                     const char* dumpfile,
                     long long dump_limit_bytes,
                     int rtsp_video_fd,
                     int rtp_control_fd) {
    struct video_bridge_options opt;

    memset(&opt, 0, sizeof(opt));
    opt.mode = VIDEO_BRIDGE_MODE_RTP;
    opt.remote_ip = remote_ip;
    opt.remote_port = remote_port;
    opt.local_port = local_port;
    opt.payload_type = payload_type;
    opt.bitrate_kbps = bitrate_kbps;
    opt.dumpfile = dumpfile;
    opt.dump_limit_bytes = dump_limit_bytes;
    opt.rtp_sink_fd = rtsp_video_fd;
    opt.rtp_control_fd = rtp_control_fd;

    return video_bridge_run_options(&opt);
}

int video_snapshot_capture(const char* output_path, int quality) {
    struct video_bridge_options opt;

    if (!output_path || !output_path[0]) {
        return 1;
    }

    memset(&opt, 0, sizeof(opt));
    opt.mode = VIDEO_BRIDGE_MODE_SNAPSHOT;
    opt.snapshot_path = output_path;
    opt.jpeg_quality = quality;

    return video_bridge_run_options(&opt);
}

#ifndef VIDEO_WORKER_H
#define VIDEO_WORKER_H

int video_worker_run(const char* remote_ip, int remote_port,
                     int local_port, int payload_type,
                     int bitrate_kbps,
                     const char* dumpfile);
int video_snapshot_capture(const char* output_path, int quality);

#endif

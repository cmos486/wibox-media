#ifndef WIBOX_RUNTIME_CONFIG_H
#define WIBOX_RUNTIME_CONFIG_H

#include <stddef.h>

int clamp_video_bitrate(int bitrate_kbps);
int clamp_video_gop_n(int gop_n);
int clamp_video_idr_interval(int idr_interval);
int clamp_video_brc_mode(int brc_mode);
int clamp_video_rtsp_periodic_idr_ms(int interval_ms);
int clamp_outgoing_call_timeout(int timeout_seconds);
int clamp_ring_snapshot_delay(int delay_ms);
int clamp_recording_max_seconds(int seconds);
int normalize_sip_target_uri(const char *input, char *out, size_t out_size);

#endif

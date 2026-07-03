#ifndef RTSP_STREAM_H
#define RTSP_STREAM_H

#include <stddef.h>

typedef void (*rtsp_stream_client_callback_t)(int video_clients,
                                              int audio_clients,
                                              void *user_data);

int rtsp_stream_start(int port, const char *local_ip, int video_enabled,
                      const char *auth_user, const char *auth_pass);
void rtsp_stream_stop(void);
void rtsp_stream_set_video_enabled(int enabled);
int rtsp_stream_get_video_pipe_fd(void);
int rtsp_stream_get_video_client_count(void);
int rtsp_stream_get_audio_client_count(void);
void rtsp_stream_set_client_callback(rtsp_stream_client_callback_t callback,
                                     void *user_data);
void rtsp_stream_send_audio_rtp(const unsigned char *rtp_packet, size_t len);

#endif

#ifndef WIBOX_SIP_SDP_H
#define WIBOX_SIP_SDP_H

#include <stddef.h>

int sip_sdp_build(const char *local_ip, int local_rtp_port,
                  int local_video_rtp_port, int video_payload_type,
                  char *out, size_t out_size);
int sip_sdp_parse(const char *sdp_content, int *remote_rtp_port,
                  int *remote_dtmf_payload_type,
                  int *remote_video_rtp_port,
                  int *remote_video_payload_type);

#endif

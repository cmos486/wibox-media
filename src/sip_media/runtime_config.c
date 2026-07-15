#include "runtime_config.h"

#include <ctype.h>
#include <string.h>

#define RING_SNAPSHOT_DELAY_MIN_MS 0
#define RING_SNAPSHOT_DELAY_MAX_MS 5000
#define RING_SNAPSHOT_DELAY_STEP_MS 500

int clamp_video_bitrate(int bitrate_kbps)
{
    int rounded;

    if (bitrate_kbps < 512) return 512;
    if (bitrate_kbps > 4096) return 4096;
    rounded = ((bitrate_kbps + 128) / 256) * 256;
    if (rounded < 512) return 512;
    if (rounded > 4096) return 4096;
    return rounded;
}

int clamp_video_gop_n(int gop_n)
{
    if (gop_n <= 0) return 25;
    if (gop_n < 5) return 5;
    if (gop_n > 120) return 120;
    return gop_n;
}

int clamp_video_idr_interval(int idr_interval)
{
    if (idr_interval <= 0) return 1;
    if (idr_interval > 10) return 10;
    return idr_interval;
}

int clamp_video_brc_mode(int brc_mode)
{
    if (brc_mode < 0 || brc_mode > 3) return 0;
    return brc_mode;
}

int clamp_video_rtsp_periodic_idr_ms(int interval_ms)
{
    if (interval_ms <= 0) return 0;
    if (interval_ms < 500) return 500;
    if (interval_ms > 10000) return 10000;
    return interval_ms;
}

int clamp_outgoing_call_timeout(int timeout_seconds)
{
    int rounded;

    if (timeout_seconds < 10) return 10;
    if (timeout_seconds > 120) return 120;
    rounded = ((timeout_seconds + 2) / 5) * 5;
    if (rounded < 10) return 10;
    if (rounded > 120) return 120;
    return rounded;
}

int clamp_ring_snapshot_delay(int delay_ms)
{
    int rounded;

    if (delay_ms < RING_SNAPSHOT_DELAY_MIN_MS) return RING_SNAPSHOT_DELAY_MIN_MS;
    if (delay_ms > RING_SNAPSHOT_DELAY_MAX_MS) return RING_SNAPSHOT_DELAY_MAX_MS;
    rounded = ((delay_ms + (RING_SNAPSHOT_DELAY_STEP_MS / 2)) /
               RING_SNAPSHOT_DELAY_STEP_MS) * RING_SNAPSHOT_DELAY_STEP_MS;
    if (rounded < RING_SNAPSHOT_DELAY_MIN_MS) return RING_SNAPSHOT_DELAY_MIN_MS;
    if (rounded > RING_SNAPSHOT_DELAY_MAX_MS) return RING_SNAPSHOT_DELAY_MAX_MS;
    return rounded;
}

int clamp_recording_max_seconds(int seconds)
{
    if (seconds <= 0) return 30;
    if (seconds > 30) return 30;
    return seconds;
}

int normalize_sip_target_uri(const char *input, char *out, size_t out_size)
{
    const char *start;
    const char *end;
    size_t len;
    size_t i;

    if (!input || !out || out_size == 0) {
        return -1;
    }

    start = input;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    len = (size_t)(end - start);
    if (len == 0 || len >= out_size || len < 5 ||
        strncmp(start, "sip:", 4) != 0) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)start[i];
        if (iscntrl(c) || isspace(c)) {
            return -1;
        }
    }

    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}

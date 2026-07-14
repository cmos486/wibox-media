/*
 * video_rtp_bridge.c - GK7102S D1 H.264 RTP sender
 *
 * SDK init order: sys_init → vi_init+open → vout_init+open → venc_init+open+map_bsb
 * Then raw fd ioctls for VENC config (SET_SRCBUF_FORMAT, etc.)
 *
 * Captures stream_id==0 as 688x576 H.264 and sends it as RTP/H264.
 *
 * Prerequisites:
 *   1. Sofia warmup once after boot (about 30s), then kill Sofia.
 *   2. Kill Sofia
 *   3. Start the call path:
 *      printf "\xfb\x14\x01\x20" > /dev/ttySGK1
 *   4. Run: video_rtp_bridge <remote_ip> <remote_port> [local_port] [payload_type]
 *      or: video_rtp_bridge --snapshot <output.jpg> [quality]
 *   5. Stop the call path:
 *      printf "\xfb\x14\x00\x1f" > /dev/ttySGK1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

#include "../sip_media/h264_annexb.h"
#include "../sip_media/snapshot_file.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "adi_types.h"
#include "adi_sys.h"
#include "adi_vi.h"
#include "adi_vout.h"
#include "adi_venc.h"

/* ================================================================
 * IOCTL definitions (type=0x76 SYS, type=0x65 VENC_ENCODE)
 * ================================================================ */
#define IOC_GET_CHIP_INFO      0x80047670
#define IOC_GET_VERSION        0x80047652  /* READ, returns version (NOT stop!) */
#define IOC_SET_ENCODE_STATE   0x40047654  /* WRITE, set encode_state (0=reset/stop) */
#define IOC_GET_LIMITS         0x80047674  /* READ, Sofia reads then writes back */
#define IOC_SET_LIMITS         0x40047673  /* WRITE, set resource limits - MUST before FORMAT */
#define IOC_SYS_0x7B           0x4004767b  /* WRITE, Sofia calls this with NULL before limits */
#define IOC_SET_SRCBUF_FORMAT  0x40047687  /* WRITE, 40 bytes struct! */
#define IOC_SET_SRCBUF_TYPE    0x40047683  /* 4x int */
#define IOC_SET_FRAME_INTERVAL 0x40046533
#define IOC_SET_ENCODE_FORMAT  0x40046528
#define IOC_SET_BITRATE        0x40046538
#define IOC_GETSET_H264_CFG    0xc0046540
#define IOC_SET_H264_CFG       0x4004653f
#define IOC_QUERY_STREAM       0xc004652a
#define IOC_START_ENCODE       0x40046541
#define IOC_FORCE_IDR          0x4004653c
#define IOC_GET_H264_QP_CFG    0xc0046545
#define IOC_SET_H264_QP_CFG    0x40046544
#define IOC_GET_STREAM         0x80046537
/* VI source capability lookup. Sofia calls this before SET_SRCBUF_FORMAT. */
#define IOC_VI_SOURCE_CAPS     0x80047305

#define DEVICE "/dev/gk_video"
#define RTSP_H264_PAYLOAD_TYPE 96
#define SNAPSHOT_STREAM_ID 2
#define SNAPSHOT_WIDTH 352
#define SNAPSHOT_HEIGHT 288
#define SNAPSHOT_FPS 5
#define SNAPSHOT_CHANNEL_ID 2
#define SNAPSHOT_MIN_JPEG_BYTES 4096

/* ================================================================
 * struct srcbuf_format_t - 40 bytes for ioctl 0x40047687
 * Discovered by reversing media.ko (sys_encode_guard_task+0x290)
 * ================================================================ */
struct srcbuf_format_t {
    uint16_t main_width;      /* 0:  must be multiple of 16 */
    uint16_t main_height;     /* 2:  must be even */
    uint16_t ch_mode_0;       /* 4:  channel group 0 mode */
    uint16_t sub1_w;          /* 6:  sub1 width  <= main_width */
    uint16_t sub1_h;          /* 8:  sub1 height <= main_height */
    uint16_t main_w_dup;      /* 10: main width repeat (validated <= main_width) */
    uint16_t main_h_dup;      /* 12: main height repeat */
    uint16_t ch_mode_1;       /* 14 */
    uint16_t sub2_w;          /* 16 */
    uint16_t sub2_h;          /* 18 */
    uint16_t main_w_dup2;     /* 20: validated <= main_width */
    uint16_t main_h_dup2;     /* 22 */
    uint16_t ch_mode_2;       /* 24 */
    uint16_t sub3_w;          /* 26 */
    uint16_t sub3_h;          /* 28 */
    uint16_t main_w_dup3;     /* 30: validated <= main_width */
    uint16_t main_h_dup3;     /* 32 */
    uint16_t ch_mode_3;       /* 34 */
    uint8_t  interlace_scan;  /* 36 */
    uint8_t  pad[3];          /* 37-39 */
} __attribute__((packed));   /* 40 bytes total */

/* ================================================================ */

static int gfd = -1;
static volatile sig_atomic_t get_stream_timed_out = 0;
static volatile sig_atomic_t stop_requested = 0;

static int force_idr_stream0(const char *reason);

struct rtp_sender {
    int fd;
    int sink_fd;
    struct sockaddr_in remote;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
    uint8_t payload_type;
};

static void on_alarm(int sig)
{
    (void)sig;
    get_stream_timed_out = 1;
}

static void on_stop(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static long long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (tv.tv_usec / 1000);
}

static int get_stream_with_timeout(GADI_SYS_HandleT venc, int stream_id,
                                   GADI_VENC_StreamT *st)
{
    int ret;
    get_stream_timed_out = 0;
    alarm(1);
    ret = gadi_venc_get_stream(venc, stream_id, st);
    alarm(0);
    if (get_stream_timed_out) {
        errno = ETIMEDOUT;
        return -1;
    }
    return ret;
}

static void try_stop_stream(void) {
    /* 0x40047654 = SET_ENCODE_STATE - value 0 = stop/reset */
    uint32_t v = 0;
    int r = ioctl(gfd, IOC_SET_ENCODE_STATE, &v);
    printf("[RESET_STATE] ioctl(0x40047654, 0) ret=%d errno=%d\n", r, errno);
    usleep(100000);
}

static void set_resource_limits(void) {
    uint8_t limits[128];
    int r;

    memset(limits, 0, sizeof(limits));
    r = ioctl(gfd, IOC_GET_LIMITS, limits);
    printf("[GET_LIMITS] ioctl(0x80047674) ret=%d errno=%d bytes=%02x %02x %02x %02x\n",
           r, errno, limits[0], limits[1], limits[2], limits[3]);

    r = ioctl(gfd, IOC_SET_LIMITS, limits);
    printf("[SET_LIMITS] ioctl(0x40047673) ret=%d errno=%d bytes=%02x %02x %02x %02x\n",
           r, errno, limits[0], limits[1], limits[2], limits[3]);
}

static int get_vi_caps_once(int fd)
{
    uint32_t caps[6] = {0}; /* Sofia returns 24 bytes in traces for this ioctl */
    int r = ioctl(fd, IOC_VI_SOURCE_CAPS, caps);
    printf("[VI_CAPS] ioctl(0x80047305) ret=%d errno=%d\n", r, errno);
    if (r == 0) {
        printf("[VI_CAPS] values: ");
        for (unsigned i = 0; i < 6; i++) {
            printf("0x%08x ", caps[i]);
        }
        printf("\n");
    }
    return r;
}

static int set_srcbuf_format(void)
{
    struct srcbuf_format_t fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.main_width = 688;       fmt.main_height = 576;
    fmt.ch_mode_0 = 1;
    fmt.sub1_w = 352;           fmt.sub1_h = 300;
    fmt.main_w_dup = 688;       fmt.main_h_dup = 576;
    fmt.ch_mode_1 = 1;
    fmt.sub2_w = 352;           fmt.sub2_h = 288;
    fmt.main_w_dup2 = 688;      fmt.main_h_dup2 = 576;
    fmt.ch_mode_2 = 1;
    fmt.sub3_w = 0;             fmt.sub3_h = 0;
    fmt.main_w_dup3 = 688;      fmt.main_h_dup3 = 576;
    fmt.ch_mode_3 = 0;
    fmt.interlace_scan = 1;

    printf("[SET_SRCBUF_FORMAT] Sofia D1 layout struct_size=%zu\n", sizeof(fmt));
    uint16_t *p = (uint16_t *)&fmt;
    printf("  bytes: %04x %04x %04x %04x %04x %04x %04x %04x\n",
           p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    int ret = ioctl(gfd, IOC_SET_SRCBUF_FORMAT, &fmt);
    printf("[SET_SRCBUF_FORMAT] -> ret=%d errno=%d (%s)\n",
           ret, errno, strerror(errno));
    return ret;
}

static void dump_bytes(const char *tag, const uint8_t *buf, size_t len)
{
    size_t n = len < 80 ? len : 80;
    printf("%s", tag);
    for (size_t i = 0; i < n; i++) {
        if ((i % 16) == 0) printf("\n  %04zu:", i);
        printf(" %02x", buf[i]);
    }
    printf("\n");
}

static int rtp_sender_open(struct rtp_sender *rtp, const char *remote_ip,
                           int remote_port, int local_port, int payload_type)
{
    struct sockaddr_in local;

    if (!rtp || !remote_ip || !remote_ip[0] ||
        remote_port <= 0 || local_port <= 0 ||
        payload_type <= 0 || payload_type > 127) {
        return -1;
    }
    if (rtp->fd >= 0) {
        close(rtp->fd);
        rtp->fd = -1;
    }

    rtp->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (rtp->fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons((uint16_t)local_port);
    if (bind(rtp->fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(rtp->fd);
        rtp->fd = -1;
        return -1;
    }

    memset(&rtp->remote, 0, sizeof(rtp->remote));
    rtp->remote.sin_family = AF_INET;
    rtp->remote.sin_port = htons((uint16_t)remote_port);
    if (inet_pton(AF_INET, remote_ip, &rtp->remote.sin_addr) != 1) {
        fprintf(stderr, "Invalid remote IP: %s\n", remote_ip);
        close(rtp->fd);
        rtp->fd = -1;
        return -1;
    }

    rtp->payload_type = (uint8_t)payload_type;
    return 0;
}

static void rtp_sender_clear_udp(struct rtp_sender *rtp)
{
    if (!rtp) {
        return;
    }
    if (rtp->fd >= 0) {
        close(rtp->fd);
        rtp->fd = -1;
    }
    memset(&rtp->remote, 0, sizeof(rtp->remote));
}

static void rtp_sender_clear_sink(struct rtp_sender *rtp)
{
    if (!rtp) {
        return;
    }
    rtp->sink_fd = -1;
}

static void rtp_sender_init(struct rtp_sender *rtp, int payload_type)
{
    memset(rtp, 0, sizeof(*rtp));
    rtp->fd = -1;
    rtp->sink_fd = -1;
    rtp->seq = (uint16_t)(time(NULL) & 0xffff);
    rtp->timestamp = (uint32_t)time(NULL) * 90000U;
    rtp->ssrc = 0x57425630U; /* "WBV0" */
    rtp->payload_type = (uint8_t)(payload_type > 0 && payload_type <= 127 ?
                                  payload_type : RTSP_H264_PAYLOAD_TYPE);
}

static void rtp_sink_write_packet(int fd, const uint8_t *pkt, size_t len)
{
    uint8_t hdr[2];
    uint8_t pkt_copy[1400];
    const uint8_t *out = pkt;
    size_t off;

    if (fd < 0 || !pkt || len == 0 || len > 0xffff) {
        return;
    }
    if (len <= sizeof(pkt_copy)) {
        memcpy(pkt_copy, pkt, len);
        pkt_copy[1] = (uint8_t)((pkt_copy[1] & 0x80) | RTSP_H264_PAYLOAD_TYPE);
        out = pkt_copy;
    }

    hdr[0] = (uint8_t)(len >> 8);
    hdr[1] = (uint8_t)(len);

    off = 0;
    while (off < sizeof(hdr)) {
        ssize_t wr = write(fd, hdr + off, sizeof(hdr) - off);
        if (wr > 0) {
            off += (size_t)wr;
            continue;
        }
        if (wr < 0 && errno == EINTR) {
            continue;
        }
        return;
    }

    off = 0;
    while (off < len) {
        ssize_t wr = write(fd, out + off, len - off);
        if (wr > 0) {
            off += (size_t)wr;
            continue;
        }
        if (wr < 0 && errno == EINTR) {
            continue;
        }
        return;
    }
}

static int rtp_send_packet(struct rtp_sender *rtp, const uint8_t *payload,
                           size_t payload_len, int marker)
{
    uint8_t pkt[1400];
    if (payload_len + 12 > sizeof(pkt)) {
        return -1;
    }

    pkt[0] = 0x80;
    pkt[1] = (marker ? 0x80 : 0x00) | (rtp->payload_type & 0x7f);
    pkt[2] = (uint8_t)(rtp->seq >> 8);
    pkt[3] = (uint8_t)(rtp->seq);
    pkt[4] = (uint8_t)(rtp->timestamp >> 24);
    pkt[5] = (uint8_t)(rtp->timestamp >> 16);
    pkt[6] = (uint8_t)(rtp->timestamp >> 8);
    pkt[7] = (uint8_t)(rtp->timestamp);
    pkt[8] = (uint8_t)(rtp->ssrc >> 24);
    pkt[9] = (uint8_t)(rtp->ssrc >> 16);
    pkt[10] = (uint8_t)(rtp->ssrc >> 8);
    pkt[11] = (uint8_t)(rtp->ssrc);
    memcpy(pkt + 12, payload, payload_len);

    rtp->seq++;
    rtp_sink_write_packet(rtp->sink_fd, pkt, payload_len + 12);
    if (rtp->fd >= 0) {
        if (sendto(rtp->fd, pkt, payload_len + 12, 0,
                   (struct sockaddr *)&rtp->remote, sizeof(rtp->remote)) < 0) {
            return -1;
        }
    }
    return 0;
}

static int rtp_write_h264_payload(void *context, const uint8_t *payload,
                                  size_t payload_len, int marker)
{
    return rtp_send_packet((struct rtp_sender *)context,
                           payload, payload_len, marker);
}

static int rtp_write_h264_parameter_set(void *context,
                                        const uint8_t *payload,
                                        size_t payload_len, int marker)
{
    (void)marker;
    return rtp_send_packet((struct rtp_sender *)context,
                           payload, payload_len, 0);
}

static int rtp_send_parameter_set(struct rtp_sender *rtp,
                                  const uint8_t *data, size_t len)
{
    return h264_annexb_packetize(data, len, 1200,
                                 rtp_write_h264_parameter_set, rtp);
}

static int rtp_send_annexb(struct rtp_sender *rtp, const uint8_t *data,
                           size_t len)
{
    return h264_annexb_packetize(data, len, 1200,
                                 rtp_write_h264_payload, rtp);
}

struct worker_snapshot_state {
    int active;
    int mjpeg_started;
    int quality;
    char path[160];
};

static void set_frame_interval_stream(int stream_id);
static void set_encode_format_stream_type(int stream_id, int channel_id,
                                          int encode_type, uint16_t width,
                                          uint16_t height, uint32_t fps);
static void set_mjpeg_config_stream(GADI_SYS_HandleT handle, int stream_id,
                                    int quality);

static int start_mjpeg_snapshot_stream(GADI_SYS_HandleT venc_handle,
                                       struct worker_snapshot_state *snapshot)
{
    uint32_t types[4] = {1, 1, 2, 0};
    int ret;

    if (!snapshot) {
        return -1;
    }
    if (snapshot->mjpeg_started) {
        return 0;
    }

    errno = 0;
    ret = ioctl(gfd, IOC_SET_SRCBUF_TYPE, types);
    printf("[SNAPSHOT_CONTROL] SET_SRCBUF_TYPE sid2=mjpeg ret=%d errno=%d\n",
           ret, errno);

    set_frame_interval_stream(SNAPSHOT_STREAM_ID);
    set_encode_format_stream_type(SNAPSHOT_STREAM_ID, SNAPSHOT_CHANNEL_ID, 2,
                                  SNAPSHOT_WIDTH, SNAPSHOT_HEIGHT,
                                  SNAPSHOT_FPS);
    set_mjpeg_config_stream(venc_handle, SNAPSHOT_STREAM_ID, snapshot->quality);

    errno = 0;
    ret = ioctl(gfd, IOC_START_ENCODE, 1U << SNAPSHOT_STREAM_ID);
    printf("[SNAPSHOT_CONTROL] START sid2 ret=%d errno=%d\n", ret, errno);
    if (ret == 0 || errno == EBUSY || errno == EALREADY) {
        snapshot->mjpeg_started = 1;
        return 0;
    }
    return -1;
}

static int poll_rtp_control(int control_fd, struct rtp_sender *rtp,
                            GADI_SYS_HandleT venc_handle,
                            struct worker_snapshot_state *snapshot)
{
    char buf[256];
    int changed = 0;

    if (control_fd < 0 || !rtp) {
        return 0;
    }

    for (;;) {
        ssize_t rd = read(control_fd, buf, sizeof(buf) - 1);
        if (rd > 0) {
            char ip[64];
            char path[160];
            int remote_port;
            int local_port;
            int payload_type;
            int quality;

            buf[rd] = '\0';
            if (sscanf(buf, "SET_RTP %63s %d %d %d",
                       ip, &remote_port, &local_port, &payload_type) == 4) {
                if (rtp_sender_open(rtp, ip, remote_port, local_port, payload_type) == 0) {
                    printf("[RTP_CONTROL] attached RTP target %s:%d local=%d payload=%d\n",
                           ip, remote_port, local_port, payload_type);
                    changed = 1;
                } else {
                    printf("[RTP_CONTROL] failed to attach RTP target %s:%d local=%d payload=%d errno=%d\n",
                           ip, remote_port, local_port, payload_type, errno);
                }
            } else if (strncmp(buf, "CLEAR_RTP", 9) == 0) {
                rtp_sender_clear_udp(rtp);
                rtp->payload_type = RTSP_H264_PAYLOAD_TYPE;
                printf("[RTP_CONTROL] detached RTP target; RTSP sink remains active\n");
                changed = 1;
            } else if (strncmp(buf, "CLEAR_RTSP", 10) == 0) {
                rtp_sender_clear_sink(rtp);
                printf("[RTP_CONTROL] detached RTSP sink; RTP target remains active\n");
                changed = 1;
            } else if (strncmp(buf, "FORCE_IDR", 9) == 0) {
                force_idr_stream0("control");
            } else if (sscanf(buf, "SNAPSHOT %159s %d", path, &quality) >= 1) {
                if (!snapshot) {
                    printf("[SNAPSHOT_CONTROL] no snapshot state available\n");
                } else if (snapshot->active) {
                    printf("[SNAPSHOT_CONTROL] snapshot already pending path=%s\n",
                           snapshot->path);
                } else {
                    memset(snapshot->path, 0, sizeof(snapshot->path));
                    strncpy(snapshot->path, path, sizeof(snapshot->path) - 1);
                    snapshot->quality = quality > 0 ? quality : 90;
                    if (snapshot->quality > 100) snapshot->quality = 100;
                    unlink(snapshot->path);
                    if (start_mjpeg_snapshot_stream(venc_handle, snapshot) == 0) {
                        snapshot->active = 1;
                        printf("[SNAPSHOT_CONTROL] armed path=%s quality=%d\n",
                               snapshot->path, snapshot->quality);
                    } else {
                        printf("[SNAPSHOT_CONTROL] failed to arm path=%s\n",
                               snapshot->path);
                    }
                }
            } else {
                printf("[RTP_CONTROL] ignored command: %s\n", buf);
            }
            continue;
        }
        if (rd < 0 && errno == EINTR) {
            continue;
        }
        if (rd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
    return changed;
}

static int force_idr_stream0(const char *reason)
{
    uint32_t mask = 1; /* 1 << stream_id 0 */
    int ret;

    errno = 0;
    ret = ioctl(gfd, IOC_FORCE_IDR, mask);
    printf("[IDR] force_idr(%s) ret=%d errno=%d\n",
           reason ? reason : "unknown", ret, errno);
    return ret;
}

static int arg_is_integer(const char *s)
{
    if (!s || !*s) {
        return 0;
    }
    while (*s) {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}

static int clamp_bitrate_kbps(int bitrate_kbps)
{
    if (bitrate_kbps <= 0) {
        return 4096;
    }
    if (bitrate_kbps < 512) {
        return 512;
    }
    if (bitrate_kbps > 4096) {
        return 4096;
    }
    return bitrate_kbps;
}

static int clamp_gop_n(int gop_n)
{
    if (gop_n <= 0) {
        return 25;
    }
    if (gop_n < 5) {
        return 5;
    }
    if (gop_n > 120) {
        return 120;
    }
    return gop_n;
}

static int clamp_idr_interval(int idr_interval)
{
    if (idr_interval <= 0) {
        return 1;
    }
    if (idr_interval > 10) {
        return 10;
    }
    return idr_interval;
}

static int clamp_brc_mode(int brc_mode)
{
    if (brc_mode < 0 || brc_mode > 3) {
        return 0;
    }
    return brc_mode;
}

static int clamp_rtsp_periodic_idr_ms(int interval_ms)
{
    if (interval_ms <= 0) {
        return 0;
    }
    if (interval_ms < 500) {
        return 500;
    }
    if (interval_ms > 10000) {
        return 10000;
    }
    return interval_ms;
}

static void set_frame_interval_stream(int stream_id)
{
    struct {
        uint32_t mask;
        uint8_t  numerator;
        uint8_t  denominator;
        uint8_t  pad[2];
    } fi;
    memset(&fi, 0, sizeof(fi));
    fi.mask = 1U << stream_id;
    fi.numerator = 60;
    fi.denominator = 60;
    int r = ioctl(gfd, IOC_SET_FRAME_INTERVAL, &fi);
    printf("[SET_FRAME_INTERVAL:%d] ret=%d errno=%d\n", stream_id, r, errno);
}

static void set_encode_format_stream_type(int stream_id, int channel_id,
                                          int encode_type,
                                          uint16_t width, uint16_t height,
                                          uint32_t fps)
{
    uint8_t fmt[128];
    memset(fmt, 0, sizeof(fmt));
    *(uint32_t *)(fmt + 0) = 1U << stream_id;
    fmt[4] = (uint8_t)encode_type;   /* 1: H264, 2: MJPEG */
    fmt[5] = (uint8_t)channel_id;
    fmt[6] = 0;                      /* flipRotate */
    *(uint16_t *)(fmt + 8) = width;
    *(uint16_t *)(fmt + 10) = height;
    *(uint16_t *)(fmt + 12) = 0;     /* xOffset */
    *(uint16_t *)(fmt + 14) = 0;     /* yOffset */
    *(uint32_t *)(fmt + 16) = fps;
    fmt[20] = 0;                     /* keepAspRat */

    int r = ioctl(gfd, IOC_SET_ENCODE_FORMAT, fmt);
    printf("[SET_ENCODE_FORMAT:%d] ret=%d errno=%d type=%d ch=%d %ux%u fps=%u\n",
           stream_id, r, errno, encode_type, channel_id, width, height, fps);
}

static void set_encode_format_stream(int stream_id, int channel_id,
                                     uint16_t width, uint16_t height,
                                     uint32_t fps)
{
    set_encode_format_stream_type(stream_id, channel_id, 1, width, height, fps);
}

static void set_bitrate_stream(int stream_id, int brc_mode,
                               uint32_t cbr, uint32_t min, uint32_t max)
{
    uint8_t br[128];
    memset(br, 0, sizeof(br));
    *(uint32_t *)(br + 0) = 1U << stream_id;
    *(uint32_t *)(br + 4) = (uint32_t)brc_mode;
    *(uint32_t *)(br + 8) = cbr;
    *(uint32_t *)(br + 12) = min;
    *(uint32_t *)(br + 16) = max;
    int r = ioctl(gfd, IOC_SET_BITRATE, br);
    printf("[SET_BITRATE:%d] ret=%d errno=%d brc=%d cbr=%u min=%u max=%u\n",
           stream_id, r, errno, brc_mode, cbr, min, max);
}

static void set_h264_config_stream(int stream_id, int gop_n, int idr_interval,
                                   int brc_mode, uint32_t cbr, uint32_t min,
                                   uint32_t max)
{
    uint8_t cfg[512];
    memset(cfg, 0, sizeof(cfg));
    *(uint32_t *)(cfg + 0) = 1U << stream_id;
    int ret = ioctl(gfd, IOC_GETSET_H264_CFG, cfg);
    printf("[H264:%d] GET ret=%d errno=%d\n", stream_id, ret, errno);
    if (ret != 0) return;

    dump_bytes("[H264] before", cfg, 80);

    *(uint16_t *)(cfg + 44) = 1;      /* gopM */
    *(uint16_t *)(cfg + 46) = (uint16_t)gop_n;
    cfg[48] = (uint8_t)idr_interval;
    cfg[49] = 0;                      /* gopModel */
    cfg[50] = 4;                      /* internal field set by Sofia wrapper */
    cfg[54] = 0;                      /* GK SDK default: produces D1 stream_id 0 reliably */
    *(uint32_t *)(cfg + 4) = cbr;     /* selected bitrate copied by wrapper */
    cfg[110] = 0;                     /* reEncMode */

    ret = ioctl(gfd, IOC_SET_H264_CFG, cfg);
    printf("[H264:%d] SET ret=%d errno=%d gop=%u/%u idr=%u profile=%u brc=%d\n",
           stream_id, ret, errno, *(uint16_t *)(cfg + 44),
           *(uint16_t *)(cfg + 46), cfg[48], cfg[54], brc_mode);
    dump_bytes("[H264] after", cfg, 80);

    set_bitrate_stream(stream_id, brc_mode, cbr, min, max);
}

static void set_mjpeg_config_stream(GADI_SYS_HandleT handle, int stream_id, int quality)
{
    GADI_VENC_MjpegConfigT cfg;
    GADI_ERR err;

    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;

    memset(&cfg, 0, sizeof(cfg));
    cfg.streamId = (GADI_U32)stream_id;
    cfg.chromaFormat = 1; /* YUV 420 */
    cfg.quality = (GADI_U8)quality;

    err = gadi_venc_set_mjpeg_config(handle, &cfg);
    printf("[MJPEG:%d] SET quality=%d ret=%d\n", stream_id, quality, err);
}

static void set_h264_qp_stream(int stream_id)
{
    uint8_t qp[32];
    memset(qp, 0, sizeof(qp));
    *(uint32_t *)(qp + 0) = 1U << stream_id;
    int ret = ioctl(gfd, IOC_GET_H264_QP_CFG, qp);
    printf("[H264_QP:%d] GET ret=%d errno=%d\n", stream_id, ret, errno);
    if (ret != 0) return;

    qp[4] = 0x17;  /* qpMinOnI */
    qp[5] = 0x33;  /* qpMaxOnI */
    qp[6] = 0x23;  /* qpMinOnP */
    qp[7] = 0x33;  /* qpMaxOnP */
    qp[8] = 3;     /* qpIWeight */
    qp[9] = 5;     /* qpPWeight */
    qp[10] = 2;    /* adaptQp */
    ret = ioctl(gfd, IOC_SET_H264_QP_CFG, qp);
    printf("[H264_QP:%d] SET ret=%d errno=%d\n", stream_id, ret, errno);
}

static void query_stream0(const char *tag)
{
    uint32_t q[8];
    memset(q, 0, sizeof(q));
    q[0] = 1;
    int r = ioctl(gfd, IOC_QUERY_STREAM, q);
    printf("[QUERY_STREAM %s] ret=%d errno=%d q=%08x %08x %08x %08x\n",
           tag, r, errno, q[0], q[1], q[2], q[3]);
}

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

static void dump_annexb_frame(FILE **fout, const uint8_t *data, size_t len,
                              long long *written, long long limit_bytes)
{
    size_t write_len = len;

    if (!fout || !*fout || !data || len == 0) {
        return;
    }
    if (limit_bytes > 0 && *written >= limit_bytes) {
        fclose(*fout);
        *fout = NULL;
        printf("[DUMP] recording limit reached (%lld bytes)\n", *written);
        return;
    }
    if (limit_bytes > 0 && *written + (long long)write_len > limit_bytes) {
        write_len = (size_t)(limit_bytes - *written);
    }
    if (write_len > 0) {
        fwrite(data, 1, write_len, *fout);
        fflush(*fout);
        *written += (long long)write_len;
    }
    if (limit_bytes > 0 && *written >= limit_bytes) {
        fclose(*fout);
        *fout = NULL;
        printf("[DUMP] recording limit reached (%lld bytes)\n", *written);
    }
}

static int video_bridge_run_options(const struct video_bridge_options *opt)
{
    const char *remote_ip;
    int remote_port;
    int local_port;
    int payload_type;
    int bitrate_kbps;
    int gop_n;
    int idr_interval;
    int brc_mode;
    int rtsp_periodic_idr_ms;
    uint32_t main_cbr;
    uint32_t main_min;
    uint32_t main_max;
    int snapshot_mode = 0;
    const char *snapshot_path = NULL;
    int jpeg_quality = 90;
    const char *dumpfile;
    long long dump_limit_bytes;
    long long dump_written = 0;
    int rtp_sink_fd;
    int rtp_control_fd;
    FILE *fout = NULL;
    struct rtp_sender rtp;
    struct worker_snapshot_state worker_snapshot;
    int udp_enabled = 0;
    GADI_ERR err;
    int ret;
    int exit_code = 0;
    long long worker_start_ms = now_ms();

    if (!opt) {
        return 1;
    }

    snapshot_mode = opt->mode == VIDEO_BRIDGE_MODE_SNAPSHOT;
    snapshot_path = opt->snapshot_path;
    jpeg_quality = opt->jpeg_quality > 0 ? opt->jpeg_quality : 90;
    remote_ip = snapshot_mode ? "snapshot" : (opt->remote_ip ? opt->remote_ip : "rtsp-only");
    remote_port = snapshot_mode ? 0 : opt->remote_port;
    local_port = snapshot_mode ? 0 : opt->local_port;
    payload_type = snapshot_mode ? 0 : opt->payload_type;
    bitrate_kbps = opt->bitrate_kbps > 0 ? opt->bitrate_kbps : 4096;
    gop_n = clamp_gop_n(opt->gop_n);
    idr_interval = clamp_idr_interval(opt->idr_interval);
    brc_mode = clamp_brc_mode(opt->brc_mode);
    rtsp_periodic_idr_ms = clamp_rtsp_periodic_idr_ms(opt->rtsp_periodic_idr_ms);
    dumpfile = snapshot_mode ? NULL : opt->dumpfile;
    dump_limit_bytes = snapshot_mode ? 0 : opt->dump_limit_bytes;
    rtp_sink_fd = snapshot_mode ? -1 : opt->rtp_sink_fd;
    rtp_control_fd = snapshot_mode ? -1 : opt->rtp_control_fd;
    rtp_sender_init(&rtp, payload_type);
    memset(&worker_snapshot, 0, sizeof(worker_snapshot));

    if (snapshot_mode && (!snapshot_path || !snapshot_path[0])) {
        fprintf(stderr, "Missing snapshot output path\n");
        return 1;
    }

    bitrate_kbps = clamp_bitrate_kbps(bitrate_kbps);
    main_cbr = (uint32_t)bitrate_kbps * 1024U;
    main_min = (uint32_t)(bitrate_kbps * 2 / 3) * 1024U;
    main_max = (uint32_t)(bitrate_kbps * 4 / 3) * 1024U;
    if (main_min < 512U * 1024U) main_min = 512U * 1024U;
    if (main_max < main_cbr) main_max = main_cbr;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_alarm;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGALRM, &sa, NULL);
    }
    signal(SIGTERM, on_stop);
    signal(SIGINT, on_stop);
    signal(SIGPIPE, SIG_IGN);

    printf("=== WiBox D1 Video RTP Bridge ===\n");
    if (snapshot_mode) {
        printf("Mode: snapshot\n");
        printf("Snapshot output: %s\n", snapshot_path);
        printf("JPEG quality: %d\n", jpeg_quality);
    } else {
        printf("Remote: %s:%d\n", remote_ip, remote_port);
        printf("Local RTP port: %d\n", local_port);
        printf("Payload type: %d\n", payload_type);
        printf("Target bitrate: %d kbps (cbr=%u min=%u max=%u)\n",
               bitrate_kbps, main_cbr, main_min, main_max);
        printf("Encoder tuning: gop_n=%d idr_interval=%d brc_mode=%d rtsp_periodic_idr_ms=%d\n",
               gop_n, idr_interval, brc_mode, rtsp_periodic_idr_ms);
    }
    if (dumpfile) printf("Debug dump: %s\n", dumpfile);
    if (rtp_sink_fd >= 0) printf("RTP sink fd: %d\n", rtp_sink_fd);
    if (rtp_control_fd >= 0) printf("RTP control fd: %d\n", rtp_control_fd);
    printf("\n");

    udp_enabled = !snapshot_mode && remote_port > 0;

    if (!snapshot_mode &&
        (payload_type <= 0 || payload_type > 127 ||
         (udp_enabled && local_port <= 0) ||
         (!udp_enabled && rtp_sink_fd < 0))) {
        fprintf(stderr, "Invalid RTP arguments\n");
        return 1;
    }
    if (udp_enabled && rtp_sender_open(&rtp, remote_ip, remote_port, local_port, payload_type) < 0) {
        return 1;
    }
    if (!snapshot_mode) {
        rtp.sink_fd = rtp_sink_fd;
    }
    if (rtp_control_fd >= 0) {
        int flags = fcntl(rtp_control_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(rtp_control_fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    if (snapshot_mode) {
        fout = fopen(snapshot_path, "wb");
        if (!fout) {
            perror("fopen snapshot");
            return 1;
        }
    }
    if (dumpfile) {
        fout = fopen(dumpfile, "wb");
        if (!fout) perror("fopen dump");
    }

    /* ── SDK Init Chain ── */
    printf("[SDK] gadi_sys_init...\n");
    err = gadi_sys_init();
    printf("[SDK] sys_init: %s\n\n", err ? "FAIL" : "OK");
    if (err) return 1;

    printf("[SDK] vi_init + vi_open...\n");
    err = gadi_vi_init();
    printf("[SDK] vi_init: %s\n", err ? "FAIL" : "OK");
    GADI_SYS_HandleT vi_handle = gadi_vi_open(&err);
    printf("[SDK] vi_open: %s handle=%p\n\n", err ? "FAIL" : "OK", vi_handle);
    if (err) return 1;

    printf("[SDK] vout_init + vout_open...\n");
    err = gadi_vout_init();
    GADI_SYS_HandleT vo_handle = gadi_vout_open(&err);
    printf("[SDK] vout: %s handle=%p\n\n", err ? "FAIL" : "OK", vo_handle);
    if (err) return 1;

    printf("[SDK] venc_init + venc_open...\n");
    err = gadi_venc_init();
    printf("[SDK] venc_init: %s\n", err ? "FAIL" : "OK");

    GADI_VENC_OpenParamsT op;
    memset(&op, 0, sizeof(op));
    op.viHandle   = vi_handle;
    op.voutHandle = vo_handle;

    GADI_SYS_HandleT venc_handle = gadi_venc_open(&op, &err);
    printf("[SDK] venc_open: %s handle=%p err=%d\n\n",
           err ? "FAIL" : "OK", venc_handle, err);
    if (err) return 1;

    /* ── Get BSB (bitstream buffer) via SDK ── */
    printf("[SDK] map_bsb...\n");
    err = gadi_venc_map_bsb(venc_handle);
    printf("[SDK] map_bsb: %s\n\n", err ? "FAIL" : "OK");
    if (err) {
        fprintf(stderr, "FATAL: map_bsb failed err=%d\n", err);
        return 1;
    }

    {
        GADI_VENC_DspMapInfoT dspInfo;
        memset(&dspInfo, 0, sizeof(dspInfo));
        err = gadi_venc_map_dsp(venc_handle, &dspInfo);
        printf("[SDK] map_dsp: %s addr=%p length=%u\n\n",
               err ? "FAIL" : "OK", dspInfo.addr, dspInfo.length);
    }

    /* ── Switch to raw fd for VENC config ── */
    printf("[RAW] Opening /dev/gk_video...\n");
    gfd = open(DEVICE, O_RDWR);
    if (gfd < 0) { perror("open"); return 1; }
    printf("[RAW] fd=%d\n\n", gfd);

    /* Sofia does this before SET_SRCBUF_FORMAT; populate sys_state[0xdc]. */
    get_vi_caps_once(gfd);

    /* ── Chip info ── */
    {
        uint32_t ci[2] = {0};
        ioctl(gfd, IOC_GET_CHIP_INFO, ci);
        printf("[INFO] chip_type=%d\n\n", ci[0]);
    }

    {
        ret = ioctl(gfd, IOC_SYS_0x7B, 0);
        printf("[SYS_0x7B] ioctl(0x4004767b) ret=%d errno=%d\n", ret, errno);
    }

    /* ── Stop/Reset encoder state ── */
    printf("[RESET] Resetting encode state...\n");
    try_stop_stream();

    /* ── Set resource limits (required before SET_SRCBUF_FORMAT) ── */
    printf("[LIMITS] Setting resource limits...\n");
    set_resource_limits();

    /* ── Key ioctl: SET_SRCBUF_FORMAT ── */
    printf("[CONFIG] SET_SRCBUF_FORMAT (Sofia D1 40-byte struct)...\n");
    ret = set_srcbuf_format();
    if (ret < 0) {
        fprintf(stderr, "FATAL: SET_SRCBUF_FORMAT failed.\n");
        fprintf(stderr, "Did Sofia run for 25+s? sys_state needs VI init.\n");
        exit_code = 1;
        goto out;
    }

    /* ── SET_SRCBUF_TYPE: channel 0 = H264 ── */
    printf("[CONFIG] SET_SRCBUF_TYPE...\n");
    {
        uint32_t types[4] = {snapshot_mode ? 2U : 1U, 1, 1, 0};
        ret = ioctl(gfd, IOC_SET_SRCBUF_TYPE, types);
        printf("[CONFIG] SET_SRCBUF_TYPE ret=%d\n\n", ret);
    }

    /* ── Stream config. Sofia configures streams 0, 1 and 2 before start. ── */
    printf("[CONFIG] Streams 0/1/2 encode defaults...\n");
    set_frame_interval_stream(0);
    if (snapshot_mode) {
        set_encode_format_stream_type(0, 0, 2, 688, 576, 5);
        set_mjpeg_config_stream(venc_handle, 0, jpeg_quality);
    } else {
        set_encode_format_stream(0, 0, 688, 576, 25);
        set_h264_config_stream(0, gop_n, idr_interval, brc_mode,
                               main_cbr, main_min, main_max);
    }

    set_frame_interval_stream(1);
    set_encode_format_stream(1, 1, 352, 300, 25);
    set_h264_config_stream(1, 25, 1, 0, 0x40000, 0x40000, 0x80000);

    set_frame_interval_stream(2);
    set_encode_format_stream(2, 2, 352, 288, 25);
    set_h264_config_stream(2, 25, 1, 0, 0x40000, 0x40000, 0x80000);
    printf("\n");

    err = gadi_vi_enable(vi_handle, 1);
    printf("[SDK] vi_enable(1): %s err=%d\n", err ? "FAIL" : "OK", err);
    usleep(100000);

    /* ── START_STREAM ── */
    printf("[START] Starting streams...\n");
    {
        query_stream0("before");
        ret = -1;
        for (int attempt = 0; attempt < 3; attempt++) {
            errno = 0;
            ret = ioctl(gfd, IOC_START_ENCODE, 1);
            printf("[START] mask=0x1 attempt=%d/3 ret=%d errno=%d\n",
                   attempt + 1, ret, errno);
            if (ret == 0) break;
            if (errno == EINVAL) break;
            usleep(100000);
        }
        query_stream0("after");
        printf("\n");
        if (ret < 0) {
            fprintf(stderr, "START_STREAM failed, trying with factory state...\n");
        }
    }
    usleep(50000);

    force_idr_stream0("startup");
    usleep(20000);
    force_idr_stream0("startup-repeat");
    usleep(20000);

    /* ── CAPTURE LOOP ── */
    printf("[CAPTURE] Starting %s capture\n", snapshot_mode ? "snapshot" : "RTP");

    h264_nal_cache_t sps_cache = {0};
    h264_nal_cache_t pps_cache = {0};
    long long total_bytes = 0;
    int frames = 0, idrs = 0, errors = 0;
    int seen_sid[16] = {0};
    int seen_nal[32] = {0};
    int sent_parameter_sets_for_first_idr = 0;
    int first_stream0_logged = 0;
    int first_rtp_logged = 0;
    int first_sps_logged = 0;
    int first_pps_logged = 0;
    int first_idr_logged = 0;
    long long capture_start_ms = now_ms();
    long long last_startup_idr_request_ms = 0;
    long long last_rtsp_idr_request_ms = 0;

    printf("[VIDEO_STARTUP] capture_loop_start=%lldms\n",
           capture_start_ms - worker_start_ms);

    for (int i = 0; !stop_requested && errors < 30; i++) {
        GADI_VENC_StreamT st;
        h264_annexb_scan_t scan;
        memset(&st, 0, sizeof(st));

        if (poll_rtp_control(rtp_control_fd, &rtp, venc_handle, &worker_snapshot)) {
            force_idr_stream0("rtp-control");
        }

        ret = get_stream_with_timeout(venc_handle, 0xFF, &st);
        if (ret < 0) { errors++; usleep(10000); continue; }
        errors = 0;

        if (!st.size || !st.addr) { usleep(5000); continue; }

        uint8_t *data = st.addr;
        uint32_t  sz  = st.size;

        if (!snapshot_mode && worker_snapshot.active &&
            st.stream_id == SNAPSHOT_STREAM_ID &&
            sz >= 4 && data[0] == 0xff && data[1] == 0xd8) {
            if (sz < SNAPSHOT_MIN_JPEG_BYTES) {
                printf("[SNAPSHOT_CONTROL] skipping small sid%d jpeg %u bytes\n",
                       SNAPSHOT_STREAM_ID, sz);
                usleep(1000);
                continue;
            }
            if (snapshot_file_write_atomic(worker_snapshot.path, data, sz) == 0) {
                printf("[SNAPSHOT_CONTROL] wrote sid%d %ux%u jpeg %u bytes to %s\n",
                       SNAPSHOT_STREAM_ID, SNAPSHOT_WIDTH, SNAPSHOT_HEIGHT,
                       sz, worker_snapshot.path);
            }
            worker_snapshot.active = 0;
            usleep(1000);
            continue;
        }
        if (!snapshot_mode && worker_snapshot.mjpeg_started &&
            st.stream_id == SNAPSHOT_STREAM_ID) {
            usleep(1000);
            continue;
        }

        if (snapshot_mode) {
            if (st.stream_id != 0) { usleep(1000); continue; }
            printf("  [snapshot pkt %03d] sid=%u pic=%u sz=%u bytes=%02x %02x %02x %02x\n",
                   i, st.stream_id, st.pic_type, sz,
                   sz > 0 ? data[0] : 0, sz > 1 ? data[1] : 0,
                   sz > 2 ? data[2] : 0, sz > 3 ? data[3] : 0);
            if (sz >= 4 && data[0] == 0xff && data[1] == 0xd8) {
                fwrite(data, 1, sz, fout);
                fflush(fout);
                total_bytes += sz;
                frames = 1;
                printf("[SNAPSHOT] wrote %u bytes to %s\n", sz, snapshot_path);
                break;
            }
            usleep(5000);
            continue;
        }

        int nal = h264_annexb_scan(data, sz, &scan,
                                  st.stream_id == 0 ? &sps_cache : NULL,
                                  st.stream_id == 0 ? &pps_cache : NULL);

        if (st.stream_id < 16) seen_sid[st.stream_id]++;
        if (st.stream_id == 0) {
            for (int n = 0; n < 32; n++) {
                /* Keep the compact legacy counters useful for the important startup NALs. */
                if ((n == 1 && scan.has_slice && !scan.has_idr) ||
                    (n == 5 && scan.has_idr) ||
                    (n == 7 && scan.has_sps) ||
                    (n == 8 && scan.has_pps)) {
                    seen_nal[n]++;
                }
            }
        } else if (nal >= 0 && nal < 32) {
            seen_nal[nal]++;
        }
        if (i < 5 || scan.has_idr || scan.has_sps || scan.has_pps) {
            printf("  [pkt %03d] sid=%u first_nal=0x%02x last_nal=0x%02x nals=%d sps=%d pps=%d idr=%d pic=%u sz=%u addr=%p\n",
                   i, st.stream_id, scan.first_nal, scan.last_nal, scan.nal_count,
                   scan.has_sps, scan.has_pps, scan.has_idr, st.pic_type, sz, st.addr);
        }

        /* Main stream only: stream_id == 0 */
        if (st.stream_id != 0) { usleep(1000); continue; }

        if (!first_stream0_logged) {
            printf("[VIDEO_STARTUP] first_stream0=%lldms size=%u first_nal=%d pic=%u\n",
                   now_ms() - worker_start_ms, sz, scan.first_nal, st.pic_type);
            first_stream0_logged = 1;
        }
        if (scan.has_sps && !first_sps_logged) {
            printf("[VIDEO_STARTUP] first_sps=%lldms\n", now_ms() - worker_start_ms);
            first_sps_logged = 1;
        }
        if (scan.has_pps && !first_pps_logged) {
            printf("[VIDEO_STARTUP] first_pps=%lldms\n", now_ms() - worker_start_ms);
            first_pps_logged = 1;
        }
        if (!scan.has_idr && idrs == 0) {
            long long now = now_ms();
            if (last_startup_idr_request_ms == 0 || now - last_startup_idr_request_ms >= 250) {
                force_idr_stream0("waiting-first-idr");
                last_startup_idr_request_ms = now;
            }
        }
        if (scan.has_idr || st.pic_type == GADI_VENC_IDR_FRAME) {
            int send_parameter_sets =
                !sent_parameter_sets_for_first_idr &&
                ((sps_cache.data && sps_cache.len) || (pps_cache.data && pps_cache.len));

            if (!first_idr_logged) {
                printf("[VIDEO_STARTUP] first_idr=%lldms sps_cached=%d pps_cached=%d\n",
                       now_ms() - worker_start_ms,
                       sps_cache.data != NULL, pps_cache.data != NULL);
                first_idr_logged = 1;
            }
            if (send_parameter_sets) {
                if (sps_cache.data && sps_cache.len) {
                    rtp_send_parameter_set(&rtp, sps_cache.data, sps_cache.len);
                }
                if (pps_cache.data && pps_cache.len) {
                    rtp_send_parameter_set(&rtp, pps_cache.data, pps_cache.len);
                }
                if (!sent_parameter_sets_for_first_idr) {
                    sent_parameter_sets_for_first_idr = 1;
                    printf("[VIDEO_STARTUP] sent_cached_parameter_sets=%lldms sps_len=%zu pps_len=%zu\n",
                           now_ms() - worker_start_ms, sps_cache.len, pps_cache.len);
                }
            }
            idrs++;
        }
        if (rtp_sink_fd >= 0 && idrs > 0 && rtsp_periodic_idr_ms > 0) {
            long long now = now_ms();
            if (last_rtsp_idr_request_ms == 0) {
                last_rtsp_idr_request_ms = now;
            } else if (now - last_rtsp_idr_request_ms >= rtsp_periodic_idr_ms) {
                force_idr_stream0("rtsp-periodic");
                last_rtsp_idr_request_ms = now;
            }
        }
        if (st.stream_id == 0) {
            if (rtp_send_annexb(&rtp, data, sz) < 0) {
                printf("[RTP] send failed errno=%d (%s)\n", errno, strerror(errno));
            } else if (!first_rtp_logged) {
                printf("[VIDEO_STARTUP] first_rtp=%lldms\n", now_ms() - worker_start_ms);
                first_rtp_logged = 1;
            }
            rtp.timestamp += 3600; /* 90 kHz / 25 fps */
            dump_annexb_frame(&fout, data, sz, &dump_written, dump_limit_bytes);
            total_bytes += sz;
            if (scan.has_idr || scan.has_slice || nal < 0) frames++;
            if (frames % 25 == 0)
                printf("  [%d frames] %lld bytes, %d IDRs (sid=%d first_nal=0x%02x sz=%u)\n",
                       frames, total_bytes, idrs, st.stream_id, scan.first_nal, sz);
        }
        usleep(3000);
    }

    printf("\n[CAPTURE] Done: %d frames, %lld bytes, %d IDRs\n",
           frames, total_bytes, idrs);
    printf("[CAPTURE] stream_ids:");
    for (int s = 0; s < 16; s++) if (seen_sid[s]) printf(" %d=%d", s, seen_sid[s]);
    printf("\n[CAPTURE] nal_types:");
    for (int n = 0; n < 32; n++) if (seen_nal[n]) printf(" %d=%d", n, seen_nal[n]);
    printf("\n");
    if (fout) fclose(fout);
    h264_nal_cache_clear(&sps_cache);
    h264_nal_cache_clear(&pps_cache);

    if (frames == 0 && snapshot_mode) {
        fprintf(stderr, "\nNO SNAPSHOT CAPTURED!\n");
        exit_code = 1;
    } else if (frames == 0) {
        fprintf(stderr, "\nNO FRAMES CAPTURED!\n");
        fprintf(stderr, "Possible causes:\n");
        fprintf(stderr, "  1. Stream ID not 0 (try 0xFF or 2)\n");
        fprintf(stderr, "  2. BSB buffer offset issue\n");
        fprintf(stderr, "  3. VENC not producing output\n");
    }

stop:
    try_stop_stream();
out:
    if (gfd >= 0) close(gfd);
    if (rtp.fd >= 0) close(rtp.fd);
    return exit_code;
}

int main(int argc, char **argv)
{
    struct video_bridge_options opt;

    memset(&opt, 0, sizeof(opt));
    opt.mode = VIDEO_BRIDGE_MODE_RTP;
    opt.local_port = 8002;
    opt.payload_type = 96;
    opt.bitrate_kbps = 4096;
    opt.jpeg_quality = 90;
    opt.rtp_sink_fd = -1;
    opt.rtp_control_fd = -1;

    if (argc >= 3 && strcmp(argv[1], "--snapshot") == 0) {
        opt.mode = VIDEO_BRIDGE_MODE_SNAPSHOT;
        opt.snapshot_path = argv[2];
        if (argc > 3) {
            opt.jpeg_quality = atoi(argv[3]);
        }
    } else if (argc >= 3) {
        opt.remote_ip = argv[1];
        opt.remote_port = atoi(argv[2]);
        if (argc > 3) opt.local_port = atoi(argv[3]);
        if (argc > 4) opt.payload_type = atoi(argv[4]);
        if (argc > 5) {
            if (arg_is_integer(argv[5])) {
                opt.bitrate_kbps = atoi(argv[5]);
                opt.dumpfile = argc > 6 ? argv[6] : NULL;
            } else {
                opt.dumpfile = argv[5];
            }
        }
    } else {
        fprintf(stderr,
                "Usage: %s <remote_ip> <remote_video_port> [local_video_port] [payload_type] [bitrate_kbps] [dump.h264]\n"
                "       %s --snapshot <output.jpg> [quality]\n",
                argv[0], argv[0]);
        return 1;
    }

    return video_bridge_run_options(&opt);
}

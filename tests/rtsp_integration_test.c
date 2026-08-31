#include "rtsp_stream.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

/* The RTSP audio backchannel plays received frames via audio_hw_send_frame();
   this integration test does not link the audio HW layer, so stub it out. */
int audio_hw_send_frame(const unsigned char *buffer, size_t len)
{
    (void)buffer;
    (void)len;
    return 0;
}

int audio_hw_frame_size(void)
{
    return 160;
}

static volatile int callback_video;
static volatile int callback_audio;

static void client_count_changed(int video, int audio, void *user_data)
{
    int *calls = (int *)user_data;
    callback_video = video;
    callback_audio = audio;
    (*calls)++;
}

static int reserve_port(void)
{
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int port;

    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(fd, (struct sockaddr *)&address, &length) != 0) {
        close(fd);
        return -1;
    }
    port = ntohs(address.sin_port);
    close(fd);
    return port;
}

static int connect_rtsp(int port)
{
    struct sockaddr_in address;
    struct timeval timeout = {2, 0};
    int attempt;

    for (attempt = 0; attempt < 50; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons((unsigned short)port);
        if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            return fd;
        }
        close(fd);
        usleep(20000);
    }
    return -1;
}

static int read_response(int fd, char *response, size_t size)
{
    size_t used = 0;
    size_t expected = 0;

    while (used + 1 < size) {
        ssize_t received = recv(fd, response + used, size - used - 1, 0);
        char *header_end;
        char *content_length;
        if (received <= 0) break;
        used += (size_t)received;
        response[used] = '\0';
        header_end = strstr(response, "\r\n\r\n");
        if (!header_end) continue;
        if (expected == 0) {
            content_length = strstr(response, "Content-Length: ");
            expected = (size_t)(header_end + 4 - response);
            if (content_length) expected += (size_t)atoi(content_length + 16);
        }
        if (used >= expected) break;
    }
    response[used] = '\0';
    return (int)used;
}

static int request(int fd, const char *method, const char *uri, int cseq,
                   const char *headers, char *response, size_t size)
{
    char data[2048];
    int length = snprintf(data, sizeof(data), "%s %s RTSP/1.0\r\nCSeq: %d\r\n%s\r\n",
                          method, uri, cseq, headers ? headers : "");
    if (length <= 0 || (size_t)length >= sizeof(data) ||
        send(fd, data, (size_t)length, 0) != length) {
        return -1;
    }
    return read_response(fd, response, size);
}

static int read_interleaved(int fd, int channel, const unsigned char *payload,
                            size_t payload_len)
{
    unsigned char buffer[512];
    size_t needed = payload_len + 4;
    size_t used = 0;

    while (used < needed) {
        ssize_t received = recv(fd, buffer + used, needed - used, 0);
        if (received <= 0) return -1;
        used += (size_t)received;
    }
    if (buffer[0] != '$' || buffer[1] != (unsigned char)channel ||
        (((size_t)buffer[2] << 8) | buffer[3]) != payload_len ||
        memcmp(buffer + 4, payload, payload_len) != 0) {
        return -1;
    }
    return 0;
}

static int wait_counts(int video, int audio)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (rtsp_stream_get_video_client_count() == video &&
            rtsp_stream_get_audio_client_count() == audio) return 0;
        usleep(10000);
    }
    return -1;
}

static int test_audio_only(int *callback_calls)
{
    static const unsigned char audio_packet[] = {0x80, 0x08, 0x00, 0x01, 0xaa, 0xbb};
    char response[4096];
    int port = reserve_port();
    int fd;

    CHECK(port > 0);
    CHECK(rtsp_stream_start(port, "127.0.0.1", 0, "", "") == 0);
    CHECK(rtsp_stream_get_video_pipe_fd() == -1);
    fd = connect_rtsp(port);
    CHECK(fd >= 0);

    CHECK(request(fd, "OPTIONS", "rtsp://127.0.0.1/live", 1, NULL,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "200 OK") != NULL);
    CHECK(strstr(response, "Public: OPTIONS") != NULL);
    CHECK(request(fd, "DESCRIBE", "rtsp://127.0.0.1/live", 2, NULL,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "m=audio 0 RTP/AVP 8") != NULL);
    CHECK(strstr(response, "m=video") == NULL);
    CHECK(request(fd, "SETUP", "rtsp://127.0.0.1/live/trackID=0", 3,
                  "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n",
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "404 Not Found") != NULL);
    CHECK(request(fd, "SETUP", "rtsp://127.0.0.1/live/trackID=1", 4,
                  "Transport: RTP/AVP/TCP;unicast;interleaved=4-5\r\n",
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "200 OK") != NULL);
    CHECK(request(fd, "PLAY", "rtsp://127.0.0.1/live", 5, NULL,
                  response, sizeof(response)) > 0);
    CHECK(wait_counts(0, 1) == 0);
    rtsp_stream_send_audio_rtp(audio_packet, sizeof(audio_packet));
    CHECK(read_interleaved(fd, 4, audio_packet, sizeof(audio_packet)) == 0);
    rtsp_stream_send_audio_rtp(NULL, sizeof(audio_packet));
    rtsp_stream_send_audio_rtp(audio_packet, 0);

    CHECK(request(fd, "PAUSE", "rtsp://127.0.0.1/live", 6, NULL,
                  response, sizeof(response)) > 0);
    CHECK(wait_counts(0, 0) == 0);
    CHECK(request(fd, "GET_PARAMETER", "rtsp://127.0.0.1/live", 7, NULL,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "200 OK") != NULL);
    CHECK(request(fd, "UNKNOWN", "rtsp://127.0.0.1/live", 8, NULL,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "405 Method Not Allowed") != NULL);
    CHECK(request(fd, "TEARDOWN", "rtsp://127.0.0.1/live", 9, NULL,
                  response, sizeof(response)) > 0);
    close(fd);
    CHECK(wait_counts(0, 0) == 0);
    rtsp_stream_stop();
    rtsp_stream_stop();
    CHECK(*callback_calls > 0);
    return 0;
}

static int test_video_auth(void)
{
    static const unsigned char video_packet[] = {0x80, 0x60, 0x01, 0x02, 0x65, 0x88};
    unsigned char pipe_packet[sizeof(video_packet) + 2];
    char response[4096];
    const char *good_auth = "Authorization: Basic dXNlcjpwYXNz\r\n";
    int port = reserve_port();
    int pipe_fd;
    int fd;

    CHECK(port > 0);
    CHECK(rtsp_stream_start(port, "127.0.0.1", 1, "user", "pass") == 0);
    pipe_fd = rtsp_stream_get_video_pipe_fd();
    CHECK(pipe_fd >= 0);
    fd = connect_rtsp(port);
    CHECK(fd >= 0);

    CHECK(request(fd, "OPTIONS", "rtsp://127.0.0.1/live", 1, NULL,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "200 OK") != NULL);
    CHECK(request(fd, "DESCRIBE", "rtsp://127.0.0.1/live", 2, NULL,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "401 Unauthorized") != NULL);
    CHECK(strstr(response, "WWW-Authenticate: Basic") != NULL);
    CHECK(request(fd, "DESCRIBE", "rtsp://127.0.0.1/live", 3,
                  "Authorization: Basic bad\r\n", response, sizeof(response)) > 0);
    CHECK(strstr(response, "401 Unauthorized") != NULL);
    CHECK(request(fd, "DESCRIBE", "rtsp://127.0.0.1/live", 4, good_auth,
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "m=video 0 RTP/AVP 96") != NULL);
    CHECK(strstr(response, "m=audio 0 RTP/AVP 8") != NULL);
    CHECK(request(fd, "SETUP", "rtsp://127.0.0.1/live/trackID=0", 5,
                  "Authorization: Basic dXNlcjpwYXNz\r\n"
                  "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n",
                  response, sizeof(response)) > 0);
    CHECK(strstr(response, "200 OK") != NULL);
    CHECK(request(fd, "PLAY", "rtsp://127.0.0.1/live", 6, good_auth,
                  response, sizeof(response)) > 0);
    CHECK(wait_counts(1, 0) == 0);

    pipe_packet[0] = 0;
    pipe_packet[1] = sizeof(video_packet);
    memcpy(pipe_packet + 2, video_packet, sizeof(video_packet));
    CHECK(write(pipe_fd, pipe_packet, sizeof(pipe_packet)) == (ssize_t)sizeof(pipe_packet));
    CHECK(read_interleaved(fd, 0, video_packet, sizeof(video_packet)) == 0);

    rtsp_stream_set_video_enabled(0);
    CHECK(wait_counts(0, 0) == 0);
    CHECK(rtsp_stream_get_video_pipe_fd() == -1);
    rtsp_stream_set_video_enabled(0);
    close(fd);
    rtsp_stream_stop();
    return 0;
}

int main(void)
{
    int callback_calls = 0;

    rtsp_stream_set_client_callback(client_count_changed, &callback_calls);
    CHECK(callback_video == 0 && callback_audio == 0);
    CHECK(test_audio_only(&callback_calls) == 0);
    CHECK(test_video_auth() == 0);
    printf("RESULT rtsp_integration PASS callbacks=%d\n", callback_calls);
    return 0;
}

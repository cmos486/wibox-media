#include "prometheus.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
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

int mqtt_is_connected(void)
{
    return 1;
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
    address.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(fd, (struct sockaddr *)&address, &length) != 0) {
        close(fd);
        return -1;
    }
    port = ntohs(address.sin_port);
    close(fd);
    return port;
}

static int http_get(int port, const char *path, char *response, size_t size)
{
    struct sockaddr_in address;
    struct timeval timeout = {2, 0};
    char request[256];
    size_t used = 0;
    int fd;
    int attempt;

    for (attempt = 0; attempt < 50; attempt++) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons((unsigned short)port);
        if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) break;
        close(fd);
        fd = -1;
        usleep(20000);
    }
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", path);
    if (send(fd, request, strlen(request), 0) < 0) {
        close(fd);
        return -1;
    }
    while (used + 1 < size) {
        ssize_t received = recv(fd, response + used, size - used - 1, 0);
        if (received <= 0) break;
        used += (size_t)received;
    }
    response[used] = '\0';
    close(fd);
    return (int)used;
}

int main(void)
{
    char response[16384];
    int port = reserve_port();

    CHECK(port > 0);
    CHECK(prometheus_start(0) == -1);
    CHECK(prometheus_start(65536) == -1);
    CHECK(prometheus_start(port) == 0);
    CHECK(prometheus_start(port) == 0);

    prometheus_set_call_active(1);
    prometheus_set_sip_call_active(1);
    prometheus_set_video_active(1);
    prometheus_set_video_enabled(1);
    prometheus_set_ringing(1);
    prometheus_inc_ring();
    prometheus_inc_door_unlock();
    prometheus_inc_call_started();
    prometheus_inc_video_started();
    prometheus_inc_uart_frame();
    prometheus_inc_uart_unknown_frame();
    prometheus_inc_uart_alarm_report();
    prometheus_inc_uart_hangup();
    prometheus_inc_uart_stop_ring();
    prometheus_inc_uart_reset();
    prometheus_inc_uart_push_state();
    prometheus_inc_uart_f1();

    CHECK(http_get(port, "/healthz", response, sizeof(response)) > 0);
    CHECK(strstr(response, "HTTP/1.1 200 OK") != NULL);
    CHECK(strstr(response, "ok\n") != NULL);

    CHECK(http_get(port, "/metrics?full=1", response, sizeof(response)) > 0);
    CHECK(strstr(response, "wibox_info{version=\"coverage-test\"") != NULL);
    CHECK(strstr(response, "wibox_mqtt_connected 1") != NULL);
    CHECK(strstr(response, "wibox_call_active 1") != NULL);
    CHECK(strstr(response, "wibox_sip_call_active 1") != NULL);
    CHECK(strstr(response, "wibox_video_active 1") != NULL);
    CHECK(strstr(response, "wibox_video_enabled 1") != NULL);
    CHECK(strstr(response, "wibox_ringing 1") != NULL);
    CHECK(strstr(response, "wibox_rings_total 1") != NULL);
    CHECK(strstr(response, "wibox_door_unlocks_total 1") != NULL);
    CHECK(strstr(response, "wibox_uart_frames_total 1") != NULL);
    CHECK(strstr(response, "wibox_uart_unknown_frames_total 1") != NULL);
    CHECK(strstr(response, "wibox_uart_f1_total 1") != NULL);

    CHECK(http_get(port, "/missing", response, sizeof(response)) > 0);
    CHECK(strstr(response, "HTTP/1.1 404 Not Found") != NULL);
    prometheus_stop();
    prometheus_stop();
    printf("RESULT prometheus_integration PASS port=%d\n", port);
    return 0;
}

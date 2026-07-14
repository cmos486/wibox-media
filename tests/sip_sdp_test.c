#include "sip_sdp.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    char sdp[1024];
    char medium[300];
    char tiny[32];
    int audio;
    int dtmf;
    int video;
    int h264;
    const char *answer =
        "v=0\r\n"
        "m=audio 41000 RTP/AVP 8 110\r\n"
        "a=rtpmap:8 PCMA/8000\r\n"
        "a=rtpmap:110 Telephone-Event/8000\r\n"
        "m=video 42000 RTP/AVP 99\r\n"
        "a=rtpmap:99 h264/90000\r\n";

    CHECK(sip_sdp_build("192.168.1.20", 8000, 0, 96, sdp, sizeof(sdp)) == 0);
    CHECK(strstr(sdp, "m=audio 8000 RTP/AVP 8 101") != NULL);
    CHECK(strstr(sdp, "PCMA/8000") != NULL);
    CHECK(strstr(sdp, "telephone-event/8000") != NULL);
    CHECK(strstr(sdp, "m=video") == NULL);

    CHECK(sip_sdp_build("192.168.1.20", 8000, 8002, 97, sdp, sizeof(sdp)) == 0);
    CHECK(strstr(sdp, "m=video 8002 RTP/AVP 97") != NULL);
    CHECK(strstr(sdp, "a=rtpmap:97 H264/90000") != NULL);
    CHECK(strstr(sdp, "a=sendonly") != NULL);
    CHECK(sip_sdp_build(NULL, 8000, 0, 96, sdp, sizeof(sdp)) == -1);
    CHECK(sip_sdp_build("", 8000, 0, 96, sdp, sizeof(sdp)) == -1);
    CHECK(sip_sdp_build("127.0.0.1", 0, 0, 96, sdp, sizeof(sdp)) == -1);
    CHECK(sip_sdp_build("127.0.0.1", 8000, 0, 96, NULL, sizeof(sdp)) == -1);
    CHECK(sip_sdp_build("127.0.0.1", 8000, 0, 96, sdp, 0) == -1);
    CHECK(sip_sdp_build("127.0.0.1", 8000, 0, 96, tiny, sizeof(tiny)) == -1);
    CHECK(sip_sdp_build("127.0.0.1", 8000, 8002, 96, tiny, sizeof(tiny)) == -1);
    CHECK(sip_sdp_build("127.0.0.1", 8000, 8002, 96,
                        medium, sizeof(medium)) == -1);

    CHECK(sip_sdp_parse(answer, &audio, &dtmf, &video, &h264) == 0);
    CHECK(audio == 41000);
    CHECK(dtmf == 110);
    CHECK(video == 42000);
    CHECK(h264 == 99);

    CHECK(sip_sdp_parse("v=0\r\n", &audio, &dtmf, &video, &h264) == 0);
    CHECK(audio == 8000 && dtmf == 101 && video == 0 && h264 == 0);
    CHECK(sip_sdp_parse("m=audio 9000 UDP 8\r\nm=video 0 RTP/AVP 96\r\n",
                        &audio, NULL, &video, NULL) == 0);
    CHECK(audio == 8000 && video == 0);
    CHECK(sip_sdp_parse("m=audio 9000\r\na=sendrecv",
                        &audio, &dtmf, &video, &h264) == 0);
    CHECK(audio == 8000 && dtmf == 101 && video == 0 && h264 == 0);
    CHECK(sip_sdp_parse(NULL, &audio, &dtmf, &video, &h264) == -1);
    CHECK(sip_sdp_parse(answer, NULL, &dtmf, &video, &h264) == -1);
    CHECK(sip_sdp_parse(answer, &audio, &dtmf, NULL, &h264) == -1);

    printf("RESULT sip_sdp PASS\n");
    return 0;
}

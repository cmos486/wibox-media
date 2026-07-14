#include "fakes/fake_pjsip.h"

#include <stdio.h>
#include <stdlib.h>

#include "../src/sip_media/sip_calling.c"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

typedef struct {
    pjsip_msg msg;
    pjsip_rx_data rx;
    pjsip_cid_hdr call_id;
    pjsip_from_hdr from;
    pjsip_to_hdr to;
    pjsip_contact_hdr contact;
    pjsip_msg_body body;
} rx_fixture_t;

static pjsip_endpoint endpoint;
static pj_pool_t test_pool;
static int state_callback_count;
static sip_call_state_t callback_old_state;
static sip_call_state_t callback_new_state;
static int audio_callback_count;
static char callback_remote_ip[32];
static int callback_audio_port;
static int callback_video_port;

static pj_str_t string_ref(const char *text) {
    pj_str_t value;
    value.ptr = (char *)text;
    value.slen = text ? (int)strlen(text) : 0;
    return value;
}

static void state_changed(sip_call_state_t old_state,
                          sip_call_state_t new_state,
                          void *user_data) {
    CHECK(user_data == &endpoint);
    state_callback_count++;
    callback_old_state = old_state;
    callback_new_state = new_state;
}

static void audio_ready(const char *remote_ip, int audio_port,
                        int video_port, void *user_data) {
    CHECK(user_data == &endpoint);
    audio_callback_count++;
    snprintf(callback_remote_ip, sizeof(callback_remote_ip), "%s",
             remote_ip ? remote_ip : "");
    callback_audio_port = audio_port;
    callback_video_port = video_port;
}

static sip_call_config_t default_config(void) {
    sip_call_config_t result;
    memset(&result, 0, sizeof(result));
    snprintf(result.target_uri, sizeof(result.target_uri),
             "sip:portal@example.test");
    result.call_timeout_seconds = 30;
    snprintf(result.local_ip, sizeof(result.local_ip), "192.0.2.10");
    result.local_sip_port = 5060;
    result.local_rtp_port = 8000;
    result.local_video_rtp_port = 8002;
    result.video_payload_type = 96;
    return result;
}

static void fresh_module(void) {
    sip_call_config_t call_config = default_config();
    sip_calling_cleanup();
    fake_pjsip_reset();
    state_callback_count = 0;
    callback_old_state = SIP_CALL_STATE_IDLE;
    callback_new_state = SIP_CALL_STATE_IDLE;
    audio_callback_count = 0;
    callback_remote_ip[0] = '\0';
    callback_audio_port = 0;
    callback_video_port = 0;
    CHECK(sip_calling_init(&call_config, &endpoint, &test_pool) == PJ_SUCCESS);
    sip_calling_set_callbacks(state_changed, audio_ready, &endpoint);
}

static void init_fixture(rx_fixture_t *fixture, int status_code,
                         const char *call_id, const char *from_tag,
                         const char *to_tag, const char *from_uri,
                         const char *contact_uri, const char *sdp,
                         const char *source_ip) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->msg.line.status.code = status_code;
    fixture->rx.msg_info.msg = &fixture->msg;
    fixture->call_id.hdr.type = PJSIP_H_CALL_ID;
    fixture->call_id.id = string_ref(call_id);
    fixture->msg.headers[PJSIP_H_CALL_ID] = (pjsip_hdr *)&fixture->call_id;
    fixture->from.hdr.type = PJSIP_H_FROM;
    fixture->from.tag = string_ref(from_tag);
    fixture->from.uri = (void *)from_uri;
    fixture->msg.headers[PJSIP_H_FROM] = (pjsip_hdr *)&fixture->from;
    fixture->to.hdr.type = PJSIP_H_TO;
    fixture->to.tag = string_ref(to_tag);
    fixture->msg.headers[PJSIP_H_TO] = (pjsip_hdr *)&fixture->to;
    fixture->contact.hdr.type = PJSIP_H_CONTACT;
    fixture->contact.uri = (void *)contact_uri;
    if (contact_uri) {
        fixture->msg.headers[PJSIP_H_CONTACT] = (pjsip_hdr *)&fixture->contact;
    }
    if (sdp) {
        fixture->body.data = (void *)sdp;
        fixture->body.len = (unsigned int)strlen(sdp);
        fixture->msg.body = &fixture->body;
    }
    CHECK(inet_pton(AF_INET, source_ip ? source_ip : "198.51.100.20",
                    &fixture->rx.pkt_info.src_addr.ipv4.sin_addr) == 1);
}

static void remove_call_id(rx_fixture_t *fixture) {
    fixture->msg.headers[PJSIP_H_CALL_ID] = NULL;
}

static void test_initialization_and_configuration(void) {
    sip_call_config_t call_config = default_config();
    char long_uri[400];

    sip_calling_cleanup();
    CHECK(sip_calling_init(NULL, &endpoint, &test_pool) == PJ_EINVAL);
    CHECK(sip_calling_init(&call_config, NULL, &test_pool) == PJ_EINVAL);
    CHECK(sip_calling_init(&call_config, &endpoint, NULL) == PJ_EINVAL);
    CHECK(sip_calling_init(&call_config, &endpoint, &test_pool) == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);
    CHECK(!sip_calling_is_call_active());
    CHECK(sip_calling_get_session() == NULL);

    sip_calling_set_call_timeout(1);
    CHECK(config.call_timeout_seconds == 10);
    sip_calling_set_call_timeout(999);
    CHECK(config.call_timeout_seconds == 120);
    sip_calling_set_call_timeout(44);
    CHECK(config.call_timeout_seconds == 44);
    CHECK(sip_calling_set_target_uri(NULL) == PJ_EINVAL);
    CHECK(sip_calling_set_target_uri("") == PJ_EINVAL);
    CHECK(sip_calling_set_target_uri("sip:new@example.test") == PJ_SUCCESS);
    memset(long_uri, 'x', sizeof(long_uri));
    long_uri[0] = 's';
    long_uri[1] = 'i';
    long_uri[2] = 'p';
    long_uri[3] = ':';
    long_uri[sizeof(long_uri) - 1] = '\0';
    CHECK(sip_calling_set_target_uri(long_uri) == PJ_SUCCESS);
    CHECK(config.target_uri[sizeof(config.target_uri) - 1] == '\0');

    config.video_payload_type = 96;
    sip_calling_set_video_config(-1, 200);
    CHECK(config.local_video_rtp_port == 0);
    CHECK(config.video_payload_type == 96);
    sip_calling_set_video_config(9002, 99);
    CHECK(config.local_video_rtp_port == 9002);
    CHECK(config.video_payload_type == 99);

    sip_calling_set_callbacks(NULL, NULL, NULL);
    set_call_state(SIP_CALL_STATE_FAILED);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);
    sip_calling_cleanup();
}

static void test_sdp_helpers(void) {
    pj_str_t result;
    int audio_port;
    int dtmf_payload;
    int video_port;
    int video_payload;
    const char *answer =
        "v=0\r\n"
        "c=IN IP4 203.0.113.7\r\n"
        "m=audio 4010 RTP/AVP 0 101\r\n"
        "a=rtpmap:101 telephone-event/8000\r\n"
        "m=video 4012 RTP/AVP 98\r\n"
        "a=rtpmap:98 H264/90000\r\n";

    fresh_module();
    CHECK(sip_calling_create_sdp_offer(NULL, "192.0.2.1", 8000, 0, 96,
                                       &result) == PJ_EINVAL);
    CHECK(sip_calling_create_sdp_offer(&test_pool, "192.0.2.1", 8000, 0, 96,
                                       NULL) == PJ_EINVAL);
    CHECK(sip_calling_create_sdp_offer(&test_pool, NULL, 8000, 0, 96,
                                       &result) == PJ_EINVAL);
    CHECK(sip_calling_create_sdp_offer(&test_pool, "192.0.2.1", 8000, 8002,
                                       96, &result) == PJ_SUCCESS);
    CHECK(result.ptr && strstr(result.ptr, "m=video 8002") != NULL);
    CHECK(sip_calling_parse_sdp_answer(NULL, &audio_port, &dtmf_payload,
                                       &video_port, &video_payload) == PJ_EINVAL);
    CHECK(sip_calling_parse_sdp_answer(answer, &audio_port, &dtmf_payload,
                                       &video_port, &video_payload) == PJ_SUCCESS);
    CHECK(audio_port == 4010 && dtmf_payload == 101);
    CHECK(video_port == 4012 && video_payload == 98);
    sip_calling_cleanup();
}

static void test_outgoing_lifecycle(void) {
    rx_fixture_t response;
    rx_fixture_t unrelated;
    const sip_call_session_t *session;
    const char *answer =
        "v=0\r\nc=IN IP4 198.51.100.40\r\n"
        "m=audio 5000 RTP/AVP 0 101\r\n"
        "a=rtpmap:101 telephone-event/8000\r\n"
        "m=video 5002 RTP/AVP 97\r\n"
        "a=rtpmap:97 H264/90000\r\n";

    fresh_module();
    CHECK(sip_calling_make_call() == PJ_SUCCESS);
    CHECK(fake_invite_sent == 1 && fake_add_ref_count == 1);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_CALLING);
    CHECK(sip_calling_is_call_active());
    CHECK(state_callback_count == 1 && callback_new_state == SIP_CALL_STATE_CALLING);
    CHECK(sip_calling_make_call() == PJ_EBUSY);
    CHECK(sip_calling_set_target_uri("sip:busy@example.test") == PJ_EBUSY);
    session = sip_calling_get_session();
    CHECK(session && session->direction == SIP_CALL_DIRECTION_OUTGOING);
    CHECK(session->call_id.slen > 0 && session->invite_cseq == 42);

    init_fixture(&unrelated, 180, "other-call", "remote", "to",
                 "sip:remote@example.test", NULL, NULL, "198.51.100.41");
    CHECK(!sip_calling_handle_response(&unrelated.rx));
    remove_call_id(&unrelated);
    CHECK(!sip_calling_handle_response(&unrelated.rx));

    init_fixture(&response, 100, current_session.call_id_buf, "remote", "to",
                 "sip:remote@example.test", NULL, NULL, "198.51.100.40");
    CHECK(sip_calling_handle_response(&response.rx));
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_CALLING);
    response.msg.line.status.code = 183;
    CHECK(sip_calling_handle_response(&response.rx));
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_RINGING);

    init_fixture(&response, 200, current_session.call_id_buf, "remote", "to-tag",
                 "sip:remote@example.test", "sip:contact@198.51.100.40:5060",
                 answer, "198.51.100.40");
    CHECK(sip_calling_handle_response(&response.rx));
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_ESTABLISHED);
    CHECK(fake_ack_sent == 1 && audio_callback_count == 1);
    CHECK(strcmp(callback_remote_ip, "198.51.100.40") == 0);
    CHECK(callback_audio_port == 5000 && callback_video_port == 5002);
    CHECK(sip_calling_handle_response(&response.rx));
    CHECK(fake_ack_sent == 2);

    CHECK(sip_calling_terminate_call() == PJ_SUCCESS);
    CHECK(fake_bye_sent == 3);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);
    CHECK(sip_calling_terminate_call() == PJ_SUCCESS);
    CHECK(fake_dec_ref_count >= 1);
    sip_calling_cleanup();
}

static void test_outgoing_failures_and_timeout(void) {
    rx_fixture_t response;

    fresh_module();
    fake_create_request_status = -11;
    CHECK(sip_calling_make_call() == -11);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);

    fresh_module();
    fake_send_request_status = -12;
    CHECK(sip_calling_make_call() == -12);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);

    fresh_module();
    sip_calling_set_call_timeout(10);
    CHECK(sip_calling_make_call() == PJ_SUCCESS);
    current_session.call_start_time = time(NULL) - 11;
    CHECK(sip_calling_check_timeout());
    CHECK(fake_cancel_sent == 1);
    CHECK(!sip_calling_check_timeout());

    fresh_module();
    CHECK(sip_calling_make_call() == PJ_SUCCESS);
    init_fixture(&response, 603, current_session.call_id_buf, "remote", "to",
                 "sip:remote@example.test", NULL, NULL, "198.51.100.50");
    CHECK(sip_calling_handle_response(&response.rx));
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);

    fresh_module();
    CHECK(sip_calling_make_call() == PJ_SUCCESS);
    fake_create_cancel_status = -13;
    CHECK(sip_calling_terminate_call() == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);

    fresh_module();
    CHECK(sip_calling_make_call() == PJ_SUCCESS);
    fake_send_request_status = -14;
    CHECK(sip_calling_terminate_call() == PJ_SUCCESS);

    fresh_module();
    current_session.state = SIP_CALL_STATE_CALLING;
    current_session.direction = SIP_CALL_DIRECTION_OUTGOING;
    outgoing_invite_tdata = NULL;
    CHECK(send_cancel_request() == PJ_EINVAL);
    CHECK(sip_calling_terminate_call() == PJ_SUCCESS);

    fresh_module();
    current_session.state = SIP_CALL_STATE_ESTABLISHED;
    current_session.direction = SIP_CALL_DIRECTION_INCOMING;
    CHECK(send_bye_request() == PJ_EINVAL);
    CHECK(sip_calling_terminate_call() == PJ_SUCCESS);
}

static void test_incoming_lifecycle(void) {
    rx_fixture_t invite;
    rx_fixture_t ack;
    rx_fixture_t wrong;
    const char *offer =
        "v=0\r\nc=IN IP4 203.0.113.30\r\n"
        "m=audio 7000 RTP/AVP 0 101\r\n"
        "a=rtpmap:101 telephone-event/8000\r\n"
        "m=video 7002 RTP/AVP 100\r\n"
        "a=rtpmap:100 H264/90000\r\n";

    fresh_module();
    init_fixture(&invite, 0, "incoming-1", "from-tag", "",
                 "sip:visitor@example.test", "sip:visitor@203.0.113.30:5060",
                 offer, "203.0.113.30");
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_INCOMING);
    CHECK(fake_response_sent == 1);
    CHECK(current_session.direction == SIP_CALL_DIRECTION_INCOMING);
    CHECK(current_session.remote_rtp_port == 7000);
    CHECK(current_session.remote_video_rtp_port == 7002);

    init_fixture(&wrong, 0, "wrong-call", "", "", NULL, NULL, NULL,
                 "203.0.113.30");
    CHECK(sip_calling_handle_incoming_ack(&wrong.rx) == PJ_EINVAL);
    remove_call_id(&wrong);
    CHECK(sip_calling_handle_incoming_ack(&wrong.rx) == PJ_EINVAL);

    init_fixture(&ack, 0, "incoming-1", "", "", NULL, NULL, NULL,
                 "203.0.113.30");
    CHECK(sip_calling_handle_incoming_ack(&ack.rx) == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_ESTABLISHED);
    CHECK(audio_callback_count == 1);

    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_ESTABLISHED);
    init_fixture(&wrong, 0, "incoming-2", "other", "", "sip:other@example.test",
                 NULL, NULL, "203.0.113.31");
    CHECK(sip_calling_handle_incoming_invite(&wrong.rx) == PJ_EBUSY);
    CHECK(fake_last_stateless_status == 486);

    CHECK(sip_calling_handle_incoming_bye(&wrong.rx) == PJ_EINVAL);
    CHECK(fake_last_stateless_status == 481);
    remove_call_id(&wrong);
    CHECK(sip_calling_handle_incoming_bye(&wrong.rx) == PJ_EINVAL);
    CHECK(sip_calling_handle_incoming_bye(&ack.rx) == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);
    CHECK(fake_last_stateless_status == 200);
    sip_calling_cleanup();
}

static void test_incoming_collisions_and_failures(void) {
    rx_fixture_t invite;
    rx_fixture_t cancel;

    fresh_module();
    CHECK(sip_calling_make_call() == PJ_SUCCESS);
    init_fixture(&invite, 0, "incoming-priority", "from", "",
                 "sip:visitor@example.test", NULL, NULL, "203.0.113.40");
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    CHECK(fake_cancel_sent == 1);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_INCOMING);
    init_fixture(&cancel, 0, "incoming-priority", "", "", NULL, NULL, NULL,
                 "203.0.113.40");
    CHECK(sip_calling_handle_incoming_cancel(&cancel.rx) == PJ_SUCCESS);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);
    CHECK(sip_calling_handle_incoming_cancel(&cancel.rx) == PJ_SUCCESS);

    fresh_module();
    fake_create_response_status = -21;
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == -21);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);

    fresh_module();
    fake_send_response_status = -22;
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == -22);
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);

    fresh_module();
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    current_session.state = SIP_CALL_STATE_ESTABLISHED;
    fake_create_response_status = -23;
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    CHECK(fake_last_stateless_status == 200);

    fresh_module();
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    remove_call_id(&cancel);
    CHECK(sip_calling_handle_incoming_ack(&cancel.rx) == PJ_EINVAL);
    current_session.state = SIP_CALL_STATE_ESTABLISHED;
    CHECK(sip_calling_handle_incoming_ack(&invite.rx) == PJ_EINVAL);
    sip_calling_cleanup();
}

static void test_long_dialog_fields_and_cleanup(void) {
    rx_fixture_t invite;
    char long_call_id[220];
    char long_tag[120];

    memset(long_call_id, 'c', sizeof(long_call_id) - 1);
    long_call_id[sizeof(long_call_id) - 1] = '\0';
    memset(long_tag, 't', sizeof(long_tag) - 1);
    long_tag[sizeof(long_tag) - 1] = '\0';
    fresh_module();
    init_fixture(&invite, 0, long_call_id, long_tag, "",
                 "sip:visitor@example.test", NULL, NULL, "203.0.113.60");
    CHECK(sip_calling_handle_incoming_invite(&invite.rx) == PJ_SUCCESS);
    CHECK(current_session.call_id.slen == (int)sizeof(current_session.call_id_buf) - 1);
    CHECK(current_session.remote_tag.slen == (int)sizeof(current_session.remote_tag_buf) - 1);
    sip_calling_cleanup();
    CHECK(sip_calling_get_state() == SIP_CALL_STATE_IDLE);
    CHECK(state_callback == NULL && audio_callback == NULL);
}

int main(void) {
    test_initialization_and_configuration();
    test_sdp_helpers();
    test_outgoing_lifecycle();
    test_outgoing_failures_and_timeout();
    test_incoming_lifecycle();
    test_incoming_collisions_and_failures();
    test_long_dialog_fields_and_cleanup();
    puts("RESULT sip_calling_mock PASS");
    return 0;
}

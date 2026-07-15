#include "call_session.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", message); \
        return -1; \
    } \
} while (0)

static int expect_event(const call_session_event_t* event,
                        const char* call_id,
                        unsigned int sequence,
                        const char* event_type,
                        const char* state,
                        const char* route,
                        int terminal) {
    CHECK(strcmp(event->call_id, call_id) == 0, "call_id changed inside a flow");
    CHECK(event->sequence == sequence, "unexpected event sequence");
    CHECK(strcmp(event->event_type, event_type) == 0, "unexpected event type");
    CHECK(strcmp(event->media_state, state) == 0, "unexpected media state");
    CHECK(strcmp(event->route, route) == 0, "unexpected route");
    CHECK(event->terminal == terminal, "unexpected terminal flag");
    return 0;
}

static int scenario_remote_sip(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "physical_panel", "ringing", "ringing",
                             "none", "alarm-report", 1000, &event) == 1,
          "remote SIP flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(expect_event(&event, id, 1, "ringing", "ringing", "none", 0) == 0,
          "invalid ringing event");
    CHECK(call_session_record(session, "sip_calling", "ringing", "sip",
                              "outgoing-call", 0, 1001, &event) == 1,
          "missing SIP calling event");
    CHECK(expect_event(&event, id, 2, "sip_calling", "ringing", "sip", 0) == 0,
          "invalid SIP calling event");
    CHECK(call_session_record(session, "established", "established", "sip",
                              "sip-established", 0, 1002, &event) == 1,
          "missing established event");
    CHECK(expect_event(&event, id, 3, "established", "established", "sip", 0) == 0,
          "invalid established event");
    CHECK(call_session_record(session, "door_opened", NULL, NULL,
                              "mqtt", 0, 1003, &event) == 1,
          "missing door event");
    CHECK(expect_event(&event, id, 4, "door_opened", "established", "sip", 0) == 0,
          "door event did not preserve state");
    CHECK(call_session_record(session, "sip_ended", "idle", "sip",
                              "remote-hangup", 1, 1004, &event) == 1,
          "missing SIP terminal event");
    CHECK(expect_event(&event, id, 5, "sip_ended", "idle", "sip", 1) == 0,
          "invalid SIP terminal event");
    CHECK(!call_session_is_active(session), "remote SIP session remained active");
    puts("SCENARIO remote_sip_answered PASS");
    return 0;
}

static int scenario_outgoing_disabled(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "physical_panel", "ringing", "ringing",
                             "none", "alarm-report", 2000, &event) == 1,
          "disabled SIP flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(call_session_record(session, "sip_disabled", "ringing", "none",
                              "outgoing-disabled", 0, 2001, &event) == 1,
          "missing disabled SIP event");
    CHECK(expect_event(&event, id, 2, "sip_disabled", "ringing", "none", 0) == 0,
          "invalid disabled SIP event");
    CHECK(call_session_record(session, "timeout", "idle", "none",
                              "ring-timeout", 1, 2060, &event) == 1,
          "missing timeout event");
    CHECK(expect_event(&event, id, 3, "timeout", "idle", "none", 1) == 0,
          "invalid timeout event");
    puts("SCENARIO outgoing_disabled_timeout PASS");
    return 0;
}

static int scenario_physical_answer(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "physical_panel", "ringing", "ringing",
                             "none", "alarm-report", 3000, &event) == 1,
          "physical answer flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(call_session_record(session, "sip_calling", "ringing", "sip",
                              "outgoing-call", 0, 3001, &event) == 1,
          "missing SIP fanout event");
    CHECK(call_session_record(session, "physical_handset_answered", "idle",
                              "physical_handset", "PHYSICAL_HANDSET_ANSWERED",
                              1, 3002, &event) == 1,
          "missing physical answer event");
    CHECK(expect_event(&event, id, 3, "physical_handset_answered", "idle",
                       "physical_handset", 1) == 0,
          "invalid physical answer event");
    puts("SCENARIO physical_handset_answered PASS");
    return 0;
}

static int scenario_developer_simulation(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "developer_simulation", "ringing", "ringing",
                             "none", "alarm-report", 4000, &event) == 1,
          "developer flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(strcmp(event.source, "developer_simulation") == 0,
          "developer source was not preserved");
    CHECK(call_session_record(session, "physical_handset_answered", "idle",
                              "physical_handset", "developer-button",
                              1, 4001, &event) == 1,
          "developer handset event missing");
    CHECK(expect_event(&event, id, 2, "physical_handset_answered", "idle",
                       "physical_handset", 1) == 0,
          "invalid developer terminal event");
    puts("SCENARIO developer_simulation PASS");
    return 0;
}

static int scenario_repeated_ring(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "physical_panel", "ringing", "ringing",
                             "none", "alarm-report", 5000, &event) == 1,
          "repeated ring flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(call_session_start(session, "physical_panel", "ringing", "ringing",
                             "none", "alarm-report", 5001, &event) == 0,
          "repeated ring created a second call_id");
    CHECK(call_session_record(session, "ring_repeated", "ringing", NULL,
                              "physical_panel", 0, 5001, &event) == 1,
          "missing repeated ring event");
    CHECK(expect_event(&event, id, 2, "ring_repeated", "ringing", "none", 0) == 0,
          "invalid repeated ring event");
    CHECK(call_session_record(session, "hang_up_0", "idle", NULL,
                              "HANG_UP_0", 1, 5030, &event) == 1,
          "missing panel hangup");
    CHECK(expect_event(&event, id, 3, "hang_up_0", "idle", "none", 1) == 0,
          "invalid panel hangup event");
    puts("SCENARIO repeated_ring_hangup PASS");
    return 0;
}

static int scenario_sip_failure(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "physical_panel", "ringing", "ringing",
                             "none", "alarm-report", 6000, &event) == 1,
          "SIP failure flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(call_session_record(session, "sip_calling", "ringing", "sip",
                              "outgoing-call", 0, 6001, &event) == 1,
          "missing SIP call attempt");
    CHECK(call_session_record(session, "sip_call_failed", "ringing", "sip",
                              "make-call-failed", 0, 6002, &event) == 1,
          "missing immediate SIP failure");
    CHECK(expect_event(&event, id, 3, "sip_call_failed", "ringing", "sip", 0) == 0,
          "invalid immediate SIP failure");
    CHECK(call_session_record(session, "timeout", "idle", "none",
                              "ring-timeout", 1, 6060, &event) == 1,
          "missing post-failure timeout");
    puts("SCENARIO sip_failure_timeout PASS");
    return 0;
}

static int scenario_incoming_sip(call_session_t* session) {
    call_session_event_t event;
    char id[CALL_SESSION_ID_SIZE];

    CHECK(call_session_start(session, "sip_incoming", "established", "established",
                             "sip", "sip-established", 7000, &event) == 1,
          "incoming SIP flow did not start");
    snprintf(id, sizeof(id), "%s", event.call_id);
    CHECK(expect_event(&event, id, 1, "established", "established", "sip", 0) == 0,
          "invalid incoming SIP event");
    CHECK(call_session_record(session, "sip_ended", "idle", "sip",
                              "remote-hangup", 1, 7010, &event) == 1,
          "incoming SIP did not end");
    CHECK(call_session_record(session, "timeout", "idle", "none",
                              "stale-timeout", 1, 7020, &event) == 0,
          "event was emitted after terminal transition");
    puts("SCENARIO incoming_sip PASS");
    return 0;
}

int main(void) {
    call_session_t session;

    if (call_session_init(&session, 0x1a2b3c4dU) != 0) {
        fprintf(stderr, "FAIL: call session init\n");
        return 1;
    }
    if (scenario_remote_sip(&session) != 0 ||
        scenario_outgoing_disabled(&session) != 0 ||
        scenario_physical_answer(&session) != 0 ||
        scenario_developer_simulation(&session) != 0 ||
        scenario_repeated_ring(&session) != 0 ||
        scenario_sip_failure(&session) != 0 ||
        scenario_incoming_sip(&session) != 0) {
        call_session_destroy(&session);
        return 1;
    }
    call_session_destroy(&session);
    puts("RESULT call_flow_e2e PASS scenarios=7");
    return 0;
}

#include "call_session.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define WORKERS 16

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

typedef struct {
    call_session_t *session;
    call_session_event_t event;
    int result;
} record_worker_t;

static void *record_event(void *arg)
{
    record_worker_t *worker = (record_worker_t *)arg;

    worker->result = call_session_record(worker->session, "progress",
                                         "ringing", "sip", "concurrent",
                                         0, 2000, &worker->event);
    return NULL;
}

static int test_api_boundaries(void)
{
    call_session_t session;
    call_session_event_t event;
    call_session_event_t terminal;

    CHECK(call_session_init(NULL, 0x11223344U) == -1);
    call_session_destroy(NULL);
    CHECK(call_session_is_active(NULL) == 0);
    CHECK(call_session_start(NULL, "physical_panel", "ringing", "ringing",
                             "none", "ring", 1000, &event) == -1);

    CHECK(call_session_init(&session, 0x11223344U) == 0);
    CHECK(call_session_record(&session, "timeout", "idle", "none",
                              "inactive", 1, 1000, &event) == 0);
    CHECK(call_session_start(&session, "physical_panel", "ringing", "ringing",
                             "none", "ring", 1000, NULL) == -1);
    CHECK(call_session_start(&session, "physical_panel", "ringing", "ringing",
                             "none", "ring", 1000, &event) == 1);
    CHECK(strcmp(event.call_id, "11223344-00000001") == 0);
    CHECK(event.sequence == 1);
    CHECK(call_session_is_active(&session) == 1);

    memset(&terminal, 0x5a, sizeof(terminal));
    CHECK(call_session_start(&session, "developer_simulation", "ringing",
                             "ringing", "none", "duplicate", 1001,
                             &terminal) == 0);
    CHECK(call_session_record(&session, "timeout", "idle", "none",
                              "expired", 1, 1002, &terminal) == 1);
    CHECK(terminal.terminal == 1);
    CHECK(terminal.sequence == 2);
    CHECK(strcmp(terminal.call_id, event.call_id) == 0);
    CHECK(call_session_is_active(&session) == 0);
    CHECK(call_session_record(&session, "timeout", "idle", "none",
                              "duplicate-terminal", 1, 1003, &terminal) == 0);

    CHECK(call_session_start(&session, "direct_sip", "established",
                             "established", "sip", "incoming", 1004,
                             &event) == 1);
    CHECK(strcmp(event.call_id, "11223344-00000002") == 0);
    CHECK(event.sequence == 1);
    CHECK(call_session_record(&session, "sip_ended", "idle", "sip",
                              "remote-bye", 1, 1005, NULL) == -1);
    CHECK(call_session_record(&session, "sip_ended", "idle", "sip",
                              "remote-bye", 1, 1005, &terminal) == 1);
    call_session_destroy(&session);
    return 0;
}

static int test_bounded_strings(void)
{
    call_session_t session;
    call_session_event_t event;
    call_session_event_t terminal;
    char value[256];

    memset(value, 'x', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';

    CHECK(call_session_init(&session, 1U) == 0);
    CHECK(call_session_start(&session, value, value, value, value, value,
                             3000, &event) == 1);
    CHECK(event.source[CALL_SESSION_SOURCE_SIZE - 1] == '\0');
    CHECK(event.event_type[CALL_SESSION_EVENT_SIZE - 1] == '\0');
    CHECK(event.route[CALL_SESSION_ROUTE_SIZE - 1] == '\0');
    CHECK(event.media_state[CALL_SESSION_STATE_SIZE - 1] == '\0');
    CHECK(event.reason[CALL_SESSION_REASON_SIZE - 1] == '\0');
    CHECK(call_session_record(&session, value, value, value, value, 1,
                              3001, &terminal) == 1);
    CHECK(terminal.reason[CALL_SESSION_REASON_SIZE - 1] == '\0');
    call_session_destroy(&session);
    return 0;
}

static int test_concurrent_sequence(void)
{
    call_session_t session;
    call_session_event_t start;
    call_session_event_t terminal;
    record_worker_t workers[WORKERS];
    pthread_t threads[WORKERS];
    int seen[WORKERS + 2] = {0};
    int i;

    CHECK(call_session_init(&session, 0xaabbccddU) == 0);
    CHECK(call_session_start(&session, "physical_panel", "ringing", "ringing",
                             "none", "concurrency", 2000, &start) == 1);

    memset(workers, 0, sizeof(workers));
    for (i = 0; i < WORKERS; i++) {
        workers[i].session = &session;
        CHECK(pthread_create(&threads[i], NULL, record_event, &workers[i]) == 0);
    }
    for (i = 0; i < WORKERS; i++) {
        CHECK(pthread_join(threads[i], NULL) == 0);
        CHECK(workers[i].result == 1);
        CHECK(workers[i].event.sequence >= 2);
        CHECK(workers[i].event.sequence <= WORKERS + 1);
        seen[workers[i].event.sequence]++;
        CHECK(strcmp(workers[i].event.call_id, start.call_id) == 0);
    }
    for (i = 2; i <= WORKERS + 1; i++) {
        CHECK(seen[i] == 1);
    }

    CHECK(call_session_record(&session, "hang_up_0", "idle", "none",
                              "uart", 1, 2001, &terminal) == 1);
    CHECK(terminal.sequence == WORKERS + 2);
    CHECK(call_session_is_active(&session) == 0);
    call_session_destroy(&session);
    return 0;
}

int main(void)
{
    if (test_api_boundaries() != 0 ||
        test_bounded_strings() != 0 ||
        test_concurrent_sequence() != 0) {
        return 1;
    }
    printf("RESULT call_session_edge PASS workers=%d\n", WORKERS);
    return 0;
}

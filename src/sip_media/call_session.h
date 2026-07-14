#ifndef WIBOX_CALL_SESSION_H
#define WIBOX_CALL_SESSION_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define CALL_SESSION_ID_SIZE 40
#define CALL_SESSION_EVENT_SIZE 40
#define CALL_SESSION_SOURCE_SIZE 40
#define CALL_SESSION_ROUTE_SIZE 32
#define CALL_SESSION_STATE_SIZE 16
#define CALL_SESSION_REASON_SIZE 80

typedef struct {
    char call_id[CALL_SESSION_ID_SIZE];
    char event_type[CALL_SESSION_EVENT_SIZE];
    char source[CALL_SESSION_SOURCE_SIZE];
    char route[CALL_SESSION_ROUTE_SIZE];
    char media_state[CALL_SESSION_STATE_SIZE];
    char reason[CALL_SESSION_REASON_SIZE];
    unsigned int sequence;
    time_t started_at;
    time_t timestamp;
    int terminal;
} call_session_event_t;

typedef struct {
    pthread_mutex_t mutex;
    uint32_t boot_nonce;
    uint32_t next_call;
    unsigned int event_sequence;
    int active;
    time_t started_at;
    char call_id[CALL_SESSION_ID_SIZE];
    char source[CALL_SESSION_SOURCE_SIZE];
    char route[CALL_SESSION_ROUTE_SIZE];
    char media_state[CALL_SESSION_STATE_SIZE];
} call_session_t;

int call_session_init(call_session_t* session, uint32_t boot_nonce);
void call_session_destroy(call_session_t* session);
int call_session_is_active(call_session_t* session);

/* Returns 1 when a new session/event is created, 0 when one is already active. */
int call_session_start(call_session_t* session,
                       const char* source,
                       const char* event_type,
                       const char* media_state,
                       const char* route,
                       const char* reason,
                       time_t timestamp,
                       call_session_event_t* event);

/* Returns 1 when an event is recorded, 0 when there is no active session. */
int call_session_record(call_session_t* session,
                        const char* event_type,
                        const char* media_state,
                        const char* route,
                        const char* reason,
                        int terminal,
                        time_t timestamp,
                        call_session_event_t* event);

#endif

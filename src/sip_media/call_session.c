#include "call_session.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char* destination, size_t destination_size,
                      const char* value, const char* fallback) {
    const char* selected = value && value[0] ? value : fallback;

    if (!destination || destination_size == 0) {
        return;
    }
    snprintf(destination, destination_size, "%s", selected ? selected : "");
}

static void fill_event(const call_session_t* session,
                       const char* event_type,
                       const char* reason,
                       int terminal,
                       time_t timestamp,
                       call_session_event_t* event) {
    memset(event, 0, sizeof(*event));
    copy_text(event->call_id, sizeof(event->call_id), session->call_id, "unknown");
    copy_text(event->event_type, sizeof(event->event_type), event_type, "unknown");
    copy_text(event->source, sizeof(event->source), session->source, "unknown");
    copy_text(event->route, sizeof(event->route), session->route, "none");
    copy_text(event->media_state, sizeof(event->media_state),
              session->media_state, "idle");
    copy_text(event->reason, sizeof(event->reason), reason, "none");
    event->sequence = session->event_sequence;
    event->started_at = session->started_at;
    event->timestamp = timestamp;
    event->terminal = terminal ? 1 : 0;
}

int call_session_init(call_session_t* session, uint32_t boot_nonce) {
    if (!session) {
        return -1;
    }

    memset(session, 0, sizeof(*session));
    session->boot_nonce = boot_nonce ? boot_nonce : 1;
    return pthread_mutex_init(&session->mutex, NULL) == 0 ? 0 : -1;
}

void call_session_destroy(call_session_t* session) {
    if (session) {
        pthread_mutex_destroy(&session->mutex);
    }
}

int call_session_is_active(call_session_t* session) {
    int active;

    if (!session) {
        return 0;
    }
    pthread_mutex_lock(&session->mutex);
    active = session->active;
    pthread_mutex_unlock(&session->mutex);
    return active;
}

int call_session_start(call_session_t* session,
                       const char* source,
                       const char* event_type,
                       const char* media_state,
                       const char* route,
                       const char* reason,
                       time_t timestamp,
                       call_session_event_t* event) {
    if (!session || !event) {
        return -1;
    }

    pthread_mutex_lock(&session->mutex);
    if (session->active) {
        pthread_mutex_unlock(&session->mutex);
        return 0;
    }

    session->next_call++;
    if (session->next_call == 0) {
        session->next_call++;
    }
    snprintf(session->call_id, sizeof(session->call_id), "%08x-%08x",
             session->boot_nonce, session->next_call);
    copy_text(session->source, sizeof(session->source), source, "unknown");
    copy_text(session->route, sizeof(session->route), route, "none");
    copy_text(session->media_state, sizeof(session->media_state), media_state, "ringing");
    session->event_sequence = 1;
    session->started_at = timestamp;
    session->active = 1;
    fill_event(session, event_type, reason, 0, timestamp, event);
    pthread_mutex_unlock(&session->mutex);
    return 1;
}

int call_session_record(call_session_t* session,
                        const char* event_type,
                        const char* media_state,
                        const char* route,
                        const char* reason,
                        int terminal,
                        time_t timestamp,
                        call_session_event_t* event) {
    if (!session || !event) {
        return -1;
    }

    pthread_mutex_lock(&session->mutex);
    if (!session->active) {
        pthread_mutex_unlock(&session->mutex);
        return 0;
    }

    if (media_state && media_state[0]) {
        copy_text(session->media_state, sizeof(session->media_state), media_state, "idle");
    }
    if (route && route[0]) {
        copy_text(session->route, sizeof(session->route), route, "none");
    }
    session->event_sequence++;
    fill_event(session, event_type, reason, terminal, timestamp, event);

    if (terminal) {
        session->active = 0;
        session->event_sequence = 0;
        session->started_at = 0;
        session->call_id[0] = '\0';
        session->source[0] = '\0';
        session->route[0] = '\0';
        session->media_state[0] = '\0';
    }
    pthread_mutex_unlock(&session->mutex);
    return 1;
}

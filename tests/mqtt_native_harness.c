#include "config.h"
#include "mqtt.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int open_count;
    int f1_count;
    int snapshot_count;
    int video_value;
    int rtsp_value;
    int video_bitrate;
    int call_timeout;
    int ring_snapshot_delay;
    int call_forward_value;
} harness_state_t;

static void on_open_door(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->open_count++;
    printf("CALLBACK open_door=%d\n", state->open_count);
}

static void on_trigger_f1(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->f1_count++;
    printf("CALLBACK trigger_f1=%d\n", state->f1_count);
}

static void on_take_snapshot(void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->snapshot_count++;
    printf("CALLBACK take_snapshot=%d\n", state->snapshot_count);
}

static void on_video_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->video_value = enabled;
    printf("CALLBACK video_enabled=%d\n", enabled);
}

static void on_rtsp_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->rtsp_value = enabled;
    mqtt_publish_rtsp_enabled(enabled);
    printf("CALLBACK rtsp_enabled=%d\n", enabled);
}

static void on_video_bitrate(int bitrate_kbps, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->video_bitrate = bitrate_kbps;
    printf("CALLBACK video_bitrate=%d\n", bitrate_kbps);
}

static void on_call_timeout(int timeout_seconds, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->call_timeout = timeout_seconds;
    printf("CALLBACK call_timeout=%d\n", timeout_seconds);
}

static void on_ring_snapshot_delay(int delay_ms, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->ring_snapshot_delay = delay_ms;
    printf("CALLBACK ring_snapshot_delay=%d\n", delay_ms);
}

static void on_call_forward_enabled(int enabled, void* user_data) {
    harness_state_t* state = (harness_state_t*)user_data;
    state->call_forward_value = enabled;
    printf("CALLBACK call_forward_enabled=%d\n", enabled);
}

int main(void) {
    wibox_config_t config;
    mqtt_callbacks_t callbacks;
    harness_state_t state;
    int i;

    memset(&callbacks, 0, sizeof(callbacks));
    memset(&state, 0, sizeof(state));
    state.video_value = -1;
    state.rtsp_value = -1;
    state.video_bitrate = -1;
    state.call_timeout = -1;
    state.ring_snapshot_delay = -1;
    state.call_forward_value = -1;

    config_init_defaults(&config);
    if (config.audio_input_gain_percent != 35 ||
        config.audio_output_volume_percent != 50 ||
        config.audio_line_mute_ms != 900) {
        fprintf(stderr, "unexpected audio defaults: input=%d output=%d mute=%d\n",
                config.audio_input_gain_percent,
                config.audio_output_volume_percent,
                config.audio_line_mute_ms);
        return 4;
    }
    config.mqtt_enabled = 1;
    strcpy(config.mqtt_host, "127.0.0.1:18883");
    strcpy(config.mqtt_user, "wibox");
    strcpy(config.mqtt_pass, "test");
    strcpy(config.mqtt_base_topic, "wibox/test");
    strcpy(config.mqtt_device_id, "test");
    strcpy(config.mqtt_device_name, "WiBox Test");
    strcpy(config.mqtt_homeassistant_prefix, "homeassistant");
    config.firmware_update_enabled = 0;

    callbacks.open_door = on_open_door;
    callbacks.trigger_f1 = on_trigger_f1;
    callbacks.take_snapshot = on_take_snapshot;
    callbacks.set_video_enabled = on_video_enabled;
    callbacks.set_rtsp_enabled = on_rtsp_enabled;
    callbacks.set_video_bitrate = on_video_bitrate;
    callbacks.set_outgoing_call_timeout = on_call_timeout;
    callbacks.set_ring_snapshot_delay = on_ring_snapshot_delay;
    callbacks.set_call_forward_enabled = on_call_forward_enabled;

    if (mqtt_init(&config, "127.0.0.1", &callbacks, &state) != 0) {
        return 2;
    }
    if (mqtt_start() != 0) {
        return 3;
    }

    for (i = 0; i < 80 && (state.open_count == 0 || state.f1_count == 0 ||
                           state.snapshot_count == 0 ||
                           state.video_value != 0 ||
                           state.rtsp_value != 1 ||
                           state.video_bitrate != 2048 ||
                           state.call_timeout != 45 ||
                           state.ring_snapshot_delay != 1500 ||
                           state.call_forward_value != 0); i++) {
        usleep(100000);
    }

    mqtt_publish_door_unlocked_pulse();
    mqtt_stop();
    printf("RESULT open=%d f1=%d snapshot=%d video=%d rtsp=%d bitrate=%d timeout=%d ring_snapshot_delay=%d call_forward=%d\n",
           state.open_count, state.f1_count, state.snapshot_count,
           state.video_value, state.rtsp_value, state.video_bitrate, state.call_timeout,
           state.ring_snapshot_delay, state.call_forward_value);
    return (state.open_count == 1 && state.f1_count == 1 && state.snapshot_count == 1 &&
            state.video_value == 0 && state.rtsp_value == 1 && state.video_bitrate == 2048 &&
            state.call_timeout == 45 && state.ring_snapshot_delay == 1500 &&
            state.call_forward_value == 0) ? 0 : 1;
}

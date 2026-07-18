#!/usr/bin/env python3
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def function_body(source, name):
    pattern = re.compile(
        r"(?:^|\n)[\w\s\*]+\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{",
        re.MULTILINE,
    )
    match = pattern.search(source)
    require(match is not None, f"missing function {name}")
    opening = source.find("{", match.start())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function {name}")


def ordered(body, *needles):
    position = -1
    for needle in needles:
        next_position = body.find(needle, position + 1)
        require(next_position >= 0, f"missing ordered token {needle}")
        position = next_position


def check_call_wiring():
    source = read("src/sip_media/sip_media.c")
    simulated = function_body(source, "handle_simulated_ding_trigger")
    ordered(simulated, "ensure_intercom_call_open", "handle_ding_trigger")

    ring = function_body(source, "handle_ding_trigger")
    require("mqtt_publish_media_state(\"ringing\")" in ring, "ring does not publish ringing")
    require("start_snapshot_capture(0" in ring, "physical/simulated ring owns no snapshot open")
    require("sip_outgoing_call_enabled" in ring, "ring is coupled unconditionally to SIP")
    require("ensure_intercom_call_open" not in ring, "physical ring sends START_CALL")

    state = function_body(source, "on_call_state_change")
    ordered(state, "simulated_ding_panel_context_active", "get_call_active_status()",
            "ensure_intercom_call_open(\"sip-established\")")
    require("old_state == SIP_CALL_STATE_ESTABLISHED" in state, "STOP_CALL is not established-only")
    require("release_sip_video_or_stop" in state and "stop_audio_session" in state,
            "SIP end does not release shared media")

    uart = function_body(source, "handle_uart_frame")
    for token in ("UART_CODE_ALARM_REPORT", "UART_CODE_HANG_UP_0",
                  "UART_CODE_HANG_UP_1", "UART_CODE_PHYSICAL_HANDSET_ANSWERED",
                  "UART_CODE_STA_TO_AP", "request_wifi_ap_mode",
                  "UART_CODE_CMD_DOWN_LONG_1", "UART_CODE_CMD_DOWN_LONG_2",
                  "WIFI_BUTTON_LONG_SEQUENCE_MAX_MS",
                  "terminate_call_from_serial", "mqtt_publish_uart_event"):
        require(token in uart, f"UART flow missing {token}")
    require("handle_ding_trigger(\"physical_panel\")" in uart,
            "physical alarm does not use native ring path")

    snapshot = function_body(source, "snapshot_thread_func")
    require("request.open_panel_context" in snapshot, "snapshot ownership flag ignored")
    require("request_video_worker_snapshot" in snapshot, "active worker snapshot path missing")
    require("video_snapshot_capture" in snapshot, "cold snapshot path missing")
    require("sip_calling_is_call_active()" in snapshot, "snapshot can close an active SIP panel")

    audio = function_body(source, "maybe_stop_audio_engine")
    require("get_audio_sip_rtp_active()" in audio, "audio ignores SIP owner")
    require("rtsp_stream_get_audio_client_count() > 0" in audio, "audio ignores RTSP owner")
    video = function_body(source, "release_sip_video_or_stop")
    require("rtsp_stream_get_video_client_count() > 0" in video, "video ignores RTSP owner")
    require("clear_video_worker_rtp" in video, "video worker cannot detach only SIP")

    reboot = function_body(source, "mqtt_reboot_device_callback")
    ordered(reboot, "already_requested = reboot_requested", "reboot_requested = 1",
            "if (already_requested)", "reboot(RB_AUTOBOOT)")

    dtmf = function_body(source, "handle_dtmf_event")
    require("unlock_door" in dtmf, "DTMF does not reach common unlock path")


def check_boot_and_release_contracts():
    boot = read("include/run.sh")
    sofia_commands = [
        line for line in boot.splitlines()
        if "Sofia_temp.sh" in line and not line.lstrip().startswith("#")
    ]
    require(len(sofia_commands) == 1, "Sofia warmup must execute exactly once")
    require(boot.count("app_watchdog.sh wibox-media-daemon") == 1,
            "production daemon supervisor is not singular")
    for token in ("/mnt/mtd/sip_media.conf", "wifi_mode.sh", "wifi_station_manager.sh",
                  "nowayout=0", "soft_noboot=0", "/mnt/mtd/post.sh", "wifi_led blue"):
        require(token in boot, f"boot contract missing {token}")
    require("timeout -t 150" not in boot, "boot retains the legacy WiFi timeout")

    wifi_manager = read("include/bin/wifi_station_manager.sh")
    heartbeat = read("include/bin/heartbeat.sh")
    require("ap_start.sh" not in wifi_manager, "station failure can enter AP mode")
    require("reboot" not in wifi_manager and "reboot" not in heartbeat,
            "station recovery can reboot the device")
    ap_start = read("include/bin/ap_start.sh")
    gpio = read("include/bin/gpio.sh")
    portal_cgi = read("include/www/wifi/cgi-bin/wifi-config.cgi")
    require("wifi_led_blink_start blue" in ap_start,
            "AP mode does not expose a persistent physical indication")
    require("wifi_led_blink_start" in gpio and "wifi_led_blink_stop" in gpio,
            "GPIO runtime does not manage the AP blink lifecycle")
    require("wifi_led_blink_stop" in portal_cgi and "wifi_led green" in portal_cgi,
            "portal success does not acknowledge Save/Cancel before reboot")
    require("station connectivity lost; scheduling reconnect\"\n        set_wifi_led red" in wifi_manager,
            "runtime station loss is not indicated in red")

    watchdog = read("include/bin/app_watchdog.sh")
    for token in ("WIBOX_OTA_GUARD_PATH", '"PREPARE"', "wait_for_ota_guard",
                  "WIBOX_WATCHDOG_DIRECT", "wibox-logrotate"):
        require(token in watchdog, f"app watchdog contract missing {token}")

    verifier = read("scripts/verify_image.sh")
    for token in ("bin/wibox-media-daemon", "bin/firmware_update",
                  "bin/app_watchdog.sh", "lib/libap.so", "lib/libadi.so",
                  "require_absent \"bin/video_rtp_bridge\"",
                  "require_absent \"bin/sip_media\""):
        require(token in verifier, f"image invariant missing {token}")

    workflow = read(".github/workflows/firmware.yml")
    ordered(workflow, "make test", "make build", "make verify-image")
    for token in ("SHA256SUMS", "MD5SUMS", "actions/upload-artifact"):
        require(token in workflow, f"release workflow missing {token}")

    deploy = read("scripts/deploy_runtime.sh")
    for forbidden in ("flash_erase", "/dev/mtd", "firmware_update --image", "reboot"):
        require(forbidden not in deploy, f"development deploy became persistent: {forbidden}")

    mqtt = read("src/sip_media/mqtt.c")
    update_launch = function_body(mqtt, "start_firmware_update_install")
    ordered(update_launch, "firmware_update_installing = 1",
            "/bin/setsid /usr/bin/firmware_update", "if (rc != 0)")
    for token in ("</dev/null", ">/tmp/firmware_update.log 2>&1 &"):
        require(token in update_launch, f"firmware updater launch missing {token}")
    require('system("/usr/bin/firmware_update' not in update_launch,
            "firmware updater remains attached to the daemon PTY")

    dockerfile = read("Dockerfile")
    require(re.search(r"CRAMFS_TOOLS_COMMIT=[0-9a-f]{40}", dockerfile),
            "cramfs tools source is not pinned to a commit")
    require("FROM ubuntu:16.04 AS builder" in dockerfile and "FROM ${BASE_IMAGE}" in dockerfile,
            "firmware build stages changed unexpectedly")

    ignored = read(".gitignore")
    for artifact in ("cramfs/", "release/", ".verify-image-root/", "*.img",
                     "/src/sip_media/*.o", "/include/bin/wibox-media-daemon", ".config"):
        require(artifact in ignored, f"generated artifact is not ignored: {artifact}")


def check_video_contracts():
    bridge = read("src/video_rtp_bridge/video_rtp_bridge.c")
    require("st.stream_id != 0" in bridge, "main RTP path no longer filters stream 0")
    require("SNAPSHOT_STREAM_ID 2" in bridge, "concurrent snapshot no longer uses stream 2")
    require("h264_annexb_packetize" in bridge and "h264_annexb_scan" in bridge,
            "tested Annex-B implementation is not wired into production")
    require("snapshot_file_write_atomic" in bridge,
            "tested atomic snapshot writer is not wired into production")
    require("dump_limit_bytes" in bridge, "diagnostic recording is not bounded")


def main():
    check_call_wiring()
    check_boot_and_release_contracts()
    check_video_contracts()
    print("RESULT runtime_contract PASS")


if __name__ == "__main__":
    main()

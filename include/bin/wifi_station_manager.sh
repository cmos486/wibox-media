#!/bin/sh

INTERFACE=${WIFI_INTERFACE:-wlan0}
CONFIG_PATH=${WIFI_CONFIG_PATH:-/mnt/mtd/wpa_supplicant.conf}
AP_REQUEST_PATH=${WIFI_AP_REQUEST_PATH:-/mnt/mtd/wifi_ap_requested}
READY_PATH=${WIFI_READY_PATH:-/tmp/wifi-station-ready}
MANAGER_PID_PATH=${WIFI_MANAGER_PID_PATH:-/var/run/wifi_station_manager.pid}
DHCP_SCRIPT=${WIFI_DHCP_SCRIPT:-/var/wifi/udhcpc.conf}
WPA_RUN_DIR=${WIFI_WPA_RUN_DIR:-/var/run/wpa_supplicant}
LOG_PATH=${WIFI_LOG_PATH:-/var/log/wifi-station.log}
ASSOC_TIMEOUT=${WIFI_ASSOC_TIMEOUT:-20}
DHCP_TIMEOUT=${WIFI_DHCP_TIMEOUT:-10}
HEALTH_INTERVAL=${WIFI_HEALTH_INTERVAL:-15}
MAX_CYCLES=${WIFI_MANAGER_MAX_CYCLES:-0}

WPA_SUPPLICANT=${WIFI_WPA_SUPPLICANT:-/usr/sbin/wpa_supplicant}
WPA_CLI=${WIFI_WPA_CLI:-/usr/sbin/wpa_cli}
UDHCPC=${WIFI_UDHCPC:-/sbin/udhcpc}
IFCONFIG=${WIFI_IFCONFIG:-/sbin/ifconfig}
KILLALL=${WIFI_KILLALL:-/bin/killall}
PIDOF=${WIFI_PIDOF:-/bin/pidof}
SLEEP=${WIFI_SLEEP:-/bin/sleep}
GPIO_SCRIPT=${WIFI_GPIO_SCRIPT:-/usr/bin/gpio.sh}
DROPBEAR_RESTART=${WIFI_DROPBEAR_RESTART:-/usr/bin/dropbear_restart.sh}

mkdir -p "$(dirname "$LOG_PATH")"

if [ -f "$MANAGER_PID_PATH" ]; then
    existing_pid=$(sed -n '1p' "$MANAGER_PID_PATH" 2>/dev/null || true)
    if [ -n "$existing_pid" ] && [ -r "/proc/$existing_pid/cmdline" ] &&
       tr '\000' ' ' <"/proc/$existing_pid/cmdline" | grep -q 'wifi_station_manager.sh'; then
        exit 0
    fi
fi
printf '%s\n' "$$" >"$MANAGER_PID_PATH"
trap 'rm -f "$MANAGER_PID_PATH"' EXIT
trap 'exit 0' HUP INT TERM

log() {
    printf '%s wifi-station: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >>"$LOG_PATH"
}

set_wifi_led() {
    if [ -f "$GPIO_SCRIPT" ]; then
        . "$GPIO_SCRIPT"
        wifi_led "$1"
    fi
}

stop_station_clients() {
    "$KILLALL" -q udhcpc 2>/dev/null || true
    "$KILLALL" -q wpa_supplicant 2>/dev/null || true
}

associated() {
    "$WPA_CLI" -i "$INTERFACE" status 2>/dev/null |
        grep -q '^wpa_state=COMPLETED$'
}

has_address() {
    "$IFCONFIG" "$INTERFACE" 2>/dev/null |
        grep -Eq 'inet addr:|inet [0-9]'
}

wait_until() {
    check=$1
    limit=$2
    elapsed=0
    while [ "$elapsed" -lt "$limit" ]; do
        "$check" && return 0
        "$SLEEP" 1
        elapsed=$((elapsed + 1))
    done
    return 1
}

restart_network_consumers() {
    if [ -x "$DROPBEAR_RESTART" ]; then
        "$DROPBEAR_RESTART" || log "Dropbear restart failed"
    else
        log "Dropbear restart helper missing"
    fi

    if "$PIDOF" wibox-media-daemon >/dev/null 2>&1; then
        "$KILLALL" -q wibox-media-daemon 2>/dev/null || true
        "$SLEEP" 2
        if "$PIDOF" wibox-media-daemon >/dev/null 2>&1; then
            "$KILLALL" -q -9 wibox-media-daemon 2>/dev/null || true
        fi
    fi
}

connect_once() {
    stop_station_clients
    "$IFCONFIG" "$INTERFACE" up >/dev/null 2>&1 || true
    "$IFCONFIG" "$INTERFACE" 0.0.0.0 >/dev/null 2>&1 || true
    rm -rf "$WPA_RUN_DIR"
    mkdir -p "$WPA_RUN_DIR"

    log "starting association timeout=${ASSOC_TIMEOUT}s"
    if ! "$WPA_SUPPLICANT" -i "$INTERFACE" -c "$CONFIG_PATH" -B >>"$LOG_PATH" 2>&1; then
        log "wpa_supplicant failed to start"
        return 1
    fi
    if ! wait_until associated "$ASSOC_TIMEOUT"; then
        log "association timed out"
        stop_station_clients
        return 1
    fi

    log "associated; waiting for DHCP timeout=${DHCP_TIMEOUT}s"
    "$UDHCPC" -i "$INTERFACE" -s "$DHCP_SCRIPT" >>"$LOG_PATH" 2>&1 &
    if ! wait_until has_address "$DHCP_TIMEOUT"; then
        log "DHCP timed out"
        stop_station_clients
        return 1
    fi

    : >"$READY_PATH"
    if [ -f /tmp/wibox-production-ready ]; then
        set_wifi_led blue
    else
        set_wifi_led green
    fi
    log "station address acquired"
    restart_network_consumers
    return 0
}

retry_delay() {
    case "$1" in
        1) echo 5 ;;
        2) echo 15 ;;
        3) echo 30 ;;
        *) echo 60 ;;
    esac
}

cycles=0
while [ ! -f "$AP_REQUEST_PATH" ] && [ -f "$CONFIG_PATH" ]; do
    cycles=$((cycles + 1))
    rm -f "$READY_PATH"

    if connect_once; then
        if [ "$MAX_CYCLES" -gt 0 ]; then
            exit 0
        fi
        while [ ! -f "$AP_REQUEST_PATH" ] && associated && has_address; do
            "$SLEEP" "$HEALTH_INTERVAL"
        done
        log "station connectivity lost; scheduling reconnect"
        set_wifi_led red
        stop_station_clients
    else
        set_wifi_led red
    fi

    if [ "$MAX_CYCLES" -gt 0 ] && [ "$cycles" -ge "$MAX_CYCLES" ]; then
        exit 1
    fi

    delay=$(retry_delay "$cycles")
    log "retrying station mode in ${delay}s"
    "$SLEEP" "$delay"
done

stop_station_clients
exit 0

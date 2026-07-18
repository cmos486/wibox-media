#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
MODE="$ROOT/include/bin/wifi_mode.sh"
MANAGER="$ROOT/include/bin/wifi_station_manager.sh"
PORTAL_START="$ROOT/include/bin/wifi_portal_start.sh"
DROPBEAR_RESTART="$ROOT/include/bin/dropbear_restart.sh"
CGI="$ROOT/include/www/wifi/cgi-bin/wifi-config.cgi"
GPIO="$ROOT/include/bin/gpio.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM

sh -n "$ROOT/include/run.sh" "$ROOT/include/bin/ap_start.sh" \
    "$ROOT/include/bin/heartbeat.sh" "$MODE" "$MANAGER" "$PORTAL_START" \
    "$DROPBEAR_RESTART" "$CGI" "$GPIO"

CONFIG="$TEST_DIR/wpa_supplicant.conf"
MARKER="$TEST_DIR/wifi_ap_requested"

# Missing credentials automatically select provisioning AP.
[ "$(WIFI_CONFIG_PATH="$CONFIG" WIFI_AP_REQUEST_PATH="$MARKER" "$MODE")" = "ap" ]

printf '%s\n' 'network={' '  ssid="Saved WiFi"' '  psk="savedsecret"' '}' >"$CONFIG"
[ "$(WIFI_CONFIG_PATH="$CONFIG" WIFI_AP_REQUEST_PATH="$MARKER" "$MODE")" = "station" ]

# A physical AP request overrides but does not modify saved credentials.
CONFIG_HASH=$(sha256sum "$CONFIG" | cut -d' ' -f1)
touch "$MARKER"
[ "$(WIFI_CONFIG_PATH="$CONFIG" WIFI_AP_REQUEST_PATH="$MARKER" "$MODE")" = "ap" ]
[ "$(sha256sum "$CONFIG" | cut -d' ' -f1)" = "$CONFIG_HASH" ]
rm -f "$MARKER"

# Mock the hardware commands and exercise one deterministic manager cycle.
cat >"$TEST_DIR/mock-wpa-cli" <<'EOF'
#!/bin/sh
if [ "${MOCK_WIFI_STATE:-failure}" = "success" ]; then
    echo 'wpa_state=COMPLETED'
else
    echo 'wpa_state=DISCONNECTED'
fi
EOF
cat >"$TEST_DIR/mock-ifconfig" <<'EOF'
#!/bin/sh
if [ "${MOCK_WIFI_STATE:-failure}" = "success" ] && [ "$#" -eq 1 ]; then
    echo 'inet addr:192.0.2.44  Bcast:192.0.2.255  Mask:255.255.255.0'
fi
exit 0
EOF
chmod +x "$TEST_DIR/mock-wpa-cli" "$TEST_DIR/mock-ifconfig"

cat >"$TEST_DIR/mock-gpio.sh" <<'EOF'
wifi_led_blink_stop() {
    printf '%s\n' stop >>"$MOCK_LED_LOG"
}
wifi_led() {
    printf '%s\n' "$1" >>"$MOCK_LED_LOG"
}
EOF
MOCK_LED_LOG="$TEST_DIR/led.log"
export MOCK_LED_LOG

# The shared Dropbear transition helper waits, retries and verifies the
# replacement instead of assuming that one immediate start succeeded.
cat >"$TEST_DIR/mock-dropbear" <<'EOF'
#!/bin/sh
count=0
[ ! -f "$MOCK_DROPBEAR_COUNT" ] || count=$(cat "$MOCK_DROPBEAR_COUNT")
count=$((count + 1))
printf '%s\n' "$count" >"$MOCK_DROPBEAR_COUNT"
[ "$count" -lt 2 ] || : >"$MOCK_DROPBEAR_STATE"
exit 0
EOF
cat >"$TEST_DIR/mock-dropbear-pidof" <<'EOF'
#!/bin/sh
[ -f "$MOCK_DROPBEAR_STATE" ]
EOF
cat >"$TEST_DIR/mock-dropbear-killall" <<'EOF'
#!/bin/sh
rm -f "$MOCK_DROPBEAR_STATE"
EOF
chmod +x "$TEST_DIR/mock-dropbear" "$TEST_DIR/mock-dropbear-pidof" \
    "$TEST_DIR/mock-dropbear-killall"
MOCK_DROPBEAR_COUNT="$TEST_DIR/dropbear.count"
MOCK_DROPBEAR_STATE="$TEST_DIR/dropbear.state"
export MOCK_DROPBEAR_COUNT MOCK_DROPBEAR_STATE
DROPBEAR_BIN="$TEST_DIR/mock-dropbear" \
DROPBEAR_PIDOF="$TEST_DIR/mock-dropbear-pidof" \
DROPBEAR_KILLALL="$TEST_DIR/mock-dropbear-killall" \
DROPBEAR_SLEEP=/bin/true \
DROPBEAR_RESTART_LOG="$TEST_DIR/dropbear.log" \
    "$DROPBEAR_RESTART"
[ "$(cat "$MOCK_DROPBEAR_COUNT")" = "2" ]
grep -q 'start attempt=1' "$TEST_DIR/dropbear.log"
grep -q 'start attempt=2' "$TEST_DIR/dropbear.log"
grep -q 'listener process available' "$TEST_DIR/dropbear.log"

# Exercise the real GPIO helper against a fake sysfs tree. The AP indicator
# must drive blue, keep a managed background process and stop on request.
for led in 10 11 12; do
    mkdir -p "$TEST_DIR/gpio/gpio$led"
    : >"$TEST_DIR/gpio/gpio$led/value"
done
(
    WIFI_LED_GPIO_ROOT="$TEST_DIR/gpio"
    WIFI_LED_BLINK_PID_PATH="$TEST_DIR/blink.pid"
    WIFI_LED_BLINK_INTERVAL=5
    export WIFI_LED_GPIO_ROOT WIFI_LED_BLINK_PID_PATH WIFI_LED_BLINK_INTERVAL
    . "$GPIO"
    trap 'wifi_led_blink_stop' EXIT HUP INT TERM
    wifi_led_blink_start blue
    sleep 0.1
    [ "$(cat "$TEST_DIR/gpio/gpio11/value")" = "1" ]
    [ "$(cat "$TEST_DIR/gpio/gpio10/value")" = "0" ]
    [ "$(cat "$TEST_DIR/gpio/gpio12/value")" = "0" ]
    blink_pid=$(awk '{ print $1 }' "$TEST_DIR/blink.pid")
    kill -0 "$blink_pid"
    wifi_led_blink_stop
    sleep 0.1
    ! kill -0 "$blink_pid" 2>/dev/null
    [ ! -f "$TEST_DIR/blink.pid" ]
    trap - EXIT HUP INT TERM
)

run_manager() {
    MOCK_WIFI_STATE=$1 \
    WIFI_CONFIG_PATH="$CONFIG" \
    WIFI_AP_REQUEST_PATH="$MARKER" \
    WIFI_READY_PATH="$TEST_DIR/ready" \
    WIFI_MANAGER_PID_PATH="$TEST_DIR/manager.pid" \
    WIFI_WPA_RUN_DIR="$TEST_DIR/wpa-run" \
    WIFI_LOG_PATH="$TEST_DIR/manager.log" \
    WIFI_ASSOC_TIMEOUT=1 \
    WIFI_DHCP_TIMEOUT=1 \
    WIFI_MANAGER_MAX_CYCLES=1 \
    WIFI_WPA_SUPPLICANT=/bin/true \
    WIFI_WPA_CLI="$TEST_DIR/mock-wpa-cli" \
    WIFI_UDHCPC=/bin/true \
    WIFI_IFCONFIG="$TEST_DIR/mock-ifconfig" \
    WIFI_KILLALL=/bin/true \
    WIFI_PIDOF=/bin/false \
    WIFI_SLEEP=/bin/true \
    WIFI_GPIO_SCRIPT="$TEST_DIR/mock-gpio.sh" \
    WIFI_DROPBEAR_RESTART=/bin/true \
    "$MANAGER"
}

run_manager success
[ -f "$TEST_DIR/ready" ]
grep -q '^green$' "$MOCK_LED_LOG"
rm -f "$TEST_DIR/ready"
: >"$MOCK_LED_LOG"
if run_manager failure; then
    echo "station manager accepted a failed association" >&2
    exit 1
fi
grep -q '^red$' "$MOCK_LED_LOG"
[ ! -f "$TEST_DIR/ready" ]
[ ! -f "$MARKER" ]
! grep -q 'ap_start.sh' "$MANAGER"
! grep -q 'reboot' "$MANAGER" "$ROOT/include/bin/heartbeat.sh"
grep -q 'WIFI_ASSOC_TIMEOUT:-20' "$MANAGER"
grep -q 'WIFI_DHCP_TIMEOUT:-10' "$MANAGER"
grep -q '1) echo 5' "$MANAGER"
grep -q '2) echo 15' "$MANAGER"
grep -q '3) echo 30' "$MANAGER"

# Saving credentials atomically clears forced AP mode.
touch "$MARKER"
body='action=save&ssid=Vecino+WiFi&psk=secreto123'
printf '%s' "$body" |
    CONTENT_LENGTH=${#body} WIFI_CONFIG_PATH="$CONFIG" WIFI_AP_REQUEST_PATH="$MARKER" \
    WIFI_REBOOT_COMMAND=/bin/true WIFI_REBOOT_DELAY=0 \
    WIFI_GPIO_SCRIPT="$TEST_DIR/mock-gpio.sh" "$CGI" >"$TEST_DIR/response"

grep -q 'Configuration saved' "$TEST_DIR/response"
grep -q '^        ssid="Vecino WiFi"$' "$CONFIG"
grep -q '^        psk="secreto123"$' "$CONFIG"
[ ! -f "$MARKER" ]
tail -2 "$MOCK_LED_LOG" | grep -q '^stop$'
tail -1 "$MOCK_LED_LOG" | grep -q '^green$'

# Cancelling preserves credentials and only clears the AP request.
CONFIG_HASH=$(sha256sum "$CONFIG" | cut -d' ' -f1)
touch "$MARKER"
cancel='action=cancel'
printf '%s' "$cancel" |
    CONTENT_LENGTH=${#cancel} WIFI_CONFIG_PATH="$CONFIG" WIFI_AP_REQUEST_PATH="$MARKER" \
    WIFI_REBOOT_COMMAND=/bin/true WIFI_REBOOT_DELAY=0 \
    WIFI_GPIO_SCRIPT="$TEST_DIR/mock-gpio.sh" "$CGI" >"$TEST_DIR/cancel-response"
grep -q 'Returning to the saved network' "$TEST_DIR/cancel-response"
[ "$(sha256sum "$CONFIG" | cut -d' ' -f1)" = "$CONFIG_HASH" ]
[ ! -f "$MARKER" ]
tail -2 "$MOCK_LED_LOG" | grep -q '^stop$'
tail -1 "$MOCK_LED_LOG" | grep -q '^green$'

# AP mode owns a persistent slow blue blink. Normal production station mode
# owns solid blue, so run.sh must not overwrite the AP indication.
grep -q 'wifi_led_blink_start blue' "$ROOT/include/bin/ap_start.sh"
grep -q 'if \[ "$WIFI_MODE" = "station" \]' "$ROOT/include/run.sh"
grep -q 'MAX_ATTEMPTS=.*3' "$DROPBEAR_RESTART"
grep -q 'listener process available' "$DROPBEAR_RESTART"
grep -q 'dropbear_restart.sh' "$MANAGER" "$PORTAL_START"

invalid='action=save&ssid=x&psk=short'
if printf '%s' "$invalid" |
    CONTENT_LENGTH=${#invalid} WIFI_CONFIG_PATH="$TEST_DIR/invalid.conf" \
    WIFI_AP_REQUEST_PATH="$MARKER" WIFI_REBOOT_COMMAND=/bin/true "$CGI" |
    grep -q 'between 8 and 63'; then
    :
else
    echo "short WPA key was accepted" >&2
    exit 1
fi

! grep -q 'if=/dev/stdin' "$CGI"
! grep -qiE 'configurar|contraseña|guardar|volver|reiniciará|solicitud|acción' \
    "$ROOT/include/www/wifi/index.html" "$CGI"

echo "Wi-Fi mode, station retry and provisioning portal tests passed"

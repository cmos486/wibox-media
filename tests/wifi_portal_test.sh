#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CGI="$ROOT/include/www/wifi/cgi-bin/wifi-config.cgi"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM

sh -n "$ROOT/include/bin/wifi_portal_start.sh" "$ROOT/include/bin/ap_start.sh" "$CGI"

body='ssid=Vecino+WiFi&psk=secreto123'
printf '%s' "$body" |
    CONTENT_LENGTH=${#body} WIFI_CONFIG_PATH="$TEST_DIR/wpa_supplicant.conf" "$CGI" >"$TEST_DIR/response"

grep -q 'Configuración guardada' "$TEST_DIR/response"
grep -q '^        ssid="Vecino WiFi"$' "$TEST_DIR/wpa_supplicant.conf"
grep -q '^        psk="secreto123"$' "$TEST_DIR/wpa_supplicant.conf"

invalid='ssid=x&psk=short'
if printf '%s' "$invalid" |
    CONTENT_LENGTH=${#invalid} WIFI_CONFIG_PATH="$TEST_DIR/invalid.conf" "$CGI" |
    grep -q 'entre 8 y 63'; then
    :
else
    echo "short WPA key was accepted" >&2
    exit 1
fi

echo "Wi-Fi portal CGI tests passed"

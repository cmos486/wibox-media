#!/bin/sh
set -eu

CONFIG_PATH=${WIFI_CONFIG_PATH:-/mnt/mtd/wpa_supplicant.conf}
AP_REQUEST_PATH=${WIFI_AP_REQUEST_PATH:-/mnt/mtd/wifi_ap_requested}
REBOOT_COMMAND=${WIFI_REBOOT_COMMAND:-/sbin/reboot}
REBOOT_DELAY=${WIFI_REBOOT_DELAY:-1}
GPIO_SCRIPT=${WIFI_GPIO_SCRIPT:-/usr/bin/gpio.sh}
MAX_BODY=4096

show_success_led() {
    if [ -f "$GPIO_SCRIPT" ]; then
        . "$GPIO_SCRIPT"
        wifi_led_blink_stop
        wifi_led green
    fi
}

html_error() {
    printf 'Status: 400 Bad Request\r\nContent-Type: text/html; charset=utf-8\r\n\r\n'
    printf '<!doctype html><html lang="en"><meta charset="utf-8"><title>Error</title><h1>Unable to continue</h1><p>%s</p><p><a href="/">Go back</a></p></html>\n' "$1"
    exit 0
}

length=${CONTENT_LENGTH:-0}
case "$length" in
    ''|*[!0-9]*) html_error 'Invalid request.' ;;
esac
[ "$length" -le "$MAX_BODY" ] || html_error 'The request is too large.'

# BusyBox httpd provides the POST body on standard input but this target does
# not expose /dev/stdin. Let dd consume its inherited stdin directly.
body=$(dd bs=1 count="$length" 2>/dev/null || true)

raw_param() {
    key=$1
    printf '%s' "$body" | tr '&' '\n' |
        awk -v wanted="$key" 'index($0, "=") == length(wanted) + 1 {
            print substr($0, length(wanted) + 2)
            exit
        }'
}

url_decode() {
    # Form encoding uses '+' for spaces and %HH for bytes.  BusyBox printf
    # supports the \xHH escape and keeps this CGI self-contained.
    encoded=$(printf '%s' "$1" | sed 's/+/ /g; s/%/\\x/g')
    printf '%b' "$encoded"
}

action=$(url_decode "$(raw_param action)")
[ -n "$action" ] || action=save

if [ "$action" = "cancel" ]; then
    [ -f "$CONFIG_PATH" ] || html_error 'No saved Wi-Fi configuration exists.'
    rm -f "$AP_REQUEST_PATH"
    sync
    show_success_led
    printf 'Content-Type: text/html; charset=utf-8\r\n\r\n'
    printf '<!doctype html><html lang="en"><meta charset="utf-8"><title>Returning to Wi-Fi</title><h1>Returning to the saved network</h1><p>WiBox will restart in station mode.</p></html>\n'
    (sleep "$REBOOT_DELAY"; "$REBOOT_COMMAND") >/dev/null 2>&1 &
    exit 0
fi
[ "$action" = "save" ] || html_error 'Invalid action.'

ssid=$(url_decode "$(raw_param ssid)")
psk=$(url_decode "$(raw_param psk)")

ssid_bytes=$(printf '%s' "$ssid" | wc -c | tr -d ' ')
psk_bytes=$(printf '%s' "$psk" | wc -c | tr -d ' ')
[ "$ssid_bytes" -ge 1 ] && [ "$ssid_bytes" -le 32 ] || html_error 'The SSID must be between 1 and 32 bytes.'
[ "$psk_bytes" -ge 8 ] && [ "$psk_bytes" -le 63 ] || html_error 'The password must be between 8 and 63 bytes.'

# Newlines/control bytes must never enter the quoted WPA configuration.
if printf '%s%s' "$ssid" "$psk" | LC_ALL=C grep -q '[[:cntrl:]]'; then
    html_error 'The network name or password contains unsupported characters.'
fi

escape_wpa() {
    printf '%s' "$1" | sed 's/[\\"]/\\&/g'
}

ssid_escaped=$(escape_wpa "$ssid")
psk_escaped=$(escape_wpa "$psk")
tmp="${CONFIG_PATH}.new.$$"
umask 077
mkdir -p "$(dirname "$CONFIG_PATH")"
{
    printf '%s\n' 'ctrl_interface=/var/run/wpa_supplicant'
    printf '%s\n' 'ap_scan=1'
    printf '%s\n' 'network={'
    printf '        ssid="%s"\n' "$ssid_escaped"
    printf '        psk="%s"\n' "$psk_escaped"
    printf '%s\n' '        scan_ssid=1'
    printf '%s\n' '        key_mgmt=WPA-PSK'
    printf '%s\n' '}'
} > "$tmp" || html_error 'Unable to write the configuration.'
mv "$tmp" "$CONFIG_PATH" || html_error 'Unable to activate the configuration.'
rm -f "$AP_REQUEST_PATH"
sync
show_success_led

printf 'Content-Type: text/html; charset=utf-8\r\n\r\n'
printf '<!doctype html><html lang="en"><meta charset="utf-8"><title>Wi-Fi saved</title><h1>Configuration saved</h1><p>WiBox will restart and connect to the selected network.</p></html>\n'
(sleep "$REBOOT_DELAY"; "$REBOOT_COMMAND") >/dev/null 2>&1 &

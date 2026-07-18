#!/bin/sh

CONFIG_PATH=${WIFI_CONFIG_PATH:-/mnt/mtd/wpa_supplicant.conf}
AP_REQUEST_PATH=${WIFI_AP_REQUEST_PATH:-/mnt/mtd/wifi_ap_requested}

if [ -f "$AP_REQUEST_PATH" ]; then
    echo ap
elif [ ! -f "$CONFIG_PATH" ]; then
    echo ap
elif ! grep -q '^[[:space:]]*ssid[[:space:]]*=' "$CONFIG_PATH"; then
    echo ap
else
    echo station
fi

#!/bin/sh

if pgrep hostapd > /dev/null; then
  echo "Running as AP, skipping."
  exit 0
fi

if [ "$(/usr/bin/wifi_mode.sh)" != "station" ]; then
  echo "Station mode is not selected, skipping."
  exit 0
fi

MANAGER_PID_PATH=/var/run/wifi_station_manager.pid
if [ -f "$MANAGER_PID_PATH" ]; then
  MANAGER_PID=$(sed -n '1p' "$MANAGER_PID_PATH" 2>/dev/null || true)
  if [ -n "$MANAGER_PID" ] && [ -r "/proc/$MANAGER_PID/cmdline" ] &&
     tr '\000' ' ' <"/proc/$MANAGER_PID/cmdline" | grep -q 'wifi_station_manager.sh'; then
    echo "Station manager is running."
    exit 0
  fi
fi

echo "Station manager was not running; restarting it while preserving uptime."
/usr/bin/wifi_station_manager.sh >/dev/null 2>&1 &

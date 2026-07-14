#!/bin/sh
set -eu

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

${CC:-cc} -std=gnu99 -Wall -Wextra -Isrc/sip_media \
  tests/watchdog_defaults_test.c src/sip_media/config.c \
  -o "$TMP_DIR/watchdog_defaults_test"
"$TMP_DIR/watchdog_defaults_test"

for file in src/sip_media/sip_media.conf.default include/etc/sip_media.conf.default; do
  grep -q '^hardware_watchdog_enabled=1$' "$file"
  grep -q '^hardware_watchdog_device=/dev/watchdog$' "$file"
  grep -q '^hardware_watchdog_timeout_seconds=30$' "$file"
  grep -q '^hardware_watchdog_feed_interval_seconds=5$' "$file"
done

grep -q '^WATCHDOG_MODULE=/ko/extdrv/goke_wdt.ko$' include/run.sh
grep -q 'insmod.*init_mode=4.*soft_noboot=0.*nowayout=0.*tmr_atboot=0.*tmr_margin=30' include/run.sh
grep -q 'wibox-firmware-update-critical' include/bin/app_watchdog.sh
grep -q 'WIBOX_OTA_GUARD_PATH' src/sip_media/hardware_watchdog.c
grep -q 'test-watchdog-guard' src/firmware_update.c

echo "watchdog defaults and OTA invariants OK"

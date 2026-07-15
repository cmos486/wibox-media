#!/bin/sh
set -eu

TMP_DIR=$(mktemp -d)
WATCHDOG_PID=""

cleanup() {
    if [ -n "$WATCHDOG_PID" ]; then
        kill "$WATCHDOG_PID" 2>/dev/null || true
        wait "$WATCHDOG_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

APP="$TMP_DIR/fake-app.sh"
cat > "$APP" <<'EOF'
#!/bin/sh
echo start >> "$APP_START_FILE"
sleep "${APP_SLEEP_SECONDS:-0.05}"
exit "${APP_EXIT_CODE:-7}"
EOF
chmod +x "$APP"

wait_for_lines() {
    file="$1"
    expected="$2"
    attempts=0
    while [ "$attempts" -lt 100 ]; do
        count=0
        [ -f "$file" ] && count=$(wc -l < "$file" | tr -d ' ')
        [ "$count" -ge "$expected" ] && return 0
        attempts=$((attempts + 1))
        sleep 0.05
    done
    return 1
}

stop_watchdog() {
    kill "$WATCHDOG_PID" 2>/dev/null || true
    wait "$WATCHDOG_PID" 2>/dev/null || true
    WATCHDOG_PID=""
}

if WIBOX_WATCHDOG_DIRECT=1 sh include/bin/app_watchdog.sh missing \
    "$TMP_DIR/not-found" "$TMP_DIR/missing.log" 0 1 >/dev/null 2>&1; then
    echo "missing application was accepted" >&2
    exit 1
fi

STARTS="$TMP_DIR/restart.starts"
LOG="$TMP_DIR/restart.log"
GUARD="$TMP_DIR/restart.guard"
APP_START_FILE="$STARTS" WIBOX_WATCHDOG_DIRECT=1 \
WIBOX_OTA_GUARD_PATH="$GUARD" WIBOX_LOG_ROTATE_INTERVAL_SECONDS=60 \
    sh include/bin/app_watchdog.sh fake "$APP" "$LOG" 0 64 &
WATCHDOG_PID=$!
wait_for_lines "$STARTS" 2
grep -q 'exited with code 7, restarting' "$LOG"
stop_watchdog

STARTS="$TMP_DIR/prepare.starts"
LOG="$TMP_DIR/prepare.log"
GUARD="$TMP_DIR/prepare.guard"
printf 'state=PREPARE\npid=999999\n' > "$GUARD"
APP_START_FILE="$STARTS" WIBOX_WATCHDOG_DIRECT=1 \
WIBOX_OTA_GUARD_PATH="$GUARD" WIBOX_LOG_ROTATE_INTERVAL_SECONDS=60 \
    sh include/bin/app_watchdog.sh fake "$APP" "$LOG" 0 64 &
WATCHDOG_PID=$!
wait_for_lines "$STARTS" 1
[ ! -e "$GUARD" ]
grep -q 'Removing stale pre-flash OTA guard' "$LOG"
stop_watchdog

STARTS="$TMP_DIR/flashing.starts"
LOG="$TMP_DIR/flashing.log"
GUARD="$TMP_DIR/flashing.guard"
printf 'state=FLASHING\npid=999999\n' > "$GUARD"
APP_START_FILE="$STARTS" WIBOX_WATCHDOG_DIRECT=1 \
WIBOX_OTA_GUARD_PATH="$GUARD" WIBOX_LOG_ROTATE_INTERVAL_SECONDS=60 \
    sh include/bin/app_watchdog.sh fake "$APP" "$LOG" 0 64 &
WATCHDOG_PID=$!
sleep 0.25
[ ! -e "$STARTS" ]
[ -e "$GUARD" ]
rm -f "$GUARD"
wait_for_lines "$STARTS" 1
grep -q 'OTA guard cleared; application restart resumed' "$LOG"
stop_watchdog

STARTS="$TMP_DIR/rotation.starts"
LOG="$TMP_DIR/rotation.log"
GUARD="$TMP_DIR/rotation.guard"
dd if=/dev/zero of="$LOG" bs=1024 count=3 2>/dev/null
APP_START_FILE="$STARTS" APP_SLEEP_SECONDS=5 WIBOX_WATCHDOG_DIRECT=1 \
WIBOX_OTA_GUARD_PATH="$GUARD" WIBOX_LOG_ROTATE_INTERVAL_SECONDS=1 \
    sh include/bin/app_watchdog.sh fake "$APP" "$LOG" 0 1 &
WATCHDOG_PID=$!
attempts=0
while [ "$attempts" -lt 60 ] && [ ! -e "$LOG.old" ]; do
    attempts=$((attempts + 1))
    sleep 0.05
done
[ -s "$LOG.old" ]
grep -q 'Log rotated by background process' "$LOG"
stop_watchdog

echo "RESULT app_watchdog_integration PASS"

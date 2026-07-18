#!/bin/sh
set -u

KILLALL=${DROPBEAR_KILLALL:-/bin/killall}
PIDOF=${DROPBEAR_PIDOF:-/bin/pidof}
SLEEP=${DROPBEAR_SLEEP:-/bin/sleep}
LOG_PATH=${DROPBEAR_RESTART_LOG:-/var/log/dropbear-restart.log}
MAX_ATTEMPTS=${DROPBEAR_RESTART_ATTEMPTS:-3}

if [ -n "${DROPBEAR_BIN:-}" ]; then
    DROPBEAR=$DROPBEAR_BIN
elif [ -x /usr/sbin/dropbear ]; then
    DROPBEAR=/usr/sbin/dropbear
elif [ -x /sbin/dropbear ]; then
    DROPBEAR=/sbin/dropbear
else
    DROPBEAR=$(command -v dropbear 2>/dev/null || true)
fi

mkdir -p "$(dirname "$LOG_PATH")"
log() {
    printf '%s dropbear-restart: %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >>"$LOG_PATH"
}

if [ -z "$DROPBEAR" ]; then
    log "dropbear binary not found"
    exit 1
fi

"$KILLALL" -q dropbear 2>/dev/null || true

# Dropbear may still own TCP 22 briefly after killall. Starting its replacement
# immediately made SSH recovery intermittent on the GK7102S.
waited=0
while "$PIDOF" dropbear >/dev/null 2>&1 && [ "$waited" -lt 5 ]; do
    "$SLEEP" 1
    waited=$((waited + 1))
done

attempt=1
while [ "$attempt" -le "$MAX_ATTEMPTS" ]; do
    log "start attempt=${attempt} waited=${waited}s"
    "$DROPBEAR" -R >>"$LOG_PATH" 2>&1 || true
    "$SLEEP" 1
    if "$PIDOF" dropbear >/dev/null 2>&1; then
        log "listener process available"
        exit 0
    fi
    "$SLEEP" "$attempt"
    attempt=$((attempt + 1))
done

log "failed after ${MAX_ATTEMPTS} attempts"
exit 1

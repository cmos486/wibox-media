#!/bin/sh

# Generic application watchdog script
# Usage: app_watchdog.sh <app_name> <app_path> [log_file] [restart_delay] [max_log_size_kb]

APP_NAME="$1"
APP_PATH="$2"
LOG_FILE="${3:-/var/log/${APP_NAME}.log}"
RESTART_DELAY="${4:-5}"
LOG_MAX_SIZE="${5:-100}"  # KB
OTA_GUARD="/tmp/wibox-firmware-update-critical"

# Validation
if [ -z "$APP_NAME" ] || [ -z "$APP_PATH" ]; then
    echo "Usage: $0 <app_name> <app_path> [log_file] [restart_delay] [max_log_size_kb]"
    echo "Example: $0 wibox-media-daemon /usr/bin/wibox-media-daemon"
    echo "Example: $0 my_app /usr/bin/my_app /var/log/custom.log 10 200"
    exit 1
fi

# Check if application exists
if [ ! -f "$APP_PATH" ]; then
    echo "Error: Application $APP_PATH not found!"
    exit 1
fi

# Make sure log directory exists
LOG_DIR=$(dirname "$LOG_FILE")
mkdir -p "$LOG_DIR"

# Background log rotation function
log_rotator() {
    # A background shell function inherits the parent script name. Give this
    # helper a distinct process name so it cannot be mistaken for a second
    # application watchdog in ps/process monitoring.
    printf '%s\n' "wibox-logrotate" > /proc/self/comm 2>/dev/null || true

    while true; do
        sleep 300  # Check every 5 minutes

        if [ -f "$LOG_FILE" ]; then
            # Check file size (in KB)
            LOG_SIZE=$(du -k "$LOG_FILE" | cut -f1)
            if [ $LOG_SIZE -gt $LOG_MAX_SIZE ]; then
                # Copy current log to backup
                cp "$LOG_FILE" "${LOG_FILE}.old"

                # Truncate original log (atomic operation)
                > "$LOG_FILE"

                echo "$(date): Log rotated by background process (was ${LOG_SIZE}KB)" >> "$LOG_FILE"
            fi
        fi
    done
}

# Start background log rotator
log_rotator &
LOG_ROTATOR_PID=$!

# Trap to clean up background process when script exits
cleanup() {
    echo "$(date): Cleaning up background processes..." >> "$LOG_FILE"
    kill $LOG_ROTATOR_PID 2>/dev/null
    exit 0
}

trap cleanup EXIT INT TERM

wait_for_ota_guard() {
    announced=0
    while [ -f "$OTA_GUARD" ]; do
        GUARD_STATE=$(sed -n 's/^state=//p' "$OTA_GUARD" 2>/dev/null | head -1)
        GUARD_PID=$(sed -n 's/^pid=//p' "$OTA_GUARD" 2>/dev/null | head -1)

        # PREPARE means flash has not been touched. If its updater vanished,
        # remove the stale guard and recover the daemon automatically.
        if [ "$GUARD_STATE" = "PREPARE" ]; then
            case "$GUARD_PID" in
                ''|*[!0-9]*) ;;
                *)
                    if ! kill -0 "$GUARD_PID" 2>/dev/null; then
                        echo "$(date): Removing stale pre-flash OTA guard (PID $GUARD_PID)" >> "$LOG_FILE"
                        rm -f "$OTA_GUARD"
                        break
                    fi
                    ;;
            esac
        fi

        if [ "$announced" = 0 ]; then
            echo "$(date): OTA guard active state=${GUARD_STATE:-unknown}; application restart paused" >> "$LOG_FILE"
            announced=1
        fi
        sleep 1
    done
    if [ "$announced" = 1 ] && [ ! -f "$OTA_GUARD" ]; then
        echo "$(date): OTA guard cleared; application restart resumed" >> "$LOG_FILE"
    fi
}

# Initial log entries
echo "Starting $APP_NAME watchdog (PID: $$)..." >> "$LOG_FILE"
echo "App path: $APP_PATH" >> "$LOG_FILE"
echo "Log file: $LOG_FILE" >> "$LOG_FILE"
echo "Restart delay: ${RESTART_DELAY}s" >> "$LOG_FILE"
echo "Max log size: ${LOG_MAX_SIZE}KB" >> "$LOG_FILE"
echo "Log rotator PID: $LOG_ROTATOR_PID" >> "$LOG_FILE"

# Main watchdog loop
while true; do
    wait_for_ota_guard
    echo "$(date): Starting $APP_NAME" >> "$LOG_FILE"

    # Start the application with logging (busybox compatible with immediate flushing)
    # Method 1: Try script command for line buffering (creates PTY)
    if command -v script >/dev/null 2>&1; then
        # script creates a pseudo-terminal which forces line buffering
        # Filter out carriage returns (^M) that script can introduce
        script -q -c "$APP_PATH" /dev/null 2>&1 | tr -d '\r' | tee -a "$LOG_FILE" >/dev/null &
        APP_PID=$!
    else
        # Method 2: Direct redirection (no tee delay, but no console output)
        $APP_PATH >> "$LOG_FILE" 2>&1 &
        APP_PID=$!
    fi

    # Wait for the process to finish
    wait $APP_PID
    EXIT_CODE=$?

    # If we get here, the app crashed or exited
    echo "$(date): $APP_NAME (PID: $APP_PID) exited with code $EXIT_CODE, restarting in ${RESTART_DELAY}s..." >> "$LOG_FILE"
    if [ ! -f "$OTA_GUARD" ]; then
        sleep "$RESTART_DELAY"
    fi
done

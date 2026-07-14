# Runtime Resilience Specification

## Purpose

Define process restart, hardware watchdog behavior, OTA coordination, log
rotation and safe user-requested reboot.

## Requirements

### Requirement: Application supervision

`app_watchdog.sh` SHALL run the media daemon, append its output to the runtime
log and restart it indefinitely after exit, with a five-second default delay.

#### Scenario: The daemon crashes

- GIVEN no OTA guard is active
- WHEN the supervised daemon exits
- THEN the exit is logged
- AND the daemon is restarted after the configured delay

### Requirement: Distinct log-rotation helper

The supervisor SHALL run one background log rotator with a distinct process
name so process monitoring does not mistake it for a second app watchdog.

#### Scenario: Processes are inspected

- GIVEN the supervisor and rotator are running
- WHEN process names are listed
- THEN the rotator is identifiable as `wibox-logrotate`

### Requirement: OTA-aware supervision

The app watchdog SHALL pause daemon starts while the OTA guard exists. A stale
PREPARE guard whose updater PID no longer exists SHALL be removed, but guards
after flash is touched SHALL not be cleared automatically.

#### Scenario: Updater dies before flash

- GIVEN guard state is PREPARE and its recorded PID is gone
- WHEN the app watchdog evaluates the guard
- THEN it removes the stale guard
- AND resumes daemon recovery

### Requirement: Stoppable production hardware watchdog

Boot SHALL reload `goke_wdt` with `nowayout=0` and create a stoppable watchdog
for normal shutdown and OTA. Missing `/dev/watchdog` SHALL log a warning while
the app supervisor continues to protect process availability.

#### Scenario: Watchdog device is absent

- GIVEN the kernel device cannot be created
- WHEN production boot and daemon startup continue
- THEN the app watchdog remains active
- AND the daemon does not crash solely because hardware watchdog startup failed

### Requirement: Main-loop health feeding

When enabled, the daemon SHALL feed the hardware watchdog only while the main
loop heartbeat remains fresh. With defaults it SHALL use a 30-second hardware
timeout, five-second feed interval and suspend feeding after roughly 15 seconds
without a main-loop heartbeat.

#### Scenario: The main loop deadlocks

- GIVEN hardware watchdog feeding is active
- WHEN main-loop heartbeat becomes stale for half the timeout
- THEN the feeder stops keepalives
- AND the hardware watchdog can reboot the device at its timeout

### Requirement: Watchdog configuration safety

Hardware watchdog timeout SHALL be constrained to 5 through 300 seconds, feed
interval SHALL be at least one second, and twice the feed interval SHALL remain
strictly below the effective timeout.

#### Scenario: Unsafe timing is configured

- GIVEN feed timing cannot satisfy the safety relationship
- WHEN watchdog startup validates it
- THEN hardware watchdog arming fails safely
- AND daemon operation continues under the app supervisor

### Requirement: Clean disarm

Normal daemon shutdown and OTA preparation SHALL disarm and close the hardware
watchdog using supported ioctl or magic-close behavior.

#### Scenario: The daemon exits cleanly

- GIVEN the hardware watchdog is active
- WHEN normal shutdown runs
- THEN the watchdog is disarmed before its descriptor closes

### Requirement: One-shot safe reboot

The MQTT Reboot Device action SHALL accept one non-retained request, make its
button unavailable, ignore duplicates, call `sync()` and request system reboot.

#### Scenario: Reboot is pressed repeatedly

- GIVEN one reboot request was accepted
- WHEN another request arrives before reboot
- THEN it is ignored

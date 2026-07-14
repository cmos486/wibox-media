# Boot and Runtime Specification

## Purpose

Define how the custom image starts the device, restores persistent settings and
establishes one authoritative media runtime.

## Requirements

### Requirement: Production boot sequence

`/usr/run.sh` SHALL initialize GPIO and LEDs, load required kernel modules,
configure networking, perform the one-time Sofia hardware warmup, prepare the
watchdog and start `wibox-media-daemon` under `app_watchdog.sh`.

#### Scenario: Normal custom boot

- GIVEN a valid custom image and persistent WiFi configuration
- WHEN the WiBox boots
- THEN networking and Dropbear SSH are started
- AND Sofia runs only for hardware warmup
- AND the media daemon starts under the application supervisor

### Requirement: Single packaged media runtime

`wibox-media-daemon` SHALL be the only packaged SIP/media runtime and SHALL own
UART, SIP, audio, video, MQTT, updates and Prometheus integration.

#### Scenario: Runtime processes are inspected

- GIVEN production boot completed
- WHEN process state is inspected
- THEN the daemon is supervised by one app watchdog
- AND Sofia is not running per call
- AND legacy listener or bridge runtimes are absent

### Requirement: Persistent identity and configuration

The hostname SHALL be derived from device identity in `mtd6`. Persistent WiFi,
daemon configuration, Dropbear keys and optional `post.sh` SHALL live under
`/mnt/mtd`; `/tmp` and `/var` SHALL remain volatile.

#### Scenario: First custom boot has no daemon config

- GIVEN `/mnt/mtd/sip_media.conf` is absent
- WHEN boot reaches daemon setup
- THEN the packaged default config is copied to the persistent path

#### Scenario: Device reboots

- GIVEN persistent configuration and SSH keys exist
- WHEN the WiBox reboots
- THEN those files survive
- AND runtime logs and temporary files do not

### Requirement: Network recovery after warmup

Because Sofia warmup can disrupt WiFi, boot SHALL re-establish station mode
after warmup and MAY fall back to the stock AP path when station configuration
cannot be used.

#### Scenario: Warmup drops WiFi

- GIVEN Sofia warmup completed
- WHEN the previous WiFi processes no longer provide connectivity
- THEN boot restarts WiFi association and DHCP before declaring completion

### Requirement: Boot status indication

The boot scripts SHALL use red for boot or WiFi failure, off while WiFi setup is
in progress, green after successful association/DHCP and blue after production
startup completes.

#### Scenario: Production startup completes

- GIVEN the daemon has been launched
- WHEN boot removes the heartbeat lock
- THEN the status LED becomes blue

### Requirement: Local boot extension

An executable `/mnt/mtd/post.sh` SHALL run after the supervised daemon is
started, without being packaged or overwritten by routine image updates.

#### Scenario: A local post hook exists

- GIVEN `/mnt/mtd/post.sh` is executable
- WHEN production boot reaches the extension point
- THEN the script is executed once for that boot

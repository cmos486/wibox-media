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

### Requirement: Deterministic WiFi mode selection

Boot SHALL complete Sofia warmup before final WiFi configuration. A persistent
`/mnt/mtd/wifi_ap_requested` marker SHALL select AP mode without attempting
station association. When the marker is absent, an existing persistent station
configuration SHALL select station mode; missing station credentials SHALL
select AP provisioning mode.

#### Scenario: AP mode was explicitly requested

- GIVEN `/mnt/mtd/wifi_ap_requested` exists
- WHEN boot completes Sofia warmup
- THEN the WiBox starts AP provisioning without waiting for station or DHCP timeouts
- AND the previous station configuration remains unchanged

#### Scenario: No station credentials exist

- GIVEN `/mnt/mtd/wpa_supplicant.conf` is absent
- AND AP mode was not explicitly requested
- WHEN boot selects its network mode
- THEN the WiBox starts AP provisioning automatically

### Requirement: Station retry without reboot

When persistent station credentials exist and AP mode was not requested, the
WiBox SHALL remain in station mode. Association SHALL use a 20-second timeout
and DHCP SHALL use a 10-second timeout.
Failure SHALL be retried with bounded backoff without rebooting or entering AP
mode. A later successful lease SHALL make SSH reachable and restart the media
daemon when necessary so it binds the acquired address.

#### Scenario: Configured router is temporarily unavailable

- GIVEN valid persistent station credentials exist
- AND `/mnt/mtd/wifi_ap_requested` is absent
- WHEN association or DHCP fails
- THEN the WiBox remains in station mode
- AND retries with bounded backoff
- AND does not reboot or start the provisioning AP

#### Scenario: Router becomes available after startup

- GIVEN the station retry manager is running without a lease
- WHEN association and DHCP later succeed
- THEN the WiBox becomes reachable without a power cycle
- AND Dropbear and the media daemon use the acquired station interface

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

# Observability Specification

## Purpose

Define health endpoints, metrics, event telemetry, release metadata and bounded
RAM-backed logs.

## Requirements

### Requirement: Prometheus health and metrics

When enabled, the daemon SHALL expose `GET /healthz` and `GET /metrics` on the
configured port.

#### Scenario: A monitoring system scrapes the device

- GIVEN Prometheus is enabled
- WHEN `/metrics` is requested
- THEN the daemon returns current metric text

### Requirement: Runtime metric families

Metrics SHALL cover build information, uptime, health, MQTT connectivity,
call/SIP/video/ringing state, ring/call/video/unlock counters, last ring/unlock,
WiFi RSSI and UART frame categories.

#### Scenario: An unknown UART frame arrives

- GIVEN metrics are enabled
- WHEN an unknown frame is parsed
- THEN total UART and unknown-frame counters reflect the event

### Requirement: Release metadata

The image SHALL expose version, commit and build timestamp in
`/usr/etc/wibox-release`, Home Assistant discovery and Prometheus build metadata.

#### Scenario: A release image boots

- GIVEN valid release metadata was packaged
- WHEN integrations publish device identity
- THEN version, commit and build timestamp are available for diagnostics

### Requirement: Stateless event telemetry

UART and call events SHALL be published as non-retained JSON with raw/alias or
call-correlation context as defined by their capability specs.

#### Scenario: A physical ring begins

- GIVEN MQTT is connected
- WHEN the UART and call handlers observe the ring
- THEN serial evidence and high-level call transition can be correlated

### Requirement: RAM-backed runtime logs

The daemon log SHALL be `/var/log/wibox-media-daemon.log` and updater log SHALL
be `/tmp/firmware_update.log`. Both SHALL remain volatile to avoid flash wear.

#### Scenario: The WiBox reboots

- GIVEN runtime logs exist
- WHEN power cycles
- THEN those logs are not expected to persist

### Requirement: Bounded daemon logging

The app supervisor SHALL check the daemon log periodically, preserve one `.old`
copy and truncate the active log when it exceeds 100 KB by default.

#### Scenario: Daemon log grows beyond its limit

- GIVEN the log exceeds the configured threshold
- WHEN the background rotator checks it
- THEN the previous content is copied to `.old`
- AND the active log is truncated and remains writable

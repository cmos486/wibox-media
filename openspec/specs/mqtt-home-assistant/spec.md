# MQTT and Home Assistant Specification

## Purpose

Define the native broker connection, Home Assistant discovery, retained state,
runtime controls and protection against action replay.

## Requirements

### Requirement: Native MQTT client

The daemon SHALL implement MQTT 3.1.1 directly and SHALL NOT spawn or package
`mosquitto_pub` or `mosquitto_sub`. It SHALL support broker host and optional
credentials from persistent configuration.

#### Scenario: MQTT is enabled

- GIVEN valid broker configuration
- WHEN the daemon starts
- THEN it connects with the native client
- AND publishes availability and discovery

### Requirement: Stable topic identity

The default base topic, device ID and device name SHALL derive from the WiBox
hostname unless explicit values are configured.

#### Scenario: Identity overrides are empty

- GIVEN the three MQTT identity fields are empty
- WHEN MQTT initializes
- THEN stable hostname-derived values are used

### Requirement: Home Assistant discovery coverage

Discovery SHALL include door, F1, reboot, developer controls, snapshots, UART
and call events, media and call ID, firmware metadata, door pulse, WiFi RSSI,
video, RTSP, outgoing SIP configuration, timeout, ring delay, call-forward and,
when enabled, firmware-update entities.

#### Scenario: Discovery is published

- GIVEN MQTT connects
- WHEN retained discovery is sent
- THEN all enabled capability entities share one device identity
- AND disabled firmware-update entities are removed

### Requirement: Retained state and edge events

Current configuration and state topics SHALL be retained. `uart/event` and
`call/event` SHALL be non-retained because they represent edges rather than
current state.

#### Scenario: Home Assistant restarts

- GIVEN retained device state exists at the broker
- WHEN Home Assistant reconnects
- THEN current state is restored
- AND old call or UART events are not replayed as new events

### Requirement: Action replay protection

Retained messages on door, F1, reboot, developer simulations, snapshot, update
check or update install command topics SHALL be ignored.

#### Scenario: The daemon reconnects after an old button press

- GIVEN the broker retained an action payload by mistake
- WHEN subscriptions are restored
- THEN the action is not executed

### Requirement: Retained runtime configuration

Retained commands SHALL be accepted for video enable, RTSP enable, bitrate,
outgoing SIP enable and target, call timeout, ring snapshot delay and
call-forward state. The effective state SHALL be republished separately.

#### Scenario: Outgoing SIP is disabled

- GIVEN a retained OFF command is accepted
- WHEN state is republished
- THEN the outgoing SIP switch is OFF
- AND target/timeout configuration availability is offline
- AND future physical rings still publish `media_state=ringing`

### Requirement: Dynamic command availability

Snapshot SHALL be unavailable when video is disabled or capture is busy.
Developer simulation buttons SHALL be unavailable while Developer Mode is OFF.
Reboot and update-install buttons SHALL become unavailable immediately after a
request is accepted.

#### Scenario: A snapshot begins

- GIVEN video is enabled and snapshot is available
- WHEN capture is accepted
- THEN snapshot availability becomes offline until capture finishes

### Requirement: Reconnect continuity

MQTT reconnect SHALL republish cached media state, active call ID and effective
runtime configuration without generating false call transitions. Developer Mode
SHALL reset to OFF after daemon restart.

#### Scenario: Broker reconnects during a call

- GIVEN media state is established and a call ID is active
- WHEN MQTT reconnects
- THEN the same state and call ID are republished
- AND no idle event is invented

### Requirement: Legacy retained cleanup

The daemon SHALL clear discovery and state for removed intermediate entities,
including legacy ringing/call/video active and last-event sensors.

#### Scenario: Firmware upgrades from a legacy release

- GIVEN the broker retains removed entity configuration
- WHEN discovery is refreshed
- THEN obsolete retained topics are cleared
- AND the simplified current entities remain

# Intercom UART Specification

## Purpose

Define ownership of `/dev/ttySGK1`, supported Fermax commands, incoming frame
effects and raw event observability.

## Requirements

### Requirement: Serial ownership

When `serial_listener_enabled=1`, the daemon SHALL own the configured intercom
device, default `/dev/ttySGK1`, for both incoming frames and outgoing commands.

#### Scenario: Serial listener starts

- GIVEN the intercom device is available
- WHEN the daemon initializes serial control
- THEN it listens for MCU frames
- AND sends the startup call-forward enable command

### Requirement: Supported outgoing commands

The daemon SHALL support main unlock, START_CALL, STOP_CALL, push-state enable
and disable, and F1 on/off commands using the documented four-byte frames.

#### Scenario: A command is sent

- GIVEN an internal action requests a supported intercom command
- WHEN the UART write succeeds
- THEN the exact frame is written once
- AND an MQTT UART event is published with `direction=out`

### Requirement: Raw and decoded input events

Every serial read SHALL be logged and published as non-retained `raw_read`
telemetry. Recognized four-byte frames SHALL additionally publish their alias,
event type, parameter and `known=true`; unrecognized FB frames SHALL publish
`unknown_fb` without crashing the daemon.

#### Scenario: An unknown frame arrives

- GIVEN the serial listener reads an unrecognized FB frame
- WHEN parsing completes
- THEN the raw bytes are preserved in telemetry
- AND the unknown-frame metric increments
- AND normal listening continues

### Requirement: Call-related frame handling

`ALARM_REPORT` SHALL enter the shared ring flow. `HANG_UP_0`, `HANG_UP_1` and
`PHYSICAL_HANDSET_ANSWERED` SHALL terminate the relevant ringing/call flow and
return high-level media state to idle.

#### Scenario: Physical handset is answered

- GIVEN remote SIP ringing is in progress
- WHEN `FB 23 00 2E` is received
- THEN it is labeled `physical_handset_answered`
- AND the shared terminal handler cancels remote ringing

### Requirement: Non-call telemetry

`PUSH_STATE_0` and `PUSH_STATE_1` SHALL update call-forward state. MCU state,
reset and long-down frames SHALL remain observable without adding values to
`media_state`.

#### Scenario: The physical forward button changes

- GIVEN the user toggles the WiBox forward button
- WHEN a push-state frame arrives
- THEN the MQTT call-forward switch reflects the physical state
- AND no ring is generated

### Requirement: Reopen guard

`intercom_reopen_guard_ms` SHALL default to zero. When configured above zero,
the daemon SHALL delay reopening after a confirmed close by at least that guard;
physical-ring snapshots SHALL remain unaffected because they do not open or
close the panel context.

#### Scenario: A guarded reopen is requested

- GIVEN the daemon recently confirmed a panel close
- WHEN another internal action requests START_CALL inside the guard window
- THEN reopening waits until the configured minimum interval has elapsed

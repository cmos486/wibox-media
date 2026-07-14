# Developer Controls Specification

## Purpose

Define opt-in simulations and bounded local diagnostics without duplicating
production call logic or leaving unsafe controls enabled after restart.

## Requirements

### Requirement: Developer mode gate

MQTT developer actions SHALL be unavailable unless Developer Mode is ON.
Developer Mode SHALL default to OFF on every daemon start, and retained
developer commands SHALL be ignored.

#### Scenario: The daemon restarts

- GIVEN Developer Mode was previously enabled
- WHEN the daemon starts again
- THEN Developer Mode is OFF
- AND simulation buttons are unavailable

### Requirement: Simulated DING parity

Developer Simulate DING SHALL feed the configured DING message through the
existing local control pipe and normal ring handler. Because no real panel ring
opened video, the simulation SHALL create the temporary panel context required
for its automatic snapshot.

#### Scenario: A simulated ring is triggered

- GIVEN Developer Mode is ON
- WHEN the MQTT simulate-DING button is pressed
- THEN the control-pipe DING path runs
- AND normal media-state, snapshot, SIP-routing, timeout and call-event logic is reused

### Requirement: Simulated handset answer

Developer Simulate Handset Answered SHALL invoke the same software handler used
for `PHYSICAL_HANDSET_ANSWERED` and SHALL only be available in Developer Mode.

#### Scenario: Simulated handset wins a ringing call

- GIVEN a simulated or physical call is ringing and Developer Mode is ON
- WHEN handset answer simulation is invoked
- THEN remote SIP ringing is cancelled through the production handler
- AND the call flow reaches its normal terminal state

### Requirement: Local control FIFO

The daemon SHALL create the configured FIFO, default `/tmp/pipe_sip`, and SHALL
support `DING`, four-byte `UART` injection, bounded `AUDIO_TEST` and bounded
`VIDEO_TEST` diagnostics for authenticated shell operators.

#### Scenario: A UART frame is injected locally

- GIVEN an operator has shell access
- WHEN a valid `UART FB ..` command is written to the FIFO
- THEN the same frame handler used by `/dev/ttySGK1` processes it

### Requirement: Bounded media diagnostics

`AUDIO_TEST` and `VIDEO_TEST` SHALL open the panel context, stream to the
specified IP/port for the requested duration and close the context afterward.

#### Scenario: A five-second video test runs

- GIVEN a valid target and duration
- WHEN `VIDEO_TEST` is accepted
- THEN media runs for the bounded duration
- AND the panel context is closed when the test ends

### Requirement: Simulations do not flash firmware

Developer controls SHALL NOT deploy binaries, alter persistent firmware or
start OTA operations.

#### Scenario: A simulation completes

- GIVEN any developer simulation was executed
- WHEN it terminates
- THEN no write to `mtd4` occurred

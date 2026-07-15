# SIP Calling Specification

## Purpose

Define incoming and outgoing SIP signaling, negotiated media and call timeout
behavior on the trusted LAN.

## Requirements

### Requirement: Configurable outgoing calls

Physical or simulated rings SHALL place an outgoing SIP INVITE to the configured
target only when `sip_outgoing_call_enabled=1`. The target SHALL be configurable
from the file and retained MQTT at runtime.

#### Scenario: Outgoing SIP is enabled

- GIVEN a valid target URI and no active call
- WHEN the shared ring handler runs
- THEN the daemon starts one outgoing SIP attempt
- AND publishes `sip_calling` for the active call ID

#### Scenario: Outgoing SIP is disabled

- GIVEN outgoing SIP is OFF
- WHEN a physical ring arrives
- THEN no INVITE is sent
- AND high-level state remains ringing until a generic terminal condition

### Requirement: Direct incoming SIP

The daemon SHALL accept supported direct incoming SIP INVITE/ACK/BYE/CANCEL
flows and correlate the established call even when no doorbell ring preceded it.

#### Scenario: A LAN client calls the WiBox

- GIVEN no call flow is active
- WHEN a valid incoming call is established
- THEN a call session is created
- AND media attaches using the negotiated endpoints

### Requirement: SDP media contract

SIP SHALL advertise PCMA/8000 and telephone-event/8000. H.264/90000 with the
configured dynamic payload SHALL be advertised only when global video is enabled.

#### Scenario: Audio-only installation answers

- GIVEN `video_enabled=0`
- WHEN offer or answer SDP is generated
- THEN PCMA and DTMF are present
- AND no H.264 media line is offered

### Requirement: Established panel context

When SIP media becomes established, the daemon SHALL open the panel context with
START_CALL when required, attach audio and negotiated video sinks, and publish
`media_state=established`.

#### Scenario: Outgoing call is answered

- GIVEN an outgoing SIP dialog is ringing
- WHEN a successful response with usable SDP arrives
- THEN the panel and negotiated media are attached
- AND the established call event is published

### Requirement: Termination handling

SIP BYE, CANCEL, failure and timeout SHALL detach SIP media. STOP_CALL SHALL be
sent only when an established daemon-owned panel context requires it; shared
RTSP engines SHALL remain active for their remaining clients.

#### Scenario: Remote SIP hangs up while RTSP remains

- GIVEN SIP and RTSP share media engines
- WHEN remote BYE arrives
- THEN SIP sinks are removed
- AND RTSP sinks continue
- AND high-level call state returns to idle

### Requirement: Generic outgoing timeout

The configurable outgoing timeout SHALL bound ringing/call-attempt state from
10 through 120 seconds and SHALL apply whether or not an INVITE was enabled.

#### Scenario: No one answers

- GIVEN a doorbell flow remains ringing without an active SIP call
- WHEN the configured timeout expires
- THEN the flow publishes terminal timeout
- AND media state returns to idle

### Requirement: Trusted-LAN signaling

The built-in SIP endpoint SHALL be treated as an unauthenticated trusted-LAN
service and SHALL not be exposed directly to the Internet.

#### Scenario: Internet calling is required

- GIVEN calls must cross network boundaries
- WHEN deployment is configured
- THEN a controlled PBX, VPN or equivalent network boundary is used

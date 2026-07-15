# Call Lifecycle Specification

## Purpose

Define a stable, observable lifecycle for physical intercom rings, developer
simulations and SIP calls without transferring media or hardware control away
from the existing daemon state machine.

## Requirements

### Requirement: Stable call identity

The daemon SHALL assign one opaque, non-empty call ID to each active call flow.
Every event in that flow SHALL use the same ID until a terminal event clears the
session, and the next flow SHALL receive a different ID.

#### Scenario: A new physical ring starts a session

- GIVEN no call flow is active
- WHEN the physical panel reports a ring
- THEN the daemon creates a non-empty call ID
- AND every subsequent event in that flow contains that call ID

#### Scenario: A repeated ring belongs to the active session

- GIVEN a physical ring flow is active
- WHEN the physical panel reports another ring before termination
- THEN the daemon keeps the existing call ID
- AND publishes a `ring_repeated` event for that ID

#### Scenario: A terminal event closes the session

- GIVEN a call flow is active
- WHEN the daemon observes a terminal timeout, hangup, cancellation, failure or call end
- THEN the terminal event contains the active call ID
- AND the active call ID becomes `none`
- AND a later call flow receives a new call ID

### Requirement: Structured MQTT call events

The daemon SHALL publish each observed call transition as non-retained JSON on
`wibox/<host>/call/event`. Each event SHALL contain `event_type`, `call_id`,
`sequence`, `source`, `route`, `media_state`, `reason`, `terminal`,
`started_at` and `ts`.

The supported event vocabulary SHALL include `ringing`, `ring_repeated`,
`ring_ignored`, `sip_calling`, `sip_disabled`, `sip_call_failed`,
`established`, `sip_ended`, `sip_failed`, `sip_cancelled`,
`physical_handset_answered`, `hang_up_0`, `hang_up_1`, `door_opened` and
`timeout`.

#### Scenario: An observed transition is published

- GIVEN a call session is active
- WHEN the call observer receives a lifecycle transition
- THEN it increments the session event sequence
- AND publishes one non-retained event containing the complete event contract

#### Scenario: Home Assistant discovers call observability

- GIVEN MQTT discovery is enabled
- WHEN the daemon publishes discovery configuration
- THEN Home Assistant receives a Call ID sensor definition
- AND receives a Call Event event-entity definition

### Requirement: Retained active call identity

The daemon SHALL retain the active call ID on `wibox/<host>/call/id`, using
`none` when no call session is active. MQTT reconnect SHALL republish the cached
call ID and cached media state without inventing an `idle` transition.

#### Scenario: MQTT reconnects during an active call

- GIVEN a call session and non-idle media state are active
- WHEN the MQTT connection is re-established
- THEN the daemon republishes the same active call ID
- AND republishes the cached media state
- AND does not terminate or replace the call session

### Requirement: Media-state compatibility

Call observability SHALL preserve the existing `media_state` contract. Its only
values SHALL remain `idle`, `ringing` and `established`.

#### Scenario: Ringing does not depend on outgoing SIP

- GIVEN outgoing SIP calls are disabled
- WHEN the physical panel reports a ring
- THEN `media_state` becomes `ringing`
- AND remains `ringing` until the generic ringing timeout or another terminal event
- AND then becomes `idle`

#### Scenario: SIP media is established

- GIVEN an active call session
- WHEN SIP reports media established
- THEN `media_state` becomes `established`
- AND a terminal call event returns it to `idle`

### Requirement: Observer-only integration

The call-session component SHALL observe lifecycle decisions made by the daemon.
It SHALL NOT send UART commands, start or stop audio, start or stop video, place
SIP calls, take snapshots or alter timeout decisions.

#### Scenario: Publishing observability has no hardware side effect

- GIVEN the daemon has decided a lifecycle transition
- WHEN the call-session component records and publishes that transition
- THEN no additional START_CALL or STOP_CALL command is sent
- AND no media worker is started or stopped by the observer

### Requirement: Physical and simulated source attribution

Events initiated by the physical portal SHALL use source `physical_panel`.
Events initiated by developer simulation SHALL use source
`developer_simulation`, while the normal ring, snapshot, SIP-routing and timeout
handlers remain shared.

#### Scenario: Developer simulation follows the production ring path

- GIVEN developer mode is enabled
- WHEN Developer Simulate DING is invoked
- THEN the simulation performs only the portal prerequisite needed by that action
- AND invokes the same native ring handling path as a physical ring
- AND subsequent events keep source `developer_simulation`

### Requirement: Physical handset answer terminates remote ringing

When the daemon receives `PHYSICAL_HANDSET_ANSWERED`, it SHALL publish a
terminal `physical_handset_answered` event for the active call ID, cancel the
remote SIP attempt and return `media_state` to `idle`.

#### Scenario: The physical handset wins the call race

- GIVEN a physical ring has started an outgoing SIP attempt
- WHEN `PHYSICAL_HANDSET_ANSWERED` is received
- THEN remote SIP ringing is cancelled
- AND a terminal `physical_handset_answered` event is published
- AND the active call ID becomes `none`

### Requirement: Direct incoming SIP is correlated

A direct incoming SIP call SHALL create a session when no call flow is active,
publish `established`, and close that session when SIP reports the call ended.

#### Scenario: Direct SIP call lifecycle

- GIVEN no call flow is active
- WHEN a direct incoming SIP call becomes established
- THEN the daemon creates a call ID and publishes `established`
- AND when SIP ends the call it publishes a terminal event with the same ID

### Requirement: Deterministic call-flow regression matrix

Continuous integration SHALL execute deterministic host-level scenarios against
the production call-session component and SHALL verify the MQTT discovery,
retention and event contracts.

#### Scenario: Required host E2E flows

- GIVEN the host test environment
- WHEN the call workflow E2E matrix runs
- THEN it covers `CALL-E2E-01` through `CALL-E2E-07`
- AND verifies session identity, event order, terminal cleanup and media-state outcomes
- AND the MQTT integration test covers `CALL-MQTT-01`

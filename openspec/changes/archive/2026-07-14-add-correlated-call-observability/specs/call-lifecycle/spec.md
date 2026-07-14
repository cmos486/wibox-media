# Delta for Call Lifecycle

## ADDED Requirements

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

### Requirement: Observer-only integration

The call-session component SHALL observe lifecycle decisions made by the daemon
and SHALL NOT control UART, SIP, audio, video or snapshots.

#### Scenario: Publishing observability has no hardware side effect

- GIVEN the daemon has decided a lifecycle transition
- WHEN the call-session component records and publishes that transition
- THEN no additional START_CALL or STOP_CALL command is sent
- AND no media worker is started or stopped by the observer

### Requirement: Physical and simulated source attribution

Physical portal events SHALL use source `physical_panel`. Developer simulation
events SHALL use source `developer_simulation` while sharing normal handlers.

#### Scenario: Developer simulation follows the production ring path

- GIVEN developer mode is enabled
- WHEN Developer Simulate DING is invoked
- THEN the same native ring handling path processes the simulated event
- AND subsequent events keep source `developer_simulation`

### Requirement: Physical handset answer terminates remote ringing

`PHYSICAL_HANDSET_ANSWERED` SHALL publish a terminal event, cancel remote SIP
ringing and return `media_state` to `idle`.

#### Scenario: The physical handset wins the call race

- GIVEN a physical ring has started an outgoing SIP attempt
- WHEN `PHYSICAL_HANDSET_ANSWERED` is received
- THEN remote SIP ringing is cancelled
- AND the active call ID becomes `none`

### Requirement: Direct incoming SIP is correlated

A direct incoming SIP call SHALL create a session at establishment and close it
when SIP reports the call ended.

#### Scenario: Direct SIP call lifecycle

- GIVEN no call flow is active
- WHEN a direct incoming SIP call becomes established
- THEN the daemon creates a call ID and publishes `established`
- AND the terminal event uses the same ID

### Requirement: Deterministic call-flow regression matrix

Continuous integration SHALL execute deterministic host-level call scenarios
and verify MQTT discovery, retention and event contracts.

#### Scenario: Required host E2E flows

- GIVEN the host test environment
- WHEN the call workflow E2E matrix runs
- THEN it covers `CALL-E2E-01` through `CALL-E2E-07`
- AND the MQTT integration test covers `CALL-MQTT-01`

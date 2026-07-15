# Access Control Specification

## Purpose

Define door unlock, auxiliary F1 and Fermax call-forward control while keeping
each physical function distinct and observable.

## Requirements

### Requirement: Main door unlock

The daemon SHALL unlock the main door when it receives DTMF `#` from an active
SIP call or a non-retained MQTT `PRESS` on `door/open/set`. It SHALL send
`FB 12 01 1E` to the intercom MCU.

#### Scenario: MQTT opens the main door

- GIVEN MQTT is connected
- WHEN a non-retained `PRESS` is received on `door/open/set`
- THEN the daemon sends the main unlock UART command once
- AND records the unlock in events and metrics

#### Scenario: DTMF opens the main door

- GIVEN a SIP call is active
- WHEN DTMF `#` arrives by RTP telephone-event or SIP INFO
- THEN the daemon invokes the same main unlock operation

### Requirement: Unlock feedback pulse

After accepting an unlock command, the daemon SHALL publish
`door/unlocked=ON` followed by `OFF`. This pulse SHALL represent command
acceptance rather than physical relay feedback.

#### Scenario: Home Assistant observes an unlock

- GIVEN an unlock command was accepted
- WHEN MQTT state is published
- THEN the Door Unlocked binary sensor pulses on and returns off

### Requirement: Auxiliary F1 pulse

The F1 function SHALL send `FB 17 01 23`, wait approximately 500 ms and send
`FB 17 00 22`. It SHALL remain separate from the main door unlock and SHALL be
represented as a button because no reliable relay feedback exists.

#### Scenario: F1 is triggered

- GIVEN an installation uses the optional F1 function
- WHEN a non-retained `PRESS` is received on `f1/trigger/set`
- THEN the daemon sends one bounded F1 on/off pulse
- AND does not publish the main Door Unlocked pulse

### Requirement: Physical call-forward control

The daemon SHALL expose the Fermax call-forward state independently of call
state. ON SHALL send `FB 19 01 25`, OFF SHALL send `FB 19 00 24`, and incoming
`PUSH_STATE_0` or `PUSH_STATE_1` SHALL update the reported state.

#### Scenario: Call forwarding is enabled

- GIVEN serial control is enabled
- WHEN call forwarding is set to ON
- THEN the daemon sends the enable frame
- AND reports the state as ON
- AND does not treat the push-state frame as a doorbell ring

### Requirement: Action replay protection

Retained MQTT messages SHALL NOT execute door unlock or F1 actions.

#### Scenario: Broker replays an old action

- GIVEN a retained `PRESS` exists on an access-control command topic
- WHEN the daemon reconnects to MQTT
- THEN the command is ignored
- AND no UART access-control frame is sent

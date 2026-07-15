# Tasks

## 1. Specify the Contract

- [x] 1.1 Define call identity lifetime and terminal cleanup.
- [x] 1.2 Define structured MQTT event fields, retention and discovery.
- [x] 1.3 Preserve the three-value media-state contract and observer-only boundary.

## 2. Implement Correlation

- [x] 2.1 Add the thread-safe production call-session component.
- [x] 2.2 Integrate physical, developer, outgoing SIP, handset and direct SIP transitions.
- [x] 2.3 Preserve cached MQTT call identity and media state on reconnect.

## 3. Add Regression Coverage

- [x] 3.1 Add deterministic `CALL-E2E-01` through `CALL-E2E-07` host flows.
- [x] 3.2 Extend the MQTT harness for `CALL-MQTT-01`.
- [x] 3.3 Expose lifecycle, MQTT and watchdog checks as distinct CI steps.

## 4. Adopt OpenSpec

- [x] 4.1 Initialize the OpenSpec core workflow for Codex.
- [x] 4.2 Add project context and hardware-safety rules.
- [x] 4.3 Add strict pinned OpenSpec validation to GitHub Actions.
- [x] 4.4 Document spec workflow and E2E traceability.

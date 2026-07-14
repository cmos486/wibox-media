# Proposal: Add Correlated Call Observability

## Intent

Make every intercom interaction traceable across UART, SIP, MQTT and Home
Assistant, and protect the supported call workflows with deterministic E2E
regressions.

## Scope

- Add a stable call ID shared by all events in one call flow.
- Publish retained active identity and non-retained structured transition events.
- Preserve cached call identity and media state across MQTT reconnects.
- Cover physical, simulated, outgoing-disabled, handset, SIP-failure and direct-SIP flows.
- Introduce OpenSpec as the versioned contract for this capability.

## Non-goals

- No changes to START_CALL or STOP_CALL decisions.
- No changes to SIP routing, codecs, Asterisk or Home Assistant automations.
- No changes to audio, video, snapshots, UART initialization or OTA behavior.
- No device deployment or firmware flashing.

## Safety

The new call-session module is an observer. Existing daemon code remains the
only authority that controls intercom hardware, media workers and SIP calls.
The Home Assistant `media_state` vocabulary remains unchanged.

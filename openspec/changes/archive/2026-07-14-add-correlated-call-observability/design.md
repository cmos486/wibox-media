# Design: Correlated Call Observability

## Context

One physical interaction crosses UART input, SIP callbacks, MQTT state and Home
Assistant events. Previously those records could not be joined reliably, and
regressions in timeout or cancellation behavior were difficult to distinguish
from separate calls.

## Technical Approach

### Thread-safe session observer

`call_session.c` owns only correlation state: active ID, event sequence, source,
route and timestamps. A boot nonce plus a monotonic per-boot counter makes IDs
unique enough for local diagnostics without persistence or external services.

The daemon calls the observer after making lifecycle decisions. The observer
does not call back into UART, SIP or media control paths.

### MQTT contract

`call/id` is retained because it represents current state. `call/event` is not
retained because it represents an edge. Each event carries enough context for
Home Assistant and logs to correlate it without querying another topic.

MQTT reconnect republishes cached state rather than forcing `idle`; transport
reconnection is not a call transition.

### State compatibility

The public `media_state` remains `idle`, `ringing` or `established`. More precise
outcomes belong in the structured event stream, avoiding state-machine growth
and preserving existing automations.

### E2E strategy

The host E2E harness drives production `call_session` code through seven
deterministic workflow scenarios. The native MQTT harness verifies discovery,
retention and JSON fields. CI presents these as separate steps so failures show
whether the regression is lifecycle, MQTT or unrelated firmware behavior.

## Risks and Mitigations

- Risk: observability changes hardware timing. Mitigation: observer-only API with no hardware handles.
- Risk: repeated rings create duplicate sessions. Mitigation: reuse the active ID until a terminal event.
- Risk: MQTT reconnect clears active state. Mitigation: cache and republish identity and media state.
- Risk: tests overstate hardware coverage. Mitigation: document that host E2E does not emulate analog media or PJSIP.

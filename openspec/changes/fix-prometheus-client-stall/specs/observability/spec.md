# Delta for Observability

## MODIFIED Requirements

### Requirement: Prometheus health and metrics

When enabled, the daemon SHALL expose `GET /healthz` and `GET /metrics` on the
configured port. An idle, incomplete or abandoned TCP client SHALL NOT prevent a
later well-formed request from receiving a response after a bounded delay.

#### Scenario: A monitoring system scrapes the device

- GIVEN Prometheus is enabled
- WHEN `/metrics` is requested
- THEN the daemon returns current metric text

#### Scenario: An idle client precedes a valid scrape

- GIVEN a TCP client has connected to the Prometheus port without sending HTTP
- WHEN another client requests `/healthz`
- THEN the idle client is closed after a bounded wait
- AND the valid request receives a health response

### Requirement: Runtime metric families

Metrics SHALL cover build information, monotonic non-negative uptime, health,
MQTT connectivity, call/SIP/video/ringing state, ring/call/video/unlock counters,
last ring/unlock, WiFi RSSI and UART frame categories.

#### Scenario: The wall clock moves backwards

- GIVEN the exporter started before a wall-clock correction
- WHEN the device wall clock is moved to an earlier value
- THEN `wibox_uptime_seconds` remains non-negative
- AND continues to represent elapsed daemon runtime

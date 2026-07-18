# Design: Fix Prometheus Client Stall

## Context

The exporter accepts and handles clients serially. `handle_client()` currently
performs an unbounded blocking `recv()`. A TCP-only probe or abandoned client can
therefore monopolize the exporter thread while the kernel continues accepting
connections into its backlog, making valid HTTP requests connect but receive no
bytes.

Daemon uptime currently subtracts two wall-clock timestamps. NTP or manual clock
correction can move that clock backwards and expose a negative Prometheus counter.

## Technical Approach

### Bounded client reads

Set `SO_RCVTIMEO` on every accepted client before reading its request. An idle or
incomplete client is closed after the bounded wait, allowing the existing serial
listener to continue. The listener architecture, routes and response formats stay
unchanged.

### Monotonic uptime

Record `CLOCK_MONOTONIC` when the exporter starts and subtract a fresh monotonic
sample while building metrics. Clamp unexpected clock-read failures or backwards
values to zero so the counter never becomes negative.

### Regression strategy

The host integration test first opens an idle TCP connection, then verifies that a
normal `/healthz` request succeeds after the idle-client deadline. It also moves a
wrapped wall clock backwards and verifies that exported uptime remains
non-negative.

## Risks and Mitigations

- Risk: a very slow legitimate request is disconnected. Mitigation: normal HTTP
  requests fit in one small packet and are sent immediately by monitoring clients.
- Risk: monotonic clock APIs vary on the target libc. Mitigation: compile the ARM
  target and validate the development binary on the WiBox before handoff.
- Risk: deployment disturbs active media. Mitigation: use the existing volatile
  development deployment workflow and verify daemon health immediately afterward.

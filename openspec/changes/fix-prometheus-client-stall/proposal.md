# Proposal: Fix Prometheus Client Stall

## Intent

Keep the Prometheus exporter responsive when a TCP client connects but does not
send a complete HTTP request, and make daemon uptime immune to wall-clock
corrections.

## Scope

- Bound how long the single exporter thread waits for an accepted client request.
- Continue serving later clients after an incomplete or idle connection times out.
- Measure daemon uptime with a monotonic clock.
- Add host regressions for an idle TCP client and a backwards wall-clock change.
- Validate the development build on a running WiBox after explicit user approval.

## Non-goals

- No new metric families or HTTP routes.
- No authentication, TLS or Internet exposure changes.
- No change to MQTT, SIP, media, UART or hardware-control behavior.
- No firmware flashing or persistent image update.

## Safety

The change is confined to the read-only observability listener and its tests. The
development deployment uses the existing volatile runtime workflow and does not
write firmware partitions.

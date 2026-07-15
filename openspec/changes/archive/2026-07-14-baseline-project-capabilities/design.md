# Design: Full Project Capability Baseline

## Context

The initial OpenSpec change covered call correlation only. Because this is the
project's first specification baseline, future work also needs reliable context
for the rest of the shipped product.

## Technical Approach

The baseline is divided by user-visible or operational capability rather than C
module. Cross-cutting behavior is referenced from the capability that owns the
contract: call identity belongs to call lifecycle, retained command safety to
MQTT, and flash/watchdog coordination to firmware updates and resilience.

`openspec/coverage.txt` is the canonical capability inventory. A small host
script compares it bidirectionally with the spec directories and confirms each
capability appears in the human coverage matrix. OpenSpec strict validation
continues to check requirement/scenario structure.

## Coverage Boundary

The baseline includes the current branch, including the call-correlation change
under review. It excludes external infrastructure and code removed from the
repository. Reverse-engineering documents can support a requirement but do not
become a requirement by themselves.

## Validation Model

Host coverage is used for deterministic state, MQTT, defaults and packaging.
WiBox validation remains required for UART timing, analog audio/video, RTSP/SIP
interop, watchdog reboot and MTD flashing.

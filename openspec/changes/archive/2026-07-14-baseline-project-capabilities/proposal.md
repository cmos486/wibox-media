# Proposal: Baseline All Project Capabilities

## Intent

Establish the first OpenSpec adoption as a complete contract for every stable
feature currently implemented or packaged by WiBox Media.

## Scope

- Inventory runtime, hardware, integration, update, installation and release capabilities.
- Add one behavior-first specification per capability.
- Record automated versus real-device validation boundaries.
- Add a CI guard that prevents capability specs from disappearing silently.

## Non-goals

- No code, firmware, hardware-control or configuration behavior changes.
- No specification of external Home Assistant automations, PBX configuration or SIP clients.
- No promotion of reverse-engineering hypotheses or removed experiments to supported behavior.

## Safety

The baseline is derived from current documentation, packaged defaults, public
module contracts, MQTT discovery, boot scripts, updater behavior and existing
tests. Hardware-dependent requirements retain explicit WiBox validation rather
than claiming host tests emulate the device.

# Specifications and E2E Traceability

WiBox Media uses [OpenSpec](https://openspec.dev/) for behavior that benefits
from an explicit, reviewable contract. OpenSpec is development tooling only. It
is not packaged in the firmware and is not a runtime dependency of the daemon.

## Repository Layout

- `openspec/specs/` contains accepted behavior, grouped by capability.
- `openspec/changes/` contains proposed work and archived change history.
- `openspec/config.yaml` records project constraints that apply to new changes.
- `.codex/skills/` contains the generated Codex OpenSpec workflow skills.

Do not attempt to describe the entire legacy firmware retrospectively. Add or
extend a capability when real work touches that behavior.

## Workflow

Use the OpenSpec commands from a Codex chat:

```text
/opsx:explore
/opsx:propose <change-name>
/opsx:apply
/opsx:sync
/opsx:archive
```

The CLI validates artifacts locally with:

```bash
npm exec --yes --package=@fission-ai/openspec@1.6.0 -- \
  openspec validate --all --strict --no-interactive
```

The `OpenSpec` GitHub Actions workflow runs the same pinned validation whenever
the specifications or generated Codex integration change.

## Complete Product Capability Baseline

The first OpenSpec adoption intentionally establishes a complete baseline of
the current repository rather than starting with one feature. The machine-
checked capability list is `openspec/coverage.txt`.

| Capability | Contract | Primary evidence or verification |
| --- | --- | --- |
| `access-control` | Door unlock, F1 and physical call-forward control | MQTT host test plus WiBox UART validation |
| `audio-media` | Direct GADI PCMA audio, tuning and shared SIP/RTSP ownership | WiBox media validation |
| `boot-and-runtime` | Boot, WiFi, SSH, Sofia warmup, daemon ownership and persistence | Image verification plus device boot |
| `build-and-release` | Reproducible build, host tests, image invariants and release assets | GitHub Actions firmware and host jobs |
| `call-lifecycle` | Correlated call states, events and terminal outcomes | `tests/call_flow_e2e.c` |
| `configuration` | Defaults, persistent file and retained runtime overrides | Defaults regression plus MQTT host test |
| `developer-controls` | Developer mode, simulated DING/handset and local FIFO diagnostics | MQTT host test, call E2E and WiBox diagnostics |
| `firmware-updates` | HTTPS release checks, guarded MTD flash and read-back verification | Image checks plus controlled WiBox OTA |
| `installation-recovery` | First install, backups, persistent WiFi and recovery levels | Documented operator procedure |
| `intercom-uart` | Known frames, commands, raw telemetry and call-forward state | WiBox UART validation |
| `mqtt-home-assistant` | Native MQTT, discovery, retention and command safety | `tests/mqtt_native_mock.py` |
| `network-services` | LAN endpoints, authentication boundaries and exposure policy | Image and device endpoint validation |
| `observability` | Prometheus, health, logs, metadata and event telemetry | Device endpoint validation |
| `rtsp-streaming` | RTSP/TCP, optional Basic auth and shared media engines | WiBox plus RTSP client validation |
| `runtime-resilience` | Process supervisor, hardware watchdog, OTA guard and safe reboot | Watchdog regression, image checks and device validation |
| `sip-calling` | Incoming/outgoing SIP, SDP, timeout and media attachment | Call E2E plus real SIP validation |
| `snapshots` | Manual, ring and concurrent-stream JPEG capture | MQTT host test plus WiBox image validation |
| `video-media` | D1 H.264 worker, encoder policy, sinks and recording limits | Firmware build plus WiBox video validation |

`scripts/verify_openspec_coverage.sh` compares this inventory with every
`openspec/specs/*/spec.md` directory. CI fails for a missing, undeclared or
undocumented capability.

## Call Lifecycle Scenario Traceability

| Scenario | Required flow |
| --- | --- |
| `CALL-E2E-01` | Physical ring -> outgoing SIP -> established -> door opened -> remote hangup |
| `CALL-E2E-02` | Physical ring with outgoing SIP disabled -> generic ringing timeout |
| `CALL-E2E-03` | Physical handset answer -> cancel remote ringing |
| `CALL-E2E-04` | Developer DING -> simulated physical handset answer |
| `CALL-E2E-05` | Repeated physical ring reuses the call ID -> UART hangup |
| `CALL-E2E-06` | Immediate SIP make-call failure -> generic ringing timeout |
| `CALL-E2E-07` | Direct incoming SIP -> established -> end |
| `CALL-MQTT-01` | Discovery, retained active call ID and non-retained structured call event |

Host tests are deterministic software regressions. They do not emulate PJSIP,
analog media hardware, SPI flash behavior or the physical portal. Requirements
that depend on those boundaries explicitly retain a WiBox validation step.

## Baseline Boundaries

The baseline covers functionality present in this repository and packaged by
the current image. It does not claim ownership of external Home Assistant
automations, Asterisk configuration, SIP provider services or client apps.
Historical video-denoise experiments and video-tuning scripts are not specified
because they are not present in the current branch. Reverse-engineering notes
remain evidence, not guaranteed product behavior, until a feature adopts them.

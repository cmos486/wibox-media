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

## Call Lifecycle Coverage

The current `call-lifecycle` capability is exercised by
`tests/call_flow_e2e.c` and the MQTT contract is exercised by
`tests/mqtt_native_mock.py`.

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

These tests are deterministic host-level regressions. They do not emulate the
PJSIP stack, analog media hardware or the physical portal, so hardware-facing
changes still require an explicitly approved device validation.

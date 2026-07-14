# Testing strategy

WiBox Media uses a layered test strategy because host CI cannot reproduce the
GK7102S analog panel, Goke encoder or MTD character device faithfully.

## Required host gates

`make test` runs protocol, state-machine, network integration, hardware mock,
updater fault-injection, supervisor sandbox and production wiring contracts.

`make coverage` measures line and branch coverage per critical C module. The
gate is per module so a large, well-covered parser cannot hide an untested
updater or hardware adapter.

`make test-spec-coverage` compares every OpenSpec requirement/scenario with
`openspec/test-coverage.tsv`. Adding, renaming or deleting a scenario without
updating its concrete evidence fails CI.

`make build-media` cross-compiles the exact ARM daemon and catches integration
or ABI errors that host mocks cannot detect.

## Evidence levels

`host` executes behavior on the CI runner with real sockets, threads and files,
plus narrow mocks only at hardware boundaries.

`contract` inspects the packaged boot/release/runtime wiring and fails when the
tested modules are no longer connected to production paths.

`device` is required for analog audio, real panel state, D1 image validity,
first installation and reboot-after-flash. Run it explicitly with
`WIBOX_DEVICE_TEST=1 tests/device_acceptance.sh <mode>`. It never deploys or
flashes firmware.

The traceability catalog is `openspec/test-evidence.tsv`. Scenarios that can
only be proven on the physical device remain visibly device-gated rather than
being presented as covered by an unrealistic host mock.

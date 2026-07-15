# Build and Release Specification

## Purpose

Define reproducible firmware construction, regression checks, image contents and
published release artifacts.

## Requirements

### Requirement: Pinned build environment

Firmware builds SHALL use the project Docker build tool so cramfs, zlib and the
ARM11/uClibc toolchain remain compatible with the device.

#### Scenario: A contributor builds firmware

- GIVEN the Docker build tool exists
- WHEN `make build` runs
- THEN the base image is prepared, runtime binaries are built, patches are applied
- AND a cramfs image is written under `release/`

### Requirement: Host regression suite

`make test` SHALL run deterministic MQTT integration, call-flow E2E and watchdog
default regressions without requiring a WiBox.

#### Scenario: Host CI runs

- GIVEN a pull request changes runtime behavior
- WHEN the Host tests job runs
- THEN MQTT contracts, required call flows and watchdog defaults are checked

### Requirement: Firmware image invariants

Image verification SHALL require the daemon, updater, app watchdog, default
config, release metadata, boot script and required libraries. It SHALL verify
the packaged daemon checksum and safe defaults.

#### Scenario: A required artifact is missing

- GIVEN a generated image lacks a required runtime artifact
- WHEN image verification runs
- THEN verification fails before release

### Requirement: Legacy artifact exclusion

The image SHALL exclude legacy listeners, web runtime scripts, shell MQTT
clients, SSH client tools, standalone media bridges, compatibility SIP binaries
and shell updater wrappers.

#### Scenario: A legacy binary is accidentally packaged

- GIVEN an excluded artifact appears in the cramfs tree
- WHEN image verification runs
- THEN verification fails and identifies the artifact

### Requirement: Development deployment is non-persistent

`deploy-runtime` SHALL place a test daemon in `/tmp` and run it without writing
`mtd4`, so reboot restores the packaged runtime.

#### Scenario: A development runtime is deployed

- GIVEN custom firmware and SSH are available
- WHEN `make deploy-runtime` runs
- THEN the test binary runs from volatile storage
- AND no firmware flash is performed

### Requirement: Release assets

Release Please SHALL own version/changelog changes, and a published GitHub
Release SHALL include the versioned image, `MD5SUMS` and `SHA256SUMS`.

#### Scenario: A release is published

- GIVEN the release PR is merged and tagged
- WHEN the firmware workflow completes
- THEN the three required release assets are attached
- AND the on-device updater can locate the image and MD5 entry

### Requirement: Generated-artifact hygiene

Generated cramfs trees, release images, verification roots and compiled binaries
SHALL remain outside version control.

#### Scenario: Source changes are prepared for review

- GIVEN a local build was performed
- WHEN the change is staged
- THEN generated firmware artifacts are not included in the commit

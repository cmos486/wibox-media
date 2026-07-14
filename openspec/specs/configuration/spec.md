# Configuration Specification

## Purpose

Define built-in defaults, persistent file overrides and safe retained runtime
configuration.

## Requirements

### Requirement: Defaults-first loading

The daemon SHALL initialize every configuration field to a built-in default
before applying file values. A missing config file SHALL leave the daemon usable
with those defaults rather than fail startup.

#### Scenario: A key is omitted

- GIVEN a valid config file omits a supported key
- WHEN configuration is loaded
- THEN the corresponding built-in default remains active

#### Scenario: The config file is missing

- GIVEN no readable runtime config is available
- WHEN the daemon starts
- THEN all built-in defaults are initialized
- AND startup continues

### Requirement: Packaged default parity

`/usr/etc/sip_media.conf.default` SHALL expose the supported persistent keys and
SHALL remain aligned with `config_init_defaults` for production defaults.

#### Scenario: Default configuration is verified

- GIVEN a firmware image is built
- WHEN defaults regression and image verification run
- THEN critical defaults match between code and packaged config

### Requirement: Safe production defaults

Fresh installs SHALL default to outgoing SIP enabled, 60-second call timeout,
video disabled, RTSP disabled, 4096 kbps video bitrate, 2000 ms ring snapshot
delay, recording disabled, MQTT enabled, updates enabled, Prometheus enabled and
hardware watchdog enabled.

#### Scenario: A fresh installation boots

- GIVEN no retained MQTT overrides exist
- WHEN the generated persistent config is loaded
- THEN the device starts with the documented safe production defaults

### Requirement: Runtime MQTT overrides

Retained MQTT configuration SHALL override file values after connection for
video enable, RTSP enable, video bitrate, outgoing SIP enable and target,
outgoing timeout, ring snapshot delay and call-forward state. Accepted values
SHALL be clamped or validated and republished.

#### Scenario: A retained bitrate is replayed

- GIVEN the config file sets one bitrate and MQTT retains another valid bitrate
- WHEN MQTT reconnects
- THEN the retained value becomes the runtime setting
- AND the accepted value is published on the state topic

### Requirement: Runtime ranges

Video bitrate SHALL be constrained to 512 through 4096 kbps, outgoing timeout
to 10 through 120 seconds, and ring snapshot delay to 0 through 5000 ms.
Unsupported free-form video resolution SHALL NOT be exposed.

#### Scenario: A runtime value is out of range

- GIVEN a configuration command exceeds its supported range
- WHEN the daemon applies the command
- THEN it clamps or rejects the value safely
- AND publishes the effective value

### Requirement: Configuration parsing compatibility

Comments, blank lines, whitespace and quoted values SHALL be supported. Known
legacy bridge, shell-MQTT and retry keys SHALL be ignored, and unknown keys
SHALL produce a warning without changing unrelated defaults.

#### Scenario: A legacy config is reused

- GIVEN the file contains an obsolete standalone-bridge key
- WHEN configuration is parsed
- THEN the obsolete key is ignored
- AND supported settings continue to load

### Requirement: Secret logging safety

Configuration diagnostics SHALL NOT print MQTT passwords and SHALL mask a
configured RTSP password.

#### Scenario: Configuration is printed

- GIVEN MQTT and RTSP credentials are configured
- WHEN startup logs the effective configuration
- THEN password values are not emitted in plaintext

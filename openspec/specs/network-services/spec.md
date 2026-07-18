# Network Services Specification

## Purpose

Define the LAN-facing endpoints, configurable ports and security boundaries of
the custom image.

## Requirements

### Requirement: Administrative SSH

The custom image SHALL provide Dropbear SSH on TCP 22 with password or key
authentication and persistent host keys under `/mnt/mtd/dropbear`.

#### Scenario: Custom firmware boots normally

- GIVEN Dropbear is present
- WHEN startup succeeds
- THEN SSH becomes available
- AND temporary boot telnet is stopped

#### Scenario: WiBox enters AP provisioning mode

- GIVEN Dropbear started before final WiFi configuration
- WHEN the AP interface and address are established
- THEN Dropbear is restarted
- AND SSH is reachable through `192.168.111.1`

### Requirement: AP provisioning portal

The firmware SHALL serve a provisioning UI on TCP 80 at
`http://192.168.111.1/` only while `ap_start.sh` owns the WiFi interface. The
portal SHALL be presented entirely in English, read the BusyBox HTTP POST body
directly from standard input, validate SSID and WPA/WPA2 credentials, atomically replace
`/mnt/mtd/wpa_supplicant.conf`, clear the AP-request marker and reboot. When a
saved station configuration exists, the portal SHALL also allow cancelling AP
mode without changing those credentials. AP availability SHALL be indicated by
a slow blue blink; an accepted Save or Cancel SHALL stop that blink and show
green before reboot.

#### Scenario: User opens the provisioning portal

- GIVEN the WiBox is in AP provisioning mode
- WHEN the user opens `http://192.168.111.1/`
- THEN all user-facing setup, validation and result text is in English

#### Scenario: User saves new WiFi credentials

- GIVEN the WiBox is in AP provisioning mode
- WHEN valid SSID and WPA/WPA2 credentials are submitted
- THEN the persistent station configuration is atomically replaced
- AND the AP-request marker is cleared
- AND the status LED changes from blinking blue to green
- AND the WiBox reboots into station mode

#### Scenario: User cancels forced AP mode

- GIVEN the AP was explicitly requested
- AND a saved station configuration exists
- WHEN the user chooses to return to the saved network
- THEN the saved credentials remain unchanged
- AND the AP-request marker is cleared
- AND the status LED changes from blinking blue to green
- AND the WiBox reboots into station mode

#### Scenario: Device is in normal station mode

- GIVEN the WiBox has a station lease
- WHEN LAN services are inspected
- THEN the provisioning HTTP server is not running

### Requirement: SIP and RTP endpoints

The daemon SHALL listen for SIP on configurable UDP port 5060 by default and use
configurable RTP audio port 8000 and video port 8002 by default. SIP/RTP SHALL
be treated as trusted-LAN services without built-in authentication or encryption.

#### Scenario: Video is disabled

- GIVEN `video_enabled=0`
- WHEN SIP SDP is generated
- THEN the audio endpoint remains advertised
- AND the video RTP endpoint is omitted

### Requirement: RTSP endpoint

When enabled, RTSP SHALL listen on configurable TCP port 8554 at `/live` and MAY
require Basic credentials. Basic authentication SHALL be documented as plaintext
LAN protection rather than encryption.

#### Scenario: RTSP credentials are configured

- GIVEN either RTSP username or password is set
- WHEN a client connects without valid Basic authorization
- THEN access is rejected

### Requirement: Prometheus endpoint

When enabled, Prometheus SHALL listen on configurable TCP port 9617 by default
and expose `/metrics` and `/healthz` without application authentication.

#### Scenario: Monitoring is disabled

- GIVEN `prometheus_enabled=0`
- WHEN the daemon starts
- THEN the Prometheus listener is not started

### Requirement: Outbound MQTT

MQTT SHALL be an outbound broker connection, TCP 1883 by default according to
the configured broker, with optional username and password.

#### Scenario: MQTT is disabled

- GIVEN `mqtt_enabled=0`
- WHEN the daemon starts
- THEN no broker connection or Home Assistant discovery is attempted

### Requirement: Trusted-network exposure policy

SIP/RTP, RTSP and Prometheus SHALL NOT be documented or configured for direct
Internet exposure. Operators SHALL use LAN isolation, firewalling or VPN for
remote access.

#### Scenario: Remote access is required

- GIVEN a user needs access outside the home network
- WHEN deployment is designed
- THEN network-level isolation or VPN is used instead of public port forwarding

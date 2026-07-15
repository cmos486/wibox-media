# Audio Media Specification

## Purpose

Define the direct analog audio path and its shared ownership between SIP and
RTSP clients.

## Requirements

### Requirement: Direct PCMA audio

The daemon SHALL own the GADI audio path and exchange PCMA/A-law audio at 8 kHz.
The default frame size SHALL be 160 samples and the default audio-chip enable
line SHALL be GPIO 18.

#### Scenario: SIP audio starts

- GIVEN a SIP call is established
- WHEN the remote PCMA RTP target is known
- THEN the daemon starts direct GADI audio
- AND sends and receives PCMA RTP without a standalone audio bridge process

### Requirement: Shared audio engine

SIP and RTSP SHALL attach independent RTP sinks to one audio engine. Removing
one sink SHALL NOT stop audio while another sink remains.

#### Scenario: SIP ends while RTSP audio remains

- GIVEN SIP and at least one RTSP audio client use the shared engine
- WHEN the SIP call ends
- THEN only the SIP target is detached
- AND RTSP audio continues without restarting the engine

#### Scenario: Last consumer leaves

- GIVEN no SIP audio target remains
- WHEN the last RTSP audio client disconnects
- THEN the daemon stops the shared audio engine

### Requirement: Audio-only operation

Disabling video SHALL NOT disable SIP or RTSP audio.

#### Scenario: Global video is disabled

- GIVEN `video_enabled=0`
- WHEN a SIP call or audio-only RTSP session is established
- THEN PCMA audio remains available
- AND no video worker is required

### Requirement: Conservative analog tuning

The default input gain SHALL be 35 percent, output volume 50 percent and line
mute 900 ms. Line mute SHALL mask open/close transients while RTP timing
continues, and configured mute values above 3000 ms SHALL be clamped.

#### Scenario: The panel line opens

- GIVEN audio starts with the default tuning
- WHEN the analog line is opened
- THEN capture is muted for the configured short interval
- AND RTP timing remains continuous

### Requirement: Fixed-gain processing

The low-level echo/noise processing SHALL remain aligned with the proven Sofia
audio path, while automatic gain control SHALL remain disabled by default to
avoid amplifying short street-noise transients.

#### Scenario: Ambient level changes suddenly

- GIVEN the default audio configuration
- WHEN a short loud transient reaches the input
- THEN the daemon does not automatically increase capture gain

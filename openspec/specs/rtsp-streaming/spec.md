# RTSP Streaming Specification

## Purpose

Define the optional RTSP/TCP stream and its coexistence with SIP and global
video configuration.

## Requirements

### Requirement: Optional listener

RTSP SHALL default to disabled. When enabled, the daemon SHALL listen on the
configured port, default 8554, and serve `rtsp://<device>/live` using RTP over
RTSP/TCP interleaving.

#### Scenario: RTSP is enabled at runtime

- GIVEN the daemon is running with RTSP disabled
- WHEN a retained runtime ON command is accepted
- THEN the RTSP listener starts without rebooting the device

### Requirement: Track advertisement follows global video

RTSP SHALL advertise PCMA audio. It SHALL advertise H.264 video only when
`video_enabled=1`; disabling video SHALL leave a valid audio-only RTSP stream.

#### Scenario: Video is disabled while RTSP remains enabled

- GIVEN RTSP is listening
- WHEN global video becomes OFF
- THEN video capture and advertisement stop
- AND PCMA audio service remains available

### Requirement: Shared media ownership

RTSP SHALL use the same audio engine and D1 H.264 worker as SIP. Client attach
and detach SHALL add or remove sinks without restarting media still used by the
other protocol.

#### Scenario: SIP attaches to active RTSP video

- GIVEN an RTSP video client already consumes the D1 worker
- WHEN SIP video becomes established
- THEN the SIP RTP target is attached to the existing worker
- AND RTSP continues without encoder restart

### Requirement: Idle listener does not open panel

An RTSP client MAY connect while no panel call exists, but RTSP SHALL NOT send
START_CALL solely to obtain a real camera image. Blue/static frames before a
physical ring or established panel context are expected hardware behavior.

#### Scenario: RTSP connects while idle

- GIVEN no physical or daemon-opened panel context exists
- WHEN a video client enters PLAY
- THEN the stream may contain blue/static video
- AND the physical handset path is not seized by RTSP

### Requirement: Client startup recovery

The video path SHALL request an IDR when a new RTSP client attaches so the
decoder can start without enabling periodic IDR spam by default.

#### Scenario: A new client enters PLAY

- GIVEN the H.264 worker is already active
- WHEN a new RTSP video client attaches
- THEN SPS/PPS and an IDR become available for decoder startup

### Requirement: Optional Basic authentication

Setting an RTSP username or password SHALL enable Basic authentication. The
feature SHALL be treated as accidental-access control on a trusted LAN, not as
encrypted transport.

#### Scenario: Authentication is not configured

- GIVEN both credential fields are empty
- WHEN a LAN client connects
- THEN no RTSP Basic challenge is required

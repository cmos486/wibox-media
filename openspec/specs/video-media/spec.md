# Video Media Specification

## Purpose

Define the proven D1 H.264 capture path, encoder policy, shared sinks and safe
recording limits.

## Requirements

### Requirement: Global video capability

`video_enabled` SHALL default to OFF and SHALL govern SIP advertisement, RTSP
video and all capture. Turning video off SHALL stop video use without disabling
audio.

#### Scenario: Fresh installation starts

- GIVEN no file or retained override enables video
- WHEN the daemon starts
- THEN no video is advertised or captured
- AND audio-only calls remain functional

### Requirement: Proven D1 main stream

Production H.264 SHALL use encoder stream 0 at `688x576` and inspect wildcard
VENC results so only stream 0 reaches main video sinks. Resolution SHALL not be
free-form configurable.

#### Scenario: Main encoder emits multiple stream IDs

- GIVEN wildcard stream retrieval returns a non-main stream
- WHEN the worker processes it for H.264 output
- THEN it does not send that stream as D1 main video

### Requirement: One video worker with dynamic sinks

One worker SHALL own `/dev/gk_video` and the D1 encoder. SIP and RTSP SHALL add
or remove RTP sinks dynamically, and the worker SHALL stop only when neither
protocol needs it.

#### Scenario: Last video consumer leaves

- GIVEN no SIP target remains
- WHEN the final RTSP video client disconnects
- THEN the worker stops and releases video hardware

### Requirement: Sofia boot warmup dependency

Until full VI/sensor initialization is reproduced, Sofia SHALL run once at boot
to warm video hardware and SHALL not be invoked per call.

#### Scenario: A later call starts video

- GIVEN boot warmup completed successfully
- WHEN SIP or RTSP requests D1 video
- THEN the in-daemon worker starts without launching Sofia again

### Requirement: H.264 packet startup

The worker SHALL parse all Annex-B NAL units, cache SPS/PPS and send them before
the first IDR needed by a sink. It SHALL force IDR at startup and client attach.

#### Scenario: First encoder buffer contains SPS, PPS and IDR

- GIVEN all three NAL types share one VENC buffer
- WHEN the worker parses the buffer
- THEN none are discarded
- AND the receiving decoder can start from that keyframe

### Requirement: Conservative encoder defaults

The default bitrate SHALL be 4096 kbps, clamped to 512 through 4096; GOP-N 25,
IDR interval 1, BRC mode 0 and periodic RTSP IDR 0 SHALL be the stable defaults.

#### Scenario: A new client attaches with periodic IDR disabled

- GIVEN natural GOP operation is active
- WHEN the client attaches
- THEN one attach-time IDR is requested
- AND periodic IDR spam remains disabled

### Requirement: Real panel image dependency

Valid H.264 may exist while the analog panel path is closed, but real camera
content SHALL require a physical ring or a daemon-owned START_CALL context.

#### Scenario: Encoder runs while panel is closed

- GIVEN no panel context is open
- WHEN H.264 capture runs for idle RTSP
- THEN blue/static content is not treated as a decoder failure

### Requirement: Flash-safe optional recording

File recording SHALL default to disabled, use a bounded duration and default to
`/tmp/wibox-last-call.h264`. Documentation SHALL prohibit recording to persistent
flash-backed storage.

#### Scenario: Recording is enabled for diagnosis

- GIVEN an operator explicitly enables bounded recording
- WHEN a call captures at high bitrate
- THEN output is limited by configured duration/bytes
- AND the default destination remains volatile storage

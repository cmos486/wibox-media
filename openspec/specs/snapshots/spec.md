# Snapshots Specification

## Purpose

Define safe JPEG capture for manual requests, real rings and developer
simulation while preserving active H.264 streams.

## Requirements

### Requirement: Video capability prerequisite

Snapshots SHALL require `video_enabled=1`. The Home Assistant Take Snapshot
button SHALL be unavailable while video is disabled or another capture runs.

#### Scenario: Snapshot is requested with video disabled

- GIVEN global video is OFF
- WHEN a manual snapshot command arrives
- THEN capture does not start
- AND snapshot availability remains offline

### Requirement: Manual cold snapshot

When no video worker or physical ring context is active, a manual snapshot SHALL
open a temporary panel context, capture MJPEG from stream 0 at D1 `688x576`, and
close the context after capture.

#### Scenario: User takes an idle snapshot

- GIVEN video is enabled and no H.264 worker is active
- WHEN Take Snapshot is pressed
- THEN START_CALL opens the real panel image path
- AND a D1 JPEG is captured
- AND the temporary context is closed afterward

### Requirement: Physical-ring snapshot

A real `ALARM_REPORT` SHALL schedule an automatic snapshot after the configured
0 through 5000 ms delay. That capture SHALL assume the panel opened video and
SHALL NOT send START_CALL or STOP_CALL.

#### Scenario: Visitor presses the physical portal

- GIVEN video is enabled
- WHEN a real panel ring is received
- THEN the daemon waits the configured ring delay
- AND captures without changing panel call ownership

### Requirement: Developer-ring snapshot

A simulated DING SHALL use the same automatic snapshot workflow but SHALL add a
temporary panel context because no physical portal opened the analog video path.

#### Scenario: Developer DING captures an image

- GIVEN Developer Mode and video are enabled
- WHEN DING simulation runs
- THEN the normal ring snapshot timing is used
- AND the simulation opens and later closes its temporary context

### Requirement: Concurrent snapshot stream

When SIP or RTSP H.264 is active on stream 0, snapshot capture SHALL use MJPEG
stream 2 at `352x288` through the existing worker so there is never a second
`/dev/gk_video` owner.

#### Scenario: Snapshot is taken during RTSP video

- GIVEN the D1 H.264 worker is serving RTSP
- WHEN a snapshot is requested
- THEN RTSP remains active
- AND stream 2 supplies the JPEG
- AND the main encoder is not restarted

### Requirement: Settled JPEG publication

The capture path SHALL discard unsuitable small startup JPEGs and publish the
selected JPEG as base64 on `snapshot/image` for the Home Assistant image entity.

#### Scenario: Initial encoder frames are incomplete

- GIVEN early JPEG frames are below the acceptance threshold
- WHEN capture starts
- THEN those frames are skipped
- AND only a valid settled JPEG is published

### Requirement: Single capture at a time

The daemon SHALL serialize snapshot work, publish availability offline while it
runs and restore availability according to global video state on completion.

#### Scenario: A second request arrives during capture

- GIVEN snapshot availability is offline because capture is active
- WHEN another action command arrives
- THEN no competing video owner or second capture is started

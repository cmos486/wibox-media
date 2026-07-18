# Architecture

This is the current production shape of the custom WiBox image.

## Boot Flow

```text
/etc/init.d/rcS
  -> mounts /usr from mtd4 cramfs
  -> mounts /mnt from mtd5 jffs2
  -> /usr/run.sh
       -> GPIO and LED setup
       -> kernel modules
       -> Dropbear SSH
       -> short Sofia warmup for video hardware
       -> WiFi mode selection
            -> station manager with bounded retry/backoff
            -> or AP + DHCP + provisioning portal
       -> app_watchdog.sh wibox-media-daemon /usr/bin/wibox-media-daemon
       -> optional /mnt/mtd/post.sh
```

Sofia is still used once per boot to initialize hardware state that the D1 video
capture path depends on. It is stopped after the warmup. It is not used per
call.

Station credentials select station mode even when the router is temporarily
unavailable. The station manager retries without rebooting and restarts network
consumers after a later lease. Missing credentials or the persistent
`wifi_ap_requested` marker selects AP mode. The physical WiFi button's observed
`CMD_DOWN_LONG_1`/`CMD_DOWN_LONG_2` sequence creates that marker; saving or
cancelling in the portal clears it. The legacy direct `STA_TO_AP` frame remains
supported for compatible MCU revisions.

## Runtime Ownership

`wibox-media-daemon` is the only packaged media runtime.

It owns:

- intercom serial state on `/dev/ttySGK1`;
- SIP signaling on UDP `5060` by default;
- PCMA RTP audio on UDP `8000` by default;
- optional H.264 RTP video on UDP `8002` by default;
- direct GADI audio hardware setup and teardown;
- D1 video worker lifecycle and sink attachment;
- DTMF door unlock from RTP telephone-event and SIP INFO;
- MQTT/Home Assistant discovery, commands and state;
- firmware update checks and install requests;
- Prometheus `/metrics` and `/healthz`.

## Call Flow

Doorbell-originated call:

```text
/usr/bin/wibox-media-daemon boot
  -> FB 19 01 25 to /dev/ttySGK1 to enable physical call forwarding
/dev/ttySGK1 ALARM_REPORT
  -> media/state = ringing
  -> optional SIP INVITE to outgoing_call_target when sip_outgoing_call_enabled=1
  -> SIP established
  -> START_CALL to /dev/ttySGK1
  -> audio RTP starts
  -> video worker starts or attaches SIP if negotiated and video_enabled=1
  -> media/state = established
```

Hangup/timeout:

```text
SIP BYE, SIP failure, HANG_UP or PHYSICAL_HANDSET_ANSWERED
  -> SIP video target is detached
  -> video worker stops only when no RTSP clients remain
  -> SIP audio target is detached
  -> audio engine stops only when no RTSP clients remain
  -> STOP_CALL when an established panel context exists
  -> media/state = idle
```

## Media Runtime

The RTSP server and the video worker are separate responsibilities:

```text
wibox-media-daemon
  -> RTSP server listens on :8554 while enabled
  -> one video worker owns /dev/gk_video and the GADI encoder
```

The RTSP server accepts clients while idle. It does not open the camera by
itself. `rtsp_enabled` only controls the listener. `video_enabled` is the global
video capability flag: when it is `0`, RTSP advertises audio-only and the video
worker is never started; when it is `1`, the first RTSP video client reaching
`PLAY` starts the same video worker used for SIP video and tees H.264 RTP into
RTSP.

There are not separate "SIP" and "preview" video workers. There is one worker
with dynamic sinks:

```text
stream_id=0 H.264 D1 688x576
  -> SIP RTP sink, when a SIP call is established
  -> RTSP sink, when RTSP clients are connected

stream_id=2 MJPEG 352x288
  -> snapshot capture, when requested
```

If RTSP/go2rtc is already connected and a SIP call is established, the daemon
attaches the SIP RTP target to the existing worker with a control command. When
the SIP call ends, the SIP RTP target is cleared and the worker keeps feeding
RTSP. The worker is stopped only after the last RTSP client disconnects and no
SIP target is attached.

Audio is managed by the daemon, not by the video worker. RTSP clients that set
up the audio track start the shared audio engine and receive PCMA RTP packets.
When a SIP call is established, the SIP audio RTP target is attached to that
same engine; when the call ends, only the SIP target is cleared. The audio
engine stops after the last RTSP client disconnects and no SIP target remains.
This also supports audio-only RTSP when `rtsp_enabled=1` and `video_enabled=0`.

Door unlock:

```text
DTMF # or MQTT door/open/set=PRESS
  -> FB 12 01 1E to /dev/ttySGK1
  -> door/unlocked pulses ON then OFF
```

Auxiliary F1:

```text
MQTT f1/trigger/set=PRESS
  -> FB 17 01 23 to /dev/ttySGK1
  -> 500 ms delay
  -> FB 17 00 22 to /dev/ttySGK1
```

F1 is an optional Fermax auxiliary relay function. It is exposed as a Home
Assistant button because the daemon has no reliable feedback state for the
physical relay.

## MQTT / Home Assistant

The daemon contains a small native MQTT 3.1.1 client. It does not package or
spawn `mosquitto_pub` or `mosquitto_sub`.

Default base topic:

```text
wibox/<hostname>
```

Primary entities:

```text
button.open_door
button.f1_function
sensor.media_state
sensor.firmware_version
sensor.firmware_commit
sensor.firmware_build_timestamp
binary_sensor.door_unlocked
sensor.wifi_rssi
switch.video_enabled
switch.rtsp_enabled
binary_sensor.firmware_update_available
sensor.firmware_update_version
button.firmware_update_refresh
button.firmware_update_install
```

`media_state` is the high-level state for automation:

```text
idle
ringing
established
```

Older intermediate sensors such as `call_active`, `sip_call_active`,
`video_active`, `last_ring` and `last_unlock` are intentionally cleared from
retained MQTT discovery/state.

## Firmware Updates

Routine updates are handled by `/usr/bin/firmware_update`.

`wibox-media-daemon` checks GitHub releases at startup and roughly once per day.
Home Assistant can force a check with `Firmware Update Refresh` and start an
install with `Firmware Update Install`.

The install button is disabled unless an update is available. After an install
request is accepted, the daemon immediately disables the button and ignores
duplicate install requests while the updater is running.

## Prometheus

When enabled, the daemon listens on port `9617` by default:

```text
GET /healthz
GET /metrics
```

Metrics include build metadata, uptime, MQTT connection state, media state,
ring/unlock/call counters, video state and WiFi RSSI.

## LED Policy

LEDs are owned by the boot and network-mode scripts, not the daemon:

```text
red    booting or station association/DHCP retry
off    WiFi setup in progress
green  WiFi associated and DHCP succeeded
blue   production station boot complete and daemon started
blue blinking slowly  AP provisioning and web portal available
```

An accepted portal Save or Cancel stops the AP blink and shows green until the
controlled reboot begins.

`gpio.sh` also initializes board lines that must be ready before media startup,
including the audio chip enable line on GPIO 18.

## Persistent Files

```text
/mnt/mtd/wpa_supplicant.conf   WiFi station config, required before first flash
/mnt/mtd/sip_media.conf        daemon runtime config
/mnt/mtd/dropbear/             SSH host keys
/mnt/mtd/post.sh               optional local boot hook
```

`/tmp` and `/var` are RAM-backed and disappear on reboot.

## Build Artifacts

The build starts from `mtd4`, extracts it into `cramfs/`, applies scripts from
`scripts/`, copies `include/`, then packs `release/latest`.

The production image should contain:

```text
/usr/bin/wibox-media-daemon
/usr/bin/firmware_update
/usr/bin/app_watchdog.sh
/usr/run.sh
/usr/etc/sip_media.conf.default
/usr/etc/wibox-release
```

It should not contain legacy listener scripts, web UI runtime scripts,
`mosquitto_*`, `ipctool`, SSH client tools, `audio_bridge`,
`video_rtp_bridge`, `sip_media`, or updater shell wrappers.

The watchdog rotates `/var/log/wibox-media-daemon.log` to
`/var/log/wibox-media-daemon.log.old` once it grows beyond 100 KB. `/var` is a
RAM filesystem on the WiBox, so logs do not consume flash.

# About this fork (`cmos486/wibox-media`)

A fork of [`segator/wibox-media`](https://github.com/segator/wibox-media) that
tracks upstream and adds a few features + fixes, and ships them to the device
over the air.

## What this fork adds on top of upstream

- **ONVIF audio backchannel** in the RTSP server (`src/sip_media/rtsp_stream.c`):
  a 3rd `sendonly` PCMA track (`trackID=2`) so go2rtc / Home Assistant can send
  microphone audio to the WiBox speaker — i.e. two-way audio without SIP/TURN.
- **AEC (echo cancellation) works from source**: the proprietary
  `audio_process.h` ABI was recovered from the official binary's DWARF, so
  echo cancellation initializes correctly in locally-built firmware (it did not
  before — it failed with `-2021`).
- **go2rtc / WebRTC interop fixes** so the RTSP stream works cleanly with
  go2rtc-backed clients: reply `461` to UDP-only SETUP (VLC and other UDP-first
  clients fall back to TCP), a video-worker lifecycle reconcile (go2rtc's
  probe+reconnect no longer leaves the stream stuck with no worker), and correct
  RTP parsing + reframing of the WebRTC microphone audio into the backchannel.
- **A jitter buffer on the microphone path** so the voice heard at the panel is
  not chopped: WebRTC delivers audio in bursts, and the AO write neither blocks
  for the frame duration nor paces itself, so playing straight off the socket
  ran dry constantly. A dedicated thread now drains a 200 ms ring on its own
  8 kHz clock (measured: 117 buffer underruns in 30 s before, 0 after).
- **The VDS bus line follows the RTSP viewers** (`rtsp_intercom_line_enabled`):
  the MCU only bridges the outdoor panel onto the module while an intercom call
  line is up, so without it a camera card shows the blue no-signal screen and
  hears only the noise floor. The daemon now opens the line while clients are
  connected and releases it afterwards, bounded by an exponential backoff when
  the MCU refuses it and by a hard `rtsp_intercom_line_max_seconds` cap so a
  shared bus is never held indefinitely.
- **Periodic work is scheduled on a monotonic clock**: the device has no RTC and
  the first NTP sync can step the wall clock backwards by hours, which silently
  froze the NAT keep-alive and both reconcile loops until it caught up.
- **A batch of correctness/robustness/security fixes** found by an in-depth
  review (memory safety, threading/locking, RTSP DoS + slot lifecycle, MQTT and
  video-worker races). Most are also proposed upstream as PRs.
- **RTSP authentication** guidance — see [SECURITY.md](SECURITY.md).

## What was worked out along the way

Much of the above exists because of behaviour that is documented nowhere and had
to be established by experiment, on a bench built from a real Fermax
installation. Written up separately, with the raw logs kept:

- **[The VDS bus, explained](docs/vds-bus.md)** - addressing, why a wrong address
  fails completely silently, how to set one without the pairing procedure (which
  does not work on every monitor), and why audio and video need an auto
  switch-on.
- **[Sharing the bus with the original intercom](docs/coexistence.md)** - what
  happens to the household's own monitor while this module is using the bus.
  Tested, including the case that looked dangerous: opening the camera while
  somebody is mid-conversation.
- **[UART codes](docs/codes.md)** - the stock firmware's complete command set,
  recovered from its binary, with the checksum proved.

## Home Assistant two-way audio (see + talk + open door, local and remote)

A full self-hosted "answer the door" setup — video, talk-back and door from
tablets at home and your phone away — using go2rtc + WebRTC and the companion
[`wibox-intercom-video-card`](https://github.com/cmos486/wibox-intercom-video-card).
Step-by-step guide: **[docs/homeassistant-two-way-audio.md](docs/homeassistant-two-way-audio.md)**.

## Branch and version model

- `custom` — our line of work: upstream release + the changes above. **All work
  lands here.**
- Tags `vX.Y.Z` — each fork release. The version string is baked into the image
  at build time (`WIBOX_VERSION`), so the device reports the real version and OTA
  version comparison behaves.

## Building

The build needs the private base image `wibox-build:latest` (ARM toolchain +
PJProject + the proprietary Goke GK710X SDK bits). It is **not** public; it is
reconstructed locally (see `~/wibox/wibox-build-base/` and `~/wibox/BUILD-NOTES.md`).
Because of that proprietary dependency the build runs locally, not in public CI.

    bash ~/wibox/build-firmware.sh            # base image + firmware -> repo/release/latest
    # or, for a versioned build:
    sg docker -c "WIBOX_VERSION=v0.18.9 make build"

## Cutting a release (tag + build + publish)

One command (see `~/wibox/fork-release.sh`):

    GH_TOKEN=ghp_xxx bash ~/wibox/fork-release.sh v0.18.9

It tags `custom`, builds with the version baked in, and uploads
`wibox-media-v0.18.9.img` + `MD5SUMS` + `SHA256SUMS` to the release.

## Updating the device (OTA)

The device's `/mnt/mtd/sip_media.conf` has `firmware_update_repo=cmos486/wibox-media`,
so it pulls releases from this fork. Trigger the update **detached** so a WiFi
drop mid-flash cannot kill it (the flash itself is local, only the download needs
the network):

    ssh root@<device> \
      'setsid nohup sh -c "firmware_update --force --no-reboot >/tmp/ota.log 2>&1; echo done" </dev/null >/dev/null 2>&1 &'
    # wait for /tmp/ota.log to show "flash verification OK", then: reboot

> Do **not** drive `firmware_update` as a foreground SSH child — WiFi drops
> during the flash and would SIGHUP-kill it mid-write. `setsid` avoids that.

## Flashing an image by hand

`firmware_update` can also install a specific image instead of whatever GitHub
says is latest - needed when the device cannot reach GitHub, and the only way to
test a build that is not a release yet:

    # copy it across (the device has no scp)
    ssh root@<device> 'cat > /tmp/fw.img' < wibox-media-vX.Y.Z.img
    ssh root@<device> 'md5sum /tmp/fw.img'      # must match MD5SUMS

    # flash, detached, without rebooting
    firmware_update --image /tmp/fw.img --expected-md5 <md5> --no-reboot

Wait for `flash verification OK` in the log, then reboot. The network drops
during the flash, so drive this from the serial console, or detach it with
`setsid` and read `/tmp/ota.log` over serial afterwards.

## What lives outside this repository

Some things this fork depends on are deliberately **not** in git. If you have
only the repo, you can read and change the code and the docs, but you cannot
build or release:

| What | Where | Why it is not here |
|---|---|---|
| Base build image `wibox-build:latest` | `~/wibox/wibox-build-base/`, notes in `~/wibox/BUILD-NOTES.md` | Contains the proprietary Goke GK710X SDK. Not redistributable, which is also why there is no public CI. |
| `fork-release.sh`, `fork-push.sh`, `fork-about.sh` | `~/wibox/` | Take a GitHub token from the environment. |
| Device backups (7 partitions, md5-verified) | `~/wibox/backup-<date>/` with a MANIFEST | Contains per-device identifiers. |
| Bench and debug helpers (`lab-*.sh`) | `~/wibox/` | Throwaway tooling for a test rig, and they carry device credentials. |
| Device IP, SSH and RTSP credentials | Not written down in git | Obvious reasons. |

What they encode that *does* matter is written up in the docs instead - the
[UART codes](docs/codes.md), [the VDS bus](docs/vds-bus.md) and
[coexistence](docs/coexistence.md) pages exist so the knowledge survives the
scripts.

## Rebasing onto a new upstream release

When upstream tags a new version:

    cd ~/wibox/repo
    git fetch origin --tags
    git checkout custom
    git rebase <new-upstream-tag>        # our footprint is small; conflicts are rare
    # rebuild + release as above, bumping the tag

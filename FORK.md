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
- **A batch of correctness/robustness/security fixes** found by an in-depth
  review (memory safety, threading/locking, RTSP DoS + slot lifecycle, MQTT and
  video-worker races). Most are also proposed upstream as PRs.
- **RTSP authentication** guidance — see [SECURITY.md](SECURITY.md).

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

## Rebasing onto a new upstream release

When upstream tags a new version:

    cd ~/wibox/repo
    git fetch origin --tags
    git checkout custom
    git rebase <new-upstream-tag>        # our footprint is small; conflicts are rare
    # rebuild + release as above, bumping the tag

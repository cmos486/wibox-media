# Hardware resilience

This document records the conservative runtime safeguards derived from the
Sofia reference implementation. They do not reproduce Sofia's application
structure or alter the normal media flow.

## Intercom reopen guard

Sofia leaves approximately three seconds between a close and a subsequent
open of the intercom channel. WiBox Media exposes the equivalent safeguard as:

```ini
intercom_reopen_guard_ms=0
```

The default is deliberately disabled to preserve released behaviour. Values
are clamped to 0-5000 ms. The guard is evaluated only when WiBox Media is about
to send `START_CALL` after a confirmed local `STOP_CALL` or an incoming
`HANG_UP_0`/`HANG_UP_1`.

The physical ring snapshot path does not send `START_CALL` or `STOP_CALL`, so
its configured snapshot delay is unchanged. A manual snapshot that needs to
open the channel, or a simulated ding after a recent close, can be delayed by
the remaining guard interval.

## Hardware watchdog

Sofia configures a 30-second hardware watchdog and feeds it every five
seconds. WiBox Media uses the same timing by default:

```ini
hardware_watchdog_enabled=1
hardware_watchdog_device=/dev/watchdog
hardware_watchdog_timeout_seconds=30
hardware_watchdog_feed_interval_seconds=5
```

The watchdog thread only feeds while the daemon's main event loop continues
to report progress. With the defaults, feeding stops after 15 seconds without
a main-loop heartbeat, allowing the hardware to recover from a deadlock as
well as from a process crash. Normal shutdown requests `WDIOC_SETOPTIONS` to
disable the watchdog and falls back to the standard magic-close byte.

The GK7102S provides `goke_wdt.ko`. Sofia's temporary warmup wrapper loads it
with `soft_noboot=1` and unloads it when Sofia exits, which explains why
`/dev/watchdog` previously disappeared before the media daemon started. The
normal boot always unloads any warmup instance and reloads the module with
`init_mode=2`, `soft_noboot=0`, `nowayout=0`, `tmr_atboot=0` and a 30-second
margin before starting WiBox Media. Keeping `nowayout=0` is mandatory so
normal shutdown and OTA can disarm it.

On the tested kernel, freezing the daemon caused a hardware reset even when
the module had been loaded with `soft_noboot=1`. That parameter must not be
treated as a non-reboot guarantee; non-destructive validation uses graceful
disarm and the updater guard, while expiry validation is expected to reboot.

## OTA safety

Download and checksum validation run with the watchdog active. Immediately
before touching `/dev/mtd4`, `firmware_update` creates
`/tmp/wibox-firmware-update-critical`, stops the media daemon, waits for its
graceful watchdog disarm and independently confirms `WDIOC_SETOPTIONS` accepts
`WDIOS_DISABLECARD`. The application supervisor will not restart the daemon
while this guard exists.

The guard records the phases `PREPARE`, `FLASHING`, `VERIFYING` and `COMPLETE`.
If the updater disappears during `PREPARE`, the supervisor can remove the
stale guard because flash has not been touched. Once erase begins, the guard is
retained on every failure to prevent an automatic reboot or execution from a
possibly incomplete `/usr`. A successful update keeps the guard until reboot.
`firmware_update --test-watchdog-guard` validates stop/disarm/resume without
unmounting or writing MTD.

The packaged `app_watchdog.sh` remains the active process-level safeguard and
restarts a crashed daemon after five seconds. It does not replace a hardware
watchdog for a kernel lockup. Development deployments must stop that supervisor
before launching a temporary daemon, otherwise it relaunches the release binary
and both instances compete for SIP, RTSP, Prometheus and UART resources.

## Other Sofia findings

- The direct audio path already matches Sofia's 8 kHz, 16-bit mono framing and
  uses the same imported acoustic-processing helper.
- D1 video capture and encoder setup are already represented by the video
  worker; Sofia remains useful only for the boot hardware warmup.
- Sofia applies YUV 3D denoise while creating its video manager. That state may
  persist after warmup, so repeating the setting at runtime is not assumed to
  produce a visible improvement.
- `/proc/goke/video_sync` participates in ISP/3A frame synchronization, not
  SIP audio/video synchronization.
- Generic SDK paths for display, backlight, ADC, I2C, SPI and RTC have no
  demonstrated benefit for this headless GK7102S flow and are intentionally
  not copied.

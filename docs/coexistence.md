# Sharing the VDS bus with the original intercom

Notes taken on 2026-09-11 against a bench built from a real Fermax
installation - outdoor panel (Citiline 98549), the original indoor monitor and
the WiBox on the same bus. That bench no longer exists, so the raw daemon logs
behind every claim here are kept in `captures/`.

The question this answers: the WiBox is going into a flat where people use
their intercom every day. Can it break it?

## What a real call looks like on the bus

Only two frames reach the module for an ordinary call answered on the original
monitor:

```
FB 11 00 1C   ALARM_REPORT                the panel is calling
FB 23 00 2E   PHYSICAL_HANDSET_ANSWERED   somebody picked up the monitor
```

The daemon then stands down on its own (`state=idle terminal=1`), which is the
behaviour you want: the monitor has the call, so the module stops ringing.

**The module is blind to the rest of the call.** Opening the door from the
original monitor produces no frame, and neither does hanging up. So a door
opened downstairs by somebody standing at the monitor will never appear in Home
Assistant - only doors opened through the WiBox itself do.

## Holding the bus blocks the doorbell

VDS carries one conversation at a time. While the module holds the line open
for RTSP viewers (`rtsp_intercom_line_enabled`), the panel gets a busy tone -
a single short beep - and **the call does not happen at all**: no frame arrives,
so nothing rings anywhere and nothing is logged. This is the bus working as
designed, not a fault; the same happens if a neighbour is talking.

Releasing is immediate (`STOP_CALL` the moment the last viewer leaves) and the
next call works straight away.

This is why `rtsp_intercom_line_max_seconds` matters, and why it latches: a
viewer that never disconnects gets the bus for at most that long, after which
the module drops it and stays off until every client has gone. Worst case is
therefore a doorbell dead for `rtsp_intercom_line_max_seconds`, once, and then
it recovers by itself. Keep the value low on a shared installation, and make
sure go2rtc pulls the stream on demand rather than holding it open.

## The MCU protects a call in progress

Opening the camera while somebody is talking on the original monitor does *not*
cut them off. The module asks for the bus and the MCU refuses within a
millisecond:

```
Sent intercom command: START_CALL [FB 14 01 20]
UART code received: HANG_UP_1 [FB 13 01 1F]     <- refusal, ~1 ms later
Intercom line dropped after 461 ms with viewers connected; next attempt in 5000 ms
```

The protection lives in Fermax's microcontroller, below anything this firmware
does, so it holds even if the daemon's own call tracking is wrong - and it is
wrong here, because `PHYSICAL_HANDSET_ANSWERED` clears the call state while the
conversation is still going. The backoff then retries at 5 s and 15 s, and
picks the line up once the conversation ends. Confirmed by ear: the person on
the monitor noticed nothing.

This also corrects the guess in `codes.md` that `FB 13 01 1F` means "opening
door": it is the MCU **declining** a request for a busy bus.

## Cold boot

From power to fully operational: **63 s** (RTSP listening, MQTT connected,
watchdog loaded, no errors). The VDS address and the call-divert flag both live
in the MCU and survive the power cut unchanged. The clock reads UTC during the
boot and becomes local once `ntpd` has run, so early log lines are an hour off
on CEST - expected, and only the scheduled reboot reads wall time.

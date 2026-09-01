# Home Assistant: two-way audio, video and door (local + remote)

This guide sets up a self-hosted "answer the door" experience with the WiBox:
**see the visitor, talk back, and open the door** — from tablets at home and
from your phone when you are away. No vendor cloud, no SIP app, no Nabu Casa.

```
WiBox (RTSP + ONVIF backchannel)  ->  go2rtc  ->  Home Assistant  ->  browser/app (WebRTC)
        192.168.x.x:8554                (WebRTC)      (signaling)         video + mic + door
```

- **Video + listen**: WebRTC (or MSE fallback) from go2rtc.
- **Talk back**: the browser microphone is sent over WebRTC to go2rtc, which
  forwards it to the WiBox RTSP **audio backchannel** (`trackID=2`, sendonly
  PCMA) — i.e. it plays out the WiBox speaker.
- **Open door**: a normal Home Assistant `button.press`.

The fork-specific firmware fixes that make this reliable (RTSP `461` transport
fallback, video-worker lifecycle reconcile, and correct backchannel RTP parsing
+ reframing for WebRTC audio) are listed in [FORK.md](../FORK.md).

## Prerequisites

- WiBox running this firmware, reachable by RTSP. Protect the stream with RTSP
  auth — see [SECURITY.md](../SECURITY.md).
- Home Assistant with **HTTPS** access (the browser only grants microphone
  access in a secure context) and the [go2rtc WebRTC
  integration](https://github.com/AlexxIT/WebRTC) installed.
- HACS (for the intercom card).

## 1. Define the WiBox as a named go2rtc stream

Add a `go2rtc.yaml` to your HA config folder (the AlexxIT integration reads it).
Keep the sections it already has and add the `streams` entry:

```yaml
streams:
  wibox: rtsp://<user>:<pass>@<wibox-ip>:8554/live
```

The WiBox advertises the ONVIF audio backchannel to go2rtc automatically, so
this single line is enough for both listening and talk-back.

## 2. Make WebRTC work when you are away (RTC on mobile data)

Home Assistant already reaches your network through your reverse proxy on 443,
but **WebRTC media does not travel over an HTTP(S) reverse proxy**. Away from
home the card would fall back to **MSE** (view-only, no talk-back). To get real
WebRTC (RTC) remotely, go2rtc needs a reachable candidate:

```yaml
# in the same go2rtc.yaml
webrtc:
  listen: ":8555"
  candidates:
    - stun:8555      # go2rtc discovers and advertises its public IP:8555
```

Then **forward port 8555 (UDP + TCP) to Home Assistant** on your router. That is
the only port you need to open; the WebRTC signaling still goes through Home
Assistant (authenticated). No TURN server is required.

> Exposing 8555 is safe: without the signed WebRTC signaling (done by HA) a
> connection to it cannot pull any stream.

Restart Home Assistant (or reload the integration) so go2rtc re-reads the config.
On mobile data the card should now show **RTC** in the corner instead of `MSe`.

## 3. Add the intercom card

Off-the-shelf cards (AlexxIT `webrtc-camera`, Advanced Camera Card) do not get
the **microphone working in the Home Assistant mobile app**: they call
`getUserMedia` too far from the user gesture, so the app blocks the mic silently.

Use the purpose-built card instead:

**➡️ [`wibox-intercom-video-card`](https://github.com/cmos486/wibox-intercom-video-card)**

It acquires the mic **in the push-to-talk button handler** (satisfying the
mobile gesture rule) and signals WebRTC over the Home Assistant WebSocket (so
go2rtc is never exposed directly). Install it as a dashboard resource (see its
README) and add:

```yaml
type: custom:wibox-intercom-video-card
stream: wibox                       # the go2rtc stream name from step 1
open_door_entity: button.<your_wibox>_open_door
ice_servers: ['stun:stun.cloudflare.com:3478']
```

Press **Descolgar / Pick up** → talk with the **push-to-talk** button → **open
the door**.

## 4. Ring -> phone notification -> open the intercom

The firmware exposes the doorbell over MQTT discovery as an `event` entity whose
`media_state` attribute becomes `ringing` when someone presses the bell. Notify
your phone and deep-link straight to the intercom dashboard:

```yaml
automation:
  - alias: WiBox ring -> notify phone
    trigger:
      - platform: state
        entity_id: event.<your_wibox>_call_event
    condition:
      # media_state == 'ringing' fires for the bell press (the ringing and
      # sip_disabled events land ~1ms apart, so match on the attribute, not the
      # event_type)
      - "{{ trigger.to_state.attributes.media_state == 'ringing' }}"
    action:
      - service: notify.mobile_app_<your_phone>
        data:
          title: "Doorbell"
          message: "Someone is at the door"
          data:
            tag: wibox-ring
            clickAction: /lovelace/intercom   # a view holding the card above
```

Opening the notification lands on the dashboard with the card; from there you
see the visitor, talk, and open the door — at home or away.

## Notes / troubleshooting

- **Mic works on PC / 5G but not on home WiFi**: the app is probably using an
  `http://` internal URL. `getUserMedia` needs HTTPS — point the app at your
  `https://` URL. (The card shows a red secure-context warning over the video
  when this happens.)
- **Away shows `MSe`, not `RTC`**: the 8555 port-forward is missing UDP, or
  go2rtc did not reload. RTC is required for talk-back; MSE is view-only.
- **Verifying talk-back reaches the WiBox**: on the device,
  `grep "backchannel audio pkt" /var/log/wibox-media-daemon.log` should show
  packets with `audio_len` and **zero** `send AO frame failed`.

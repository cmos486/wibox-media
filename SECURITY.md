# Security notes

This firmware runs on a doorbell/intercom device on your LAN. A few services are
exposed and ship with insecure defaults so that first boot "just works" — you
should harden them for any real deployment. Credentials are **per-device**
(stored in `/mnt/mtd/sip_media.conf`, which survives reboots and OTA); nothing
below is baked into the firmware image, so every install sets its own.

## RTSP stream (`:8554/live`) — enable authentication

By default the RTSP video/audio stream is **unauthenticated**: anyone who can
reach the device on the network can pull the camera. Protect it with HTTP Basic
auth by setting a user and password in `/mnt/mtd/sip_media.conf`:

```
rtsp_auth_user=<user>
rtsp_auth_pass=<password>
```

Prefer a random, URL-safe (alphanumeric) password so the RTSP URL needs no
escaping:

```sh
tr -dc 'A-Za-z0-9' </dev/urandom | head -c 24; echo
```

Apply the change with a reboot (config is read at daemon start):

```sh
sed -i 's|^rtsp_auth_user=.*|rtsp_auth_user=<user>|' /mnt/mtd/sip_media.conf
sed -i 's|^rtsp_auth_pass=.*|rtsp_auth_pass=<password>|' /mnt/mtd/sip_media.conf
reboot
```

Verify: `DESCRIBE` without credentials must return `401 Unauthorized`; with
`Authorization: Basic <base64(user:password)>` it returns `200 OK`.

Any RTSP consumer (go2rtc, Home Assistant, VLC, …) must then use the
authenticated URL:

```
rtsp://<user>:<password>@<device-ip>:8554/live
```

Leaving both values empty disables authentication (not recommended).

## SSH (`:22`, Dropbear)

The device ships with the vendor default root login (`root` / a well-known
password). A shared password documented in a community firmware is not a secret,
so **change it on each deployment** rather than relying on the default:

```sh
passwd        # run on the device over SSH; sets a new root password
```

For stronger security, install an SSH public key and disable password login.

> Note: assisted OTA/deploy workflows log in as `root` over SSH. If you change the
> password, use the new one (or a key) in those workflows.

## MQTT

The device is an MQTT **client**: it connects out to *your* broker (e.g. Home
Assistant / Mosquitto). The `mqtt_user` / `mqtt_pass` in the config are only the
credentials it presents. MQTT security (users, ACLs, broker exposure) is
configured on **your broker**, not in this firmware. The only device-side concern
is that those credentials are stored in cleartext in `/mnt/mtd/sip_media.conf`,
so keep SSH access to the device restricted.

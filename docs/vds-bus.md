# 🚪 The Fermax VDS bus, explained

Everything on this page was worked out by watching a real Fermax installation
rebuilt on a bench: outdoor panel, the original indoor monitor and a WiBox, all
on the same wires. If you only read one page before installing this firmware in
a flat somebody lives in, read this one.

---

## 🧩 What VDS actually is

Fermax VDS is a **digital intercom bus**. Every door station and every monitor
in the building hangs off the same pair of wires, and they take turns talking.
Two things follow from that, and they explain most of the surprises:

1. **Everything is addressed.** When somebody presses a button at the street
   panel, the panel puts a number on the bus - the flat being called. Every
   monitor in the building hears it; only the one with that number rings.
2. **Only one conversation fits at a time.** While two devices are talking, the
   bus is busy for everybody else, and the panel answers a new press with a
   short "busy" beep.

### Where the WiBox sits

The WiBox is **not** a new flat on the bus. It is a **clone of the monitor it is
installed next to**: same address, so whatever calls that monitor also calls the
WiBox. That is why Fermax's own pairing procedure asks you to press the door
button *on the monitor* - the module is learning "which terminal am I copying".

```
  street panel  ──┬── monitor (flat 1)
                  └── WiBox   (flat 1)   ← same address, on purpose
```

---

## 🔢 The address is everything

Inside the WiBox there are two brains:

| | |
|---|---|
| 🐧 **Linux side** | The camera, WiFi, and this firmware. What you can edit. |
| 🔒 **Fermax MCU** | A closed microcontroller that owns the bus. Holds the address. |

**The address lives in the MCU, not in any file.** This has consequences worth
knowing up front:

- ✅ It survives a reboot, a power cut, and a firmware update. *(Both verified on
  the bench - the module was updated and power-cycled with the address intact.)*
- ✅ It also survives switching between this firmware and the stock one.
- ❌ It is **not** in your backups, because it is not in flash.

### 😶 What a wrong address looks like

Nothing. That is the whole problem. The MCU filters the bus **silently**: with
the wrong address, a street call produces no frame, no log line, no event - the
module behaves exactly as if nobody had rung. We confirmed the stock Fermax
firmware does the same, so this is not something this firmware can work around.

> 💡 **If doorbell calls never arrive but everything else works, suspect the
> address first.** It is far more likely than wiring, and it costs two minutes
> to check.

---

## 🔎 Reading the current address

The module reports it when asked. From Home Assistant, look at the **VDS
Address** entity. From a shell on the device:

```sh
printf '\xfb\x10\x04\x1f' > /dev/ttySGK1
```

Watch the log and the MCU answers with three frames:

```
FB 18 01 24    ← the stored address: 1
FB 16 00 21    ← MCU state
FB 19 01 25    ← call divert active
```

`FB 18 FA` means **250**, which is the MCU's way of saying *"no address
programmed"*.

---

## ✍️ Setting the address

There are two routes. The second one is the one you want.

### Route A - Fermax's official pairing (PB2)

Short press **PB2** (the installer button) → the power LED blinks red quickly →
within 10 seconds press the **door-release button on the monitor**. The MCU
copies whatever address it hears.

This is what the manual documents, and it works on the monitors Fermax tested
against. **On our bench it never once captured an address**, across a dozen
attempts, including with the genuine Fermax firmware running. The likely reason:
the bench's LOFT monitor only emits its door-release frame *during a call*, so
during those 10 quiet seconds there is simply nothing on the bus to copy. Other
monitors (VEO) emit it from idle and pair fine.

> ⚠️ Entering pairing mode **turns off call divert**, and with divert off the
> module reports no calls at all. This firmware turns it back on automatically
> afterwards; the stock firmware does too. If you ever pair by hand, check the
> divert before concluding anything is broken.

### Route B - set it directly ⭐

Because route A is unreliable, this fork lets you type the number in. Two steps:

**1. Clear whatever the MCU holds** - five short presses of **PB2**. The module
reports `FB 18 FA` (250 = empty) and clears the saved WiFi too.

**2. Write the address you want** - in Home Assistant, set the **VDS Address**
number entity. That is it.

The rule that makes this work, and that cost a full day to find:

> 🔑 **`FB 10 <addr>` writes the address only when the MCU is empty.** Against an
> MCU that already holds one, the exact same frame behaves as a harmless status
> query. So "clear first, then write" is not a ritual - it is the only order
> that does anything.

Verify afterwards by reading it back, then make a real call from the street.

---

## 📞 Why audio and video need the bus "opened"

A monitor does not receive the panel's camera and microphone all the time - it
would be a building-wide party line. The panel is bridged onto a terminal only
while that terminal has a call up, or when it asks for one on purpose. Fermax
calls that second thing **auto switch-on** (*autoencendido*): "show me the door
even though nobody rang".

So when you open the camera in Home Assistant, this firmware performs an auto
switch-on for you (`rtsp_intercom_line_enabled`, on by default). Without it you
get exactly the symptoms that sent us down this path:

| Without the bus opened | With it opened |
|---|---|
| 🔵 Blue "no signal" screen | 📹 The real camera image |
| 🔇 Faint hiss, about -44 dBFS | 🔊 Panel audio, over 100× louder |
| 🙊 Your voice goes nowhere | 🗣️ Your voice comes out of the panel |

### The trade-off, and the guard rails

Opening the bus means **occupying** it. While the WiBox holds it, the doorbell
is busy for everybody. That is not a bug in this firmware - it is how one shared
pair of wires works - but it has to be bounded, so:

- The line is opened only while somebody is actually watching, and released
  **the moment the last viewer disconnects**.
- `rtsp_intercom_line_max_seconds` (default 180) is a hard cap. A dashboard card
  left open forever gets the bus for at most that long, after which the module
  drops it and **stays off** until that client goes away.
- If the MCU refuses, the module backs off (5s → 15s → 60s → 300s) instead of
  hammering a shared bus.

> 📺 **Set go2rtc to pull the stream on demand, not permanently.** A stream held
> open around the clock is the one configuration that turns the guard rails
> above into a daily annoyance.

Full test results, including what happens when somebody is already talking on
the monitor: **[Sharing the bus with the original intercom](coexistence.md)**.

---

## 🔌 The wiring

The WiBox connects to the **monitor's** connector, J1, labelled `+ L - V M Ct F1`:

| Pin | What it is |
|---|---|
| `+` `-` | Power |
| `L` | The VDS data bus |
| `V` `M` | Video coax (screen and shield) |
| `Ct` | Call-in signal |
| `F1` | Fermax auxiliary function |

A working installation needs only five of them: `+`, `-`, `L` and the video
coax. Our bench ran with `Ct` and `F1` unconnected and everything worked, so if
you are chasing a fault, **those two are not it**.

### 💡 About the LEDs

Only the **right (power) LED** follows Fermax's table:

| Colour | Meaning |
|---|---|
| 🔴 Slow blink | No VDS address programmed |
| 🔴 Fast blink | Pairing mode |
| 🟢 Green | Address OK, call divert off |
| 🔵 Blue | Call divert active, or a call in progress |

The **left (WiFi) LED is driven by this firmware**, not by Fermax, so its colour
says nothing about the bus. When the two disagree, **believe the log, not the
LEDs** - we wasted hours on this.

---

## 📻 The frames

The module and the MCU talk over `/dev/ttySGK1` in four-byte frames:

```
FB <command> <data> <checksum>        checksum = command + data + 0x0B
```

The complete list of what the stock firmware sends, how the checksum was proved,
and every frame we have identified: **[UART codes](codes.md)**.

The two that matter most day to day:

```
FB 11 00 1C    somebody is ringing from the street
FB 23 00 2E    somebody picked up the original monitor
```

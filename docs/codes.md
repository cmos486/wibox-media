# UART codes

Device is `/dev/ttySGK1`.
Direction `in` means read, `out` means write.

All codes start with hex `FB` and end with a CRC code.

The CRC is the **sum of every byte in the frame, modulo 240** - confirmed in the
stock binary's frame builder (`mov r1, #240` right before the modulo call). The
handy shortcut `CRC = b1 + b2 + 0x0B` follows from it, because `0xFB mod 240 = 11`.

Given code `FB 20 00`, CRC = `(0xFB + 0x20 + 0x00) mod 240` = `B + 20 + 00` = `0x2B`.

Frames are 4 bytes (3 payload + CRC). The builder also supports a 7-byte form
(6 payload + CRC) and the receive logger has a 7-value format string, so the MCU
can send the long form, but the stock app never does.

| Code | Direction | Name | Description |
|------|-----------|------|-------------|
| `FB 00 00 FF` | in | unknown | Unknown, appeared after closing call. |
| `FB 10 00 1B` | out | unknown | Set after rebooting - CRecord::SetMode(2) |
| `FB 10 04 1F` | out | CUart::Start | Initialize the hardware? Run at Sofia start. |
| `FB 10 5E 79` | out | Unknown | Unknown. Appears after init. |
| `FB 11 00 1C` | in | AlarmReport | Calling at door from the outside. Additional params: ch = 1 |
| `FB 12 01 1E` | out | TRANSFER_CMD_UNLOCK_DOOR | Open the door, relay NO 1. |
| `FB 13 00 1E` | in | HANG_UP 0x00 | Received when door times out without response (30 seconds) |
| `FB 13 01 1F` | in | HANG_UP 0x01 | The MCU declining the bus. Sent within ~1 ms of a `START_CALL` it will not grant - most often because a conversation is already up on the original monitor. See [coexistence.md](coexistence.md). |
| `FB 14 00 1F` | out | StopStreamReader | End intercom call. |
| `FB 14 01 20` | in/out | StartStreamReader | Start a door call. Received after call, success? Additional params. chn = 1, stream = 1 |
| `FB 15 00 20` | out | CallGuard | Action to call guard. |
| `FB 15 03 23` | in | CallGuard_Error_2 | Guardian not available. |
| `FB 16 00 21` | in | MCU_STATE 0x00 | unknown, appears after init |
| `FB 16 01 22` | in | MCU_STATE 0x01 | After clicking button P2 (reset). LED blinks to red. |
| `FB 17 00 22` | out | F1FuncOff | Turn off optional F1 auxiliary relay/function. |
| `FB 17 01 23` | out | CallF1Func | Turn on optional F1 auxiliary relay/function. This is not the main door opener; Fermax uses F1/F2 for installation-specific extras such as an additional door, lights or lift relay. |
| `FB 18 xx yy` | in | SAVE_ADDR 0xxx | The VDS address the module is programmed with. Reported on `CUART_START` and after a successful address programming. |
| `FB 19 00 24` | in/out | PUSH_STATE 0x00 | Physical call-forward state off, observed when the WiBox forward button is toggled. Also sent by Sofia `SetPushState(0)`. This is not a doorbell event. |
| `FB 19 01 25` | in/out | PUSH_STATE 0x01 | Physical call-forward state on. Also sent by Sofia `SetPushState(1)`. The daemon sends this once on boot when serial listening is enabled. This is not a doorbell event. |
| `FB 20 00 2B` | in | CMD_RESET | After clicking button P1 (wifi) 5 times. Triggers Sofia to delete wifi and reboot. |
| `FB 21 00 2C` | in | STA_TO_AP | Legacy direct station-to-AP request retained for compatible MCU revisions. |
| `FB 23 00 2E` | in | PHYSICAL_HANDSET_ANSWERED 0x00 | Pick the call from physical intercom phone. Additional params: ch = 1 |
| `FB 24 01 30` | in | CMD_DOWN_LONG 0x01 | Physical WiFi long-press stage 1, observed on GK7102S hardware. |
| `FB 24 02 31` | in | CMD_DOWN_LONG 0x02 | Long-press completion, observed about 3 seconds after stage 1. The custom daemon requires the ordered pair within 10 seconds before requesting AP mode. |
| `FB 26 00 31` | in | CMD_FAC_SSID_POSTFIX 0x00 | Unknown, received on booting new version B013. |

## What the stock firmware can send (complete)

Recovered by disassembling the original Fermax `Sofia` binary. The whole transmit
path is two functions - `uart_write(buf,len)` with a single caller, and a frame
builder with ten - so this table is the **complete** set of commands the stock
app can send to the MCU, not a sample of what happened to be observed. Function
names and line numbers come from the binary's own error strings
(`app/Functions/Uart.cpp`).

| Frame | Uart.cpp | Function |
|-------|----------|----------|
| `FB 10 00` | - | init / mode |
| `FB 10 xx` | - | init; `xx` is a device field, on one unit it equalled that unit's VDS address |
| `FB 12 01` | 698 | `UnlockChnLock` - open the door |
| `FB 14 xx` | 535 | `OpenDoorBell` |
| `FB 14 00` | 546 | `CloseDoorBell` |
| `FB 15 00` | 619 | `CallGuard` |
| `FB 17 01` | 577 | `CallF1Func` |
| `FB 17 00` | 597 | `F1FuncOff` |
| `FB 19 xx` | 477 | `SetPushState` |
| `FB 25 xx` | 607 | `NoticeLedTest` |

**`FB 10 xx` writes the VDS address, but only while the MCU holds none.** That
single rule explains why the frame looks inert most of the time: with an address
already stored it behaves as a plain status query, which is exactly what the
stock app relies on - it announces its configured address at every start-up
(`CUart::Start` then `FB 10 <addr>`) and the MCU keeps whatever it already had.
`FB 18 xx` (SAVE_ADDR) is report-only and carries the address the MCU holds;
**250 (0xFA) means unprogrammed**.

See [Programming the VDS address](#programming-the-vds-address) for the full
procedure.

## Reading the programmed VDS address

Writing `CUART_START` makes the MCU dump its state, which is the only way to read
back which VDS address the module is programmed with:

```sh
printf '\xfb\x10\x04\x1f' > /dev/ttySGK1
# -> FB 18 02 25   VDS address = 2
#    FB 16 00 21   MCU state
#    FB 19 01 25   call divert active
```

The factory default address is `0xF0` (240), so any other value means the module
has been programmed at some point. The daemon does not send `CUART_START` at
startup, so the MCU stays quiet until something asks.

## Programming the VDS address

The address lives in the MCU and survives firmware changes, so it must be dealt
with on its own terms. **The MCU only accepts a new address while it holds none**,
and it reports that empty state as address **250**.

### Clearing it: five short presses of PB2

The only way found to clear it. The MCU then reports:

```text
FB 18 FA    SAVE_ADDR 250   -> unprogrammed
FB 19 00    call divert off
FB 20 00    CMD_RESET
```

The PWR LED goes red. Note `CMD_RESET` makes this daemon reboot, and the boot
runs Fermax's `Sofia_temp.sh` warm-up, whose Sofia writes its own configured
address straight back into the freshly emptied MCU. To keep it empty, stop the
daemon **and disarm the hardware watchdog** first (`printf 'V' > /dev/watchdog`,
or it reboots about 30 s later).

### Setting it

Either from Home Assistant - the **VDS Address** number entity, which also shows
the current value - or over the UART:

```sh
printf '\xfb\x10\x01\x1c' > /dev/ttySGK1   # address 1; checksum = cmd + data + 0x0B
printf '\xfb\x10\x04\x1f' > /dev/ttySGK1   # verify: the MCU answers FB 18 <addr>
```

`~/wibox/lab-set-vds-addr.sh <n>` does the whole thing including the watchdog
disarm. Once stored, the address survives reboots: the warm-up's `FB 10 xx` is
ignored from then on.

### Learning it from the installation instead

If the flat's call code is unknown, the Fermax procedure can supply it, and it
needs the same empty MCU: short press (< 2 s) on **PB2**, the PWR LED blinks red
faster (`MCU_STATE_1`), then within **10 seconds** press the **door-release button
on the monitor** - not the call button on the outdoor panel. Entering programming
mode also turns the divert off, and this daemon restores it on exit.

Note the LEDs are not a reliable indicator here: the right-hand PWR LED follows
the Fermax table (red slow = no address, red fast = programming, green = address
set with divert off, blue = divert on), but the left-hand WiFi LED is driven by
this firmware, not by the MCU. Read the UART log instead.

Other unknown found:

```
CMD_FACTORY_MODE 0x%02x
AACB version 20190802 `FD 32 30 31 39 30 38 30 32 00`
```

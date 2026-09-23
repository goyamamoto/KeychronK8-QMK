# Keychron K8 (SN32F248B) Bluetooth link

What the stock firmware does to talk to the Bluetooth module, as read from
`Keychron_K8_RGB_ANSI_v1.06_240B.bin`. The ISO v1.02 and Hotswap v1.06 images
build the same packet types. Addresses below refer to the ANSI v1.06 image;
reproduce with `./fetch_stock.sh` and `./disasm.py <image> <start> <end>`.

The module is the ITON module that `drivers/bluetooth/iton_bt.c` already
drives: every packet type, command byte and both handshake lines below match
that driver's tables. Names in the tables come from `iton_bt.h`.

Status column: **code** = read directly from the disassembly;
**iton** = byte matches the `iton_bt` driver, meaning taken from it;
**hw** = observed on a K8 (ANSI RGB, stock v1.07 hardware) running QMK with `ITON_BT_DEBUG`;
**guess** = meaning inferred, not yet confirmed on hardware.

## Physical link

| Item | Value | Status | Where |
|---|---|---|---|
| Bus | SPI0 (`0x4001C000`) | code | `0x5d9c` init |
| MCU role | SPI **slave** (`CTRL0.MS=1`); the module drives SCK | code, hw | `0x5d9c` |
| Frame | 8-bit (`CTRL0.DL=7`), `CTRL1=0` (MSB first, CPOL=0, CPHA=0) | code, hw | `0x5d9c` |
| IRQ | SPI0 RX-FIFO-threshold interrupt, NVIC IRQ 6 | code | `0x5e06`–`0x5e20` |
| Request line | P0.0 (A0) output: set high before a packet, cleared when the packet is sent | code, hw | `BSET`/`BCLR` on GPIO0 bit 0 |
| Direction line | P0.1 (A1) input: high = module is sending to the MCU, low = MCU sends the next byte | code, hw | ISR `0x47cc` |
| SPI0 pins (SCK/MOSI/MISO/SEL) | fixed-function; QMK's matrix and RGB pins leave them alone, since the link works | hw | |

## Packet format (MCU → module)

A packet is one type byte followed by its payload, one byte per SPI frame.
P0.0 goes high, the first byte goes into `SPI0.DATA`, and the ISR sends the
rest as the module clocks them out.

| Type | Payload length | Payload | Meaning | Status |
|---|---|---|---|---|
| `A1` | 8 | modifiers, reserved, 6 keycodes | `report_hid` | code, iton, hw |
| `A2` | 15 | bitmap | `report_nkro` | code, iton |
| `A3` | 2 | usage high byte, low byte | `report_consumer` | code, iton |
| `A4` | 1 | usage low byte | `report_system` | code, iton |
| `A5` | 1 | `0x00` or `0x03` | `report_fn` | code, iton |
| `A6` | 2 | command, argument | `control`, see below | code, iton |

### `A6` commands

| Bytes | `iton_bt` name | Where stock builds it | Status |
|---|---|---|---|
| `A6 58 01` | `mode_usb` | `0x43b0`, from the mode timer when the mode byte is 0 (guess: Cable) | code, iton |
| `A6 51 81` / `82` / `83` | `switch_profile` + 0/1/2 | `0x4414`, from the mode timer when the mode byte is 1 (guess: BT) | code, iton, hw (`81`, `82`) |
| `A6 51 74` / `51 75` | `os_mac` / `os_win` | `0x45aa` / `0x460a`, right after `0x4414`, chosen by flag `0x20000109` | code, iton, hw (`74`) |
| `A6 51 70` | `reset_pairing`: the module answers `B6 51 78` and does not become discoverable | `0x466a`, when a hold counter runs out in BT mode | code, iton, hw |
| `A6 51 89` | `enter_pairing`: the module answers `B6 51 77` and shows up as "Keychron K8" to hosts | not built by any stock command builder | iton, hw |
| `A6 51 61` | `query_battery_level` | `0x448a` | code, iton, hw |
| `A6 51 65` / `51 68` | `disable_sleep` / `enable_sleep` | `0x44ea` / `0x454a`, alternating on flag `0x2000015e` | code, iton |
| `A6 25 01` / `02` / `03` | `sleep_idle_10m` / `20m` / `30m` | `0x46d0`; `0x47a8` arms timers of 6000/12000/18000 ticks for the same setting | code, iton |

None of the command builders writes `51 62` (`mode_bt`) or `51 89` (`enter_pairing`), so
how stock starts pairing is still unknown; `51 89` works regardless.
`0x4174` resends whatever command bytes the builders left in the buffer.

Timing seen on hardware: a packet sent right after `switch_profile` is not
clocked out within 100 ms; 300 ms later it is. Moving the Cable/BT slide switch
resets the MCU (USB re-enumerates). On Cable, `58 01` is not clocked out.

## Packet format (module → MCU)

Received bytes go into the RX buffer at `0x20000388`. The parser at `0x41b6`
acts once 2 or 3 bytes have arrived.

| Bytes | `iton_bt` name | Status |
|---|---|---|
| `B1 xx` | `led_state`; stock keeps `xx & 7` (Num/Caps/Scroll) | code, iton, hw (Caps) |
| `B6 5A 01` / `02` / `04` | battery below 30 % / 30–70 % / above 70 %, also the reply to `51 61` | code, iton, hw |
| `B6 5A 06` / `07` / `0A` | voltage low / low-power shutdown / left low-battery mode | code, iton, hw (`0A` right after connecting) |
| `B6 51 76` / `77` / `78` / `79` | connected / entered pairing / disconnected / connecting | code, iton, hw (`76`–`78`) |

Stock never answers these notifications, so QMK builds without
`ITON_BT_ENABLE_ACK`.

## Open items

- How stock starts pairing, given that none of its command builders writes `51 89`.
- What stock does on `B6 5A 07` (low-power shutdown); QMK keeps running.
- How stock puts the MCU to sleep between key presses.

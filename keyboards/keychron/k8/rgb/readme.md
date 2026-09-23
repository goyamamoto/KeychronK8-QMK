# Keychron K8 RGB

QMK for the original Keychron K8 with RGB backlight (SN32F248B MCU), wired and
Bluetooth.

Build with `make keychron/k8/rgb/<variant>:<keymap>`, where the variant is
`ansi`, `iso`, `optical_ansi` or `optical_iso`. Every build has Bluetooth; the
keymap picks the rest:

| Keymap | Layout | Adds |
|---|---|---|
| `ansi` | ANSI | nothing: Keychron's layout |
| `usjis` | ANSI | [US-JIS features](#us-jis-features-usjis-keymap) |
| `ansi_via` | ANSI | [VIA](#via-ansi_via-and-usjis_via-keymaps) |
| `usjis_via` | ANSI | US-JIS features and VIA |
| `iso` | ISO | nothing: Keychron's layout |

For example `make keychron/k8/rgb/ansi:usjis_via`, or
`make keychron/k8/rgb/optical_ansi:ansi_via` for an Optical ANSI board.

The white-backlight K8 (SN32F260) and the K8 Pro are different boards and are
not covered here.

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools)
and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for
building QMK in general.

## Flashing

[`../flash/`](../flash/) holds everything needed:

    ../flash/build_flasher.sh                    # once: builds ../flash/.flasher/sonixflasher
    ../flash/flash_k8.py keychron_k8_rgb_ansi_ansi.bin

`build_flasher.sh` fetches [SonixFlasherC](https://github.com/SonixQMK/SonixFlasherC)
3.0.0 and applies `sonixflasherc-control-only.patch`. Unpatched 3.0.0 exits
on macOS because it cannot detach the keyboard's HID driver; the flasher only
exchanges feature reports, which travel over endpoint 0 without detaching or
claiming the interface. The older GUI Sonix Flasher is archived and marked
"DO NOT USE - DEPRECATED AND POTENTIALLY DANGEROUS"; do not use it.

`flash_k8.py` refuses a file that does not look like an SN32F248B image or
has no K8 USB descriptor, and refuses an image built for another layout than
the keyboard reports. It then asks before writing. It handles each state the
keyboard can be in:

| Keyboard shows up as | What happens |
|---|---|
| stock firmware (`05ac:024f`/`0250`/`0251`) | the stock updater command (`-r hfd`) switches it to ISP mode without opening the case; the USB ID does not change |
| this QMK (`3434:fe0e`/`fe0f`) | press Fn+Esc when asked, to reach the ROM bootloader |
| ROM bootloader (`0c45:7040`) | flashed directly; the layout cannot be checked |

The same steps by hand, as used on hardware:

    sonixflasher -v 05ac/024f -r hfd -i       # stock: switch to ISP mode and identify
    sonixflasher -v 05ac/024f -f keychron_k8_rgb_ansi_ansi.bin
    sonixflasher -v 0c45/7040 -f keychron_k8_rgb_ansi_ansi.bin   # after Fn+Esc

On hardware, `flash_k8.py` has flashed this QMK over itself through Fn+Esc.
From stock firmware it sends `-r hfd` and `-f` in one run, which has not been
tried; only the separate runs above have. The SonixQMK install guide also reaches
the ROM bootloader by shorting the BOOT pad under the spacebar while plugging
in USB.

To go back to stock firmware, flash the image that matches the keyboard's stock
USB ID (`05AC:024F` ANSI, `0250` ISO, `0251` JIS) the same way.
[`../re/extract_official.py`](../re/extract_official.py) takes ANSI v1.07 from
Keychron's Windows updater; [`../re/fetch_stock.sh`](../re/fetch_stock.sh)
downloads older images from the SonixQMK database.

## Bluetooth

The K8's Bluetooth module is an ITON module on SPI0, driven by the `iton_bt`
driver. The protocol notes are in
[`../re/bluetooth_protocol.md`](../re/bluetooth_protocol.md).

| Control | Action |
|---|---|
| Side switch on Cable | USB only |
| Side switch on BT | Reports go over Bluetooth to the last selected profile |
| Fn+1 / Fn+2 / Fn+3 | Switch to profile 1/2/3 (BT only) |
| Hold Fn+1/2/3 for 3 s | Put that profile into pairing mode (BT only) |
| Fn+B | Show the battery level on the number row for 3 s (BT only) |

Moving the side switch restarts the keyboard.

The key of the current profile shows the link state:

| State | Profile key |
|---|---|
| Pairing | blinks blue |
| Reconnecting | blinks white |
| Connected | green for 3 s |
| Disconnected | red for 3 s |

The battery level lights 10 number keys green above 70 %, 6 keys yellow from
30 % to 70 %, and 3 keys red below 30 %. Esc blinks red while the module
reports low battery. These indicators only show while the RGB matrix is on.

On Bluetooth, the backlight turns off after 5 minutes without input and comes
back on the next key press.

The MCU never sleeps, so the battery drains faster than with stock firmware.

### Tested on hardware

On an ANSI RGB K8 (stock USB ID `05AC:024F`) with a Mac as host:

- flashing from stock firmware with `-r hfd`, and from this firmware with Fn+Esc (by hand and with `flash_k8.py`)
- pairing, typing, and Caps Lock LED over Bluetooth
- switching between two paired hosts with Fn+1/Fn+2
- battery level display and the pairing indicator
- the backlight turning off after 5 minutes idle on battery and back on at the next key press
- Bluetooth notifications and the Caps Lock LED through the SN32 line interrupt (`ITON_BT_SN32_LINE_IRQ`), in the debug log
- VIA (`ansi_via`): a key reassigned in VIA works, stays after unplugging, and works over Bluetooth
- VIA protocol 13 (`usjis_via`): VIA's Lighting menu offers the `RM_*` keycodes

The ISO and optical variants build but have not been tried with Bluetooth,
and the Optical ISO keymap is marked untested even for wired use.

## US-JIS features (`usjis` keymap)

The `usjis` keymap, for ANSI and Optical ANSI boards, adds the features of
[zmk-kb1-usjis](https://github.com/goyamamoto/zmk-kb1-usjis) for the Keychron
B1 Pro and of the NuPhy Air60 V2 port, so a US keyboard types as printed on a
host set to the Japanese keyboard layout.

| Key | Action |
|---|---|
| Fn+Tab | US-JIS mode on/off. Tab blinks green (on) or red (off) for 3 s. Off on first boot; the setting is kept across power cycles. Also the `USJIS_ON` / `USJIS_OFF` keycodes (`UJ_ON` / `UJ_OFF`) |
| Key left of Space | Tap: IME off (Win: Muhenkan, Mac: Eisu). Hold: Alt (Win) / Cmd (Mac) |
| Key right of Space | Tap: IME on (Win: Henkan, Mac: Kana). Hold: Alt (Win) / Cmd (Mac) |
| Key printed Caps Lock | Left Ctrl |
| Key printed Ctrl (bottom left) | Caps Lock |

US-JIS mode only applies with the OS switch on Windows. It replaces the twenty
chords that type `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ | : ' " `` with the
JIS chord that produces the printed character; everything else passes through.
The decision is made when a key goes down and undone when it goes up, and
while a substituted key is held, Shift follows the most recently pressed key.
A mode change while a key is held waits until all keys are released. The
table and rules are in [`usjis.c`](usjis.c), ported from the Air60 V2 port.

On Windows, assign Muhenkan to "IME off" and Henkan to "IME on" in Microsoft
IME (Settings, Time & Language, Language & region, Japanese, Microsoft IME,
Key and touch customization). macOS needs no setup, and on macOS keep the OS
switch on Mac: macOS types a US keyboard as printed anyway.

A key pressed while an IME key is held takes the modifier at once
(`HOLD_ON_OTHER_KEY_PRESS` in `keymaps/usjis/config.h`). The keymap's
`rules.mk` defines `USJIS_ENABLE` and adds `usjis.c`; other keymaps build
without them.

[`hosttest/`](hosttest/) compiles the real `usjis.c` against stub QMK headers
and checks the twenty substitutions and the state transitions of the
zmk-kb1-usjis spec (`make -C hosttest`). For S02, S03, S05 and S11 to S14 it
checks every report in order, not only the final state: the host picks a
character from the report in which its key goes down.

Tried on hardware with Windows set to the Japanese layout, over USB and
Bluetooth: the substitutions including `\`, `_` and `|` (JIS keycodes `0x87`
and `0x89`), holding a substituted key while pressing another (`=` then `A`
gives `=a`, `@` then `A` gives `@A`), the IME keys and the Caps Lock/Ctrl swap.

## VIA (`ansi_via` and `usjis_via` keymaps)

These keymaps are `ansi` and `usjis` with [VIA](https://usevia.app) enabled.
VIA's list of keyboards has the K8 Pro but not the original K8, so load the
definition once: in VIA's settings turn on "Show Design tab", then in the
Design tab load [`keymaps/ansi_via/k8_ansi_via.json`](keymaps/ansi_via/k8_ansi_via.json).
It lists the K8's own keycodes (battery level, US-JIS on/off/toggle) as
custom keycodes; the US-JIS ones only do something in `usjis_via`. The
firmware speaks VIA protocol 13, so VIA's Lighting menu offers the RGB matrix
keycodes (`RM_TOGG` and so on) that drive the K8's backlight.

VIA keeps its own copy of the layers in EEPROM, so the VIA keymaps use a
1 KiB EEPROM (the others 256 bytes). Switching between a VIA and a non-VIA
build resets the saved settings once (RGB mode, Bluetooth profile number).
The Bluetooth profile keys use QMK's `BT_PRF1`-`BT_PRF3`; whether VIA's key
picker offers them has not been checked, and the firmware's default keymap
has them on Fn+1/2/3 either way.

## Debugging the Bluetooth link

A console build logs every packet to and from the module:

    make keychron/k8/rgb/ansi:ansi CONSOLE_ENABLE=yes EXTRAFLAGS=-DITON_BT_DEBUG

Read it with [`../debug/qmk-console`](../debug/) (`make -C ../debug`), `qmk
console` or any hid_listen-style reader while USB stays connected. Each line is `<ms> <event> <first 3 bytes>`. The millisecond timer
wraps every 65 s. The event is `T` (packet queued), `D` (SPI transfer
finished), `X` (packet dropped because the module never clocked it out), or
`R` (packet received). The log starts 3 s after boot so the host has time to
reattach the console.

The console build cannot be combined with VIA: together they need more USB
endpoints than the MCU has.

RAM is the tight resource on this 8 KiB MCU. To fit Bluetooth and VIA, the
EEPROM cache is 256 bytes except in the VIA keymaps, `rgb.c` replaces libc's
`rand()` (which pulled in about 400 bytes of stdio state), the direction line
uses the SN32 port interrupt directly instead of PAL callbacks (a 512-byte
table), and the `typing_heatmap` and `digital_rain` effects, which need a
frame buffer, are left out. With `usjis_via` about 170 bytes remain free.

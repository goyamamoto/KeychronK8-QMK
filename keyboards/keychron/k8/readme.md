# Keychron K8 QMK guide

QMK firmware for the original Keychron K8 with RGB backlight (SN32F248B),
wired and Bluetooth. This guide goes from choosing a firmware to flashing it
and using it, in that order.

1. [Choose a firmware](#1-choose-a-firmware)
2. [Install the tools](#2-install-the-tools)
3. [Get the source and the stock images](#3-get-the-source-and-the-stock-images)
4. [Build or download](#4-build-or-download)
5. [Flash](#5-flash)
6. [Use it](#6-use-it): [Bluetooth](#61-bluetooth), [US-JIS](#62-us-jis), [VIA](#63-via)
7. [Go back to stock firmware](#7-go-back-to-stock-firmware)
8. [Troubleshooting](#8-troubleshooting)
9. [Tested and not tested](#9-tested-and-not-tested)
10. [For developers](#10-for-developers)

Supported: the K8 with RGB backlight in ANSI, ISO, Optical ANSI and Optical
ISO. Not supported: the white-backlight K8 (SN32F260) and the K8 Pro, which
are different boards.

## 1. Choose a firmware

Every firmware has Bluetooth. Pick a keymap for what else you want:

| Keymap | For | Adds |
|---|---|---|
| `ansi` | ANSI | nothing: Keychron's layout |
| `usjis` | ANSI | [US-JIS features](#62-us-jis), for a US keyboard on a host set to the Japanese layout |
| `ansi_via` | ANSI | [VIA](#63-via) |
| `usjis_via` | ANSI | US-JIS features and VIA |
| `iso` | ISO | nothing: Keychron's layout |

and the variant for your board: `ansi`, `iso`, `optical_ansi` or
`optical_iso`. The build target is `keychron/k8/rgb/<variant>:<keymap>`, for
example `keychron/k8/rgb/ansi:usjis_via`, or `keychron/k8/rgb/optical_ansi:ansi`
for an Optical ANSI board.

## 2. Install the tools

On macOS with Homebrew:

    brew install git libusb hidapi pkgconf

and [Docker Desktop](https://www.docker.com/products/docker-desktop/), which
runs the QMK toolchain. The builds here used
`ghcr.io/qmk/qmk_cli@sha256:b7d7fa8fb4432b569931de5ad59098cb788f440ed61a62c5126746b71aee0f4a`.

For `flash/extract_official.py` (step 3) also:

    python3 -m pip install pefile

## 3. Get the source and the stock images

    git clone https://github.com/goyamamoto/KeychronK8-QMK.git
    cd KeychronK8-QMK
    git submodule update --init --recursive
    keyboards/keychron/k8/flash/fetch_stock.sh
    keyboards/keychron/k8/flash/extract_official.py

The stock images are only needed to go back to stock firmware (step 7).
`fetch_stock.sh` downloads the stock images the SonixQMK database keeps;
`extract_official.py` takes ANSI v1.07 out of Keychron's Windows updater.
Both write to `keyboards/keychron/k8/flash/stock/`, which git ignores: the
images are Keychron's and are not part of this repository.

## 4. Build or download

**Download.** Each [release](https://github.com/goyamamoto/KeychronK8-QMK/releases)
(tags `QMK-K8BLE-v<version>`) has every image of step 1, named
`keychron_k8_rgb_<variant>_<keymap>.bin`, built by GitHub Actions without
the debug console or log. Put the file in the repository root and check it:

    sha256sum -c SHA256SUMS --ignore-missing
    gh attestation verify keychron_k8_rgb_ansi_usjis_via.bin -R goyamamoto/KeychronK8-QMK

The release also has an SPDX SBOM and the complete source, submodules
included. Flashing (step 5) still needs this repository for `flash_k8.py`.

**Build.** From the repository root, with the target from step 1:

    docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
        make keychron/k8/rgb/ansi:usjis_via

The firmware lands in the repository root, named after the target, for
example `keychron_k8_rgb_ansi_usjis_via.bin`.

## 5. Flash

Build the flasher once:

    keyboards/keychron/k8/flash/build_flasher.sh

It builds [SonixFlasherC](https://github.com/SonixQMK/SonixFlasherC) 3.0.0
with `flash/sonixflasherc-control-only.patch`, which it needs on macOS (see
[step 10](#10-for-developers)). Do not use the older GUI Sonix Flasher: it is
archived and marked "DO NOT USE - DEPRECATED AND POTENTIALLY DANGEROUS".

Then, with the keyboard on USB and its side switch on **Cable**:

    keyboards/keychron/k8/flash/flash_k8.py keychron_k8_rgb_ansi_usjis_via.bin

`flash_k8.py` finds the keyboard in whichever state it is in:

| The keyboard runs | What happens |
|---|---|
| Stock firmware (first time) | The script sends an ISP command that stock firmware accepts, so there is no need to open the case |
| This firmware (later) | Press **Fn+Esc** when the script asks, to enter the bootloader |
| The ROM bootloader | Flashed directly |

Before writing, it checks that the file is an SN32F248B image built for a K8
and that the image's layout (ANSI, ISO) matches the keyboard's, then asks for
confirmation. Keep the cable plugged in until it reports that the keyboard
restarted. The first boot after changing between a VIA and a non-VIA keymap
resets the saved settings once (RGB mode, Bluetooth profile number).

If the keyboard does not respond at all, the ROM bootloader can also be
reached by shorting the BOOT pad under the spacebar while plugging in USB, as
the SonixQMK install guide describes.

## 6. Use it

### 6.1 Bluetooth

| Control | Action |
|---|---|
| Side switch on Cable | USB |
| Side switch on BT | Bluetooth, to the last selected profile |
| Fn+1 / Fn+2 / Fn+3 | Switch to profile 1 / 2 / 3 (on BT) |
| Hold Fn+1 / 2 / 3 for 3 s | Put that profile into pairing mode (on BT) |
| Fn+B | Show the battery level on the number row for 3 s (on BT) |

Moving the side switch restarts the keyboard.

To pair a host: switch to BT, hold Fn+1 until the 1 key blinks blue, then
connect to "Keychron K8" from the host.

The key of the current profile shows the link state:

| State | Profile key |
|---|---|
| Pairing | blinks blue |
| Reconnecting | blinks white |
| Connected | green for 3 s |
| Disconnected | red for 3 s |

The battery level lights 10 number keys green above 70 %, 6 keys yellow from
30 % to 70 %, and 3 keys red below 30 %. Esc blinks red while the module
reports low battery. These indicators only show while the backlight is on.

On Bluetooth the backlight turns off after 5 minutes without input and comes
back on the next key press. After 10 minutes without input the keyboard
sleeps; any key wakes it, and the Bluetooth connection stays up. It does not sleep while a host uses it over USB or while a key is
held down, and not at all on Optical boards.

### 6.2 US-JIS

In the `usjis` and `usjis_via` keymaps, a US keyboard types as printed on a
host set to the Japanese keyboard layout.

| Key | Action |
|---|---|
| Fn+Tab | US-JIS mode on/off. Tab blinks green (on) or red (off) for 3 s. Off on first boot; kept across power cycles |
| Key left of Space | Tap: IME off (Win: Muhenkan, Mac: Eisu). Hold: Alt (Win) / Cmd (Mac) |
| Key right of Space | Tap: IME on (Win: Henkan, Mac: Kana). Hold: Alt (Win) / Cmd (Mac) |
| Key printed Caps Lock | Left Ctrl |
| Key printed Ctrl (bottom left) | Caps Lock |

US-JIS mode only applies with the OS switch on Windows. It replaces the twenty
chords that type `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ | : ' " `` with the JIS
chord that produces the printed character; everything else passes through.
On macOS keep the OS switch on Mac: macOS types a US keyboard as printed
anyway.

On Windows, assign Muhenkan to "IME off" and Henkan to "IME on" in Microsoft
IME (Settings, Time & Language, Language & region, Japanese, Microsoft IME,
Key and touch customization). macOS needs no setup.

### 6.3 VIA

In the `ansi_via` and `usjis_via` keymaps, keys can be changed in
[VIA](https://usevia.app) without rebuilding.

1. Open VIA in Chrome or Edge and, in its settings, turn on "Show Design tab".
2. In the Design tab, load
   [`rgb/keymaps/ansi_via/k8_ansi_via.json`](rgb/keymaps/ansi_via/k8_ansi_via.json).
   VIA's list has the K8 Pro but not the original K8, so this is needed once.
3. Connect the keyboard and change keys in the Configure tab.

The definition adds the K8's own keys as custom keycodes: battery level, and
US-JIS on/off/toggle (which only do something in `usjis_via`). The Lighting
menu offers the RGB keycodes (`RM_TOGG` and so on). The Bluetooth profile keys
are `BT_PRF1` to `BT_PRF3`; whether VIA's key picker offers them has not been
checked, but the default keymap has them on Fn+1/2/3.

## 7. Go back to stock firmware

Flash the stock image that matches the keyboard's stock USB ID (ANSI
`05AC:024F`, ISO `05AC:0250`, JIS `05AC:0251`) from step 3 in the same way:

    keyboards/keychron/k8/flash/flash_k8.py keyboards/keychron/k8/flash/stock/Keychron_K8_RGB_ANSI_v1.07_official.bin

This installs Keychron's program, not a copy of what the keyboard held
before.

## 8. Troubleshooting

If Bluetooth misbehaves, a debug build logs every packet between the MCU and
the Bluetooth module on the USB console. It cannot include VIA (the two need
more USB endpoints than the MCU has), so build it from `ansi` or `usjis`:

    docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
        make keychron/k8/rgb/ansi:usjis CONSOLE_ENABLE=yes EXTRAFLAGS=-DITON_BT_DEBUG

Flash it, keep USB connected, and read the log:

    make -C keyboards/keychron/k8/debug && keyboards/keychron/k8/debug/qmk-console

(`qmk console` or any hid_listen-style reader works too.) The log starts 3 s
after boot. Each line is `<ms> <event> <first 3 bytes>`, the millisecond
timer wrapping every 65 s:

| Event | Meaning |
|---|---|
| `T` | Packet queued for the module |
| `D` | SPI transfer finished |
| `X` | Packet dropped: the module never clocked it out |
| `R` | Packet received from the module |

The packet codes are those of [`drivers/bluetooth/iton_bt.h`](../../../drivers/bluetooth/iton_bt.h),
for example `R B6 51 77` (pairing started) and `R B6 51 76` (connected).

## 9. Tested and not tested

Tested on an ANSI RGB K8 (stock USB ID `05AC:024F`):

- Flashing from stock firmware, and from this firmware with Fn+Esc
- Bluetooth with a Mac: pairing, typing, Caps Lock LED, switching between two
  hosts, battery display, pairing indicator, backlight timeout
- US-JIS on Windows with the Japanese layout, over USB and Bluetooth: all
  twenty substitutions including `\`, `_` and `|`, holding a substituted key
  while pressing another (`=` then `A` gives `=a`, `@` then `A` gives `@A`),
  the IME keys and the Caps Lock/Ctrl swap
- VIA: keys changed in VIA work, stay after unplugging and work over
  Bluetooth; the Lighting menu offers the `RM_*` keycodes
- Sleep on Bluetooth with a Mac, with the cable unplugged and the idle time
  shortened to 20 s: the keyboard sleeps, a key wakes it, the key is typed,
  the connection stays up, and the Caps Lock LED stays in step with the Mac
  whether Caps Lock or another key wakes it. Once, in an earlier test of the
  same code, the LED stayed on after the Mac had turned Caps Lock off

Checked in the emulator ([`sim/renode/`](sim/renode/), no keyboard needed;
the Bluetooth module is a model there): the start-up sequence, connection,
typing including six keys and the volume key, the Caps Lock LED from the
host, battery query and low-battery flag, pairing, profile switching,
switching between cable and Bluetooth, recovery from a module that stops
answering, and sleep and wake. It also shows that Bluetooth typing goes on
while USB is suspended.

Not tested:

- Sleep: the battery life, and the 10-minute idle time of the release images
- Going back to stock firmware (step 7)
- ISO and Optical boards (they build; the Optical ISO keymap is marked
  untested even wired)
- `flash_k8.py` flashing from stock firmware in one run (the same steps run
  separately by hand have been tested)
- Whether VIA's key picker offers the Bluetooth profile keys

Limitations:

- There is no VIA keymap for ISO.
- Only keys wake the keyboard from sleep, so a Caps Lock change the host
  sends while it sleeps can be missed and the LED stays as it was until the
  next change. The emulator shows this (a `KNOWN` line) under the assumption
  that the module does not send the change again; it may be what the
  hardware test above saw once.

## 10. For developers

| Path | Contents |
|---|---|
| [`rgb/`](rgb/) | The keyboard: `rgb.c` (Bluetooth control, indicators, US-JIS hooks), `sleep.c` (deep sleep on Bluetooth), `usjis.c`, keymaps, HAL configuration |
| [`rgb/hosttest/`](rgb/hosttest/) | Host tests for `usjis.c`: `make -C keyboards/keychron/k8/rgb/hosttest` |
| [`sim/renode/`](sim/renode/) | The firmware on an emulated SN32F248B with a model Bluetooth module (Renode): Bluetooth and sleep tests, see its README |
| [`flash/`](flash/) | `build_flasher.sh`, the SonixFlasherC patch, `flash_k8.py`, `fetch_stock.sh` and `extract_official.py` for the stock images |
| [`debug/`](debug/) | The console reader for the debug build |
| [`release/`](release/) | `build_all.sh` (every release image, a check that none has the debug console or log, `SHA256SUMS`) and `sbom.py` (SPDX SBOM); the GitHub workflows in `.github/workflows/k8-*.yml` run them; pushing a tag `QMK-K8BLE-v<version>` publishes a release |
| `drivers/bluetooth/iton_bt.c` | The Bluetooth module driver (in the QMK tree) |

**Bluetooth.** The module is an ITON module on SPI0 with the MCU as SPI
slave, driven by `iton_bt`; A0 requests a transfer and A1 tells its
direction.

**US-JIS.** The substitution is decided when a key goes down and undone when
it goes up; while a substituted key is held, Shift follows the most recently
pressed key, and the key and its Shift go out in one report. A mode change
while a key is held waits until all keys are released. The keymap's
`rules.mk` defines `USJIS_ENABLE` and adds `usjis.c`. The host tests check
the twenty substitutions and the state transitions of the
[zmk-kb1-usjis](https://github.com/goyamamoto/zmk-kb1-usjis) specification,
and every report of the sequences S02, S03, S05 and S11 to S14.

**RAM.** The MCU has 8 KiB. To fit Bluetooth and VIA, the EEPROM cache is
256 bytes (1 KiB in the VIA keymaps), `rgb.c` replaces libc's `rand()`
(which pulled in about 400 bytes of stdio state), the A1 line uses the SN32
port interrupt directly (`ITON_BT_SN32_LINE_IRQ`) instead of PAL callbacks
and their 512-byte table, and the `typing_heatmap` and `digital_rain`
effects, which need a frame buffer, are left out. `usjis_via` leaves about
170 bytes free.

**Sleep.** `sleep.c` stops the LED and key-scan timer CT16B1, drives the key columns high and
the LED rows low, arms a rising-edge interrupt on the six key rows (the board
pulls them down), switches the clock to ILRC and enters deep sleep
(`PMU_CTRL` = 2, `WFI`). A key press wakes it; the clock, the pins and the
timer are restored and the idle time starts again. The Bluetooth module is
not told anything and keeps its connection. `K8_BT_SLEEP_TIMEOUT` sets the
idle time.

**Flasher patch.** SonixFlasherC 3.0.0 exits on macOS because it cannot
detach the keyboard's HID driver. It only exchanges feature reports, which
travel over endpoint 0 without detaching or claiming the interface, so the
patch turns those two failures into warnings. The same flashing by hand:

    sonixflasher -v 05ac/024f -r hfd -i       # stock: enter ISP mode, identify
    sonixflasher -v 05ac/024f -f keychron_k8_rgb_ansi_ansi.bin
    sonixflasher -v 0c45/7040 -f keychron_k8_rgb_ansi_ansi.bin   # after Fn+Esc

The upstream tree is [SonixQMK](https://github.com/SonixQMK/qmk_firmware),
branch `sn32_develop`, commit `b7b245d0`.

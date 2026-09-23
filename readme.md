# QMK for the Keychron K8, with Bluetooth

QMK firmware for the original Keychron K8 with RGB backlight, built on
[SonixQMK](https://github.com/SonixQMK/qmk_firmware). Unlike SonixQMK's K8
firmware, it keeps the K8's Bluetooth.

## What it does

- **Bluetooth**: three host profiles, pairing, battery level, link indicators
- **US-JIS** (optional keymap): a US keyboard types as printed on a host set to
  the Japanese layout, with IME keys beside Space and Caps Lock/Ctrl swapped
- **VIA** (optional keymap): change keys without rebuilding
- **Flashing from stock firmware** without opening the case

Supported: the K8 with RGB backlight (ANSI, ISO, Optical). Not the
white-backlight K8 or the K8 Pro.

## How to use it

Follow the [Keychron K8 QMK guide](keyboards/keychron/k8/readme.md):

1. [Choose a firmware](keyboards/keychron/k8/readme.md#1-choose-a-firmware)
2. [Install the tools](keyboards/keychron/k8/readme.md#2-install-the-tools)
3. [Get the source and the stock images](keyboards/keychron/k8/readme.md#3-get-the-source-and-the-stock-images)
4. [Build](keyboards/keychron/k8/readme.md#4-build)
5. [Flash](keyboards/keychron/k8/readme.md#5-flash)
6. [Use it](keyboards/keychron/k8/readme.md#6-use-it)
7. [Go back to stock firmware](keyboards/keychron/k8/readme.md#7-go-back-to-stock-firmware)

## What differs from SonixQMK

The QMK tree is large, but only these paths differ from SonixQMK
(`sn32_develop` at commit `b7b245d0`); everything else is upstream code,
unchanged:

| Path | What changed |
|---|---|
| [`keyboards/keychron/k8/`](keyboards/keychron/k8/) | The K8: Bluetooth control, keymaps, US-JIS, flash and analysis tools, the guide |
| [`drivers/bluetooth/iton_bt.c`](drivers/bluetooth/iton_bt.c), [`iton_bt.h`](drivers/bluetooth/iton_bt.h), [`bluetooth_drivers.c`](drivers/bluetooth/bluetooth_drivers.c) | Fixes to the Bluetooth module driver, an SN32 line interrupt that saves RAM, an opt-in debug log |
| [`quantum/via.c`](quantum/via.c), [`via.h`](quantum/via.h) | VIA protocol 13 from upstream QMK (#26001), so VIA offers the RGB matrix keycodes |
| `readme.md` | This section |

[All changes against SonixQMK](https://github.com/goyamamoto/KeychronK8-QMK/compare/b7b245d014721f727f0f8f701026939468f4c916...main)
on GitHub; in a clone, `git diff --stat b7b245d0 HEAD`.

The rest of this file is QMK's own README.

---

# THIS IS THE DEVELOP BRANCH

Warning- This is the `develop` branch of QMK Firmware. You may encounter broken code here. Please see [Breaking Changes](https://docs.qmk.fm/#/breaking_changes) for more information.

# Quantum Mechanical Keyboard Firmware

[![Current Version](https://img.shields.io/github/tag/qmk/qmk_firmware.svg)](https://github.com/qmk/qmk_firmware/tags)
[![Discord](https://img.shields.io/discord/440868230475677696.svg)](https://discord.gg/qmk)
[![Docs Status](https://img.shields.io/badge/docs-ready-orange.svg)](https://docs.qmk.fm)
[![GitHub contributors](https://img.shields.io/github/contributors/qmk/qmk_firmware.svg)](https://github.com/qmk/qmk_firmware/pulse/monthly)
[![GitHub forks](https://img.shields.io/github/forks/qmk/qmk_firmware.svg?style=social&label=Fork)](https://github.com/qmk/qmk_firmware/)

This is a keyboard firmware based on the [tmk\_keyboard firmware](https://github.com/tmk/tmk_keyboard) with some useful features for Atmel AVR and ARM controllers, and more specifically, the [OLKB product line](https://olkb.com), the [ErgoDox EZ](https://ergodox-ez.com) keyboard, and the Clueboard product line.

## Documentation

* [See the official documentation on docs.qmk.fm](https://docs.qmk.fm)

The docs are powered by [VitePress](https://vitepress.dev/). They are also viewable offline; see [Previewing the Documentation](https://docs.qmk.fm/#/contributing?id=previewing-the-documentation) for more details.

You can request changes by making a fork and opening a [pull request](https://github.com/qmk/qmk_firmware/pulls).

## Supported Keyboards

* [Planck](/keyboards/planck/)
* [Preonic](/keyboards/preonic/)
* [ErgoDox EZ](/keyboards/ergodox_ez/)
* [Clueboard](/keyboards/clueboard/)
* [Cluepad](/keyboards/clueboard/17/)
* [Atreus](/keyboards/atreus/)

The project also includes community support for [lots of other keyboards](/keyboards/).

## Maintainers

QMK is developed and maintained by Jack Humbert of OLKB with contributions from the community, and of course, [Hasu](https://github.com/tmk). The OLKB product firmwares are maintained by [Jack Humbert](https://github.com/jackhumbert), the Ergodox EZ by [ZSA Technology Labs](https://github.com/zsa), the Clueboard by [Zach White](https://github.com/skullydazed), and the Atreus by [Phil Hagelberg](https://github.com/technomancy).

## Official Website

[qmk.fm](https://qmk.fm) is the official website of QMK, where you can find links to this page, the documentation, and the keyboards supported by QMK.

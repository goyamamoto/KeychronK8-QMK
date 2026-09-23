# QMK for the Keychron K8, with Bluetooth

This repository is [SonixQMK](https://github.com/SonixQMK/qmk_firmware)
(`sn32_develop` at commit `b7b245d0`) plus changes for the original Keychron K8
with RGB backlight (SN32F248B). The QMK tree is large, but only these paths
differ from SonixQMK; everything else is upstream code, unchanged:

| Path | What changed |
|---|---|
| [`keyboards/keychron/k8/`](keyboards/keychron/k8/) | Bluetooth through the K8's ITON module, keymaps (`usjis` adds a US-JIS mode, IME keys beside Space and a Caps Lock/Ctrl swap; `ansi_via` and `usjis_via` add VIA), readme |
| [`keyboards/keychron/k8/flash/`](keyboards/keychron/k8/flash/) | A patch and build script for SonixFlasherC on macOS, and a flash script with safety checks |
| [`keyboards/keychron/k8/re/`](keyboards/keychron/k8/re/) | Notes on the stock firmware's Bluetooth protocol and Keychron's updaters, scripts to fetch and disassemble stock images |
| [`keyboards/keychron/k8/debug/`](keyboards/keychron/k8/debug/) | A reader for the Bluetooth debug console |
| [`drivers/bluetooth/iton_bt.c`](drivers/bluetooth/iton_bt.c), [`iton_bt.h`](drivers/bluetooth/iton_bt.h), [`bluetooth_drivers.c`](drivers/bluetooth/bluetooth_drivers.c) | Fixes to the ITON driver (send timeout, keyboard LEDs and system keys over Bluetooth, a bounds check), an SN32 line interrupt that saves RAM, and an opt-in debug log |
| [`quantum/via.c`](quantum/via.c), [`via.h`](quantum/via.h) | VIA protocol 13 from upstream QMK (#26001), so VIA offers the RGB matrix keycodes (`RM_*`) in its key picker |
| `readme.md` | This section |

[All changes against SonixQMK](https://github.com/goyamamoto/KeychronK8-QMK/compare/b7b245d014721f727f0f8f701026939468f4c916...main)
on GitHub. In a clone, `git diff --stat b7b245d0 HEAD` lists the same files.

Start with [`keyboards/keychron/k8/readme.md`](keyboards/keychron/k8/readme.md):
building, flashing (also from stock firmware without opening the case), going
back to stock, and what has been tested on hardware.

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

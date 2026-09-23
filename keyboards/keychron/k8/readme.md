# Keychron K8

The supported boards are the RGB-backlit K8 variants in [`rgb/`](rgb/); see
[`rgb/readme.md`](rgb/readme.md) for the features, flashing details, and test
status. [`flash/`](flash/) builds the patched SonixFlasherC and flashes with
safety checks. [`re/`](re/) holds the notes and scripts used to work out the
stock firmware's Bluetooth protocol ([`re/bluetooth_protocol.md`](re/bluetooth_protocol.md))
and to recover stock images from Keychron's updaters
([`re/stock_updater.md`](re/stock_updater.md)).
[`debug/`](debug/) reads the Bluetooth debug console.

## Starting from a fresh clone

Every build has Bluetooth. The keymap picks the rest:

| Keymap | Build target | Firmware |
|---|---|---|
| `ansi` | `keychron/k8/rgb/ansi:ansi` | Keychron's ANSI layout |
| `usjis` | `keychron/k8/rgb/ansi:usjis` | `ansi` plus US-JIS mode, IME keys beside Space and the Caps Lock/Ctrl swap, for a US keyboard on a Japanese-layout host |
| `ansi_via` | `keychron/k8/rgb/ansi:ansi_via` | `ansi` with VIA |
| `usjis_via` | `keychron/k8/rgb/ansi:usjis_via` | `usjis` with VIA |
| `iso` | `keychron/k8/rgb/iso:iso` | Keychron's ISO layout |

The Optical boards use the same keymaps (`optical_ansi:usjis` and so on).

Tools, on macOS with Homebrew:

    brew install git libusb hidapi pkgconf
    python3 -m pip install pefile capstone     # only for re/ scripts

and Docker (Docker Desktop), which runs the QMK toolchain. The builds here used
`ghcr.io/qmk/qmk_cli@sha256:b7d7fa8fb4432b569931de5ad59098cb788f440ed61a62c5126746b71aee0f4a`.

Get the code and the stock images (needed to go back to stock firmware and
for the disassembly):

    git clone https://github.com/goyamamoto/KeychronK8-QMK.git
    cd KeychronK8-QMK
    git submodule update --init --recursive
    keyboards/keychron/k8/re/fetch_stock.sh
    keyboards/keychron/k8/re/extract_official.py

`fetch_stock.sh` downloads the stock images the SonixQMK database keeps;
`extract_official.py` takes ANSI v1.07 out of Keychron's Windows updater. Both
write to `re/stock/`, which git ignores: the images are Keychron's and are not
part of this repository. `flash/build_flasher.sh` clones SonixFlasherC.

Build the firmware (the `.bin` files land in the repository root); pick the
target from the table above:

    docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
        make keychron/k8/rgb/ansi:ansi

Build the flasher and flash, with the side switch on Cable:

    keyboards/keychron/k8/flash/build_flasher.sh
    keyboards/keychron/k8/flash/flash_k8.py keychron_k8_rgb_ansi_ansi.bin

Back to stock:

    keyboards/keychron/k8/flash/flash_k8.py keyboards/keychron/k8/re/stock/Keychron_K8_RGB_ANSI_v1.07_official.bin

The Bluetooth debug build and its console reader:

    docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
        make keychron/k8/rgb/ansi:ansi CONSOLE_ENABLE=yes EXTRAFLAGS=-DITON_BT_DEBUG
    make -C keyboards/keychron/k8/debug && keyboards/keychron/k8/debug/qmk-console

Disassemble the stock firmware (see [`re/bluetooth_protocol.md`](re/bluetooth_protocol.md)):

    keyboards/keychron/k8/re/disasm.py keyboards/keychron/k8/re/stock/Keychron_K8_RGB_ANSI_v1.06_240B.bin 0x3f9a 0x4860

The US-JIS host tests:

    make -C keyboards/keychron/k8/rgb/hosttest

The upstream SonixQMK tree is `https://github.com/SonixQMK/qmk_firmware`,
branch `sn32_develop`; both code branches start from its commit `b7b245d0`.

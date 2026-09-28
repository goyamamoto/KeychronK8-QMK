#!/bin/sh
# Builds the emulation image the Renode tests run: the ansi keymap with two
# changes that only matter under Renode. Never flash it.
#   * CORTEX_ALTERNATE_SWITCH: ChibiOS switches threads through PendSV.
#     Renode's Cortex-M0 never takes the NMI the default ARMv6-M port uses
#     after an interrupt, and the firmware stops at the first one.
#   * Short idle timeouts (backlight 10 s, sleep 20 s instead of 5 and 10
#     minutes), so the sleep tests take seconds.
# Run from the repository root inside the QMK toolchain, e.g.
#   docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
#       keyboards/keychron/k8/sim/renode/build.sh
set -eu

FLAGS="-DCORTEX_ALTERNATE_SWITCH=TRUE -DK8_BT_RGB_TIMEOUT=10000 -DK8_BT_SLEEP_TIMEOUT=20000"

make keychron/k8/rgb/ansi:ansi EXTRAFLAGS="$FLAGS" TARGET=keychron_k8_rgb_ansi_ansi_renode

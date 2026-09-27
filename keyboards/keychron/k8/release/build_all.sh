#!/bin/sh
# Builds every K8 release image into an output directory, checks that none of
# them carries the debug console or the Bluetooth debug log, and writes
# SHA256SUMS. Run from the repository root inside the QMK toolchain, e.g.
#   docker run --rm -v "$PWD":/qmk_firmware -w /qmk_firmware ghcr.io/qmk/qmk_cli \
#       keyboards/keychron/k8/release/build_all.sh dist
set -eu

OUT=${1:-dist}

# <variant>:<keymap>. The ANSI keymaps also fit the Optical ANSI board, `iso`
# fits both ISO boards.
TARGETS="
ansi:ansi ansi:usjis ansi:ansi_via ansi:usjis_via
optical_ansi:ansi optical_ansi:usjis optical_ansi:ansi_via optical_ansi:usjis_via
iso:iso
optical_iso:iso
"

# Present only with CONSOLE_ENABLE=yes or -DITON_BT_DEBUG.
DEBUG_SYMBOLS='^(console_task|ConsoleReport|iton_bt_log)$'

mkdir -p "$OUT"
for t in $TARGETS; do
    variant=${t%%:*}
    keymap=${t#*:}
    name=keychron_k8_rgb_${variant}_${keymap}
    make "keychron/k8/rgb/$variant:$keymap"
    if arm-none-eabi-nm ".build/$name.elf" | awk '{print $NF}' | grep -Eq "$DEBUG_SYMBOLS"; then
        echo "$name: debug console or Bluetooth debug log in a release image" >&2
        exit 1
    fi
    cp "$name.bin" "$OUT/"
done

(cd "$OUT" && sha256sum ./*.bin | sed 's| \./| |' > SHA256SUMS)
cat "$OUT/SHA256SUMS"

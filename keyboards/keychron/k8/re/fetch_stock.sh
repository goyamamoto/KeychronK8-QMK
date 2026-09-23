#!/bin/sh
# Downloads the stock K8 RGB images from the SonixQMK database into ./stock/.
# The images are Keychron's; keep them out of git.
set -eu
cd "$(dirname "$0")"
mkdir -p stock
base=https://raw.githubusercontent.com/SonixQMK/Mechanical-Keyboard-Database/main/stockFWs/Keychron/240B
for f in Keychron_K8_RGB_ANSI_v1.06_240B.bin Keychron_K8_RGB_ISO_v1.02_240B.bin \
         Keychron_K8_RGB_JIS_v1.07_240B.bin Keychron_K8-Hotswap_RGB_v1.06_240B.bin \
         Keychron_K8-Optical_RGB_v1.03_240B.bin; do
    curl -fsSL -o "stock/$f" "$base/$f"
done
ls -l stock

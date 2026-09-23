#!/bin/sh
# Builds SonixFlasherC 3.0.0 with sonixflasherc-control-only.patch into
# ./.flasher/sonixflasher. Needs make, a C compiler, pkg-config and libusb
# (macOS: brew install libusb pkgconf).
#
# The source comes from .flasher/SonixFlasherC-3.0.0.tar.gz if a copy is
# there (for building offline), otherwise from GitHub at the pinned commit.
# SonixFlasherC is GPL-3.0; it is built here, not stored in the code branches.
set -eu
cd "$(dirname "$0")"

REPO=https://github.com/SonixQMK/SonixFlasherC
COMMIT=73f9fab8a82c5c4f91181d276676c8f936726925 # tag 3.0.0
TARBALL=.flasher/SonixFlasherC-3.0.0.tar.gz
SRC=.flasher/SonixFlasherC

mkdir -p .flasher
rm -rf "$SRC"
if [ -f "$TARBALL" ]; then
    tar -xzf "$TARBALL" -C .flasher
else
    git clone --quiet "$REPO" "$SRC"
    git -C "$SRC" checkout --quiet "$COMMIT"
fi
(cd "$SRC" && patch -p1 --quiet < ../../sonixflasherc-control-only.patch)
make -C "$SRC" BACKEND=libusb
cp "$SRC/sonixflasher" .flasher/sonixflasher
./.flasher/sonixflasher -V

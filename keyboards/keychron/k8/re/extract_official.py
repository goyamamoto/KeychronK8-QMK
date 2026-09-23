#!/usr/bin/env python3
"""Pull the stock K8 RGB image out of Keychron's official Windows updater.

The updater keeps the full 64 KiB flash image as plain RCDATA resource 4000,
next to its expected 16-bit word sum (resource 4014, hex text) and target
VID/PID (4008/4009). No decryption is involved.

Usage:
    pip install pefile
    ./extract_official.py        # writes stock/Keychron_K8_RGB_ANSI_v1.07_official.bin

It reads the updater from stock/official/K8-A2-87K-RGB-V1.07.exe if a copy is
there, and downloads it from Keychron otherwise.
"""
import pathlib
import struct
import urllib.request

import pefile

UPDATER_URL = "https://cdn.shopify.com/s/files/1/0059/0630/1017/files/K8-A2-87K-RGB-V1.07.exe?v=1642821187"
STOCK = pathlib.Path(__file__).parent / "stock"
UPDATER = STOCK / "official" / "K8-A2-87K-RGB-V1.07.exe"
OUT = STOCK / "Keychron_K8_RGB_ANSI_v1.07_official.bin"


def rcdata(pe, res_id):
    for kind in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if kind.id != pefile.RESOURCE_TYPE["RT_RCDATA"]:
            continue
        for res in kind.directory.entries:
            if res.id == res_id:
                entry = res.directory.entries[0].data.struct
                return pe.get_data(entry.OffsetToData, entry.Size)
    raise KeyError(res_id)


def main():
    exe = UPDATER.read_bytes() if UPDATER.exists() else urllib.request.urlopen(UPDATER_URL).read()
    pe = pefile.PE(data=exe)

    image = rcdata(pe, 4000)
    expected = int(rcdata(pe, 4014).decode("utf-16le"), 16)
    vid = rcdata(pe, 4008).decode("utf-16le")
    pid = rcdata(pe, 4009).decode("utf-16le")

    checksum = sum(struct.unpack(f"<{len(image) // 2}H", image)) & 0xFFFF
    if checksum != expected:
        raise SystemExit(f"checksum {checksum:04X} != expected {expected:04X}")

    OUT.parent.mkdir(exist_ok=True)
    OUT.write_bytes(image)
    print(f"{OUT.name}: {len(image)} bytes for {vid}:{pid}, checksum {checksum:04X} OK")


if __name__ == "__main__":
    main()

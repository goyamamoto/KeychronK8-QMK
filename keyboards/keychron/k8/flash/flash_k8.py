#!/usr/bin/env python3
"""Flash a Keychron K8 RGB (SN32F248B) with SonixFlasherC, after sanity checks.

    ./build_flasher.sh                         # once
    ./flash_k8.py keychron_k8_rgb_ansi_ansi.bin
    ./flash_k8.py ../re/stock/Keychron_K8_RGB_ANSI_v1.07_official.bin

Before anything is written, the script checks that the file looks like an
SN32F248B image, reads which K8 layout it was built for from its USB device
descriptor, finds the keyboard (stock firmware, this QMK, or the ROM
bootloader), and refuses a layout mismatch. It then asks for confirmation.
"""
import argparse
import pathlib
import re
import struct
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
DEFAULT_FLASHER = HERE / ".flasher" / "sonixflasher"

FLASH_SIZE = 64 * 1024
RAM_START, RAM_END = 0x20000000, 0x20002000

BOOTLOADER = (0x0C45, 0x7040)
LAYOUTS = {
    (0x05AC, 0x024F): ("stock", "ANSI"),
    (0x05AC, 0x0250): ("stock", "ISO"),
    (0x05AC, 0x0251): ("stock", "JIS"),
    (0x3434, 0xFE0E): ("QMK", "ANSI"),
    (0x3434, 0xFE0F): ("QMK", "ISO"),
}


def fail(msg):
    sys.exit(f"error: {msg}")


def check_image(path):
    try:
        image = path.read_bytes()
    except OSError as e:
        fail(f"cannot read {path}: {e.strerror}")
    if not 1024 <= len(image) <= FLASH_SIZE:
        fail(f"{path.name} is {len(image)} bytes; an SN32F248B image is at most {FLASH_SIZE}")

    sp, reset = struct.unpack_from("<II", image)
    if not RAM_START < sp <= RAM_END:
        fail(f"initial stack pointer {sp:#010x} is outside the 8 KiB RAM")
    if not (reset & 1 and reset < len(image)):
        fail(f"reset vector {reset:#010x} does not point to Thumb code inside the image")

    for m in re.finditer(rb"\x12\x01..\x00\x00\x00.(..)(..)", image, re.S):
        ids = tuple(struct.unpack("<H", g)[0] for g in m.groups())
        if ids in LAYOUTS:
            return ids
    fail("no Keychron K8 USB device descriptor in the image; is it a K8 RGB firmware?")


def connected_ids():
    """VID/PID pairs of the USB devices currently attached."""
    ids = set()
    if sys.platform == "darwin":
        out = subprocess.run(["ioreg", "-p", "IOUSB", "-l", "-w0"], capture_output=True, text=True).stdout
        for block in out.split("+-o ")[1:]:
            vid = re.search(r'"idVendor" = (\d+)', block)
            pid = re.search(r'"idProduct" = (\d+)', block)
            if vid and pid:
                ids.add((int(vid.group(1)), int(pid.group(1))))
    else:
        for dev in pathlib.Path("/sys/bus/usb/devices").glob("*"):
            try:
                ids.add((int((dev / "idVendor").read_text(), 16), int((dev / "idProduct").read_text(), 16)))
            except (FileNotFoundError, ValueError):
                pass
    return ids


def find_keyboard():
    ids = connected_ids()
    found = [i for i in ids if i in LAYOUTS or i == BOOTLOADER]
    if len(found) > 1:
        fail(f"more than one K8 or Sonix bootloader attached: {', '.join(f'{v:04x}:{p:04x}' for v, p in found)}")
    return found[0] if found else None


def wait_for(target, seconds):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if target in connected_ids():
            return True
        time.sleep(0.5)
    return False


def describe(ids):
    if ids == BOOTLOADER:
        return "ROM bootloader (0c45:7040)"
    kind, layout = LAYOUTS[ids]
    return f"{kind} firmware, {layout} ({ids[0]:04x}:{ids[1]:04x})"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", type=pathlib.Path)
    ap.add_argument("--flasher", type=pathlib.Path, default=DEFAULT_FLASHER)
    ap.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    ap.add_argument("--force", action="store_true", help="flash even if the layouts do not match")
    args = ap.parse_args()

    if not args.flasher.exists():
        fail(f"{args.flasher} not found; run ./build_flasher.sh first")

    image_ids = check_image(args.image)
    print(f"image:    {args.image.name}: {describe(image_ids)}")

    keyboard = find_keyboard()
    if keyboard is None:
        fail("no Keychron K8 found; connect it by USB with the side switch on Cable")
    print(f"keyboard: {describe(keyboard)}")

    if keyboard == BOOTLOADER:
        print("note: the bootloader does not tell which layout the keyboard is; check the image by hand")
    else:
        image_layout, keyboard_layout = LAYOUTS[image_ids][1], LAYOUTS[keyboard][1]
        if image_layout != keyboard_layout and not args.force:
            fail(f"image is for {image_layout} but the keyboard is {keyboard_layout} (use --force to override)")

    if keyboard in LAYOUTS and LAYOUTS[keyboard][0] == "QMK":
        print("Press Fn+Esc on the K8 to enter the bootloader (waiting 30 s)...")
        if not wait_for(BOOTLOADER, 30):
            fail("the ROM bootloader did not appear")
        keyboard = BOOTLOADER
        print(f"keyboard: {describe(keyboard)}")

    if keyboard == BOOTLOADER:
        target = ["-v", "0c45/7040"]
    else:
        # Stock firmware switches to ISP mode on this command and keeps its USB ID.
        target = ["-v", f"{keyboard[0]:04x}/{keyboard[1]:04x}", "-r", "hfd"]

    print("\nKeep the side switch on Cable and the cable plugged in until flashing finishes.")
    if not args.yes:
        try:
            answer = input("Flash now? [y/N] ")
        except EOFError:
            answer = ""
        if answer.strip().lower() != "y":
            sys.exit("aborted")

    cmd = [str(args.flasher), *target, "-f", str(args.image)]
    print("$", " ".join(cmd))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    output = []
    for line in proc.stdout:
        sys.stdout.write(line)
        output.append(line)
    if proc.wait() != 0 or not any("FLASHING COMPLETED SUCCESSFULLY" in line for line in output):
        fail("flashing did not complete; the ROM bootloader sits outside the flash being written, reach it with the BOOT pad")

    print(f"\nwaiting for {describe(image_ids)} ...")
    if wait_for(image_ids, 15):
        print("done: the keyboard restarted with the new firmware")
    else:
        fail(f"the keyboard did not come back as {image_ids[0]:04x}:{image_ids[1]:04x}; replug it and check")


if __name__ == "__main__":
    main()

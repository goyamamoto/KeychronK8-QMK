#!/usr/bin/env python3
"""Annotated Thumb disassembly of a stock Keychron K8 (SN32F248B) firmware image.

Usage:
    ./fetch_stock.sh                     # downloads the stock images into ./stock/
    pip install capstone
    ./disasm.py stock/Keychron_K8_RGB_ANSI_v1.06_240B.bin 0x3f9a 0x4860

PC-relative literal loads are resolved and peripheral addresses are named
after the SN32F240B register map (lib/chibios-contrib/.../SN32F240B.h).
"""
import struct
import sys

from capstone import CS_ARCH_ARM, CS_MODE_THUMB, Cs

PERIPHERALS = {
    0x40000000: "CT16B0", 0x40002000: "CT16B1", 0x40010000: "WDT",
    0x40012000: "UART2", 0x40014000: "UART1", 0x40016000: "UART0",
    0x40018000: "I2C0", 0x4001C000: "SPI0", 0x40026000: "ADC",
    0x40032000: "PMU", 0x40042000: "PFPA",
    0x40044000: "GPIO0", 0x40046000: "GPIO1", 0x40048000: "GPIO2", 0x4004A000: "GPIO3",
    0x4005C000: "USB", 0x4005E000: "SYS1", 0x40060000: "SYS0", 0x40062000: "FLASH",
    0xE000E000: "SCS",
}


def name(value):
    for base, periph in PERIPHERALS.items():
        if base <= value < base + 0x2000:
            return f"{periph}+{value - base:#x}"
    if 0x20000000 <= value < 0x20002000:
        return f"RAM {value:#x}"
    return hex(value)


def main():
    image = open(sys.argv[1], "rb").read()
    start, end = int(sys.argv[2], 16), int(sys.argv[3], 16)
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.skipdata = True
    for insn in md.disasm(image[start:end], start):
        line = f"{insn.address:5x}: {insn.mnemonic:8} {insn.op_str}"
        if insn.mnemonic == "ldr" and "[pc" in insn.op_str:
            offset = int(insn.op_str.split("#")[-1].rstrip("]"), 16) if "#" in insn.op_str else 0
            literal = ((insn.address + 4) & ~3) + offset
            line += f"   ; ={name(struct.unpack_from('<I', image, literal)[0])}"
        print(line)


if __name__ == "__main__":
    main()

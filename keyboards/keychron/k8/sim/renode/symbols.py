#!/usr/bin/env python3
# Writes "name address size" for every object and function in an ELF32
# file. The tests look variables up here: Renode's own symbol lookup can
# return a neighbouring symbol for small variables.
import struct
import sys


def symbols(path):
    data = open(path, "rb").read()
    shoff, = struct.unpack_from("<I", data, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", data, 0x2E)
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    for sh in sections:
        if sh[1] != 2:  # SHT_SYMTAB
            continue
        strtab = sections[sh[6]]
        for off in range(sh[4], sh[4] + sh[5], 16):
            name, value, size, info, _, _ = struct.unpack_from("<IIIBBH", data, off)
            if (info & 0xF) not in (1, 2):  # STT_OBJECT, STT_FUNC
                continue
            start = strtab[4] + name
            yield data[start:data.index(b"\0", start)].decode(), value, size


if __name__ == "__main__":
    for name, value, size in symbols(sys.argv[1]):
        print("%s 0x%08x %d" % (name, value, size))

#!/usr/bin/env python3
"""
Wraps a raw, headerless HFS disk image (as produced by Retro68's Rez
--cc <name>.dsk output) in a real Apple DiskCopy 4.2 header, so the
result is a genuine DC42 image mountable by Mac OS's own Disk Copy
utility and by DC42-aware tools (emulators, disk image converters).

Header layout and checksum algorithm per the DiskCopy 4.2 format
specification (68kMLA wiki, sourced from the CiderPress and Mini vMac
implementations): checksum is a running sum of the image data taken as
big-endian 16-bit words, rotated right by one bit after each addition.

Usage: make_diskcopy42.py <raw.dsk> <out.dsk> <volume-name>
"""

import struct
import sys


def dc42_checksum(data: bytes) -> int:
    checksum = 0
    for i in range(0, len(data) - 1, 2):
        word = (data[i] << 8) | data[i + 1]
        checksum = (checksum + word) & 0xFFFFFFFF
        checksum = ((checksum >> 1) | ((checksum & 1) << 31)) & 0xFFFFFFFF
    return checksum


def make_dc42(raw_path: str, out_path: str, name: str) -> None:
    with open(raw_path, "rb") as f:
        data = f.read()

    # Disk encoding 0x01 / format byte 0x22 below specifically mean "800K
    # Mac GCR disk" -- Retro68 rounds its raw image up to the next 800K
    # multiple, so this only stays correct as long as the app + resources
    # fit in a single 800K image. If iWordle.dsk ever grows past that,
    # this needs a real per-size lookup instead of the hardcoded values.
    if len(data) != 800 * 1024:
        raise ValueError(
            f"{raw_path} is {len(data)} bytes, expected exactly 800K (819200) -- "
            "the app has outgrown a single floppy image; update the disk "
            "encoding/format byte logic in this script before proceeding"
        )

    name_bytes = name.encode("ascii")[:63]

    header = bytearray(84)
    header[0] = len(name_bytes)
    header[1:1 + len(name_bytes)] = name_bytes
    struct.pack_into(">I", header, 0x40, len(data))   # data size
    struct.pack_into(">I", header, 0x44, 0)           # tag size (none)
    struct.pack_into(">I", header, 0x48, dc42_checksum(data))
    struct.pack_into(">I", header, 0x4C, 0)           # tag checksum (none)
    header[0x50] = 0x01                               # disk encoding: 800K GCR dsdd
    header[0x51] = 0x22                               # format byte: Mac 800K
    struct.pack_into(">H", header, 0x52, 0x0100)      # magic / private word

    with open(out_path, "wb") as f:
        f.write(bytes(header))
        f.write(data)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <raw.dsk> <out.dsk> <volume-name>", file=sys.stderr)
        sys.exit(1)
    make_dc42(sys.argv[1], sys.argv[2], sys.argv[3])

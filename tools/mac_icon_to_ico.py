#!/usr/bin/env python3
"""Build a Windows .ico from the classic Mac icon family in icons.r.

Usage: python3 -I tools/mac_icon_to_ico.py src/platform/macos9/icons.r out.ico

Reads the 'ICN#', 'icl4', 'icl8', 'ics#', 'ics4' and 'ics8' (128) hex data
blocks and emits four palettized images: 32x32 4bpp/8bpp and 16x16 4bpp/8bpp.
No 32bpp entries on purpose -- Windows 95/NT 4 can pick them at true-color
depths and render them wrong. Standard library only.
"""
import re
import struct
import sys

EXPECTED = {'ICN#': 256, 'icl4': 512, 'icl8': 1024,
            'ics#': 64, 'ics4': 128, 'ics8': 256}

MAC4 = [0xFFFFFF, 0xFCF305, 0xFF6402, 0xDD0806, 0xF20884, 0x4600A5,
        0x0000D4, 0x02ABEA, 0x1FB714, 0x006411, 0x562C05, 0x90713A,
        0xC0C0C0, 0x808080, 0x404040, 0x000000]


def mac8_color(i):
    levels = [0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00]
    ramp = [0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11]
    if i <= 214:
        return (levels[i // 36] << 16) | (levels[(i // 6) % 6] << 8) | levels[i % 6]
    if i == 255:
        return 0
    k = (i - 215) // 10
    v = ramp[(i - 215) % 10]
    return [v << 16, v << 8, v, (v << 16) | (v << 8) | v][k]


def fail(msg):
    sys.stderr.write('mac_icon_to_ico: %s\n' % msg)
    sys.exit(1)


def parse_blocks(text):
    blocks = {}
    for m in re.finditer(r"data\s+'([^']{4})'\s*\(\s*128\b[^)]*\)\s*\{(.*?)\n\};",
                         text, re.S):
        if m.group(1) not in EXPECTED:
            continue
        hexstr = ''.join(re.findall(r'\$"([0-9A-Fa-f\s]*)"', m.group(2)))
        hexstr = re.sub(r'\s+', '', hexstr)
        blocks[m.group(1)] = bytes.fromhex(hexstr)
    for name, size in EXPECTED.items():
        if name not in blocks:
            fail("missing data block '%s' (128)" % name)
        if len(blocks[name]) != size:
            fail("'%s' is %d bytes, expected %d" % (name, len(blocks[name]), size))
    return blocks


def mask_bits(data, size):
    """Per-pixel opacity (True = opaque) from the second half of ICN#/ics#."""
    rowbytes = size // 8
    base = rowbytes * size
    out = []
    for y in range(size):
        row = []
        for x in range(size):
            b = data[base + y * rowbytes + x // 8]
            row.append(bool((b >> (7 - x % 8)) & 1))
        out.append(row)
    return out


def pixels(data, size, bpp):
    out = []
    for y in range(size):
        row = []
        for x in range(size):
            if bpp == 4:
                b = data[(y * size + x) // 2]
                row.append(b >> 4 if x % 2 == 0 else b & 15)
            else:
                row.append(data[y * size + x])
        out.append(row)
    return out


def build_image(pix, mask, size, bpp, mac_color):
    ncolors = 1 << bpp
    palette = [0x000000]          # index 0 is black: transparent pixels land here
    remap = {}
    xor_rows = []
    for y in range(size):
        row = []
        for x in range(size):
            if not mask[y][x]:
                row.append(0)
                continue
            rgb = mac_color(pix[y][x])
            if rgb == 0:
                idx = 0
            else:
                if rgb not in remap:
                    remap[rgb] = len(palette)
                    palette.append(rgb)
                idx = remap[rgb]
            row.append(idx)
        xor_rows.append(row)
    if len(palette) > ncolors:
        fail('too many distinct colors for %d bpp' % bpp)
    palette += [0] * (ncolors - len(palette))

    xor = bytearray()
    xor_stride = ((size * bpp + 31) // 32) * 4
    for y in range(size - 1, -1, -1):
        row = xor_rows[y]
        buf = bytearray(xor_stride)
        for x in range(size):
            if bpp == 4:
                buf[x // 2] |= row[x] << 4 if x % 2 == 0 else row[x]
            else:
                buf[x] = row[x]
        xor += buf
    andm = bytearray()
    and_stride = ((size + 31) // 32) * 4
    for y in range(size - 1, -1, -1):
        buf = bytearray(and_stride)
        for x in range(size):
            if not mask[y][x]:        # AND bit 1 = transparent
                buf[x // 8] |= 0x80 >> (x % 8)
        andm += buf

    hdr = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, bpp, 0,
                      len(xor) + len(andm), 0, 0, 0, 0)
    pal = b''.join(struct.pack('<BBBB', c & 255, (c >> 8) & 255, c >> 16, 0)
                   for c in palette)
    return hdr + pal + bytes(xor) + bytes(andm)


def main():
    if len(sys.argv) != 3:
        fail('usage: mac_icon_to_ico.py icons.r out.ico')
    with open(sys.argv[1], 'r', encoding='latin-1') as f:
        blocks = parse_blocks(f.read())

    mask32 = mask_bits(blocks['ICN#'], 32)
    mask16 = mask_bits(blocks['ics#'], 16)
    images = [
        (32, 4, build_image(pixels(blocks['icl4'], 32, 4), mask32, 32, 4, lambda i: MAC4[i])),
        (32, 8, build_image(pixels(blocks['icl8'], 32, 8), mask32, 32, 8, mac8_color)),
        (16, 4, build_image(pixels(blocks['ics4'], 16, 4), mask16, 16, 4, lambda i: MAC4[i])),
        (16, 8, build_image(pixels(blocks['ics8'], 16, 8), mask16, 16, 8, mac8_color)),
    ]

    out = bytearray(struct.pack('<HHH', 0, 1, len(images)))
    offset = 6 + 16 * len(images)
    for size, bpp, data in images:
        out += struct.pack('<BBBBHHII', size, size, 16 if bpp == 4 else 0, 0,
                           1, bpp, len(data), offset)
        offset += len(data)
    for _, _, data in images:
        out += data
    with open(sys.argv[2], 'wb') as f:
        f.write(out)


main()

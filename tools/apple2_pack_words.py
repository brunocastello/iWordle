#!/usr/bin/env python3
"""Pack src/core/word_list.h into WORDS.DAT for the Apple II build.

usage: apple2_pack_words.py word_list.h WORDS.DAT

Format (all u16 little-endian):
  0     "IWD1"
  4     u16 total word count
  6     u16 answer count
  8     677 x u16 bucket start (index into the entry array; bucket =
        (first-'A')*26 + (second-'A'); the last one is the total count)
  1362  676 x u8 answers per bucket
  2038  N x u16 entries: bit 15 = is an answer, bits 14-10 / 9-5 / 4-0 =
        letters 3, 4, 5 (A=0); sorted within each bucket
"""
import re
import struct
import sys

MAX_WORDS = 10000
MAX_BUCKET = 200


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    text = open(sys.argv[1], encoding="ascii").read()
    split = text.index("kWordleValidGuesses[]")
    answers = set(re.findall(r'"([A-Z]{5})"', text[:split]))
    guesses = set(re.findall(r'"([A-Z]{5})"', text[split:]))
    words = sorted(answers | guesses)
    assert len(words) <= MAX_WORDS, len(words)

    buckets = [[] for _ in range(676)]
    for w in words:
        b = (ord(w[0]) - 65) * 26 + (ord(w[1]) - 65)
        e = ((ord(w[2]) - 65) << 10) | ((ord(w[3]) - 65) << 5) | (ord(w[4]) - 65)
        if w in answers:
            e |= 0x8000
        buckets[b].append(e)

    starts = []
    pos = 0
    for b in buckets:
        starts.append(pos)
        pos += len(b)
    starts.append(pos)
    counts = [sum(1 for e in b if e & 0x8000) for b in buckets]
    assert max(len(b) for b in buckets) <= MAX_BUCKET
    assert max(counts) <= 255

    out = bytearray(b"IWD1")
    out += struct.pack("<HH", len(words), len(answers))
    out += struct.pack("<677H", *starts)
    out += bytes(counts)
    for b in buckets:
        for e in sorted(b, key=lambda x: x & 0x7FFF):
            out += struct.pack("<H", e)
    assert len(out) == 2038 + 2 * len(words)
    open(sys.argv[2], "wb").write(out)
    print("WORDS.DAT: %d words, %d answers, %d bytes (max bucket %d)" % (
        len(words), len(answers), len(out), max(len(b) for b in buckets)))


main()

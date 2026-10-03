#!/usr/bin/env python3
"""Generate the 8K CHR-ROM (tiles.chr) from ASCII-art tiles.

Characters map to 2-bit colour indices: '.'=0 '1'=1 '2'=2 '3'=3.
Tile N lives in the first (background + sprite, neslib default) pattern
table; both pattern tables get the same data.
"""
import sys

TILES = {
    0x01: [  # player: smiley
        "..3333..",
        ".333333.",
        "33133133",
        "33333333",
        "31333313",
        "33111133",
        ".333333.",
        "..3333..",
    ],
    0x02: [  # background border brick
        "33333333",
        "1112.111",
        "1112.111",
        "22222222",
        "33333333",
        "1.1112.1",
        "1.1112.1",
        "22222222",
    ],
}


def encode(rows):
    lo, hi = bytearray(8), bytearray(8)
    for y, row in enumerate(rows):
        assert len(row) == 8, row
        for x, ch in enumerate(row):
            v = 0 if ch == "." else int(ch)
            bit = 0x80 >> x
            if v & 1:
                lo[y] |= bit
            if v & 2:
                hi[y] |= bit
    return bytes(lo + hi)


def main(out):
    table = bytearray(0x1000)
    for idx, rows in TILES.items():
        table[idx * 16:(idx + 1) * 16] = encode(rows)
    with open(out, "wb") as f:
        f.write(table * 2)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "tiles.chr")

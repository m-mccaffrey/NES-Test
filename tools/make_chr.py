#!/usr/bin/env python3
"""Build an 8K CHR-ROM from a plain-text tile description.

usage: make_chr.py TILES.txt OUT.chr

Tile file syntax (blank lines and '#' comments are ignored):

    tile 0x60          an 8x8 tile; the next 8 lines are its pixels
    ........
    ...

    image 0x80 2x2     an image W tiles wide and H tiles tall; the next H*8
    ................   lines (W*8 chars each) become tiles N, N+1, ... in
                       row-major order (2x2 = a 16x16 sprite: TL, TR, BL, BR)

    font PATH color=C offset=O
                       render every glyph of a font file (path relative to
                       this file) into tile ord(char)+O using colour index C

Pixel characters: '.' = colour 0 (transparent), '1' '2' '3' = colour 1-3.

Font files contain `glyph X` headers (X is a single character) followed by
up to 8 rows of '#' (on) and '.' (off), at most 7 columns wide. Glyphs are
drawn with a 1-pixel left margin.

Both pattern tables ($0000 and $1000) get the same 256 tiles.
"""
import os
import re
import sys


def encode(rows):
    lo, hi = bytearray(8), bytearray(8)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            v = 0 if ch == "." else int(ch)
            bit = 0x80 >> x
            if v & 1:
                lo[y] |= bit
            if v & 2:
                hi[y] |= bit
    return bytes(lo + hi)


def check_rows(rows, w, h, where):
    if len(rows) != h or any(len(r) != w or not re.fullmatch(r"[.123]+", r) for r in rows):
        raise SystemExit(f"{where}: expected {h} rows of {w} chars from '.123'")


def load_font(path):
    glyphs, cur = {}, None
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith(";"):
                continue
            m = re.fullmatch(r"glyph (.)", line)
            if m:
                cur = glyphs.setdefault(m.group(1), [])
                continue
            if cur is None or not re.fullmatch(r"[#.]{1,7}", line) or len(cur) >= 8:
                raise SystemExit(f"{path}:{n}: bad glyph row {line!r}")
            cur.append(line)
    return glyphs


def build(path):
    tiles = {}

    def put(idx, rows, where):
        if not 0 <= idx < 256:
            raise SystemExit(f"{where}: tile index {idx:#x} out of range")
        if idx in tiles:
            raise SystemExit(f"{where}: tile {idx:#x} defined twice")
        tiles[idx] = encode(rows)

    with open(path) as f:
        lines = [(n, l.split("#", 1)[0].rstrip()) for n, l in enumerate(f, 1)]
    lines = [(n, l) for n, l in lines if l]

    i = 0
    while i < len(lines):
        n, line = lines[i]
        where = f"{path}:{n}"
        words = line.split()
        if words[0] == "tile":
            rows = [l for _, l in lines[i + 1:i + 9]]
            check_rows(rows, 8, 8, where)
            put(int(words[1], 0), rows, where)
            i += 9
        elif words[0] == "image":
            base = int(words[1], 0)
            w, h = (int(v) for v in words[2].split("x"))
            rows = [l for _, l in lines[i + 1:i + 1 + h * 8]]
            check_rows(rows, w * 8, h * 8, where)
            for ty in range(h):
                for tx in range(w):
                    put(base + ty * w + tx,
                        [r[tx * 8:tx * 8 + 8] for r in rows[ty * 8:ty * 8 + 8]], where)
            i += 1 + h * 8
        elif words[0] == "font":
            opts = dict(w.split("=", 1) for w in words[2:])
            color = opts.get("color", "1")
            offset = int(opts.get("offset", "0"), 0)
            font = load_font(os.path.join(os.path.dirname(path), words[1]))
            for ch, glyph in font.items():
                rows = []
                for y in range(8):
                    g = glyph[y] if y < len(glyph) else ""
                    rows.append(("." + g.replace("#", color)).ljust(8, "."))
                put(ord(ch) + offset, rows, where)
            i += 1
        else:
            raise SystemExit(f"{where}: unknown directive {words[0]!r}")
    return tiles


def main():
    src, out = sys.argv[1], sys.argv[2]
    table = bytearray(0x1000)
    for idx, data in build(src).items():
        table[idx * 16:(idx + 1) * 16] = data
    with open(out, "wb") as f:
        f.write(table * 2)


if __name__ == "__main__":
    main()

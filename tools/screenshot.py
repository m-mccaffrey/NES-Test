#!/usr/bin/env python3
"""Run the ROM headlessly and save a PNG of a frame.

usage: screenshot.py ROM OUT.png [FRAMES] [BUTTONS...]
  BUTTONS: any of up down left right a b start select, held the whole time.
"""
import struct
import sys
import zlib

import cynes

BUTTONS = {name.lower(): getattr(cynes, "NES_INPUT_" + name)
           for name in ("UP", "DOWN", "LEFT", "RIGHT", "A", "B", "START", "SELECT")}


def write_png(path, frame):
    h, w, _ = frame.shape
    raw = b"".join(b"\x00" + frame[y].tobytes() for y in range(h))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    rom, out = sys.argv[1], sys.argv[2]
    frames = int(sys.argv[3]) if len(sys.argv) > 3 else 60
    pad = 0
    for name in sys.argv[4:]:
        pad |= BUTTONS[name.lower()]
    nes = cynes.NES(rom)
    nes.controller = pad
    frame = nes.step(frames)
    write_png(out, frame[:, :, :3].copy())


if __name__ == "__main__":
    main()

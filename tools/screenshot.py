#!/usr/bin/env python3
"""Run a ROM headlessly and save a PNG of the last frame.

usage: screenshot.py ROM OUT.png [--frames N] [--four-score]
                     [--hold PLAYER:BUTTON[,BUTTON...]]...

  --hold 1:right,a   hold buttons on controller 1 for the whole run
                     (buttons: up down left right a b start select)
  --four-score       attach a Four Score so controllers 3/4 work
                     (needs the patched cynes, see tools/install_cynes.sh)
"""
import argparse
import struct
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


def parse_hold(spec):
    player, _, buttons = spec.partition(":")
    bits = 0
    for name in filter(None, buttons.split(",")):
        bits |= BUTTONS[name.lower()]
    return (int(player) - 1) * 8, bits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=60)
    ap.add_argument("--four-score", action="store_true")
    ap.add_argument("--hold", action="append", default=[])
    args = ap.parse_args()

    nes = cynes.NES(args.rom)
    if args.four_score:
        nes.four_score = True
    for spec in args.hold:
        shift, bits = parse_hold(spec)
        nes.controller |= bits << shift
    frame = nes.step(args.frames)
    write_png(args.out, frame[:, :, :3].copy())


if __name__ == "__main__":
    main()

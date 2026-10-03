"""Emulator-driven tests: boot the real ROM in the cynes headless NES
emulator, press buttons, and inspect RAM, OAM and the rendered frame.

Run with:  make rom-test   (needs `pip install cynes numpy`)
"""
import os
import re
import unittest

import numpy as np
from cynes import (NES, NES_INPUT_DOWN, NES_INPUT_LEFT, NES_INPUT_RIGHT,
                   NES_INPUT_UP)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROM = os.path.join(ROOT, "build", "game.nes")
LABELS = os.path.join(ROOT, "build", "game.labels")
HEADER = os.path.join(ROOT, "src", "player.h")

OAM_BUF = 0x0200
BOOT_FRAMES = 30


def load_labels():
    labels = {}
    with open(LABELS) as f:
        for line in f:
            m = re.match(r"al ([0-9A-Fa-f]+) \.(\S+)", line)
            if m:
                labels[m.group(2)] = int(m.group(1), 16)
    return labels


def load_defines():
    """Pull numeric #defines out of player.h so tests track the C source."""
    defs = {}
    with open(HEADER) as f:
        for line in f:
            m = re.match(r"#define (\w+)\s+(.+?)\s*(/\*.*)?$", line)
            if m:
                try:
                    defs[m.group(1)] = eval(m.group(2), {}, dict(defs))
                except Exception:
                    pass
    return defs


LBL = load_labels()
DEF = load_defines()
PLAYER = LBL["_player"]


class RomTest(unittest.TestCase):
    def setUp(self):
        self.nes = NES(ROM)
        self.frame = self.nes.step(BOOT_FRAMES)
        self.assertFalse(self.nes.has_crashed)

    def hold(self, buttons, frames):
        self.nes.controller = buttons
        self.frame = self.nes.step(frames)
        self.nes.controller = 0
        self.assertFalse(self.nes.has_crashed)

    def pos(self):
        return self.nes[PLAYER], self.nes[PLAYER + 1]

    def test_rom_header(self):
        with open(ROM, "rb") as f:
            data = f.read()
        self.assertEqual(data[:4], b"NES\x1a")
        self.assertEqual(len(data), 16 + 32768 + 8192)

    def test_boots_to_start_position(self):
        self.assertEqual(self.pos(), (DEF["PLAYER_START_X"], DEF["PLAYER_START_Y"]))

    def test_idle_does_not_move(self):
        start = self.pos()
        self.hold(0, 60)
        self.assertEqual(self.pos(), start)

    def test_dpad_moves_sprite(self):
        for button, dx, dy in [(NES_INPUT_RIGHT, 1, 0), (NES_INPUT_LEFT, -1, 0),
                               (NES_INPUT_DOWN, 0, 1), (NES_INPUT_UP, 0, -1)]:
            with self.subTest(button=button):
                x0, y0 = self.pos()
                self.hold(button, 20)
                x1, y1 = self.pos()
                # Input latency can eat a frame or so; require clear motion
                # in the right direction and none on the other axis.
                self.assertGreaterEqual((x1 - x0) * dx + (y1 - y0) * dy, 15)
                self.assertEqual(x1 - x0 if dx == 0 else y1 - y0, 0)

    def test_clamped_to_screen(self):
        self.hold(NES_INPUT_LEFT | NES_INPUT_UP, 300)
        self.assertEqual(self.pos(), (DEF["PLAYER_MIN_X"], DEF["PLAYER_MIN_Y"]))
        self.hold(NES_INPUT_RIGHT | NES_INPUT_DOWN, 300)
        self.assertEqual(self.pos(), (DEF["PLAYER_MAX_X"], DEF["PLAYER_MAX_Y"]))

    def test_oam_matches_player(self):
        self.hold(NES_INPUT_RIGHT, 10)
        self.nes.step(1)
        x, y = self.pos()
        oam_y, tile, attr, oam_x = (self.nes[OAM_BUF + i] for i in range(4))
        self.assertEqual((oam_x, oam_y, tile, attr), (x, y, 1, 0))

    def test_sprite_is_drawn_where_expected(self):
        """Check the actual rendered pixels move with the sprite."""
        before = self.nes.step(1).astype(int)
        self.hold(NES_INPUT_RIGHT, 40)
        after = self.nes.step(1).astype(int)
        x, y = self.pos()
        diff = np.any(before != after, axis=2)
        ys, xs = np.nonzero(diff)
        self.assertTrue(len(xs) > 0, "frame did not change after moving")
        # Pixels that changed should only be around the old and new
        # sprite locations (OAM Y is one line above the drawn sprite).
        self.assertTrue(((xs >= DEF["PLAYER_START_X"]) & (xs < x + 8)).all())
        self.assertTrue(((ys >= y) & (ys <= y + 9)).all())
        # And the new location must contain non-background pixels.
        bg = after[0, 128]
        patch = after[y + 1:y + 9, x:x + 8]
        self.assertTrue(np.any(np.any(patch != bg, axis=2)))


if __name__ == "__main__":
    unittest.main()

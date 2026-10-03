"""Emulator-driven tests for the D-pad demo: boot the real ROM in cynes,
press buttons, and inspect RAM, OAM and the rendered frame.

Run with:  make rom-test-demo   (needs `pip install cynes numpy`)
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "tools"))
import nestest  # noqa: E402
from nestest import DOWN, LEFT, RIGHT, UP  # noqa: E402

DEF = nestest.load_defines("games/demo/player.h")
OAM_BUF = 0x0200
BOOT_FRAMES = 30


class DemoRomTest(unittest.TestCase):
    def setUp(self):
        self.m = nestest.Machine("demo")
        self.m.step(BOOT_FRAMES)

    def hold(self, buttons, frames):
        self.m.press([buttons], frames)

    def pos(self):
        return tuple(self.m.array("_player", 2))

    def test_rom_header(self):
        with open(nestest.rom_path("demo"), "rb") as f:
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
        for button, dx, dy in [(RIGHT, 1, 0), (LEFT, -1, 0), (DOWN, 0, 1), (UP, 0, -1)]:
            with self.subTest(button=button):
                x0, y0 = self.pos()
                self.hold(button, 20)
                x1, y1 = self.pos()
                # Input latency can eat a frame or so; require clear motion
                # in the right direction and none on the other axis.
                self.assertGreaterEqual((x1 - x0) * dx + (y1 - y0) * dy, 15)
                self.assertEqual(x1 - x0 if dx == 0 else y1 - y0, 0)

    def test_clamped_to_screen(self):
        self.hold(LEFT | UP, 300)
        self.assertEqual(self.pos(), (DEF["PLAYER_MIN_X"], DEF["PLAYER_MIN_Y"]))
        self.hold(RIGHT | DOWN, 300)
        self.assertEqual(self.pos(), (DEF["PLAYER_MAX_X"], DEF["PLAYER_MAX_Y"]))

    def test_oam_matches_player(self):
        self.hold(RIGHT, 10)
        self.m.step(1)
        x, y = self.pos()
        oam_y, tile, attr, oam_x = (self.m.nes[OAM_BUF + i] for i in range(4))
        self.assertEqual((oam_x, oam_y, tile, attr), (x, y, 1, 0))

    def test_sprite_is_drawn_where_expected(self):
        """Check the actual rendered pixels move with the sprite."""
        before = self.m.step(1).astype(int)
        self.hold(RIGHT, 40)
        after = self.m.step(1).astype(int)
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

"""Emulator-driven tests for the 4-player rail shooter.

Boots the real ROM in cynes (patched with Four Score support, see
tools/install_cynes.sh), drives up to four controllers, and checks RAM,
the controller-reading code, and the rendered frames.

Run with:  make rom-test-shooter
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "tools"))
import nestest  # noqa: E402
from nestest import A, B, DOWN, LEFT, RIGHT, SELECT, START, UP  # noqa: E402

D = nestest.load_defines("games/shooter/game.h")
N = D["NUM_PLAYERS"]
E = D["MAX_ENEMIES"]
MAX_BOOT_FRAMES = 120

HUD_TEXT_ROWS = slice(8, 16)       # tile row 1: scores
HUD_STATUS_ROWS = slice(16, 24)    # tile row 2: lives / game over
HUD_ALL_ROWS = slice(0, 24)
GROUND_ROWS = slice(208, 240)      # below any enemy or crosshair start


APU_STATUS = 0x4015   # read: bit set while that channel's length counter runs
CH_PULSE1, CH_PULSE2, CH_TRIANGLE, CH_NOISE = 1, 2, 4, 8


class ShooterTest(unittest.TestCase):
    four_score = True
    start_game = True   # press START on the title screen in setUp

    def setUp(self):
        if self.four_score:
            nestest.require_four_score(self)
        self.m = nestest.Machine("shooter", four_score=self.four_score)
        # Wait until the main loop has run a frame (drawing both nametables
        # with rendering off takes a few frames).
        for self.boot_frames in range(MAX_BOOT_FRAMES):
            self.m.step(1)
            if self.m.peek("_game_frame"):
                break
        else:
            self.fail("game loop never started")
        if self.start_game:
            self.m.press([START], 1)
            self.m.step(1)
            self.assertEqual(self.m.peek("_game_state"), D["STATE_PLAY"])

    # --- helpers ---------------------------------------------------------
    def active(self):
        return self.m.array("_player_active", N)

    def crosshair(self, p):
        return self.m.peek("_cross_x", p), self.m.peek("_cross_y", p)

    def score(self, p):
        d = self.m.array("_score", N * 4)[p * 4:p * 4 + 4]
        return d[0] * 1000 + d[1] * 100 + d[2] * 10 + d[3]

    def alive_enemies(self):
        st = self.m.array("_enemy_state", E)
        xs = self.m.array("_enemy_x", E)
        ys = self.m.array("_enemy_y", E)
        return [(i, xs[i], ys[i]) for i in range(E) if st[i] == D["ENEMY_ALIVE"]]

    def join_all(self):
        self.m.press([0, A, A, A], 1)
        self.m.step(1)
        self.assertEqual(self.active(), [1] * N)

    def bot_kill(self, p, max_frames=900):
        """Steer player p's crosshair onto an enemy and shoot it.

        Returns the frame count it took; fails if no kill happened."""
        start = self.score(p)
        fire = False
        for t in range(max_frames):
            pads = [0] * N
            targets = [e for e in self.alive_enemies() if e[1] > 24]
            if targets:
                cx, cy = self.crosshair(p)
                hx, hy = cx + D["CROSS_HOT"], cy + D["CROSS_HOT"]
                _, ex, ey = min(targets, key=lambda e: abs(e[1] + 8 - hx) + abs(e[2] + 8 - hy))
                # Lead the target: it moves left one pixel per frame.
                dx, dy = (ex + 7) - hx, (ey + 8) - hy
                if dx > 1:
                    pads[p] |= RIGHT
                elif dx < -1:
                    pads[p] |= LEFT
                if dy > 1:
                    pads[p] |= DOWN
                elif dy < -1:
                    pads[p] |= UP
                fire = not fire and abs(dx) <= 5 and abs(dy) <= 5
                if fire:
                    pads[p] |= A
            self.m.set_pads(pads)
            self.m.step(1)
            if self.score(p) > start:
                self.m.set_pads([])
                return t
        self.fail(f"player {p + 1} bot failed to score in {max_frames} frames")


class BootAndInput(ShooterTest):
    def test_rom_header(self):
        with open(nestest.rom_path("shooter"), "rb") as f:
            data = f.read()
        self.assertEqual(data[:4], b"NES\x1a")
        self.assertEqual(len(data), 16 + 32768 + 8192)

    def test_boot_state(self):
        self.assertEqual(self.active(), [1, 0, 0, 0])
        self.assertEqual(self.m.peek("_lives"), D["START_LIVES"])
        self.assertEqual(self.m.peek("_game_state"), D["STATE_PLAY"])
        self.assertEqual(self.m.peek("_pads_fourscore"), 1)

    def test_every_button_on_every_pad_reads_correctly(self):
        """The Four Score reader maps each pad and button to the right bit."""
        for p in range(N):
            for button in (A, B, SELECT, START, UP, DOWN, LEFT, RIGHT):
                pads = [0] * N
                pads[p] = button
                self.m.set_pads(pads)
                self.m.step(1)
                expected = [0] * N
                expected[p] = nestest.to_neslib(button)
                self.assertEqual(self.m.array("_pads", N), expected,
                                 f"pad {p + 1} button {button:#04x}")
        self.m.set_pads([])

    def test_players_join_with_a_or_start(self):
        self.m.press([0, A], 1)
        self.m.press([0, 0, START], 1)
        self.m.press([0, 0, 0, A], 1)
        self.m.step(1)
        self.assertEqual(self.active(), [1, 1, 1, 1])

    def test_four_crosshairs_move_independently(self):
        self.join_all()
        before = [self.crosshair(p) for p in range(N)]
        self.m.press([LEFT, RIGHT, UP, DOWN], 10)
        after = [self.crosshair(p) for p in range(N)]
        moves = [(a[0] - b[0], a[1] - b[1]) for a, b in zip(after, before)]
        # One frame of input latency is fine; direction and axis must match.
        self.assertLess(moves[0][0], -10); self.assertEqual(moves[0][1], 0)
        self.assertGreater(moves[1][0], 10); self.assertEqual(moves[1][1], 0)
        self.assertEqual(moves[2][0], 0); self.assertLess(moves[2][1], -10)
        self.assertEqual(moves[3][0], 0); self.assertGreater(moves[3][1], 10)


class NoFourScore(ShooterTest):
    """Two plain controllers: they report 1s after 8 bits, which must not
    be mistaken for players 3/4 holding every button."""
    four_score = False

    def test_no_phantom_players(self):
        self.m.press([0, 0, 0xFF, 0xFF], 60)
        self.assertEqual(self.m.peek("_pads_fourscore"), 0)
        self.assertEqual(self.m.array("_pads", N)[2:], [0, 0])
        self.assertEqual(self.active(), [1, 0, 0, 0])

    def test_two_players_still_work(self):
        self.m.press([0, A], 1)
        self.m.step(1)
        self.assertEqual(self.active(), [1, 1, 0, 0])
        x0 = self.crosshair(1)[0]
        self.m.press([0, RIGHT], 10)
        self.assertGreater(self.crosshair(1)[0], x0)


class Gameplay(ShooterTest):
    def test_each_player_can_shoot_an_enemy(self):
        self.join_all()
        for p in range(N):
            with self.subTest(player=p + 1):
                before = [self.score(q) for q in range(N)]
                self.bot_kill(p)
                after = [self.score(q) for q in range(N)]
                self.assertEqual(after[p], before[p] + 10)
                self.assertEqual([after[q] for q in range(N) if q != p],
                                 [before[q] for q in range(N) if q != p])
                self.m.poke("_lives", D["START_LIVES"])   # keep the round going

    def test_kill_is_shown_in_that_players_hud_slot(self):
        self.join_all()
        self.m.step(1)
        hud_before = self.m.frame[HUD_TEXT_ROWS].astype(int)
        self.bot_kill(1)
        self.m.step(2)   # HUD is uploaded during the next vblank
        hud_after = self.m.frame[HUD_TEXT_ROWS].astype(int)
        changed = np.nonzero(np.any(hud_before != hud_after, axis=(0, 2)))[0]
        self.assertTrue(len(changed) > 0, "HUD did not change after a kill")
        # Slot 2 occupies tile columns 9..14.
        self.assertTrue(((changed >= 9 * 8) & (changed < 15 * 8)).all(),
                        f"HUD changed outside player 2's slot: x={changed.min()}..{changed.max()}")

    def test_escapes_cost_lives_then_game_over_then_restart(self):
        self.join_all()
        lives = self.m.peek("_lives")
        flashed = False
        for _ in range(3000):
            self.m.step(1)
            now = self.m.peek("_lives")
            if now < lives:
                # The screen flashes red when a life is lost.
                self.m.step(1)
                sky = self.m.frame[40:160].reshape(-1, 3)
                flash = np.mean(np.all(sky == sky[0], axis=1)) > 0.9 and sky[0].sum() > 0
                if flash and not flashed:
                    # Every player's HUD text must stay visible on the flash.
                    for p in range(N):
                        slot = self.m.frame[HUD_TEXT_ROWS, (8 * p + 1) * 8:(8 * p + 7) * 8]
                        self.assertGreater(len({tuple(c) for c in slot.reshape(-1, 3)}), 1,
                                           f"player {p + 1} HUD invisible during flash")
                flashed |= flash
                lives = now
            if self.m.peek("_game_state") == D["STATE_OVER"]:
                break
        self.assertEqual(self.m.peek("_game_state"), D["STATE_OVER"])
        self.assertEqual(self.m.peek("_lives"), 0)
        self.assertTrue(flashed, "no red flash seen when a life was lost")

        # Frozen: scrolling stops and crosshairs ignore the D-pad.
        scroll = self.m.peek("_scroll_x")
        pos = self.crosshair(0)
        self.m.press([RIGHT], 30)
        self.assertEqual(self.m.peek("_scroll_x"), scroll)
        self.assertEqual(self.crosshair(0), pos)

        self.m.press([0, 0, START], 1)   # any player may restart
        self.m.step(1)
        self.assertEqual(self.m.peek("_game_state"), D["STATE_PLAY"])
        self.assertEqual(self.m.peek("_lives"), D["START_LIVES"])
        self.assertEqual(self.active(), [1, 1, 1, 1])


def white_pixels(frame, rows):
    region = frame[rows].reshape(-1, 3).astype(int)
    return int(np.sum(np.all(region > 200, axis=1)))


class TitleScreen(ShooterTest):
    start_game = False

    def test_title_waits_for_start(self):
        self.assertEqual(self.m.peek("_game_state"), D["STATE_TITLE"])
        scroll = self.m.peek("_scroll_x")
        self.m.press([0, A], 300)          # player 2 joins; nothing starts
        self.assertEqual(self.m.peek("_game_state"), D["STATE_TITLE"])
        self.assertEqual(self.m.peek("_scroll_x"), scroll)
        self.assertEqual(self.m.peek("_lives"), D["START_LIVES"])
        self.assertEqual(self.alive_enemies(), [])
        self.assertEqual(self.active()[:2], [1, 1])

    def test_title_text_is_removed_when_play_starts(self):
        title_rows = slice(9 * 8, 13 * 8)
        self.assertGreater(white_pixels(self.m.step(1), title_rows), 100)
        self.m.press([0, 0, START], 1)     # unjoined player 3 joins and starts
        self.m.step(4)
        self.assertEqual(self.m.peek("_game_state"), D["STATE_PLAY"])
        self.assertEqual(self.active()[2], 1)
        # Only stars (a few pixels each) remain.
        self.assertLess(white_pixels(self.m.frame, title_rows), 12)


class PauseAndSound(ShooterTest):
    def apu(self):
        return self.m.nes[APU_STATUS]

    def test_pause_freezes_everything(self):
        self.m.step(120)
        hud = self.m.frame[HUD_STATUS_ROWS].copy()
        self.m.press([START], 1)
        self.m.step(1)
        self.assertEqual(self.m.peek("_game_state"), D["STATE_PAUSE"])
        frozen = (self.m.peek("_scroll_x"), self.m.array("_enemy_x", E), self.crosshair(0))
        self.m.press([RIGHT | A], 60)
        self.assertEqual((self.m.peek("_scroll_x"), self.m.array("_enemy_x", E),
                          self.crosshair(0)), frozen)
        self.assertFalse(np.array_equal(hud, self.m.frame[HUD_STATUS_ROWS]), "no PAUSED text")
        self.m.press([START], 1)
        self.m.step(2)
        self.assertEqual(self.m.peek("_game_state"), D["STATE_PLAY"])
        self.assertNotEqual(self.m.peek("_scroll_x"), frozen[0])

    def test_sound_effects_reach_the_apu(self):
        # Silence all channels (clears length counters), then re-enable.
        self.m.nes[APU_STATUS] = 0x00
        self.m.nes[APU_STATUS] = 0x0F
        self.m.step(1)
        self.assertEqual(self.apu() & (CH_NOISE | CH_PULSE1 | CH_TRIANGLE), 0)
        self.m.press([A], 1)                       # shot -> noise
        self.m.step(1)
        self.assertTrue(self.apu() & CH_NOISE, "no gunshot sound")
        self.bot_kill(0)                           # hit -> pulse 1
        self.m.step(1)
        self.assertTrue(self.apu() & CH_PULSE1, "no hit sound")
        lives = self.m.peek("_lives")
        for _ in range(1500):                      # escape -> triangle
            self.m.step(1)
            if self.m.peek("_lives") < lives:
                break
        self.m.step(1)
        self.assertTrue(self.apu() & CH_TRIANGLE, "no life-lost sound")


class Armor(ShooterTest):
    def test_armored_enemy_needs_three_hits_and_scores_30(self):
        self.m.poke("_spawn_count", D["ARMOR_FIRST"])
        self.m.poke("_spawn_timer", 1)
        self.m.step(2)
        kinds = self.m.array("_enemy_kind", E)
        alive = self.alive_enemies()
        self.assertEqual(len(alive), 1)
        self.assertEqual(kinds[alive[0][0]], D["KIND_ARMOR"])
        self.m.poke("_spawn_timer", 255)
        before = self.score(0)
        self.bot_kill(0)   # keeps firing until the score changes
        self.assertEqual(self.score(0), before + 30)
        # It survived two hits first (hp 3 -> 1), and the second pulse
        # channel played the armour ping.
        self.assertEqual(self.m.peek("_enemy_hp", alive[0][0]), 1)
        self.assertTrue(self.m.nes[APU_STATUS] & CH_PULSE2, "no armour ping")


class Rendering(ShooterTest):
    def test_hud_is_static_while_playfield_scrolls(self):
        """The sprite-0 split keeps the HUD still while rows below scroll."""
        f1 = self.m.step(1).astype(int)
        f2 = self.m.step(1).astype(int)
        np.testing.assert_array_equal(f1[HUD_ALL_ROWS], f2[HUD_ALL_ROWS])
        g1, g2 = f1[GROUND_ROWS], f2[GROUND_ROWS]
        self.assertFalse(np.array_equal(g1, g2), "ground did not move")
        # Camera moves right, so scenery shifts one pixel left per frame.
        np.testing.assert_array_equal(g2[:, :-1], g1[:, 1:])

    def test_score_slots_use_player_colours(self):
        self.join_all()
        self.m.step(2)
        row = self.m.frame[HUD_TEXT_ROWS]
        bg = tuple(self.m.frame[0, 0])
        colours = []
        for p in range(N):
            slot = row[:, (8 * p + 1) * 8:(8 * p + 7) * 8].reshape(-1, 3)
            cs = {tuple(c) for c in slot} - {bg}
            self.assertEqual(len(cs), 1, f"slot {p + 1} colours: {cs}")
            colours.append(cs.pop())
        self.assertEqual(len(set(colours)), N, "player colours are not distinct")

    def test_game_over_text_appears(self):
        status_play = self.m.step(1)[HUD_STATUS_ROWS].astype(int)
        self.m.poke("_lives", 1)
        for _ in range(1500):
            self.m.step(1)
            if self.m.peek("_game_state") == D["STATE_OVER"]:
                break
        self.m.step(2)
        status_over = self.m.frame[HUD_STATUS_ROWS].astype(int)
        self.assertFalse(np.array_equal(status_play, status_over))


class Performance(ShooterTest):
    def test_no_lag_frames_with_four_players_and_full_enemy_waves(self):
        """The main loop must fit in one frame, or the split and scrolling
        stutter. Stress it: 4 players moving and shooting, max enemies."""
        self.join_all()
        rng = np.random.default_rng(1)
        dirs = [0, UP, DOWN, LEFT, RIGHT, UP | LEFT, DOWN | RIGHT]
        max_alive = 0
        for t in range(60 * 40):
            if t % 60 == 0:
                self.m.poke("_lives", 9)
                self.m.poke("_spawn_interval", D["SPAWN_MIN"])
            pads = [int(rng.choice(dirs)) | (A if t % 2 else 0) for _ in range(N)]
            self.m.set_pads(pads)
            self.m.step(1)
            max_alive = max(max_alive, len(self.alive_enemies()))
        self.assertEqual(self.m.peek("_game_state"), D["STATE_PLAY"])
        self.assertEqual(max_alive, E)
        self.assertEqual(self.m.peek("_lag_frames"), 0)


if __name__ == "__main__":
    unittest.main()

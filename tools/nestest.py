"""Helpers for emulator-driven ROM tests (cynes headless NES emulator).

Typical use from games/<game>/tests/test_*.py:

    sys.path.insert(0, <repo>/tools); import nestest
    m = nestest.Machine("shooter", four_score=True)
    m.step(30)
    m.press([nestest.RIGHT, 0, nestest.A], frames=10)   # pads 1, 2, 3
    m.peek("_lives"); m.array("_cross_x", 4)

C globals are looked up by their assembler name (leading underscore) in the
ld65 label file, so tests follow the code without hard-coded addresses.
"""
import os
import re
import unittest

import cynes

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Controller bits as cynes expects them (first bit shifted out = A = 0x80).
A, B, SELECT, START = (cynes.NES_INPUT_A, cynes.NES_INPUT_B,
                       cynes.NES_INPUT_SELECT, cynes.NES_INPUT_START)
UP, DOWN, LEFT, RIGHT = (cynes.NES_INPUT_UP, cynes.NES_INPUT_DOWN,
                         cynes.NES_INPUT_LEFT, cynes.NES_INPUT_RIGHT)


def to_neslib(bits):
    """cynes button bits -> neslib pad_poll()/PAD_* bits (bit-reversed)."""
    return int(f"{bits:08b}"[::-1], 2)


def has_four_score():
    """True if cynes was built with tools/cynes-fourscore.patch."""
    return hasattr(cynes.NES, "four_score")


def require_four_score(test):
    """Skip (or fail, when REQUIRE_FOURSCORE=1 as in CI) without the patch."""
    if has_four_score():
        return
    msg = "cynes lacks Four Score support; run tools/install_cynes.sh"
    if os.environ.get("REQUIRE_FOURSCORE"):
        test.fail(msg)
    raise unittest.SkipTest(msg)


def rom_path(game):
    return os.path.join(ROOT, "build", game + ".nes")


def load_labels(game):
    labels = {}
    with open(os.path.join(ROOT, "build", game + ".labels")) as f:
        for line in f:
            m = re.match(r"al ([0-9A-Fa-f]+) \.(\S+)", line)
            if m:
                labels[m.group(2)] = int(m.group(1), 16)
    return labels


def load_defines(*rel_paths):
    """Numeric #defines from C headers, so tests track the source."""
    defs = {}
    for rel in rel_paths:
        with open(os.path.join(ROOT, rel)) as f:
            for line in f:
                m = re.match(r"#define (\w+)\s+(.+?)\s*(/\*.*)?$", line)
                if m:
                    try:
                        defs[m.group(1)] = eval(m.group(2), {}, dict(defs))
                    except Exception:
                        pass
    return defs


class Machine:
    def __init__(self, game, four_score=False):
        self.nes = cynes.NES(rom_path(game))
        self.labels = load_labels(game)
        if four_score:
            self.nes.four_score = True
        self.frame = None

    def addr(self, name):
        return self.labels[name]

    def peek(self, name, offset=0):
        return self.nes[self.labels[name] + offset]

    def poke(self, name, value, offset=0):
        self.nes[self.labels[name] + offset] = value

    def array(self, name, n):
        base = self.labels[name]
        return [self.nes[base + i] for i in range(n)]

    def set_pads(self, pads):
        value = 0
        for i, bits in enumerate(pads):
            value |= (bits & 0xFF) << (8 * i)
        self.nes.controller = value

    def step(self, frames=1):
        # cynes returns a live view of its framebuffer; copy so frames can
        # be compared across steps.
        self.frame = self.nes.step(frames).copy()
        if self.nes.has_crashed:
            raise AssertionError("CPU hit an illegal opcode")
        return self.frame

    def press(self, pads, frames=1):
        """Hold pads (list of per-controller cynes bits) then release."""
        self.set_pads(pads)
        self.step(frames)
        self.set_pads([])
        return self.frame

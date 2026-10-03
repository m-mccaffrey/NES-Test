# NES-Test

A minimal NES game written in C with [cc65](https://cc65.github.io/) and
Shiru's [neslib](https://github.com/clbr/neslib): an 8×8 sprite you move
with the D-pad, kept inside the screen.

## Build

```sh
sudo apt-get install cc65     # or build cc65 from source
make                          # -> build/game.nes  (NROM, mapper 0)
```

Open `build/game.nes` in any emulator (Mesen, FCEUX, Nestopia…).

## Testing

There are three layers:

| Layer | Command | What it checks |
|---|---|---|
| Compile-time | `make` | `#error` if `player.h` button bits drift from neslib's `PAD_*` |
| Host unit tests | `make unit-test` | `src/player.c` (pure logic, no hardware) built with the system `cc`: start position, each direction, diagonals, screen clamping |
| Emulator tests | `make rom-test` | Boots the **real ROM** in the [cynes](https://github.com/Youlixx/cynes) headless emulator, presses buttons, and inspects RAM (`_player`, found via the ld65 label file), the OAM buffer at `$0200`, and the rendered framebuffer pixels |

`make test` runs both suites. Emulator tests need `pip install cynes numpy`.

`make screenshot HOLD="right down" FRAMES=120` saves `build/screenshot.png`
after holding the given buttons, handy for eyeballing changes without an
emulator UI.

GitHub Actions (`.github/workflows/ci.yml`) builds, runs all tests, and
uploads the ROM plus a screenshot as an artifact.

Keep game logic in hardware-free files like `player.c` so it can be unit-tested
on the host. Use the emulator tests for anything involving neslib, the PPU,
or input.

## Layout

```
src/main.c          neslib setup + main loop
src/player.[ch]     movement logic (host-testable)
tools/make_chr.py   generates the CHR-ROM from ASCII-art tiles
tools/screenshot.py headless PNG capture
nes.cfg             ld65 linker config (NROM-256)
lib/neslib/         neslib (clbr fork), crt0.s lightly patched:
                    SFX off, stub music header, CHARS includes tiles.chr
tests/              test_player.c (host), test_rom.py (emulator)
```

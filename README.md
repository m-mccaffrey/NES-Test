# NES-Test

NES games in C with [cc65](https://cc65.github.io/) and Shiru's
[neslib](https://github.com/clbr/neslib), plus automated tests that run the
real ROMs in a headless emulator.

| ROM | What it is |
|---|---|
| `build/shooter.nes` | 4-player rail shooter (NES Four Score / Famicom 4-player) |
| `build/demo.nes` | An 8×8 sprite you move with the D-pad |

## Build

```sh
sudo apt-get install cc65
make                  # all ROMs; or `make shooter`, `make demo`
```

Open a ROM in any emulator (Mesen, FCEUX, Nestopia…). For 4 players, enable
the Four Score / NES Satellite (or Famicom 4-player adapter) in the emulator's
input settings.

## Rail shooter

The camera moves right along a scrolling landscape while bats fly in from
the right. Each player aims a crosshair in their own colour and shoots.

- The title screen ("RAIL RAIDERS") waits for **Start**. Players can join there with **A**.
- **D-pad** aims, **A** fires (one shot per press, 10-frame cooldown), **Start** pauses and resumes.
- Player 1 starts in the game. Players 2–4 join any time by pressing **A** or **Start**.
- A hit gives 10 points to whoever made it. The explosion is drawn in that player's colour.
- A bat that reaches the left edge costs the team one of its 5 shared lives, and the screen flashes.
- At 0 lives it's game over; any player presses **Start** to play again.
- Bats spawn faster over time, with up to 6 on screen at once.
- From the 9th spawn on, every 4th enemy is an armoured beetle. It takes 3 hits (and flickers when hit), moves 1.5× faster and is worth 30 points.
- Sound effects are written straight to the APU: gunshot (noise), hit (falling pulse), armour ping, join/pause blip (pulse 2) and life lost (triangle). neslib's FamiTone update is disabled, because with no music it would overwrite them every frame.

How it's put together:

- `games/shooter/game.c` holds all the rules and touches no hardware, so it also compiles on your PC for unit tests.
- `games/shooter/main.c` draws everything and runs the frame loop.
- `games/shooter/pads.s` reads all 4 controllers. It reads 24 bits from each port and accepts players 3/4 only when the Four Score signature is present. Plain controllers send 1s after their 8 bits, so without that check players 3/4 would look like they were holding every button. Famicom expansion-port controllers (D1) are supported as well.
- The HUD (scores in each player's colour, lives) stays still above the scrolling playfield. A sprite-0 hit triggers neslib's `split()`.
- Sprites are drawn with the crosshairs first, so the 8-sprites-per-line limit never hides a crosshair. The enemy draw order rotates each frame, so overflow flickers instead of hiding the same bat every frame.

## Testing

```sh
pip install numpy
./tools/install_cynes.sh     # cynes emulator + Four Score patch (needs cmake, a C++ compiler)
make test                    # everything; or test-shooter, unit-test, rom-test, ...
```

There are three layers:

| Layer | Where | What it checks |
|---|---|---|
| Compile-time | `main.c` | `#error` if the game's button bits drift from neslib's `PAD_*` |
| Host unit tests | `games/*/tests/test_*.c` | The hardware-free game logic, built with the system `cc`. For the shooter: joining, independent crosshairs, clamping, hitbox edges, cooldown/no auto-fire, one kill per shot, score carry and cap, spawning and difficulty ramp, lives, game over, restart |
| Emulator tests | `games/*/tests/test_*.py` | The **real ROM** in [cynes](https://github.com/Youlixx/cynes), driven through its controller ports. Checks RAM (C globals found via the ld65 label file), OAM, and the rendered pixels |

Shooter emulator tests:

- **Input:** every button on every one of the 4 pads arrives at the right bit. With the Four Score unplugged there are no phantom players 3/4, and 2 players still work.
- **Gameplay:** for each of the 4 players, a bot steers the crosshair onto a bat and shoots it. The test checks that only that player's score and HUD slot change. Another test lets bats escape, then checks lives, the red flash (with every HUD colour still visible), game over, the freeze, and restart.
- **Rendering:** the HUD rows stay pixel-identical while the ground shifts exactly 1 px per frame, which shows the split works. The score slots use 4 distinct player colours, and the game-over text appears.
- **Title, pause, armour:** the title screen holds until Start and its text is cleared when play begins. Pause freezes scrolling, enemies and crosshairs and shows PAUSED. An armoured enemy survives two hits and scores 30 on the third.
- **Sound:** each effect is confirmed on the right APU channel by reading the `$4015` length-counter status.
- **Performance:** 4 players moving and firing against full enemy waves for 40 seconds produce zero lag frames. The ROM counts main-loop overruns in `lag_frames`.

The tests have been checked against deliberately broken builds. With the
signature check removed, the split removed, a slow main loop, the sound
calls removed, or the title text left behind, the matching test fails.

cynes upstream only emulates 2 controllers. `tools/cynes-fourscore.patch`
(about 50 lines, against a pinned commit) adds `nes.four_score = True` and a
32-bit `nes.controller` (8 bits per pad). It also makes plain controllers
return 1 after 8 reads, as real ones do. Without the patch, the 4-player
tests skip, or fail when `REQUIRE_FOURSCORE=1` is set, as it is in CI.

`make screenshot GAME=shooter FRAMES=300 FOURSCORE=1 HOLD="1:right,a 3:start"`
saves `build/shooter.png`.

GitHub Actions (`.github/workflows/ci.yml`) builds the ROMs, installs the
patched emulator, runs every test, and uploads the ROMs and screenshots.

## Layout

```
games/<name>/       one directory per ROM -> build/<name>.nes
  main.c            hardware side (neslib); other .c files must be hardware-free
  tiles.txt         CHR tiles as ASCII art (tools/make_chr.py)
  tests/            test_*.c (host unit tests), test_*.py (emulator tests)
assets/font.txt     5x7 font shared by games
lib/neslib/         neslib (clbr fork), lightly patched: SFX off, stub music
                    header, CHARS includes the game's tiles.chr, oam_off defined
nes.cfg             ld65 config (NROM-256, mapper 0)
tools/              make_chr.py, screenshot.py, nestest.py (test helpers),
                    install_cynes.sh + cynes-fourscore.patch
```

Adding a game means creating `games/<name>/` with a `main.c` and a `tiles.txt`.
The Makefile picks it up automatically.

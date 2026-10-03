#ifndef PLAYER_H
#define PLAYER_H

/* Pure game logic: no neslib / hardware dependencies, so it can be
   compiled and unit-tested on the host with a normal C compiler. */

/* D-pad bits, matching neslib's PAD_* values (pad_poll() bit order).
   tests/test_rom.py checks this against the real ROM. */
#define BTN_UP    0x10
#define BTN_DOWN  0x20
#define BTN_LEFT  0x40
#define BTN_RIGHT 0x80

/* Visible play area for an 8x8 sprite (top/bottom 8px are often
   overscanned on NTSC TVs). */
#define PLAYER_MIN_X 0
#define PLAYER_MAX_X (256 - 8)
#define PLAYER_MIN_Y 8
#define PLAYER_MAX_Y (240 - 16)

#define PLAYER_SPEED 1

#define PLAYER_START_X 124
#define PLAYER_START_Y 112

typedef struct {
    unsigned char x;
    unsigned char y;
} player_t;

void player_init(player_t *p);
void player_update(player_t *p, unsigned char pad);

#endif

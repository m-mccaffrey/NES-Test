#ifndef GAME_H
#define GAME_H

/* Rail-shooter game logic. No neslib or hardware access, so this file and
   game.c compile on the host for unit tests (tests/test_game.c). main.c
   reads the pads, calls game_update() once per frame and draws the result.

   State is kept in global arrays rather than structs: cc65 generates much
   faster code for fixed-address arrays than for pointer-to-struct access. */

#define NUM_PLAYERS 4
#define MAX_ENEMIES 6

/* Button bits, in neslib pad_poll() order. */
#define BTN_A      0x01
#define BTN_B      0x02
#define BTN_SELECT 0x04
#define BTN_START  0x08
#define BTN_UP     0x10
#define BTN_DOWN   0x20
#define BTN_LEFT   0x40
#define BTN_RIGHT  0x80

/* Screen coordinates are sprite top-left. The top 32 lines are the HUD. */
#define CROSS_MIN_X 0
#define CROSS_MAX_X 248
#define CROSS_MIN_Y 32
#define CROSS_MAX_Y 224
#define CROSS_SPEED 2
#define CROSS_HOT   3       /* hotspot offset inside the 8x8 crosshair */

#define SHOT_COOLDOWN 10    /* frames between shots */
#define SHOT_FLASH    4     /* frames the "firing" crosshair is shown */

#define ENEMY_SIZE     16
#define ENEMY_SPAWN_X  232
#define ENEMY_MIN_Y    48   /* spawn height is ENEMY_MIN_Y + (rand & 127) */
#define ENEMY_BOB      12   /* vertical bob amplitude */
#define EXPLODE_FRAMES 16

#define SPAWN_FIRST    60   /* frames before the first enemy */
#define SPAWN_START    90   /* initial frames between spawns */
#define SPAWN_MIN      30   /* fastest spawn rate */
#define SPAWN_RAMP     2    /* spawn interval shrinks by this per spawn */

#define START_LIVES    5

#define KIND_BAT       0
#define KIND_ARMOR     1    /* takes ARMOR_HP hits, faster, worth 30 */
#define ARMOR_HP       3
#define ARMOR_FIRST    8    /* spawns before armoured enemies appear */
#define HIT_FLASH      6    /* frames an armoured enemy flashes when hit */

#define STATE_PLAY 0
#define STATE_OVER 1
#define STATE_TITLE 2
#define STATE_PAUSE 3

#define ENEMY_NONE      0
#define ENEMY_ALIVE     1
#define ENEMY_EXPLODING 2

/* game_events bits: what happened during the last game_update(). */
#define EV_SHOT      0x01
#define EV_HIT       0x02
#define EV_LIFE_LOST 0x04
#define EV_GAME_OVER 0x08
#define EV_RESTART   0x10
#define EV_JOIN      0x20
#define EV_ARMOR     0x40  /* hit an armoured enemy without killing it */
#define EV_PAUSE     0x80  /* paused, unpaused or started from the title */

extern unsigned char game_state;
extern unsigned char game_events;
extern unsigned char game_frame;
extern unsigned char lives;

extern unsigned char player_active[NUM_PLAYERS];
extern unsigned char cross_x[NUM_PLAYERS];
extern unsigned char cross_y[NUM_PLAYERS];
extern unsigned char shot_timer[NUM_PLAYERS];
extern unsigned char score[NUM_PLAYERS][4];   /* decimal digits, MSD first */

extern unsigned char enemy_state[MAX_ENEMIES];
extern unsigned char enemy_x[MAX_ENEMIES];
extern unsigned char enemy_y[MAX_ENEMIES];
extern unsigned char enemy_base_y[MAX_ENEMIES];
extern unsigned char enemy_timer[MAX_ENEMIES]; /* age, or explosion countdown */
extern unsigned char enemy_owner[MAX_ENEMIES]; /* player who destroyed it */
extern unsigned char enemy_kind[MAX_ENEMIES];
extern unsigned char enemy_hp[MAX_ENEMIES];
extern unsigned char enemy_flash[MAX_ENEMIES]; /* hit flash countdown */
extern unsigned char spawn_count;              /* saturates at 255 */

extern unsigned char spawn_timer;
extern unsigned char spawn_interval;
extern unsigned char rng;

/* Power-on reset: title screen, only player 1 joined. */
void game_init(void);

/* New round with the same players: scores, lives and enemies reset. */
void game_restart(void);

/* Advance one frame. pads[i] is player i's buttons (BTN_* bits). */
void game_update(const unsigned char *pads);

#endif

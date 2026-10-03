#include "game.h"

unsigned char game_state;
unsigned char game_events;
unsigned char game_frame;
unsigned char lives;

unsigned char player_active[NUM_PLAYERS];
unsigned char cross_x[NUM_PLAYERS];
unsigned char cross_y[NUM_PLAYERS];
unsigned char shot_timer[NUM_PLAYERS];
unsigned char score[NUM_PLAYERS][4];

unsigned char enemy_state[MAX_ENEMIES];
unsigned char enemy_x[MAX_ENEMIES];
unsigned char enemy_y[MAX_ENEMIES];
unsigned char enemy_base_y[MAX_ENEMIES];
unsigned char enemy_timer[MAX_ENEMIES];
unsigned char enemy_owner[MAX_ENEMIES];
unsigned char enemy_kind[MAX_ENEMIES];
unsigned char enemy_hp[MAX_ENEMIES];
unsigned char enemy_flash[MAX_ENEMIES];
unsigned char spawn_count;

unsigned char spawn_timer;
unsigned char spawn_interval;
unsigned char rng;

static unsigned char pad_prev[NUM_PLAYERS];

/* Scratch variables: file-static is much faster than locals under cc65. */
static unsigned char p, j, d, pad, pressed, hx, hy, restart, start, pause;

#define CROSS_START_Y 128
static const unsigned char cross_start_x[NUM_PLAYERS] = { 64, 104, 144, 184 };

/* ENEMY_BOB + ENEMY_BOB * sin(i * 2pi / 16) */
static const unsigned char bob[16] = {
    12, 17, 20, 23, 24, 23, 20, 17, 12, 7, 4, 1, 0, 1, 4, 7
};

/* 8-bit Galois LFSR, period 255 (rng must never be 0). */
static unsigned char next_rand(void)
{
    if (rng & 1)
        rng = (rng >> 1) ^ 0xB8;
    else
        rng >>= 1;
    return rng;
}

static void reset_crosshair(void)
{
    cross_x[p] = cross_start_x[p];
    cross_y[p] = CROSS_START_Y;
    shot_timer[p] = 0;
}

void game_restart(void)
{
    for (p = 0; p < NUM_PLAYERS; ++p) {
        reset_crosshair();
        score[p][0] = score[p][1] = score[p][2] = score[p][3] = 0;
    }
    for (j = 0; j < MAX_ENEMIES; ++j)
        enemy_state[j] = ENEMY_NONE;

    lives = START_LIVES;
    spawn_count = 0;
    spawn_timer = SPAWN_FIRST;
    spawn_interval = SPAWN_START;
    game_state = STATE_PLAY;
}

void game_init(void)
{
    for (p = 0; p < NUM_PLAYERS; ++p) {
        player_active[p] = 0;
        pad_prev[p] = 0;
    }
    player_active[0] = 1;
    rng = 0x5A;
    game_frame = 0;
    game_restart();
    game_state = STATE_TITLE;
}

/* +10 points for player p (tens digit, with carry), capped at 9990. */
static void add_score(void)
{
    if (score[p][0] == 9 && score[p][1] == 9 && score[p][2] == 9)
        return;
    for (d = 2; score[p][d] == 9; --d)
        score[p][d] = 0;
    ++score[p][d];
}

static void move_crosshair(void)
{
    if (pad & BTN_LEFT)
        cross_x[p] = cross_x[p] >= CROSS_MIN_X + CROSS_SPEED ? cross_x[p] - CROSS_SPEED : CROSS_MIN_X;
    else if (pad & BTN_RIGHT)
        cross_x[p] = cross_x[p] <= CROSS_MAX_X - CROSS_SPEED ? cross_x[p] + CROSS_SPEED : CROSS_MAX_X;

    if (pad & BTN_UP)
        cross_y[p] = cross_y[p] >= CROSS_MIN_Y + CROSS_SPEED ? cross_y[p] - CROSS_SPEED : CROSS_MIN_Y;
    else if (pad & BTN_DOWN)
        cross_y[p] = cross_y[p] <= CROSS_MAX_Y - CROSS_SPEED ? cross_y[p] + CROSS_SPEED : CROSS_MAX_Y;
}

/* Player p pulls the trigger: destroy the first live enemy under the
   crosshair's hotspot. */
static void fire(void)
{
    shot_timer[p] = SHOT_COOLDOWN;
    game_events |= EV_SHOT;

    hx = cross_x[p] + CROSS_HOT;
    hy = cross_y[p] + CROSS_HOT;

    for (j = 0; j < MAX_ENEMIES; ++j) {
        /* Unsigned wrap-around turns "hx < x" into a large value, so one
           compare per axis covers both edges. */
        if (enemy_state[j] == ENEMY_ALIVE &&
            (unsigned char)(hx - enemy_x[j]) < ENEMY_SIZE &&
            (unsigned char)(hy - enemy_y[j]) < ENEMY_SIZE) {
            enemy_owner[j] = p;
            if (enemy_hp[j] > 1) {
                --enemy_hp[j];
                enemy_flash[j] = HIT_FLASH;
                game_events |= EV_ARMOR;
                return;
            }
            enemy_state[j] = ENEMY_EXPLODING;
            enemy_timer[j] = EXPLODE_FRAMES;
            add_score();
            if (enemy_kind[j] == KIND_ARMOR) {
                add_score();
                add_score();
            }
            game_events |= EV_HIT;
            return;
        }
    }
}

static void update_players(const unsigned char *pads)
{
    restart = start = pause = 0;

    for (p = 0; p < NUM_PLAYERS; ++p) {
        pad = pads[p];
        pressed = pad & ~pad_prev[p];
        pad_prev[p] = pad;

        if (!player_active[p]) {
            /* Drop-in: A or START joins at any time. */
            if (pressed & (BTN_A | BTN_START)) {
                player_active[p] = 1;
                reset_crosshair();
                game_events |= EV_JOIN;
            }
            if (game_state != STATE_TITLE || !(pressed & BTN_START))
                continue;
        }

        if (game_state == STATE_TITLE) {
            if (pressed & BTN_START)
                start = 1;
            continue;
        }

        if (game_state == STATE_PAUSE) {
            if (pressed & BTN_START)
                pause = 1;
            continue;
        }

        if (game_state == STATE_PLAY && (pressed & BTN_START)) {
            pause = 1;
            continue;
        }

        if (game_state == STATE_OVER) {
            if (pressed & BTN_START)
                restart = 1;
            continue;
        }

        if (shot_timer[p])
            --shot_timer[p];
        move_crosshair();
        if ((pressed & BTN_A) && !shot_timer[p])
            fire();
    }
}

static void lose_life(void)
{
    game_events |= EV_LIFE_LOST;
    if (lives)
        --lives;
    if (!lives) {
        game_state = STATE_OVER;
        game_events |= EV_GAME_OVER;
    }
}

static void update_enemies(void)
{
    for (j = 0; j < MAX_ENEMIES; ++j) {
        if (enemy_state[j] == ENEMY_EXPLODING && game_state != STATE_PAUSE) {
            if (--enemy_timer[j] == 0)
                enemy_state[j] = ENEMY_NONE;
        } else if (enemy_state[j] == ENEMY_ALIVE && game_state == STATE_PLAY) {
            if (enemy_flash[j])
                --enemy_flash[j];
            /* Armoured enemies move an extra pixel every other frame. */
            if (enemy_kind[j] == KIND_ARMOR && (game_frame & 1) && enemy_x[j])
                --enemy_x[j];
            if (enemy_x[j] == 0) {
                /* Got past the players. */
                enemy_state[j] = ENEMY_NONE;
                lose_life();
            } else {
                --enemy_x[j];
                ++enemy_timer[j];
                enemy_y[j] = enemy_base_y[j] - ENEMY_BOB + bob[(enemy_timer[j] >> 2) & 15];
            }
        }
    }
}

static void spawn_enemies(void)
{
    if (game_state != STATE_PLAY || --spawn_timer)
        return;

    spawn_timer = spawn_interval;
    if (spawn_interval >= SPAWN_MIN + SPAWN_RAMP)
        spawn_interval -= SPAWN_RAMP;
    else
        spawn_interval = SPAWN_MIN;

    for (j = 0; j < MAX_ENEMIES; ++j) {
        if (enemy_state[j] == ENEMY_NONE) {
            enemy_state[j] = ENEMY_ALIVE;
            enemy_x[j] = ENEMY_SPAWN_X;
            enemy_base_y[j] = ENEMY_MIN_Y + (next_rand() & 127);
            enemy_y[j] = enemy_base_y[j];
            enemy_timer[j] = 0;
            enemy_flash[j] = 0;
            if (spawn_count >= ARMOR_FIRST && (spawn_count & 3) == 0) {
                enemy_kind[j] = KIND_ARMOR;
                enemy_hp[j] = ARMOR_HP;
            } else {
                enemy_kind[j] = KIND_BAT;
                enemy_hp[j] = 1;
            }
            if (spawn_count != 255)
                ++spawn_count;
            return;
        }
    }
}

void game_update(const unsigned char *pads)
{
    game_events = 0;
    ++game_frame;

    update_players(pads);
    if (start) {
        game_restart();
        game_events |= EV_PAUSE;
        return;
    }
    if (pause) {
        game_state = game_state == STATE_PAUSE ? STATE_PLAY : STATE_PAUSE;
        game_events |= EV_PAUSE;
        return;
    }
    if (restart) {
        game_restart();
        game_events |= EV_RESTART;
        return;
    }
    update_enemies();
    spawn_enemies();
}

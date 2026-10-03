/* Host-side unit tests for game.c (compiled with the system cc). */
#include <stdio.h>
#include <string.h>
#include "game.h"

static int failures;

#define CHECK_EQ(actual, expected) do { \
    int a_ = (actual), e_ = (expected); \
    if (a_ != e_) { \
        printf("%s:%d: %s == %d, expected %d\n", __FILE__, __LINE__, #actual, a_, e_); \
        failures++; \
    } \
} while (0)

static unsigned char pads[NUM_PLAYERS];

static void frames(int n)
{
    while (n--)
        game_update(pads);
}

/* Press and release a button on one pad (2 frames). */
static void tap(int player, unsigned char button)
{
    pads[player] |= button;
    frames(1);
    pads[player] &= ~button;
    frames(1);
}

static int score_value(int player)
{
    return score[player][0] * 1000 + score[player][1] * 100 +
           score[player][2] * 10 + score[player][3];
}

static int count_enemies(unsigned char state)
{
    int i, n = 0;
    for (i = 0; i < MAX_ENEMIES; i++)
        n += enemy_state[i] == state;
    return n;
}

/* Put a live enemy in slot 0 right under player's crosshair hotspot. */
static void enemy_under_crosshair(int player)
{
    enemy_state[0] = ENEMY_ALIVE;
    enemy_x[0] = cross_x[player] + CROSS_HOT - 4;
    enemy_y[0] = enemy_base_y[0] = cross_y[player] + CROSS_HOT - 4;
    enemy_timer[0] = 0;
}

static void setup(void)
{
    memset(pads, 0, sizeof pads);
    game_init();
    game_restart();      /* skip the title screen */
    spawn_timer = 255;   /* keep random spawns out of the way */
}

static void test_init(void)
{
    setup();
    CHECK_EQ(player_active[0], 1);
    CHECK_EQ(player_active[1], 0);
    CHECK_EQ(player_active[2], 0);
    CHECK_EQ(player_active[3], 0);
    CHECK_EQ(lives, START_LIVES);
    CHECK_EQ(game_state, STATE_PLAY);
    CHECK_EQ(count_enemies(ENEMY_NONE), MAX_ENEMIES);
}

static void test_join(void)
{
    setup();
    pads[2] = BTN_A;
    frames(1);
    CHECK_EQ(player_active[2], 1);
    CHECK_EQ(game_events & EV_JOIN, EV_JOIN);
    /* The join press must not also fire. */
    frames(5);
    CHECK_EQ(shot_timer[2], 0);

    setup();
    tap(3, BTN_START);
    CHECK_EQ(player_active[3], 1);

    /* Other buttons don't join. */
    setup();
    pads[1] = BTN_RIGHT | BTN_B | BTN_SELECT;
    frames(10);
    CHECK_EQ(player_active[1], 0);
}

static void test_inactive_players_ignore_input(void)
{
    int x;
    setup();
    x = cross_x[1];
    pads[1] = BTN_RIGHT;
    frames(10);
    CHECK_EQ(cross_x[1], x);
}

static void test_crosshairs_move_independently(void)
{
    int p, x[NUM_PLAYERS], y[NUM_PLAYERS];
    setup();
    for (p = 1; p < NUM_PLAYERS; p++)
        tap(p, BTN_A);
    for (p = 0; p < NUM_PLAYERS; p++) {
        x[p] = cross_x[p];
        y[p] = cross_y[p];
    }
    pads[0] = BTN_LEFT;
    pads[1] = BTN_RIGHT;
    pads[2] = BTN_UP;
    pads[3] = BTN_DOWN;
    frames(5);
    CHECK_EQ(cross_x[0], x[0] - 5 * CROSS_SPEED); CHECK_EQ(cross_y[0], y[0]);
    CHECK_EQ(cross_x[1], x[1] + 5 * CROSS_SPEED); CHECK_EQ(cross_y[1], y[1]);
    CHECK_EQ(cross_x[2], x[2]); CHECK_EQ(cross_y[2], y[2] - 5 * CROSS_SPEED);
    CHECK_EQ(cross_x[3], x[3]); CHECK_EQ(cross_y[3], y[3] + 5 * CROSS_SPEED);
}

static void test_crosshair_clamped(void)
{
    setup();
    pads[0] = BTN_LEFT | BTN_UP;
    frames(200);
    CHECK_EQ(cross_x[0], CROSS_MIN_X);
    CHECK_EQ(cross_y[0], CROSS_MIN_Y);
    pads[0] = BTN_RIGHT | BTN_DOWN;
    frames(200);
    CHECK_EQ(cross_x[0], CROSS_MAX_X);
    CHECK_EQ(cross_y[0], CROSS_MAX_Y);
}

static void test_shoot_hits_enemy(void)
{
    setup();
    tap(1, BTN_A);            /* player 2 joins */
    enemy_under_crosshair(1);
    tap(1, BTN_A);
    CHECK_EQ(enemy_state[0], ENEMY_EXPLODING);
    CHECK_EQ(enemy_owner[0], 1);
    CHECK_EQ(score_value(1), 10);
    CHECK_EQ(score_value(0), 0);
    frames(EXPLODE_FRAMES);
    CHECK_EQ(enemy_state[0], ENEMY_NONE);
}

static void test_shot_hitbox_edges(void)
{
    /* Hotspot exactly on each corner of the 16x16 box hits; one pixel
       outside misses. */
    static const int offsets[][3] = {
        { 0, 0, 1 }, { 15, 0, 1 }, { 0, 15, 1 }, { 15, 15, 1 },
        { -1, 0, 0 }, { 16, 0, 0 }, { 0, -1, 0 }, { 0, 16, 0 },
    };
    unsigned i;
    for (i = 0; i < sizeof offsets / sizeof offsets[0]; i++) {
        setup();
        enemy_state[0] = ENEMY_ALIVE;
        enemy_x[0] = cross_x[0] + CROSS_HOT - offsets[i][0];
        enemy_y[0] = enemy_base_y[0] = cross_y[0] + CROSS_HOT - offsets[i][1];
        enemy_timer[0] = 0;
        /* Fire on the same frame the enemy is placed, before it moves. */
        pads[0] = BTN_A;
        frames(1);
        if (offsets[i][2] != (enemy_state[0] == ENEMY_EXPLODING))
            printf("hitbox offset (%d,%d): expected %s\n", offsets[i][0],
                   offsets[i][1], offsets[i][2] ? "hit" : "miss");
        CHECK_EQ(enemy_state[0] == ENEMY_EXPLODING, offsets[i][2]);
    }
}

static void test_miss_scores_nothing(void)
{
    setup();
    enemy_state[0] = ENEMY_ALIVE;
    enemy_x[0] = 200;
    enemy_y[0] = enemy_base_y[0] = 60;
    cross_x[0] = 20;
    cross_y[0] = 150;
    tap(0, BTN_A);
    CHECK_EQ(enemy_state[0], ENEMY_ALIVE);
    CHECK_EQ(score_value(0), 0);
    CHECK_EQ(game_events & EV_HIT, 0);
}

static void test_fire_needs_new_press_and_cooldown(void)
{
    int i, shots;
    setup();
    pads[0] = BTN_A;
    frames(1);
    CHECK_EQ(shot_timer[0], SHOT_COOLDOWN);
    /* Holding A doesn't auto-fire. */
    frames(SHOT_COOLDOWN * 3);
    CHECK_EQ(shot_timer[0], 0);

    /* Mashing every other frame for 40 frames only fires on frames
       0, 10, 20 and 30. */
    setup();
    shots = 0;
    for (i = 0; i < 40; i++) {
        pads[0] = (i & 1) ? 0 : BTN_A;
        frames(1);
        shots += (game_events & EV_SHOT) != 0;
    }
    CHECK_EQ(shots, 40 / SHOT_COOLDOWN);
}

static void test_one_kill_per_shot(void)
{
    setup();
    enemy_under_crosshair(0);
    enemy_state[1] = ENEMY_ALIVE;
    enemy_x[1] = enemy_x[0];
    enemy_y[1] = enemy_base_y[1] = enemy_y[0];
    enemy_timer[1] = 0;
    tap(0, BTN_A);
    CHECK_EQ(count_enemies(ENEMY_EXPLODING), 1);
    CHECK_EQ(score_value(0), 10);
}

static void test_score_carry_and_cap(void)
{
    setup();
    score[0][0] = 0; score[0][1] = 0; score[0][2] = 9; score[0][3] = 0;
    enemy_under_crosshair(0);
    tap(0, BTN_A);
    CHECK_EQ(score_value(0), 100);

    setup();
    score[0][0] = 9; score[0][1] = 9; score[0][2] = 9; score[0][3] = 0;
    enemy_under_crosshair(0);
    tap(0, BTN_A);
    CHECK_EQ(score_value(0), 9990);
}

static void test_enemies_spawn_and_move_left(void)
{
    int x;
    setup();
    spawn_timer = 1;
    frames(1);
    CHECK_EQ(count_enemies(ENEMY_ALIVE), 1);
    CHECK_EQ(enemy_x[0], ENEMY_SPAWN_X);
    CHECK_EQ(enemy_y[0] >= ENEMY_MIN_Y && enemy_y[0] < ENEMY_MIN_Y + 128, 1);
    x = enemy_x[0];
    frames(10);
    CHECK_EQ(enemy_x[0], x - 10);
    /* Bobbing stays inside the playfield. */
    frames(150);
    CHECK_EQ(enemy_y[0] >= CROSS_MIN_Y, 1);
}

static void test_spawn_rate_ramps_and_caps(void)
{
    int i, max_alive = 0;
    setup();
    spawn_timer = SPAWN_FIRST;
    for (i = 0; i < 60 * 60; i++) {
        frames(1);
        if (count_enemies(ENEMY_ALIVE) > max_alive)
            max_alive = count_enemies(ENEMY_ALIVE);
        if (game_state != STATE_PLAY)
            game_restart(), lives = 99;
    }
    CHECK_EQ(spawn_interval, SPAWN_MIN);
    CHECK_EQ(max_alive, MAX_ENEMIES);
}

static void test_escape_costs_life_and_game_over(void)
{
    int i;
    setup();
    for (i = 0; i < START_LIVES; i++) {
        enemy_state[0] = ENEMY_ALIVE;
        enemy_x[0] = 0;
        enemy_y[0] = enemy_base_y[0] = 100;
        frames(1);
        CHECK_EQ(lives, START_LIVES - 1 - i);
        CHECK_EQ(game_events & EV_LIFE_LOST, EV_LIFE_LOST);
    }
    CHECK_EQ(game_state, STATE_OVER);
    CHECK_EQ(game_events & EV_GAME_OVER, EV_GAME_OVER);

    /* Frozen while over: no movement, no spawns, no shooting. */
    spawn_timer = 1;
    pads[0] = BTN_RIGHT;
    i = cross_x[0];
    frames(10);
    CHECK_EQ(cross_x[0], i);
    CHECK_EQ(count_enemies(ENEMY_ALIVE), 0);
}

static void test_restart_keeps_players(void)
{
    setup();
    tap(2, BTN_A);
    enemy_under_crosshair(2);
    tap(2, BTN_A);
    lives = 1;
    enemy_state[1] = ENEMY_ALIVE;
    enemy_x[1] = 0;
    enemy_y[1] = enemy_base_y[1] = 100;
    frames(1);
    CHECK_EQ(game_state, STATE_OVER);

    tap(0, BTN_A);                 /* A doesn't restart */
    CHECK_EQ(game_state, STATE_OVER);
    tap(2, BTN_START);             /* any active player's START does */
    CHECK_EQ(game_state, STATE_PLAY);
    CHECK_EQ(lives, START_LIVES);
    CHECK_EQ(score_value(2), 0);
    CHECK_EQ(player_active[0], 1);
    CHECK_EQ(player_active[2], 1);
    CHECK_EQ(player_active[1], 0);
    CHECK_EQ(count_enemies(ENEMY_NONE), MAX_ENEMIES);
}

static void test_simultaneous_escapes_dont_underflow(void)
{
    int i;
    setup();
    lives = 1;
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemy_state[i] = ENEMY_ALIVE;
        enemy_x[i] = 0;
        enemy_y[i] = enemy_base_y[i] = 100;
    }
    frames(1);
    CHECK_EQ(lives, 0);
    CHECK_EQ(game_state, STATE_OVER);
}

static void put_armor(void)
{
    enemy_under_crosshair(0);
    enemy_kind[0] = KIND_ARMOR;
    enemy_hp[0] = ARMOR_HP;
}

static void test_title_screen(void)
{
    memset(pads, 0, sizeof pads);
    game_init();
    CHECK_EQ(game_state, STATE_TITLE);
    /* Nothing happens on the title: no spawns, no lives lost. */
    frames(SPAWN_FIRST * 3);
    CHECK_EQ(count_enemies(ENEMY_NONE), MAX_ENEMIES);
    /* A on another pad joins but doesn't start. */
    tap(2, BTN_A);
    CHECK_EQ(player_active[2], 1);
    CHECK_EQ(game_state, STATE_TITLE);
    tap(0, BTN_START);
    CHECK_EQ(game_state, STATE_PLAY);
    CHECK_EQ(lives, START_LIVES);

    /* An unjoined player's START joins and starts in one press. */
    game_init();
    tap(3, BTN_START);
    CHECK_EQ(player_active[3], 1);
    CHECK_EQ(game_state, STATE_PLAY);
}

static void test_pause(void)
{
    int x, ex;
    setup();
    tap(1, BTN_A);
    enemy_under_crosshair(0);
    ex = enemy_x[0];
    pads[1] = BTN_START;
    frames(1);
    CHECK_EQ(game_events & EV_PAUSE, EV_PAUSE);
    pads[1] = 0;
    frames(1);
    CHECK_EQ(game_state, STATE_PAUSE);
    x = cross_x[0];
    pads[0] = BTN_RIGHT | BTN_A;
    spawn_timer = 1;
    frames(30);
    pads[0] = 0;
    CHECK_EQ(cross_x[0], x);                       /* frozen */
    CHECK_EQ(enemy_x[0], ex);                      /* pausing happens before movement */
    CHECK_EQ(enemy_state[0], ENEMY_ALIVE);         /* can't shoot while paused */
    CHECK_EQ(count_enemies(ENEMY_ALIVE), 1);       /* no spawns */
    frames(1);
    tap(0, BTN_START);                             /* any active player resumes */
    CHECK_EQ(game_state, STATE_PLAY);
    /* An unjoined player's START joins rather than pausing. */
    tap(3, BTN_START);
    CHECK_EQ(player_active[3], 1);
    CHECK_EQ(game_state, STATE_PLAY);
}

static void test_armor_takes_three_hits(void)
{
    int i;
    setup();
    put_armor();
    for (i = 0; i < ARMOR_HP - 1; i++) {
        pads[0] = BTN_A;
        frames(1);
        CHECK_EQ(game_events & EV_ARMOR, EV_ARMOR);
        CHECK_EQ(enemy_state[0], ENEMY_ALIVE);
        CHECK_EQ(enemy_flash[0] > 0, 1);
        pads[0] = 0;
        frames(SHOT_COOLDOWN);
        /* keep it under the crosshair */
        enemy_x[0] = cross_x[0] + CROSS_HOT - 4;
        enemy_y[0] = enemy_base_y[0] = cross_y[0] + CROSS_HOT - 4;
    }
    CHECK_EQ(score_value(0), 0);
    pads[0] = BTN_A;
    frames(1);
    pads[0] = 0;
    CHECK_EQ(enemy_state[0], ENEMY_EXPLODING);
    CHECK_EQ(score_value(0), 30);
}

static void test_armor_is_faster_and_spawns_later(void)
{
    int i, armored = 0, early_armor = 0;
    setup();
    put_armor();
    enemy_x[0] = 200;
    frames(20);
    CHECK_EQ(enemy_x[0], 200 - 30);

    setup();
    lives = 99;
    for (i = 0; i < 60 * 60; i++) {
        int before = spawn_count, j;
        frames(1);
        for (j = 0; j < MAX_ENEMIES; j++)
            if (spawn_count != before && enemy_state[j] == ENEMY_ALIVE &&
                enemy_x[j] == ENEMY_SPAWN_X && enemy_kind[j] == KIND_ARMOR) {
                armored++;
                if (before < ARMOR_FIRST)
                    early_armor++;
            }
        if (game_state != STATE_PLAY)
            game_restart(), lives = 99;
    }
    CHECK_EQ(armored > 3, 1);
    CHECK_EQ(early_armor, 0);
}

int main(void)
{
    test_title_screen();
    test_pause();
    test_armor_takes_three_hits();
    test_armor_is_faster_and_spawns_later();
    test_init();
    test_join();
    test_inactive_players_ignore_input();
    test_crosshairs_move_independently();
    test_crosshair_clamped();
    test_shoot_hits_enemy();
    test_shot_hitbox_edges();
    test_miss_scores_nothing();
    test_fire_needs_new_press_and_cooldown();
    test_one_kill_per_shot();
    test_score_carry_and_cap();
    test_enemies_spawn_and_move_left();
    test_spawn_rate_ramps_and_caps();
    test_escape_costs_life_and_game_over();
    test_restart_keeps_players();
    test_simultaneous_escapes_dont_underflow();
    if (failures) {
        printf("FAILED: %d check(s)\n", failures);
        return 1;
    }
    printf("shooter game unit tests: OK\n");
    return 0;
}

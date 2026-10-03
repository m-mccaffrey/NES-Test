/* Host-side unit tests for src/player.c (compiled with the system cc). */
#include <stdio.h>
#include "player.h"

static int failures;

#define CHECK_EQ(actual, expected) do { \
    int a_ = (actual), e_ = (expected); \
    if (a_ != e_) { \
        printf("%s:%d: %s == %d, expected %d\n", __FILE__, __LINE__, #actual, a_, e_); \
        failures++; \
    } \
} while (0)

static void hold(player_t *p, unsigned char pad, int frames)
{
    while (frames--) player_update(p, pad);
}

static void test_init(void)
{
    player_t p;
    player_init(&p);
    CHECK_EQ(p.x, PLAYER_START_X);
    CHECK_EQ(p.y, PLAYER_START_Y);
}

static void test_no_input(void)
{
    player_t p;
    player_init(&p);
    hold(&p, 0, 100);
    CHECK_EQ(p.x, PLAYER_START_X);
    CHECK_EQ(p.y, PLAYER_START_Y);
}

static void test_each_direction(void)
{
    player_t p;
    player_init(&p); hold(&p, BTN_RIGHT, 10);
    CHECK_EQ(p.x, PLAYER_START_X + 10); CHECK_EQ(p.y, PLAYER_START_Y);
    player_init(&p); hold(&p, BTN_LEFT, 10);
    CHECK_EQ(p.x, PLAYER_START_X - 10); CHECK_EQ(p.y, PLAYER_START_Y);
    player_init(&p); hold(&p, BTN_DOWN, 10);
    CHECK_EQ(p.x, PLAYER_START_X); CHECK_EQ(p.y, PLAYER_START_Y + 10);
    player_init(&p); hold(&p, BTN_UP, 10);
    CHECK_EQ(p.x, PLAYER_START_X); CHECK_EQ(p.y, PLAYER_START_Y - 10);
}

static void test_diagonal(void)
{
    player_t p;
    player_init(&p);
    hold(&p, BTN_UP | BTN_RIGHT, 5);
    CHECK_EQ(p.x, PLAYER_START_X + 5);
    CHECK_EQ(p.y, PLAYER_START_Y - 5);
}

static void test_clamps_at_edges(void)
{
    player_t p;
    player_init(&p); hold(&p, BTN_LEFT | BTN_UP, 300);
    CHECK_EQ(p.x, PLAYER_MIN_X); CHECK_EQ(p.y, PLAYER_MIN_Y);
    player_init(&p); hold(&p, BTN_RIGHT | BTN_DOWN, 300);
    CHECK_EQ(p.x, PLAYER_MAX_X); CHECK_EQ(p.y, PLAYER_MAX_Y);
}

static void test_opposite_buttons_dont_wrap(void)
{
    /* Left+Right pressed together (possible on worn pads / emulators). */
    player_t p;
    p.x = 0; p.y = PLAYER_MIN_Y;
    hold(&p, BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN, 50);
    CHECK_EQ(p.x <= PLAYER_MAX_X, 1);
    CHECK_EQ(p.y >= PLAYER_MIN_Y && p.y <= PLAYER_MAX_Y, 1);
}

int main(void)
{
    test_init();
    test_no_input();
    test_each_direction();
    test_diagonal();
    test_clamps_at_edges();
    test_opposite_buttons_dont_wrap();
    if (failures) {
        printf("FAILED: %d check(s)\n", failures);
        return 1;
    }
    printf("player unit tests: OK\n");
    return 0;
}

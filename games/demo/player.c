#include "player.h"

void player_init(player_t *p)
{
    p->x = PLAYER_START_X;
    p->y = PLAYER_START_Y;
}

void player_update(player_t *p, unsigned char pad)
{
    if ((pad & BTN_LEFT) && p->x > PLAYER_MIN_X + PLAYER_SPEED - 1)
        p->x -= PLAYER_SPEED;
    else if ((pad & BTN_RIGHT) && p->x < PLAYER_MAX_X - PLAYER_SPEED + 1)
        p->x += PLAYER_SPEED;

    if ((pad & BTN_UP) && p->y > PLAYER_MIN_Y + PLAYER_SPEED - 1)
        p->y -= PLAYER_SPEED;
    else if ((pad & BTN_DOWN) && p->y < PLAYER_MAX_Y - PLAYER_SPEED + 1)
        p->y += PLAYER_SPEED;
}

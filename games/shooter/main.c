/* 4-player rail shooter: hardware side (drawing, HUD, input, scrolling).
   The rules live in game.c. */

#include "neslib.h"
#include "game.h"
#include "pads.h"
#include "tiles.h"

#if BTN_A != PAD_A || BTN_START != PAD_START || BTN_UP != PAD_UP || \
    BTN_DOWN != PAD_DOWN || BTN_LEFT != PAD_LEFT || BTN_RIGHT != PAD_RIGHT
#error "game.h BTN_* bits must match neslib PAD_* bits"
#endif

/* Screen layout (tile rows): 0-2 HUD text, 3 HUD bar (split point),
   4-19 sky, 20-21 mountains, 22-29 ground. Only rows 4+ scroll. */
#define ROW_SCORES 1
#define ROW_STATUS 2
#define ROW_PEAKS  20
#define ROW_GRASS  22

#define SPRITE0_X  128
#define SPRITE0_Y  23     /* OAM Y is one line above the drawn sprite */

#define FLASH_FRAMES 6
#define FLASH_COLOR  0x06     /* dark red: must differ from every player colour */

/* Background palettes: colour 1 is a player's colour (used only by HUD
   text, one palette per score slot), colours 2/3 are shared scenery.
   Sprite palettes: colour 1 = player colour, 2 = enemy body, 3 = white. */
static const unsigned char palette[32] = {
    0x0f, 0x16, 0x13, 0x30,   0x0f, 0x21, 0x09, 0x19,
    0x0f, 0x2a, 0x09, 0x19,   0x0f, 0x28, 0x09, 0x19,
    0x0f, 0x16, 0x15, 0x30,   0x0f, 0x21, 0x15, 0x30,
    0x0f, 0x2a, 0x15, 0x30,   0x0f, 0x28, 0x15, 0x30,
};

/* VRAM update list (neslib set_vram_update format) for the two HUD lines:
   "1P0000  2P0000  3P0000  4P0000" and the status line. */
#define HUD_SCORES_LEN 30
#define HUD_STATUS_LEN 22
#define HUD_SCORES 3                          /* offset of the score text */
#define HUD_STATUS (HUD_SCORES + HUD_SCORES_LEN + 3)
#define HUD_RESTORE (HUD_STATUS + HUD_STATUS_LEN)   /* optional 32-tile row rewrite */
static unsigned char hud[HUD_RESTORE + 3 + 32 + 1] = {
    MSB(NTADR_A(1, ROW_SCORES)) | NT_UPD_HORZ, LSB(NTADR_A(1, ROW_SCORES)), HUD_SCORES_LEN,
};

/* Star columns for sky rows 4-19 (0xff = none). */
static const unsigned char star1_a[16] = { 20, 9, 25, 3, 4, 6, 23, 3, 13, 2, 5, 27, 26, 4, 15, 5 };
static const unsigned char star1_b[16] = { 31, 28, 6, 18, 21, 19, 10, 15, 1, 17, 24, 16, 7, 16, 3, 25 };
static const unsigned char star2[16]   = { 0xff, 27, 0xff, 12, 0xff, 7, 0xff, 14, 0xff, 3, 0xff, 25, 0xff, 3, 0xff, 14 };

static const char text_lives[] = "LIVES ";
static const char text_over[]  = "GAME OVER  PRESS START";
static const char text_start[] = "PRESS START   A: JOIN";
static const char text_pause[] = "PAUSED";

/* Title text, drawn over the sky in nametable A and removed when play
   starts (one row per frame, through the HUD update list). */
#define ROW_TITLE1 9
#define ROW_TITLE2 12
static const char title1[] = "RAIL  RAIDERS";
static const char title2[] = "1-4 PLAYERS";
static unsigned char restore_rows;   /* bit 0: ROW_TITLE1, bit 1: ROW_TITLE2 */

#define APU(addr) (*(volatile unsigned char *)(addr))

/* Exported for the emulator tests. */
unsigned int scroll_x;
unsigned char lag_frames;     /* main-loop iterations that overran a frame */

static unsigned char i, k, e, id, r, c, t, flash, enemy_rot, clock_prev;
static unsigned char row[32];
static unsigned char *hp;
static const char *sp;

/* Fill row[] with sky row r (4..19). */
static void build_sky_row(void)
{
    memfill(row, 0, 32);
    k = r - 4;
    row[star1_a[k]] = TILE_STAR1;
    row[star1_b[k]] = TILE_STAR1;
    if (star2[k] != 0xff)
        row[star2[k]] = TILE_STAR2;
}

static void draw_text(unsigned int adr, const char *s)
{
    vram_adr(adr);
    while (*s)
        vram_put(*s++ + TILE_TEXT_WHITE);
}

static void draw_nametable(unsigned int nt)
{
    vram_adr(nt);
    vram_fill(0, 32 * 3);
    vram_fill(TILE_HUD_BAR, 32);

    /* Rows are built in a buffer and written with vram_write(): much
       faster than one vram_put() call per tile. */
    for (r = 4; r < ROW_PEAKS; ++r) {
        build_sky_row();
        vram_write(row, 32);
    }
    for (c = 0; c < 32; ++c)
        row[c] = TILE_PEAKS + (c & 3);
    vram_write(row, 32);
    vram_fill(TILE_MOUNTAIN, 32);
    vram_fill(TILE_GRASS, 32);
    for (r = ROW_GRASS + 1; r < 30; ++r) {
        for (c = 0; c < 32; ++c)
            row[c] = (r + c) & 1 ? TILE_GROUND_A : TILE_GROUND_B;
        vram_write(row, 32);
    }

    /* Attributes, 4x4-tile groups. HUD: each score slot (8 tiles) gets its
       player's palette in the top half; sky/mountains palette 0; ground 1. */
    for (c = 0; c < 8; ++c)
        vram_put((c >> 1) * 0x05);
    vram_fill(0x00, 8 * 4);
    vram_fill(0x50, 8);
    vram_fill(0x55, 8 * 2);
}

static void hud_build(void)
{
    hp = hud + HUD_SCORES;
    for (i = 0; i < NUM_PLAYERS; ++i) {
        *hp++ = '1' + i;
        *hp++ = 'P';
        for (k = 0; k < 4; ++k)
            *hp++ = player_active[i] ? '0' + score[i][k] : '-';
        if (i < NUM_PLAYERS - 1) {
            *hp++ = ' ';
            *hp++ = ' ';
        }
    }

    hp = hud + HUD_STATUS;
    hp[-3] = MSB(NTADR_A(1, ROW_STATUS)) | NT_UPD_HORZ;
    hp[-2] = LSB(NTADR_A(1, ROW_STATUS));
    hp[-1] = HUD_STATUS_LEN;
    sp = game_state == STATE_OVER ? text_over :
         game_state == STATE_TITLE ? text_start :
         game_state == STATE_PAUSE ? text_pause : text_lives;
    for (k = 0; *sp; ++k)
        *hp++ = *sp++ + TILE_TEXT_WHITE;
    if (game_state == STATE_PLAY) {
        *hp++ = '0' + lives + TILE_TEXT_WHITE;
        ++k;
    }
    for (; k < HUD_STATUS_LEN; ++k)
        *hp++ = ' ' + TILE_TEXT_WHITE;

    /* After the title, put the stars back under the title text. */
    if (restore_rows && game_state != STATE_TITLE) {
        r = restore_rows & 1 ? ROW_TITLE1 : ROW_TITLE2;
        restore_rows &= restore_rows & 1 ? 2 : 1;
        *hp++ = MSB(NTADR_A(0, r)) | NT_UPD_HORZ;
        *hp++ = LSB(NTADR_A(0, r));
        *hp++ = 32;
        build_sky_row();
        for (k = 0; k < 32; ++k)
            *hp++ = row[k];
    }
    *hp = NT_UPD_EOF;
}

static void draw_enemy(void)
{
    if (enemy_state[e] == ENEMY_ALIVE) {
        if (enemy_flash[e] & 2)
            return;   /* flicker when hit */
        t = (enemy_kind[e] == KIND_ARMOR ? TILE_ARMOR : TILE_ENEMY) + ((game_frame & 8) >> 1);
        k = 0;
    } else {
        t = TILE_EXPLODE + (enemy_timer[e] < EXPLODE_FRAMES / 2 ? 4 : 0);
        k = enemy_owner[e];
    }
    r = enemy_x[e];
    c = enemy_y[e];
    id = oam_spr(r,     c,     t,     k, id);
    id = oam_spr(r + 8, c,     t + 1, k, id);
    id = oam_spr(r,     c + 8, t + 2, k, id);
    id = oam_spr(r + 8, c + 8, t + 3, k, id);
}

static void draw_sprites(void)
{
    /* Sprite 0 sits behind the opaque HUD bar; its hit triggers the split. */
    id = oam_spr(SPRITE0_X, SPRITE0_Y, TILE_SPRITE0, OAM_BEHIND, 0);

    /* Crosshairs first so they win the 8-sprites-per-line limit. */
    for (i = 0; i < NUM_PLAYERS; ++i) {
        if (player_active[i]) {
            t = shot_timer[i] > SHOT_COOLDOWN - SHOT_FLASH ? TILE_CROSS_FIRE : TILE_CROSS;
            id = oam_spr(cross_x[i], cross_y[i], t, i, id);
        }
    }

    /* Rotate enemy draw order each frame so overflow flickers instead of
       hiding the same enemy every frame. */
    e = enemy_rot;
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (enemy_state[e] != ENEMY_NONE)
            draw_enemy();
        if (++e == MAX_ENEMIES)
            e = 0;
    }
    if (++enemy_rot == MAX_ENEMIES)
        enemy_rot = 0;

    oam_hide_rest(id);
}

/* Sound effects, written straight to the APU (one channel each). */
static void play_sounds(void)
{
    if (game_events & EV_SHOT) {          /* noise: gunshot */
        APU(0x400c) = 0x02;
        APU(0x400e) = 0x0a;
        APU(0x400f) = 0x08;
    }
    if (game_events & EV_HIT) {           /* pulse 1: falling boom */
        APU(0x4000) = 0x84;
        APU(0x4001) = 0x82;
        APU(0x4002) = 0x40;
        APU(0x4003) = 0x09;
    }
    if (game_events & EV_ARMOR) {         /* pulse 2: high ping */
        APU(0x4004) = 0x82;
        APU(0x4005) = 0x00;
        APU(0x4006) = 0x30;
        APU(0x4007) = 0x08;
    } else if (game_events & (EV_JOIN | EV_PAUSE | EV_RESTART)) {   /* pulse 2: rising blip */
        APU(0x4004) = 0x44;
        APU(0x4005) = 0x8b;
        APU(0x4006) = 0xd0;
        APU(0x4007) = 0x08;
    }
    if (game_events & EV_LIFE_LOST) {     /* triangle: low thud */
        APU(0x4008) = 0x30;
        APU(0x400a) = 0x80;
        APU(0x400b) = 0x0a;
    }
}

void main(void)
{
    pal_all(palette);
    draw_nametable(NAMETABLE_A);
    draw_nametable(NAMETABLE_B);
    draw_text(NTADR_A(10, ROW_TITLE1), title1);
    draw_text(NTADR_A(11, ROW_TITLE2), title2);
    restore_rows = 3;
    APU(0x4015) = 0x0f;

    game_init();
    hud_build();
    flush_vram_update(hud);
    set_vram_update(hud);
    draw_sprites();

    ppu_on_all();
    clock_prev = nesclock();

    while (1) {
        ppu_wait_nmi();
        t = nesclock();
        if ((unsigned char)(t - clock_prev) != 1)
            ++lag_frames;
        clock_prev = t;

        /* The NMI set scroll 0 for the HUD; scroll the playfield once the
           beam reaches sprite 0. */
        split(scroll_x, 0);

        pads_read();
        game_update(pads);

        if (game_state == STATE_PLAY)
            scroll_x = (scroll_x + 1) & 511;

        if (game_events & EV_LIFE_LOST)
            flash = FLASH_FRAMES;
        if (flash) {
            --flash;
            pal_col(0, flash ? FLASH_COLOR : palette[0]);
        }

        play_sounds();
        draw_sprites();
        hud_build();
    }
}

#include "neslib.h"
#include "player.h"

/* Compile-time guard: player.c must use neslib's button bit order. */
#if BTN_UP != PAD_UP || BTN_DOWN != PAD_DOWN || BTN_LEFT != PAD_LEFT || BTN_RIGHT != PAD_RIGHT
#error "player.h BTN_* bits must match neslib PAD_* bits"
#endif

#define TILE_PLAYER 0x01
#define TILE_BG     0x02

/* Required by neslib's display.sinc helpers (next free OAM offset). */
#pragma bss-name(push, "ZEROPAGE")
unsigned char oam_off;
#pragma bss-name(pop)
#pragma zpsym("oam_off")

/* Exported (non-static) so tests can find its address in the label file. */
player_t player;

static const unsigned char palette[32] = {
    0x0f, 0x00, 0x10, 0x30,   0x0f, 0x00, 0x10, 0x30,
    0x0f, 0x00, 0x10, 0x30,   0x0f, 0x00, 0x10, 0x30,
    0x0f, 0x16, 0x27, 0x30,   0x0f, 0x16, 0x27, 0x30,
    0x0f, 0x16, 0x27, 0x30,   0x0f, 0x16, 0x27, 0x30,
};

void main(void)
{
    unsigned char pad;

    pal_all(palette);

    /* Simple border so there's something on screen besides the sprite. */
    vram_adr(NAMETABLE_A);
    vram_fill(TILE_BG, 32);
    vram_adr(NAMETABLE_A + 29 * 32);
    vram_fill(TILE_BG, 32);

    player_init(&player);
    ppu_on_all();

    while (1) {
        ppu_wait_nmi();
        pad = pad_poll(0);
        player_update(&player, pad);
        oam_spr(player.x, player.y, TILE_PLAYER, 0, 0);
    }
}

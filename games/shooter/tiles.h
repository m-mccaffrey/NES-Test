#ifndef TILES_H
#define TILES_H

/* Tile numbers in tiles.txt. */

#define TILE_TEXT_WHITE 0x80   /* add to an ASCII char for colour-3 text */

#define TILE_STAR1      0x60
#define TILE_STAR2      0x61
#define TILE_PEAKS      0x62   /* 4 tiles wide */
#define TILE_MOUNTAIN   0x66
#define TILE_GRASS      0x67
#define TILE_GROUND_A   0x68
#define TILE_GROUND_B   0x69
#define TILE_HUD_BAR    0x6a

#define TILE_CROSS      0x70
#define TILE_CROSS_FIRE 0x71
#define TILE_SPRITE0    0x7f

#define TILE_ENEMY      0x80   /* 2 frames of 4 tiles (16x16) */
#define TILE_EXPLODE    0x88   /* 2 frames of 4 tiles (16x16) */
#define TILE_ARMOR      0x90   /* 2 frames of 4 tiles (16x16) */

#endif

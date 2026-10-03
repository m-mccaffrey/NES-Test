#ifndef PADS_H
#define PADS_H

/* Up to four controllers (pads.s). Supports the NES Four Score / NES
   Satellite adapter and Famicom expansion-port controllers 3 and 4. */

/* Buttons for players 1-4 in neslib bit order (PAD_A = 0x01 ... PAD_RIGHT
   = 0x80), refreshed by pads_read(). */
extern unsigned char pads[4];

/* 1 if the last pads_read() saw a Four Score signature on both ports. */
extern unsigned char pads_fourscore;

void __fastcall__ pads_read(void);

#endif

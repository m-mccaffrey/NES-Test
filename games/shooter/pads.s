; Four-player controller reading.
;
; After a strobe, each port shifts out a 24-bit report on D0 when a Four
; Score is attached:
;   bits  1-8   controller 1 ($4016) / controller 2 ($4017)
;   bits  9-16  controller 3 ($4016) / controller 4 ($4017)
;   bits 17-24  signature: 0,0,0,1,0,0,0,0 ($4016) / 0,0,1,0,0,0,0,0 ($4017)
; Standard controllers return 1s after the first 8 bits, so the signature
; check is what stops players 3/4 seeing phantom "all buttons held" input.
; Without a Four Score, players 3/4 come from the Famicom expansion port,
; which reports on D1 during the first 8 reads (always 0 on an NES).
;
; Bits are rotated in from the top, so the first button read (A) ends up
; in bit 0: the same order as neslib's pad_poll() and PAD_* constants.

	.export _pads_read, _pads, _pads_fourscore

JOYPAD1 = $4016
JOYPAD2 = $4017

SIG1 = %00001000	; 4th signature bit read on $4016
SIG2 = %00000100	; 3rd signature bit read on $4017

.segment "ZEROPAGE"

exp3:	.res 1		; Famicom expansion controllers (D1)
exp4:	.res 1
fs3:	.res 1		; Four Score controllers 3/4 (bits 9-16)
fs4:	.res 1
sig1:	.res 1		; signatures (bits 17-24)
sig2:	.res 1

.segment "BSS"

_pads:		.res 4
_pads_fourscore:	.res 1

.segment "CODE"

; void __fastcall__ pads_read(void);
_pads_read:
	lda #1
	sta JOYPAD1
	lda #0
	sta JOYPAD1

	ldx #8
@first8:
	lda JOYPAD1
	lsr a		; D0: controller 1
	ror _pads+0
	lsr a		; D1: Famicom controller 3
	ror exp3
	lda JOYPAD2
	lsr a		; D0: controller 2
	ror _pads+1
	lsr a		; D1: Famicom controller 4
	ror exp4
	dex
	bne @first8

	ldx #8
@second8:
	lda JOYPAD1
	lsr a
	ror fs3
	lda JOYPAD2
	lsr a
	ror fs4
	dex
	bne @second8

	ldx #8
@signature:
	lda JOYPAD1
	lsr a
	ror sig1
	lda JOYPAD2
	lsr a
	ror sig2
	dex
	bne @signature

	lda sig1
	cmp #SIG1
	bne @no_fourscore
	lda sig2
	cmp #SIG2
	bne @no_fourscore

	lda fs3
	sta _pads+2
	lda fs4
	sta _pads+3
	lda #1
	sta _pads_fourscore
	rts

@no_fourscore:
	lda exp3
	sta _pads+2
	lda exp4
	sta _pads+3
	lda #0
	sta _pads_fourscore
	rts

NAME   := game
BUILD  := build
ROM    := $(BUILD)/$(NAME).nes

CC65   ?= cc65
CA65   ?= ca65
LD65   ?= ld65
PYTHON ?= python3
HOSTCC ?= cc

CFLAGS := -Oirs -t nes -I lib/neslib
SRCS   := src/main.c src/player.c
OBJS   := $(BUILD)/crt0.o $(SRCS:src/%.c=$(BUILD)/%.o)

.PHONY: all test unit-test rom-test clean screenshot
.SECONDARY:

all: $(ROM)

$(BUILD):
	mkdir -p $@

$(BUILD)/tiles.chr: tools/make_chr.py | $(BUILD)
	$(PYTHON) $< $@

$(BUILD)/crt0.o: lib/neslib/crt0.s lib/neslib/*.sinc $(BUILD)/tiles.chr
	$(CA65) -t nes -I lib/neslib --bin-include-dir $(BUILD) $< -o $@

$(BUILD)/%.s: src/%.c src/*.h lib/neslib/neslib.h | $(BUILD)
	$(CC65) $(CFLAGS) $< -o $@

$(BUILD)/%.o: $(BUILD)/%.s
	$(CA65) -t nes $< -o $@

$(ROM): $(OBJS) nes.cfg
	$(LD65) -C nes.cfg -o $@ $(OBJS) nes.lib \
		-m $(BUILD)/$(NAME).map -Ln $(BUILD)/$(NAME).labels --dbgfile $(BUILD)/$(NAME).dbg

# Host-compiled unit tests of the pure game logic.
$(BUILD)/test_player: tests/test_player.c src/player.c src/player.h | $(BUILD)
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -Isrc tests/test_player.c src/player.c -o $@

unit-test: $(BUILD)/test_player
	$<

# Emulator-driven tests of the real ROM (needs: pip install cynes numpy).
rom-test: $(ROM)
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

test: unit-test rom-test

clean:
	rm -rf $(BUILD)

# Headless screenshot: make screenshot [FRAMES=90] [HOLD="right down"]
FRAMES ?= 90
HOLD   ?=
screenshot: $(ROM)
	$(PYTHON) tools/screenshot.py $(ROM) $(BUILD)/screenshot.png $(FRAMES) $(HOLD)

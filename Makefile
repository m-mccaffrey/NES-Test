# Each directory in games/ is one ROM: build/<game>.nes.
#   games/<game>/*.c, *.s   sources (main.c is the hardware side; every other
#                           .c file must be hardware-free so it can also be
#                           compiled on the host for unit tests)
#   games/<game>/tiles.txt  CHR tiles (tools/make_chr.py)
#   games/<game>/tests/     test_*.c host unit tests, test_*.py emulator tests

GAMES  := $(notdir $(wildcard games/*))
BUILD  := build

CC65   ?= cc65
CA65   ?= ca65
LD65   ?= ld65
PYTHON ?= python3
HOSTCC ?= cc

CFLAGS := -Oirs -t nes -I lib/neslib

.PHONY: all test unit-test rom-test clean screenshot $(GAMES)
.SECONDARY:
.DELETE_ON_ERROR:

all: $(GAMES)

# --- per-game rules -------------------------------------------------------
define GAME_RULES
$(1)_C     := $$(wildcard games/$(1)/*.c)
$(1)_S     := $$(wildcard games/$(1)/*.s)
$(1)_OBJS  := $(BUILD)/$(1)/crt0.o $$($(1)_C:games/%.c=$(BUILD)/%.o) $$($(1)_S:games/%.s=$(BUILD)/%.o)
$(1)_LOGIC := $$(filter-out games/$(1)/main.c,$$($(1)_C))
$(1)_UNIT  := $$(patsubst games/$(1)/tests/%.c,$(BUILD)/$(1)/tests/%,$$(wildcard games/$(1)/tests/test_*.c))

$(1): $(BUILD)/$(1).nes

$(BUILD)/$(1).nes: $$($(1)_OBJS) nes.cfg
	$(LD65) -C nes.cfg -o $$@ $$($(1)_OBJS) nes.lib \
		-m $(BUILD)/$(1).map -Ln $(BUILD)/$(1).labels --dbgfile $(BUILD)/$(1).dbg

$(BUILD)/$(1)/tests/%: games/$(1)/tests/%.c $$($(1)_LOGIC) $$(wildcard games/$(1)/*.h)
	@mkdir -p $$(@D)
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -Igames/$(1) $$< $$($(1)_LOGIC) -o $$@

unit-test-$(1): $$($(1)_UNIT)
	@for t in $$^; do echo "== $$$$t"; ./$$$$t || exit 1; done

rom-test-$(1): $(BUILD)/$(1).nes
	@if ls games/$(1)/tests/test_*.py >/dev/null 2>&1; then \
		$(PYTHON) -m unittest discover -s games/$(1)/tests -p 'test_*.py' -v; fi

test-$(1): unit-test-$(1) rom-test-$(1)
.PHONY: unit-test-$(1) rom-test-$(1) test-$(1)
endef
$(foreach g,$(GAMES),$(eval $(call GAME_RULES,$(g))))

# --- shared pattern rules ---------------------------------------------------
$(BUILD)/%/tiles.chr: games/%/tiles.txt tools/make_chr.py $(wildcard assets/*.txt)
	@mkdir -p $(@D)
	$(PYTHON) tools/make_chr.py $< $@

$(BUILD)/%/crt0.o: lib/neslib/crt0.s $(wildcard lib/neslib/*.sinc) $(BUILD)/%/tiles.chr
	$(CA65) -t nes -I lib/neslib --bin-include-dir $(@D) $< -o $@

$(BUILD)/%.o: games/%.s
	@mkdir -p $(@D)
	$(CA65) -t nes $< -o $@

$(BUILD)/%.s: games/%.c $(wildcard games/*/*.h) lib/neslib/neslib.h
	@mkdir -p $(@D)
	$(CC65) $(CFLAGS) -I $(dir $<) $< -o $@

$(BUILD)/%.o: $(BUILD)/%.s
	$(CA65) -t nes $< -o $@

# --- aggregate targets --------------------------------------------------------
unit-test: $(GAMES:%=unit-test-%)
rom-test: $(GAMES:%=rom-test-%)
test: unit-test rom-test

# Headless screenshot, e.g.
#   make screenshot GAME=shooter FRAMES=300 HOLD="1:right,a 3:start" FOURSCORE=1
GAME      ?= shooter
FRAMES    ?= 90
HOLD      ?=
FOURSCORE ?=
screenshot: $(BUILD)/$(GAME).nes
	$(PYTHON) tools/screenshot.py $< $(BUILD)/$(GAME).png --frames $(FRAMES) \
		$(if $(FOURSCORE),--four-score) $(foreach h,$(HOLD),--hold $(h))

clean:
	rm -rf $(BUILD)

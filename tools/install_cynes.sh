#!/bin/sh
# Build and install the cynes emulator with NES Four Score support.
#
# Upstream cynes emulates two controllers. tools/cynes-fourscore.patch adds
# controllers 3/4 behind an NES Four Score (`nes.four_score = True`, and
# `nes.controller` takes 8 bits per pad, pad 1 in the low byte), and makes
# plain controllers report 1s after 8 reads like real hardware.
set -eu

CYNES_REPO=https://github.com/Youlixx/cynes
CYNES_COMMIT=6f8d3a427484bc36684a512672f16487cfb86170
PYTHON=${PYTHON:-python3}

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

git init -q "$work/cynes"
git -C "$work/cynes" fetch -q --depth 1 "$CYNES_REPO" "$CYNES_COMMIT"
git -C "$work/cynes" checkout -q FETCH_HEAD
git -C "$work/cynes" apply "$here/cynes-fourscore.patch"

"$PYTHON" -m pip install "$work/cynes" "$@"

cd "$here/.."
"$PYTHON" -c "import cynes; assert hasattr(cynes.NES, 'four_score'); print('installed cynes', cynes.__version__)"

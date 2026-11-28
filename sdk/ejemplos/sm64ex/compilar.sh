#!/usr/bin/env bash
# Compiles sm64ex for Switch with Greeny achievements.
#
#   ./compilar.sh /path/to/baserom.us.z64
#
# You need Docker and YOUR OWN legally dumped Super Mario 64 ROM (US version). The ROM is only used
# to extract the game's assets on your machine; nothing from it is included in this repository.
# Output: sm64ex-greeny/build/us_nx/sm64.us.f3dex2e.nro
set -euo pipefail

ROM="${1:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="$HERE/../.."
DIR="$PWD/sm64ex-greeny"
COMMIT=48765081b8c8dcc6ac5e238911c7509589ce0384   # fgsfdsfgs/sm64ex, branch "switch"

if [ -z "$ROM" ] || [ ! -f "$ROM" ]; then
    echo "Usage: $0 /path/to/baserom.us.z64" >&2
    exit 1
fi

if [ ! -d "$DIR" ]; then
    git clone https://github.com/fgsfdsfgs/sm64ex.git "$DIR"
    git -C "$DIR" checkout -q "$COMMIT"
fi

cp "$ROM" "$DIR/baserom.us.z64"
# greeny.h, greeny_sm64.c, the 3 calls in pc_main.c and the Makefile fix for current devkitPro images
if ! grep -q greeny_sm64_update "$DIR/src/pc/pc_main.c"; then
    git -C "$DIR" apply "$HERE/sm64ex-greeny.patch"
fi
cp "$SDK/greeny.h" "$HERE/greeny_sm64.c" "$DIR/src/pc/" # always the latest SDK

docker run --rm -v "$DIR":/src -w /src devkitpro/devkita64:latest \
    bash -c 'make TARGET_SWITCH=1 VERSION=us -j"$(nproc)"'

echo
echo "Done: $DIR/build/us_nx/sm64.us.f3dex2e.nro"
echo "Copy it to sdmc:/switch/ and open it from the homebrew menu."

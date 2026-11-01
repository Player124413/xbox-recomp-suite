#!/usr/bin/env bash
# Sonic Heroes Linux build: prepare the SDK, extract the
# game from an ISO if needed, write the recompiler config from the builder's own
# XBE, recompile it, build the runtime and run the SDK tests.
#
#   ./build.sh                 release build
#   ./build.sh debug           debug build
#   ./build.sh --generate-only recompile the guest code only (for the Android build)
set -euo pipefail
GAME_TITLE="Sonic Heroes - XBE Recomp"
GAME_XBE=default.xbe
GAME_EXE=SonicHeroes
cd "$(dirname "$(realpath "$0")")"
PRESET=linux-release
GENERATE_ONLY=0
for arg in "$@"; do
    case "$arg" in
        debug) PRESET=linux-debug ;;
        --generate-only) GENERATE_ONLY=1 ;;
        *) echo "usage: $0 [debug] [--generate-only]"; exit 2 ;;
    esac
done
OUT=out/build/$PRESET

echo
echo "  $GAME_TITLE"
echo "  =================="
echo

fail() {
    echo
    echo "  BUILD FAILED. $*"
    echo
    exit 1
}

# ---------------------------------------------------------------- toolchain
echo "[1/5] Toolchain"
missing=()
for tool in clang++ cmake ninja git ld.lld pkg-config python3; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
if command -v pkg-config >/dev/null; then
    pkg-config --exists sdl3 || missing+=("SDL3")
    pkg-config --exists vulkan || missing+=("Vulkan loader")
fi
if ((${#missing[@]})); then
    echo "      missing: ${missing[*]}"
    echo "      Debian/Ubuntu: sudo apt install clang lld cmake ninja-build git pkg-config python3 libsdl3-dev libvulkan-dev"
    echo "      plus your GPU's Vulkan driver"
    fail "Install the missing tools, then run this again."
fi
echo "      $(clang++ --version | head -1)"

# ---------------------------------------------------------------- the game
echo "[2/5] Game files"
mkdir -p assets
if [ ! -f "assets/$GAME_XBE" ]; then
    iso=$(find assets -maxdepth 1 -iname '*.iso' -type f | sort | head -1)
    [ -n "$iso" ] || {
        echo "      Put the game in the assets folder: its disc image (.iso), or $GAME_XBE with its folders beside it."
        fail "No game in $(realpath assets)."
    }
    size=$(du -h "$iso" | cut -f1)
    echo "      extracting $(basename "$iso") ($size) into assets; the image may be deleted afterwards"
    python3 ../../tools/prepare.py SonicHeroes >/dev/null
    cmake --preset "$PRESET" >/dev/null || fail "CMake could not configure."
    cmake --build --preset "$PRESET" --target iso_extract >/dev/null || fail "iso_extract did not build."
    "$OUT/iso_extract" --all "$iso" assets || fail "$(basename "$iso") could not be extracted."
    [ -f "assets/$GAME_XBE" ] || fail "$(basename "$iso") has no $GAME_XBE at its root."
fi
echo "      assets/$GAME_XBE"

# ---------------------------------------------------------------- config
echo "[3/5] Recompiler config"
python3 ../../tools/prepare.py SonicHeroes >/dev/null
if [ ! -f config/game.toml ] || { grep -q '^entry = 0x10000$' config/game.toml && ! grep -A2 '^\[symbols\]' config/game.toml | grep -q '='; }; then
    python3 ../../tools/sonic_heroes.py "assets/$GAME_XBE" || fail "Could not read the XBE header."
else
    echo "      config/game.toml already written for this executable"
fi
# If the image's library build has checked-in signatures, let xbe_symbols fill
# in the symbol and data tables. An older build keeps the generated config.
if ! grep -A2 '^\[symbols\]' config/game.toml | grep -q '='; then
    cmake --preset "$PRESET" >/dev/null || fail "CMake could not configure."
    cmake --build --preset "$PRESET" --target xbe_symbols >/dev/null || fail "xbe_symbols did not build."
    if [ -f "$OUT/xbe_symbols" ]; then
        SDK=$(head -1 "$OUT/sdk_dir.txt")
        echo "      trying xbe_symbols for $GAME_XBE"
        if "$OUT/xbe_symbols" "assets/$GAME_XBE" "$SDK/xdk" --out config/game.toml; then
            echo "      symbols written"
        else
            echo "      no checked-in signatures for this XDK build; continuing with the generated config"
        fi
    fi
fi
cmake --preset "$PRESET" >/dev/null || fail "CMake could not configure."
# argument registers xbe_symbols could not order are left as "?": the recompiler needs them filled in by hand
if grep -n '^[A-Za-z_0-9]* *= *\[.*\"?\"' config/game.toml; then
    echo "      These [abi] entries in config/game.toml need their argument locations in API order: each comment says"
    echo "      which registers the function reads and how many stack bytes it pops (4 per \"stack\" argument)."
    fail "Fill in the [abi] entries above, then run this again."
fi

# ---------------------------------------------------------------- game
echo "[4/5] Game code"
if [ "$GENERATE_ONLY" = 1 ]; then
    echo "      recompiling $GAME_XBE (guest code only)"
    cmake --build --preset "$PRESET" --target recompile_SonicHeroes -j "$(nproc)" || fail "The recompiler failed."
    echo
    echo "  Guest code is in generated/game; run tools/build_android.py SonicHeroes next."
    exit 0
fi
echo "      the first build recompiles $GAME_XBE: 30 to 90 minutes on weaker systems ..."
cmake --build --preset "$PRESET" --target SonicHeroes -j "$(nproc)" || fail "Read the last message above."

echo "[5/5] Tests"
ctest --preset linux-release --output-on-failure >/dev/null || echo "      warning: the tests failed; run: ctest --preset linux-release"

echo
echo "  $GAME_TITLE built."
echo "  $OUT/$GAME_EXE"
echo
if [ -t 0 ] && [ -z "${CI:-}" ]; then
    read -r -p "  Play now? [y/N] " answer
    [[ "$answer" =~ ^[Yy] ]] && (cd "$OUT" && exec "./$GAME_EXE")
fi
exit 0

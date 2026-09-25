#!/usr/bin/env bash
# ============================================================================
# build.sh - builds the ROM and its BPS patch in one command.
#
#   ./build.sh                      # uses ./baserom.gba for the BPS if present
#   ./build.sh /path/to/clean.gba   # clean Pokemon Emerald (U), CRC32 1F1C08FB
#
# Needs (Ubuntu / Debian / WSL):
#   sudo apt install build-essential libpng-dev python3 \
#        gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi
# (devkitARM also works: export DEVKITARM=/opt/devkitpro/devkitARM)
#
# Output:
#   out/PerfectEmerald_HnSBG_DynamicWeather.gba   ready to play
#   out/PerfectEmerald_HnSBG_DynamicWeather.bps   patch for a clean Emerald (U)
# The clean ROM is only needed for the .bps; the game builds from source alone.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
NAME="Perfect_Emerald_v3.1"
EXPECTED_CRC=1F1C08FB

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing '$1' - see the install line at the top of build.sh"; exit 1; }; }
need make; need python3; need gcc
if [ -z "${DEVKITARM:-}" ]; then need arm-none-eabi-gcc; fi

CLEAN="${1:-baserom.gba}"
if [ -f "$CLEAN" ]; then
    CRC=$(python3 -c "import zlib,sys;print('%08X'%(zlib.crc32(open(sys.argv[1],'rb').read())&0xffffffff))" "$CLEAN")
    if [ "$CRC" != "$EXPECTED_CRC" ]; then
        echo "ERROR: $CLEAN has CRC32 $CRC, expected $EXPECTED_CRC (Pokemon Emerald (U))."
        exit 1
    fi
    [ "$CLEAN" -ef baserom.gba ] || cp "$CLEAN" baserom.gba
fi

JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo "== building with $JOBS jobs"
make -j"$JOBS" MODERN=1

mkdir -p out
cp pokeemerald_modern.gba "out/$NAME.gba"
python3 - "out/$NAME.gba" <<'PY'
import sys, zlib, hashlib
d = open(sys.argv[1], 'rb').read()
print(f"== {sys.argv[1]}: {len(d):,} bytes, CRC32 {zlib.crc32(d) & 0xffffffff:08X}, md5 {hashlib.md5(d).hexdigest()}")
PY

if [ -f baserom.gba ]; then
    python3 tools/bps/bps.py create baserom.gba "out/$NAME.gba" "out/$NAME.bps"
else
    echo "== no clean ROM given: skipped the .bps (./build.sh /path/to/clean_emerald.gba)"
fi
echo "== done: $(ls out)"

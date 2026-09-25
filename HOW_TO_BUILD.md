# Perfect Emerald — HnS backgrounds + dynamic weather (open-source art build)

Base: Perfect Emerald source (Modern Emerald 3.5 + ORAS tilesets + HnS 2.0.6
battle backgrounds). This tree adds the fixes and systems listed in
`CHANGES.md`. It contains **no custom-drawn battle backgrounds**: every
background comes from HnS 2.0.6 / Modern Emerald.

## Option 1 — one command on Linux / WSL / macOS
```bash
sudo apt install build-essential libpng-dev python3 \
     gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi
./build.sh /path/to/clean_emerald.gba      # Pokemon Emerald (U), CRC32 1F1C08FB
```
Output in `out/`:
* `PerfectEmerald_HnSBG_DynamicWeather.gba` — ready to play
* `PerfectEmerald_HnSBG_DynamicWeather.bps` — patch for a clean Emerald (U)

The clean ROM is only used for the `.bps`; the game itself builds from source
(`./build.sh` with no argument builds just the `.gba`).
devkitARM works too (`export DEVKITARM=/opt/devkitpro/devkitARM`).

## Option 2 — no PC toolchain (GitHub Actions)
1. Create a **private** GitHub repository and upload this folder.
2. Optional, for the `.bps`: add your clean ROM as `baserom.gba` (private repo only).
3. Actions → *Build Perfect Emerald* → *Run workflow*, then download the artifact.

## Applying the patch
Any BPS patcher (Rom Patcher JS in a browser, Floating IPS, …) with a clean
Emerald (U) ROM, or:
`python3 tools/bps/bps.py apply clean.gba PerfectEmerald_HnSBG_DynamicWeather.bps out.gba`

## Plain make
`make -j$(nproc) MODERN=1` → `pokeemerald_modern.gba`

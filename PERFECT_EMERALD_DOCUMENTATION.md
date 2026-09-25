# Perfect Emerald — complete documentation

**Current version: v3.1** (25 September 2026)

Perfect Emerald is a Pokémon Emerald ROM hack built from source. This document
covers everything done to it, from the first request to v3.1: what was asked,
what was built, how each system works today, where the code lives, and what is
still open.

---

## Contents
1. Starting point
2. Building and patching
3. Full timeline (every request, in order)
4. How everything works now (feature reference)
5. Options menu
6. Flags and save data
7. Files changed
8. Known issues and open items
9. How the work was tested
10. Credits

---

## 1. Starting point

On 23 September 2026 two files were supplied:

* `PerfectEmerald_HnSBG_DynamicWeather_source.zip` — a decompilation-based source
  tree of **Modern Emerald** (resetes12's pokeemerald hack) that already had the
  Heart & Soul ("HnS") battle backgrounds and an early dynamic-weather system;
* a clean Pokémon Emerald (U) ROM (CRC32 `1F1C08FB`), used only to make `.bps` patches.

The first request was simply to build the source into a playable `.gba`.

---

## 2. Building and patching

```
./build.sh /path/to/clean_emerald.gba
```
* Needs `arm-none-eabi-gcc` (13.2 used), `make`, `libpng`, Python 3.
* `build.sh` builds the helper tools, runs `make MODERN=1`, and writes the ROM and a
  verified `.bps` patch to `out/`. Without a clean ROM it only builds the `.gba`.
* `tools/bps/bps.py create|apply` makes and applies BPS patches (works with Floating
  IPS, Rom Patcher JS, beat, …).
* The ROM keeps the Emerald header (BPEE, 128 KB flash + real-time clock) so flash
  carts and emulators detect saving and the clock correctly.

---

## 3. Full timeline

Versions are named "Perfect Emerald vX.Y" (rule set on 24 Sept: first build v1.0,
then v1.1, v1.2 …).

### 23–24 September — first builds (became v1.0)
1. **Build the ROM** from the uploaded source (`Make me this rom as a .gba file`).
2. **Cloud reflection bug:** cloud sprites were pinned to Route 120 map coordinates
   and showed through houses. Replaced by moving cloud **shadows** on the ground and
   cloud **reflections** that only appear while fully over water.
   Checked dynamic weather (rain, snow…), that the overworld weather carries into
   battle, and that story scripts (legendaries, Sootopolis, desert, Mt. Pyre…) take
   over the weather and hand it back afterwards.
3. **Path battle background:** a copy of tall grass named Path. Trainer battles
   outdoors on routes with no wild-Pokémon terrain nearby use Path; trainer battles
   on land never get the lake/ocean background.
4. **Looping battle weather:** rain, sandstorm, sun and hail animate continuously
   while the weather lasts, through menus and attack animations (not in the Bag or
   party screen).
5. **Battle weather tint** kept on for the whole battle.
6. **Enemy intro loop:** while you are in the battle menus the opposing Pokémon
   replays its intro animation; choosing an action stops it.
7. **Dynamic weather, big pass:** weather effects in the overworld; **Mist** (a copy
   of fog, no shading, 85 % more transparent, likely 04:30–06:30); cloud shadows and
   reflections; snow turns map tiles frosty; land battles switch to snow backgrounds
   when it snows; rain/thunderstorm/downpour make puddles with reflections
   (downpour 15 % more); ash constant near the volcano and in lava caves; sandstorm
   only in the desert and, 25 % less often, right next to it; weather neighbour
   rules (mist–fog, mist–snow, rain–thunderstorm, sunny–cloudy, cloudy–rain,
   rain–downpour, downpour–thunderstorm, sunny–drought); maps of the same terrain
   share weather; exactly one route per city shares the city's weather, the others
   differ; location-based weather odds.
8. Two ROMs had been output by mistake; naming rule agreed → **v1.0**.

### 24 September
* **v1.1** — Options crash on real hardware on the "Wild Music" line: saves could hold
  out-of-range option values; all options are now clamped on boot, New Game and
  when the menu opens.
* **Leob0505 water background** — a ready-to-merge port packaged as a zip (not
  compiled into any ROM).
* **v1.1B** — two supplied pictures became the tall-grass battle background (day and
  night). (Removed in v2.0.)
* **v1.2** — floating enemy sprite fixed (the intro loop now restores the sprite
  exactly); puddles got a sand rim, sizes 1–7 tiles (bigger = rarer), 10 % more,
  1-tile puddles, puddles in towns and cities.
* Question answered: thunderstorm makes the same number of puddles as rain.
* **v1.3** — CFRU battle backgrounds from the supplied FireRed "Battle Backgrounds
  Patch" (20 pictures, pixel-exact), as a new BATTLE TERRAIN choice. (A parallel chat
  had built a v1.3 from CFRU's GitHub pictures; it was replaced.)
* **v1.4** — terrain circles ported from Modern Emerald 3.5, dynamic per terrain,
  shown on the New backgrounds only; drought 15 % dimmer.
* **v1.5** — new title logo; tall grass swapped (Heart & Soul on New, custom on
  CFRU); after-battle background flash fixed; drought in battle 10 % brighter than
  Sunny Day and overworld drought flashes only 10 % brighter than normal; puddle life
  cycle (see §4.3), frozen puddles, puddles never spawn on people/items/Cut
  trees/Smash rocks.
* **v1.6** — 15 % fewer puddles; the weather check reads the clock less often and in
  a different frame from the game's own time events (real-hardware audio drops).
* Reference sheets of every CFRU and Heart & Soul background were produced.
* **v2.0** — the **Perfect** background set, new **BACKGROUND** option
  (Old / Modern / Perfect), **BATTLE TERRAIN** became With / Without circles on every
  background, puddles capped at 4 tiles and 15 % more, thinner rim and miniature
  1-tile puddle, "v3.5" removed from the title screen, custom tall grass deleted;
  full test pass.
* **v2.1** — new title logo ("Pokémon Perfect Emerald Version"); documentation,
  source zip and `.bps` delivered.
* **v2.2** — reflections now show in puddles; puddles on city pavements. A
  replacement for the GBA's built-in decompression (aimed at the audio drops) froze
  battles in testing and was removed.

### 25 September
* **v2.3** — puddles no longer glow at night; no puddles in towns/cities, "wet ground"
  instead; 1-tile puddle = miniature of Emerald's own puddle; 4-tile puddles =
  Emerald's own 2×2 puddle; 5 % fewer puddles; mist keeps the normal night shade;
  specks under the title logo removed.
* **v2.4** (built in a parallel chat) — no 3-tile puddles; 2-tile puddles drawn from
  an uploaded picture (pale "pill" shape); 1-tile = shrunk Emerald puddle.
* **v2.5** — 1-tile puddle redrawn square with uneven corners.
* **v2.6** — 2-tile puddles redrawn uneven (both directions).
* **v2.7** — 2-tile puddles recoloured to match the 1- and 4-tile puddles; this
  documentation rewritten from the start; `.gba`, `.bps` and source delivered.
* **v2.8** — drought sunlight in battle is now 7 % brighter than Sunny Day (was
  10 %); a first start defaults to BACKGROUND = Perfect and BATTLE TERRAIN = With,
  and those two choices now survive starting a New Game (they used to be wiped
  with the other story flags).
* **v2.9** — cloud shadows no longer get cut up by the tiles on the top layer (roof
  edges, grass tufts along paths, tree tops): they are now a hardware "darken"
  window that shades every layer and the people under it evenly, and they fade in
  and out; cloud reflections on water are solid soft blues; walking into an area
  with different weather changes the brightness three times more slowly (clear
  weather no longer snaps back at once); map tiles also frost while the weather is
  turning to snow.
* **v3.0** — cloud reflections on water take the day/night shade (they stayed bright
  on dark water) and stay hidden while a weather change is still dimming the scene;
  the snow frost now eases in (and out) whenever it starts snowing — before, it was
  skipped when the previous weather was just as dark (overcast, rain, ash).

* **v2.9 save point** (source only, not released) — see v3.0b.
* **v3.0b** —
  * **Texture fix:** since v1.2, puddle art had been written into General-tileset
    tiles that looked unused, but other tilesets' metatiles point at General tiles
    too, so trees, roofs and buildings in 36 tilesets showed wrong textures. All
    original tiles were restored; duplicate General tiles (same picture or a
    mirror) were merged to free 56 slots, and the puddle/snow art lives there now.
    Every original metatile in every tileset was checked to look exactly as in the
    original game.
  * **Drought:** only 12:00–15:59 inside the day period; dusk gives plain sun, night
    clear skies (an area could sit in a drought for most of a day).
  * **Cloud reflections** never appear under the player.
  * **OWE** (option, Off/On/Restrict): wild Pokémon walking in the grass
    (`src/overworld_wild_encounters.c`).
  * **Start-menu clock:** HH:MM in the chosen window frame at the top left, only
    while the start menu is open.
  * **Enemy shadows** (option SHADOWS, On by default): every opposing Pokémon gets a
    shadow, not only floating ones.
  * **Defaults on a new game:** BACKGROUND Perfect, BATTLE TERRAIN With, FAST INTRO
    On, SHADOWS On, OWE Off.
  * **Snow piles:** while it snows, white mounds of 1, 2 or 4 tiles form on short
    grass like puddles do; they go when the snow stops (falling on puddles, it
    still freezes them instead).

* **v3.1** — cloud reflections on water removed; clouds only cast shadows (which
  darken water too). The reflections had to jump away from the player, which
  looked like clouds teleporting. First ROM released with all the v3.0b changes.

---

## 4. How everything works now

### 4.1 Dynamic weather (overworld)
Code: `src/field_weather_effect.c` (dynamic weather section),
`src/data/dynamic_weather_map_roles.h`, `src/field_weather.c`, `src/field_tasks.c`,
`src/coord_event_weather.c`, `src/field_specials.c`, `src/overworld.c`.

* Maps with header weather `WEATHER_DYNAMIC` (all outdoor routes, towns, cities,
  Safari Zone, Battle Frontier, Petalburg Woods, Mt. Pyre exterior) take their
  weather from the real-time clock: four periods a day (morning, day, evening,
  night), fixed per save file and per day, so soft resets don't reroll it.
* **Zones and climates.** Maps are grouped into weather zones; each zone has a climate
  with its own odds (rainforest wet, desert sandstorms, mountains snow…).
* **Map roles** (generated from the map connections): one route per city shares
  its weather, the city's other routes get a different but neighbouring weather,
  routes leading into another terrain keep their own weather, routes between routes
  copy a same-terrain neighbour.
* **Neighbours:** harsh sun – sunny – cloudy – overcast – rain – downpour –
  thunderstorm; fog – mist – snow; sandstorm next to harsh sun/sunny.
* **Special rules:** volcanic ash constant around the volcano and in lava caves
  (Fiery Path, Magma Hideout, Terra Cave); sandstorm only in the Route 111 desert
  (Mauville 25 % less), desert music only with its own sandstorm; dawn mist
  04:30–06:30; harsh sun turns clear at night.
* **Story weather wins:** scripts and weather tiles take control
  (`FLAG_DYNAMIC_WEATHER_OVERRIDDEN`); control returns on map reload, `resetweather`
  or a `COORD_EVENT_WEATHER_DYNAMIC` tile (Kyogre/Groudon, Sootopolis, Route 111
  desert, ash patches, Mt. Pyre fog…). Routes 119/123 no longer revert to their old
  fixed cycle. "Continue" refreshes the weather at once.
* Refreshed about every 2 minutes, half a cycle after the game's own time events,
  reading the clock at most once every half second.

### 4.2 Weather effects
* **Mist** (`WEATHER_MIST` = 16): fog without shading, 85 % more transparent; at night
  its blend is lowered so the scene stays at the normal night shade.
* **Clouds:** drifting shadows made with the GBA's OBJ window + darken effect
  (BLDY 4/16), so ground, roofs, tree tops and people under a cloud are all shaded
  evenly; they fade in/out one step every 6 frames. The full-screen window 0 the
  overworld normally keeps is shrunk while shadows are up (it outranks the OBJ
  window) and restored afterwards. No reflections on water since v3.1 (the code
  is kept behind `CLOUD_REFLECTIONS_ON`).
* **Weather changes between areas:** the brightness change runs three times slower
  than before (60 frames per step; clear weather used to snap back instantly).
* **Snow:** frosty tile palette (text box excluded). The frost has its own level
  (0–6/32 toward pale blue-white) that steps every 30 frames toward full while it
  snows and back to none afterwards; entering a map where it is already snowing
  shows it at once.
* **Drought:** overworld flashes peak about 10 % brighter than normal.

### 4.3 Rain puddles (routes)
Code: end of `src/field_weather_effect.c`; shapes and piece sets in
`src/data/rain_puddle_shapes.h`; art in `data/tilesets/primary/general/`.

* Form on open short grass on routes (towns and cities use wet ground, §4.4).
* **Sizes:** 1 tile, 2 tiles (either direction), 4 tiles (2×2); odds 30 : 22 : 12.
* **Look (v2.7):** every size uses Emerald's own puddle colours — moving blue water
  with a dark edge over grass. 4 tiles = Emerald's original 2×2 puddle; 1 and 2 tiles
  are hand-drawn uneven shapes in the same colours.
* **Reflections:** the water is on the bottom layer, so reflections show; walking
  through splashes.
* **Rate per grass tile:** rain / thunderstorm 4.66 %, downpour 5.36 %.
* **Fresh random set** every time a map is entered.
* **Life cycle while you stay:**

  | Weather | Puddles |
  |---|---|
  | rain, thunderstorm, downpour | full set |
  | rain → cloudy, overcast, mist or fog | 20 % fewer |
  | rain → snow | frozen (slippery ice), 20 % fewer; a 2×2 keeps its top row |
  | rain → snow → sunny | thawed back to water, 20 % fewer |
  | no rain again after that | dried up |

* Never placed on or next to other puddles/ponds, nor on people, items, Cut trees,
  Smash rocks, signs, hidden items or warps.
* Earlier looks (v1.0 native 2×2, v1.2 sand-rimmed 1–7 tiles, v2.0 thin rim, v2.4
  pale pills) are superseded.

### 4.4 Wet ground in towns and cities
While it rains in a town or city, every open walkable outdoor tile acts like a
puddle surface for the player: a see-through reflection shows on the ground and
steps splash. No puddle tiles are placed. (Player only: giving every NPC a
reflection would use up the sprite colour slots.)

### 4.5 Battle weather effects (`src/battle_weather_fx.c`)
* Rain, sandstorm, harsh sun and hail loop for as long as the weather lasts (menus,
  messages, attack animations; not the Bag/party screens).
* Constant tint: rain darker, sand tan, hail darker, sun brighter; harsh sun from an
  overworld drought is 7 % brighter than Sunny Day's (blend weight 88/256 vs 82/256).
* Nothing is redrawn once the battle is decided (fixes the after-battle flash).
* Uses its own random numbers, so link battles stay in sync.

### 4.6 Enemy intro loop
The opposing Pokémon replays its intro animation every few seconds while you are in
the battle menus (no cry). Choosing an action stops it and restores the sprite exactly.

### 4.7 Battle terrain rules (`src/battle_setup.c`)
* `BATTLE_TERRAIN_PATH` for outdoor trainer battles with no wild-Pokémon terrain
  nearby; land trainer battles never get lake/ocean.
* Snow: grass/path/plain/sand/forest battles use Snow, mountains Rock Snow.

### 4.8 Background sets (`src/battle_bg.c`) — option BACKGROUND
| Setting | Set |
|---|---|
| Old | Modern Emerald's original backgrounds |
| Modern | CFRU backgrounds (supplied FireRed patch, `graphics/battle_terrain/cfru_fr/`) |
| Perfect | CFRU with the Perfect edits (`graphics/battle_terrain/perfect/`) |

* **Modern (CFRU):** 20 pictures, pixel-exact; generated dusk/night colours for the 11
  outdoor scenes; Gym picture for every battle inside a Gym; Space for the Champion.
  Route 113 ash falls back to the Heart & Soul picture.
* **Perfect:** tall grass = the supplied path pictures (day, night, and a drought
  version while an overworld drought is on); underwater = the supplied underwater
  picture; lava caves and Mt. Chimney = Volcano; buildings = Lab; Gyms, League rooms,
  Battle Frontier and Trainer Hill = Gym; trainer battles on bridges = Town; Abandoned
  Ship = Space; Neutral not used; everything else as CFRU.
* Pictures with more colours than the GBA allows were converted with a palette
  optimiser; the night path and underwater pictures changed slightly in colour.

### 4.9 Terrain circles (`src/battle_terrain_circles.c`) — option BATTLE TERRAIN
Platforms from Modern Emerald 3.5 drawn under the Pokémon: grass, long grass,
sand, water, pond, rock, cave, building, plain, underwater, plus recoloured snow and
ash. **With:** on every background (all sets, special battles too), matched to the
terrain and time of day; they slide in with the background, follow shakes, take the
tints, hide while a move swaps the background; rock circles on bridges.
**Without:** none.

### 4.10 Title screen (`src/title_screen.c`, `graphics/title_screen/`)
"Pokémon" logo with ™ on the logo layer and "Perfect Emerald Version" as the banner
(two 64×64 sprites); "v3.5" removed; stray specks cleaned.

---

## 5. Options menu (`src/options_plus_menu.c`)
Battle page:
* **OWE** — Off / On / Restrict (overworld wild encounters; Restrict keeps them in
  the grass). Default Off.
* **BATTLE TERRAIN** — With / Without (terrain circles). Default on a first start: With.
* **BACKGROUND** — Old / Modern / Perfect. Default on a first start: Perfect.
* **SHADOWS** — Off / On (shadows under the opposing Pokémon). Default On.
* **FAST INTRO** — default On.

Defaults are set in `SetDefaultOptions()` (`src/new_game.c`), which runs when the game
boots with no save. Both choices are stored partly in flags, so `NewGameInitData()`
keeps them through the New Game reset; whatever you pick in OPTION before starting
is what the new game uses.

All multi-choice options are clamped into range on boot, New Game and menu open
(v1.1 hardware crash fix, `SanitizeOptions()` in `src/new_game.c`).

---

## 6. Flags and save data
| Flag | Use |
|---|---|
| 0x289 `FLAG_OWE_ON` | OWE = On or Restrict |
| 0x28A `FLAG_OWE_RESTRICT` | OWE = Restrict |
| 0x28B `FLAG_BATTLE_SHADOWS_OFF` | SHADOWS = Off |
| 0x28E `FLAG_BATTLE_TERRAIN_CIRCLES_OFF` | BATTLE TERRAIN = Without |
| 0x28F `FLAG_DYNAMIC_WEATHER_OVERRIDDEN` | a script owns the weather on a dynamic map |
| 0x290 `FLAG_CFRU_BATTLE_BACKGROUNDS` | legacy (v1.3), no longer read |
| 0x291 `FLAG_PERFECT_BATTLE_BACKGROUNDS` | BACKGROUND = Perfect (set by default on a first start) |

`optionsNewBackgrounds` (existing save field): 0 = Old, 1 = Modern or Perfect.
New saves and old saves both work; nothing else in the save layout changed.

---

## 7. Files changed
**New:** `src/battle_weather_fx.c`, `src/battle_terrain_circles.c`,
`src/data/dynamic_weather_map_roles.h`, `src/data/rain_puddle_shapes.h`,
`include/battle_weather_fx.h`, `include/battle_terrain_circles.h`,
`graphics/battle_terrain/{cfru_fr,perfect,circles,path,path_2}/`.

**Changed:** `src/field_weather_effect.c`, `src/field_weather.c`, `src/field_tasks.c`,
`src/coord_event_weather.c`, `src/field_specials.c`, `src/overworld.c`, `src/script.c`,
`src/event_object_movement.c`, `src/field_effect_helpers.c`, `src/battle_bg.c`,
`src/battle_setup.c`, `src/battle_main.c`, `src/battle_anim.c`,
`src/battle_anim_mons.c`, `src/battle_anim_normal.c`, `src/battle_intro.c`,
`src/battle_script_commands.c`, `src/battle_anim_utility_funcs.c`,
`src/battle_controller_player.c`, `src/pokemon.c`, `src/pokemon_animation.c`,
`src/options_plus_menu.c`, `src/new_game.c`, `src/intro.c`, `src/title_screen.c`,
`data/scripts/abnormal_weather.inc`, `data/battle_anim_scripts.s`,
`data/tilesets/primary/general/*` (puddle tiles/metatiles),
secondary tilesets of Rustboro, Slateport, Lilycove, Fortree, Fallarbor, Mossdeep and
Ever Grande (unused city-puddle pieces from v2.2), `data/maps/*/map.json`,
`include/constants/{weather,battle,flags}.h`, `graphics/weather/drought/*`,
`graphics/title_screen/*`, `build.sh`.

---

## 8. Known issues and open items
* **Sound glitch on real hardware** (short drops, worse when running around): not
  reproduced in an accurate emulator. Clock reads were reduced (v1.6, v2.2). The
  likely cause is the GBA's built-in decompression blocking interrupts on real
  hardware; a replacement was written and matched all 4,000 compressed files, but it
  froze battles in testing and was removed. Still open.
* Leob0505 water background: port packaged separately, not in the ROM.
* Heart & Soul backgrounds are no longer selectable; they only fill gaps (Route 113 ash).
* Wet-ground reflections and splashes are for the player only.
* Frozen puddles are slippery (standard ice).

---

## 9. How the work was tested
Each version was built and run headless in mGBA with a test-only boot hook (not part
of this source) that warps to any map, sets the clock and weather, and starts wild
or trainer battles, taking screenshots. Checks included: weather in every zone,
puddle placement and life cycle, reflections, battle backgrounds in every set and
time of day, terrain circles, option screens, title screen, real boot to New Game,
and frame-by-frame checks of the after-battle fade. The source rebuilds to the
delivered ROM byte for byte.

---

## 10. Credits
* pokeemerald decompilation — pret. Modern Emerald — resetes12 and contributors.
* Heart & Soul battle backgrounds as shipped in the base source.
* CFRU "Battle Backgrounds Patch FR": LibertyTwins, princess-phoenix, carchagui,
  aveontrainer, WesleyFG, kWharever, worldslayer608, knizz.
* Terrain circles: Modern Emerald 3.5.
* Leob0505 water background (separate port): Team Aqua's Hideout asset repo.
* Path (day/night/drought), underwater, title-logo and puddle artwork: supplied by the
  Perfect Emerald author.

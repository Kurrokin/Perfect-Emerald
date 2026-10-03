# Perfect Emerald — complete documentation

**Current version: v7.1** (3 October 2026)

Perfect Emerald is a Pokémon Emerald ROM hack built from source. This document
covers everything done to it, from the first request to v7.1: what was asked,
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
    On, SHADOWS On, OWE Off (On since v3.2).
  * **Snow piles:** while it snows, white mounds of 1, 2 or 4 tiles form on short
    grass like puddles do; they go when the snow stops (falling on puddles, it
    still freezes them instead).

* **v3.1** — cloud reflections on water removed; clouds only cast shadows (which
  darken water too). The reflections had to jump away from the player, which
  looked like clouds teleporting. First ROM released with all the v3.0b changes.

* **v3.2** — OWE (overworld wild encounters) is On by default for a new game.

* **v3.3** —
  * **UI INTERFACE** (Main options, above TEXT SPEED): Default / Perfect. Perfect
    is the "Unbound start menu" ported from miriamlefae's pokeemerald-expansion
    branch `feat/usm/upcoming` (`src/unbound_start_menu.c`, graphics in
    `graphics/unbound_start_menu/`): icon bar, weekday and time, button hints,
    Safari/Pyramid info. Save and Pyramid Retire use the normal save dialog;
    DexNav and the debug menu are not included; icon order is not saved.
  * **WEATHER EFFECT** (Battle options, under OWE): Regular / Perfect (default
    Perfect = the always-on weather effects and tint).
  * **BATTLE ANIMS**: On / Off / Perfect (default Perfect; called Extended in
    v3.3); the foe's idle animation loop only runs with Perfect.
  * Battle options order: OWE, WEATHER EFFECT, BACKGROUND, SHADOWS, BATTLE TERRAIN.
  * Shadows under the opposing trainer, and under a Pokémon just before and just
    after it evolves.
  * Enemy shadows stay during attacks, and appear as the Pokémon comes out.
  * Mist a little less see-through (blend 3/16); cloud shadows fade in more gently.
  * Tracked puddles per map: 48 (RAM is nearly full).

* **v3.4** — BATTLE ANIMS choices are On / Off / Perfect (the word "Extended" is gone).

* **v3.5** — puddles, ice, thaw and drying react at the weather check *after* the
  one that started the rain/snow/sun (about 2¼ minutes later); every change is
  done one puddle at a time (one every 20 frames) so they appear, freeze, thaw and
  go gradually; at most 45 puddles per map; mist a little less see-through
  (blend 4/16).

* **v4.0** —
  * **Puddles/ice/snow piles** react 10 s after the weather changes on screen and
    dissolve in / out (8 dither steps under a grass overlay sprite): option
    **PUDDLES** (Main): Off / Slow (2 s) / Fast (1 s), default Slow.
  * **WEATHER SPREAD** (Main, under WEATHER SYSTEM): Normal / Extended / Perfect -
    see v4.5.
* **WEATHER SYSTEM** (Main, under UI INTERFACE): Scripted (each map's original
    weather, taken from Modern Emerald 3.5; `src/data/original_map_weather.h`) /
    Perfect (dynamic, default). Takes effect on the next map load.
  * **Ripples:** now and then in open pond water (away from the edges); in rain,
    lots of ripples on every reflective surface and on the wet ground of towns.
  * **Battle options order:** BATTLE STYLE first; MOVE SPLIT then CURSOR MEMORY
    last. **BACKGROUND** shows one value between arrows, lined up with the other
    rows (it was pushed to the left).
  * **Perfect UI:** no sprite flash on a black screen when opening a menu.

* **v4.1** — snow cover: 10 s after snow starts, the map is covered over about 8 s
  (green colours - grass, bushes, leaves - turn to shaded snow, everything else
  frosts a little) and melts the same way when it stops. Done with the map
  colours, so no tiles are replaced and nothing else is given up.

* **v4.2** — **BACKGROUND = Modern +** (Old / Modern / Modern + / Perfect): the 23
  supplied "PokeEmerald expansion" pictures (`graphics/battle_terrain/modern_plus/`):
  tall grass, path (+ drought) for paths and plain ground, beach (dusk = "BEACH 2"),
  desert, lake, ocean, mountain (dusk = "MOUNTAIN 2"), cave, water cave ("CAVE 2"),
  snow and underwater, each with its night picture where supplied; other terrains,
  Gyms and the Champion use Modern. New `tilesetTwilight` / `tilemapTwilight` for
  dusk/dawn pictures. Clear-weather pond ripples every 1–3 s on a random open pond
  tile in view.

* **v4.3** — snow cover follows the supplied example sheet: snow settles on the
  brightest greens first (the tops of leaves, grass tufts, bushes, the ground),
  then works down to the darkest greens, which become shaded snow so trees and
  grass keep their shape; 10 s after snow starts, over about 12 s. (The sheet is a
  scaled phone screenshot in another art style, so its tiles couldn't be used
  directly; the game has no snowy tiles of its own.)

* **v4.4** — snowy tiles: Emerald's own trees, fences, signs and boulders (35 tiles
  of the General tileset) get snow in three stages - a dusting on the tops of
  leaf clumps, rails, posts and boards, then snow, then thicker snow - swapped
  into video memory as the snow cover grows and back as it melts
  (`src/data/snow_tile_swaps.h`, drawn from the tiles themselves in the style of
  the supplied sheets). Ponds never freeze. Nothing is replaced in the map data.

* **v4.5** — **WEATHER SPREAD** (Main, under WEATHER SYSTEM; greyed out with
  Scripted weather):
  * **Normal** — as v4.4: weather changes at 05:00, 10:00, 17:00 and 21:00; 19 areas.
  * **Extended** — same 19 areas, new times: 00:00, 05:00, 10:00, 14:00, 18:00.
  * **Perfect** (default for a new game) — new times, and neighbouring areas share
    one weather, giving 9 zones: Littleroot+Petalburg, Rustboro+Petalburg Woods,
    Meteor Falls+Fallarbor/Route 113, Desert+Volcano, Slateport+Dewford,
    Mauville+Fortree/Routes 119-120, Lilycove+Mt. Pyre, Mossdeep+Sootopolis+Ever
    Grande, Pacifidlog seas+Battle Frontier. Each zone uses its first area's
    climate; the volcano's ash and the desert's sandstorm still apply.
  Harsh sun: only 10:00-18:00 (and 12:00-15:59 at most) with the new times.

* **v4.7** — fixes:
  * **Screen tearing / sound drops on hardware:** every decompression into RAM
    (sprite sheets, palettes, tilesets, menus such as the Bag opened to plant a
    Berry) now runs with interrupts on (`src/lz77_irq_friendly.c`, all calls routed
    by `gba/syscall.h`); the BIOS version held the VBlank interrupt back, so the
    screen's palette/scroll update could land mid-frame and the sound driver ran
    late. Video / palette memory destinations still use the BIOS. Checked on all
    5,608 compressed files: identical output.
  * **Mist** is now an even haze (the GBA's brighten effect, 4/16 by day, 1/16 at
    night) over every layer - tree tops and roofs included - instead of fog
    sprites that sat below the top layer.
  * **Snow** turns a green the same shade of snow whatever palette it is in (the
    grass ring drawn around puddles and soil in another palette turned blue-grey);
    the ground is whiter.
* **v4.8** — reflections: with the mist no longer made of sprites, reflections
  show in mist again (sprites hide any sprite below them). Checked at a pond in
  clear weather, clouds, rain, mist and snow. Fog, ash and sandstorm are still
  sprite layers that cover reflections, as in the original game.

* **v4.9** — review of the Perfect Emerald code with extra compiler checks, a 22-map
  tour; two unused variables removed.
* **v5.0** — cheaper per-frame work (puddles, overworld Pokemon, mist, snow colours).
  (v5.0B was dropped; v5.1 continues from v5.0.)
* **v5.1** —
  * **POND BEHAVIOR** (Main, under WEATHER SYSTEM): Dynamic (ponds freeze in snow:
    the pond-water tiles are swapped for ice tiles and the water stops moving; never
    on a map with sea) / Default (default for saves and new games).
  * **WEATHER SPREAD** renamed and reordered: Normal / Wide (9 wide areas, the old
    "Perfect") / Perfect (new times, normal areas; the old "Extended"; default for
    new games). **BACKGROUND** "Perfect" is now **Almost Perfect**.
  * **Time of day overlays** (outdoors, on top of the day/night tint, fading over
    about 5 s): dawn 05:00-05:59 soft pink; dusk 17:00-17:29 orange, 17:30-17:59
    purple twilight; dead of night 02:00-04:59 8% darker (towns and cities too).
    Done as colour filters inside the day/night palette mix
    (`ApplyTimeTintToPalettes`), so every palette rebuild keeps them.
  * **Overworld Pokemon:** 02:00-04:59 15% chance of a random Ghost type, 05:00-07:59
    10% chance of a random Electric type (non-legendary, with an overworld sprite;
    grass only).
  * **Lightning in thunderstorm battles** (WEATHER EFFECT Perfect): the battle
    background flashes every 4-12 s; never the text box, menus, health boxes or
    Pokemon, never during a move animation, a fade, or another colour effect.
  * **Waves** move twice as fast in rain outdoors. **Ripples** in ponds can appear
    at the edges too; in rain only the rain ripples.
  * **Mist**: the drifting cloudy texture is back, as an OBJ-window brighten effect,
    so it covers tree tops and roofs and keeps reflections.
  * **Perfect start menu**: LEFT/RIGHT only, wrapping (Pokedex <-> Options).
  * **Fixes**: text left behind when scrolling UP from the first option (the list
    is longer than two screens); options kept through New Game include POND
    BEHAVIOR.

* **v5.2** — weather and time of day change the overworld wild Pokemon (grass only,
  never on water). Pools are built from the game's own species data by
  `genowe.py` into `src/data/owe_condition_species.h`: never legendary, never a
  fish (Water 2 egg group plus a list), always with an overworld sprite; "small" =
  first stage and at most 1.0 m. Weather first: downpour 15% small Water;
  thunderstorm 15% Electric; rain 15% Bug; drought 15% Fire/Flying, else 15% small
  Fire on Routes 111-113 / Mauville; sunny 15% small Grass; clouds or overcast 15%
  Flying; snow 15% small Ice; fog 15% first-stage Dragon line; mist 15% Ground.
  Then time of day: 02-05 15% Ghost; 05-06 15% small Fairy; 06-10 10% small
  Electric; 10-17 10% any (not Ghost, Dark or pure Poison); 17-18 15% small
  Psychic; 18-02 15% Dark.

* **v5.3** —
  * **Thunderstorms always rain by day:** from 05:00 to 17:59 the overworld lightning
    only starts while the rain is fully falling, and in battle the flashes only
    come while the rain is on screen.
  * **Battle weather from the first frame:** the rain / sandstorm / harsh sun / hail
    that the overworld brings into the battle shows from the start of the battle
    scene (the trainer slides in, or the wild Pokemon appears, already in it), then
    the battle's own weather takes over.
  * **Lightning in battle** now also lights up the opposing trainer and Pokemon
    (their own palettes, brightened at the end of each frame after every other
    palette change, in step with the background), never the player's trainer or
    Pokemon, the health boxes, the text box or the menus.

* **v5.4** — the battle lightning also lights up the battle terrain circles (both,
  as part of the ground). "Small" for the weather / time-of-day spawns is now up to
  1.2 m (first stage, all other rules unchanged): adds Slowpoke and Seel to the
  downpour Water list, Slowpoke and Solrock to the dusk Psychic list.

* **v5.5** — Poke Ball flashes: when a ball opens (throw / catch, break-out, and
  the send-out ball of your or the opponent's Pokemon) the game flashes the battle
  background white using a fixed mask of palettes 1-3. The new background sets use
  palettes 2-4, so the part drawn with palette 4 (tree tops, sky...) and the terrain
  circles stayed unflashed - the top of the picture stood out. All four calls
  (`battle_anim_throw.c`, `pokeball.c`) now use the full battle-background mask
  (palettes 1-4 + terrain circles). Checked on Old, Modern, Modern + and Almost
  Perfect, for the send-out and the catch flashes: none leaves part of the
  background unflashed.

* **v5.6** —
  * **Mist** is exactly the v4.5 mist again (tiny drifting mist clouds). Reflections
    can't show through it: where sprites overlap, the GBA shows the one with the
    higher priority setting; the clouds need priority 2 to cover the ground, and
    reflections need priority 3 to stay tucked under the shoreline. (Putting the
    reflections first in the sprite order was tried; it makes no difference.)
  * **Puddles cut in half:** outside the map the game reads the map's border block
    (often grass), so a 2-tile or 4-tile puddle could be placed with part of it past
    the edge - only the inside part was drawn. Every puddle cell (and snow pile) is
    now inside the map with one tile to spare, and the fade-in / fade-out overlay
    covers a puddle completely or not at all.
  * 1- and 2-tile puddles use the uneven rimmed art (unchanged; checked).

* **v5.7** — mist also over tree tops and roofs (the top map layer gets a matching
  haze); pond ripples over the whole surface (big ponds are mostly shallow
  "puddle" water); wild overworld Pokemon never step onto tiles with tree-top or
  roof art and keep ground priority.
* **v5.8** — **new party screen** with UI INTERFACE = Perfect (Default keeps the
  original): the "Yes You" design - PARTY banner, dark grid background, blue slot
  boxes with a cyan HP strip, the selected slot lighter with a red outline, grey
  for fainted, dark empty slots, new HP bar colours, new status icons (PAR BRN
  PSN SLP FRZ), and the arrow panel as the Cancel button (lighter when selected;
  no Poke Ball sprites). Built by `tools/gen_perfect_party.py` from the design
  sheet into `graphics/party_menu/perfect/` (tiles, tilemaps, 11 palettes with
  every slot state) and `include/perfect_party_ui.h`; `party_menu.c` picks the
  Perfect assets, slot art, text positions and button art when the flag is set.
  The battle HP bars (BW style) are not done yet.

* **v6.0** —
  * **Sprites in rain (freezes, glitches):** rain used 59-63 of the 64 sprites (ripples
    up to 26, rain drops 24) - effects lost graphics or colours, the Perfect start menu
    broke, and a battle could hang at its start. Now ripples never use more than 16
    (12 in a downpour), rain and thunderstorms make 22 drop sprites (a downpour 24),
    puddles are 10% fewer, and ripples / puddle fades always leave 16 sprites free.
    Measured: rain 47, downpour 46, Rustboro in rain 48 of 64. Rain drops whose sprite
    could not be made are skipped (they used to be read anyway).
  * **Start menus:** while the vanilla or Perfect start menu is open the weather
    stands still (no drops falling, lightning, ripples, puddle fades or weather
    changes; screen fades still run).
  * **Mist:** the tiny drifting clouds now brighten everything under them - ground,
    tree tops, roofs, you, people and Pokemon - as an OBJ window, so nothing is drawn
    over reflections and they show again. Still during start menus; hidden while the
    Perfect start menu is open (it uses the OBJ window).
  * **Rain in towns and cities:** everyone reflects on the wet ground - people who stand
    still and the follower too (checked twice a second); never on grass: the ground
    check uses tables of grass metatiles made from the art (`tools`-made
    `src/data/grassy_metatiles.h`). A reflection with no free palette shows unfiltered
    instead of not at all.
  * **Options** reorganised into pages - Main, UI, World and Weather, Battle (with
    "Battle animation:", "Battle scene:", "Battle settings:" headers), Sound - and
    every setting shows only its current choice between arrows, in the same columns.
    AUTO RUN is one setting: Off / Land / Everywhere. FAST INTRO is now INTRO and FAST
    BATTLES is BATTLE TEXT (Slow / Fast). BATTLE ANIMS: Off (none) / Partially (moves
    only) / On (+ intro animations) / Modern (+ idle animations with a pause) / Perfect
    (+ idle without a pause). New UI BAG setting (Vanilla / Modern / Perfect) - the new
    Bag itself is not in yet.
  * Dawn pink made a little stronger (it was faint); dusk orange and purple confirmed.

* **v6.1** — **UI BAG = Modern**: the new bag look from the "Bag Screen with Party"
  design, fitted to the game's bag layout - orange background with a darker band, a
  dark item-list panel with white text, a dark description panel, a white item-icon
  box, a dark pocket-name tab and new pocket dots (`tools/gen_modern_bag.py` ->
  `graphics/bag/modern/`; the tiles the bag code draws itself - the pocket-switch wipe
  and the dots - sit at the same tile numbers). **Perfect** shows the same look for now;
  the party panels on the left (using items on a party member there) are not in yet.

* **v6.2** — **UI BAG = Perfect**: the Modern look plus your party in six panels
  (3 x 2) where the bag picture was - each Pokemon's icon and an HP bar (green /
  yellow / red), drawn in a window over panel art (`tools/gen_modern_bag.py perfect`).
  Using or giving an item still asks which Pokemon on the party screen.

* **v6.3** —
  * Bag: Perfect party panels spaced clear of the item-icon box; pocket name shadow dark.
  * POKEMON'S ANIMATION (was BATTLE ANIMS): Off / On / Always (idle, no pause) /
    Modern (idle, long pause) / Perfect (very short pause) / Perfect 2 (1 s pause);
    ANIM SPEED is now second on the Battle page.
  * Mist: returning from Options (or anything from the start menu) in mist hung the
    game - the mist's set-up waited for the start menu to close. Fixed (set up at once).
  * Every weather sprite stands still while a start menu is open.
  * No cloud shadows at night (18:00 until dawn at 05:00): clear sky instead.
  * Weather / time spawns: no fossil Pokemon, no pseudo-legendary lines, only species
    that live wild somewhere in the game; starters (all generations, whole lines) only
    as a 1-in-100 pick from the matching pool.
  * Party screen (Perfect UI): name and level centred right of the icon, HP numbers
    centred on the lower band; in the wide slots the gender sits next to the level.
  * Snow: puddles freeze only once the snow has covered every tile.
  * A player named Rudy finds a Dratini, a Charmander and a Gastly (level 5, male, his
    ID, maximum friendship) in the last PC box after the first Gym badge
    (`src/perfect_gifts.c`).

* **v6.4** —
  * **MIST** setting (World and Weather, under WEATHER SPREAD): **Old** = the v6.3 mist
    (cloud shapes brighten what is under them); **Perfect** (default) = the first mist's
    soft drifting texture on its own layer (BG0, tiles 0x2C0-0x2FF, palette 13), blended
    over everything with the GBA alpha effect: ground, tree tops, roofs, the player,
    people and Pokemon, and reflections (drawn underneath, they show through). The
    texture follows the map as you walk and drifts slowly; it steps aside while a text
    box or menu is on screen (they share BG0) and during screen fades; it covers the
    leftover map of the closed map-name pop-up.
  * Cloud shadows: four shapes (the cloud and its mirror images).
  * Battle: the new background pictures are blanked under the menus (rows 14-19), so
    nothing shows through the move info box corner.

* **v6.5** — SPAWN RATE setting (World and Weather, under WEATHER SPREAD): Less = every
  weather / time rule 2%, More = as listed (10-15%), Perfect (default) = 7%; the
  1-in-100 starter pick applies on top. The "lives wild in this game" rule and the
  day-time "any Pokemon" rule are gone (by day: the area's own Pokemon and the weather
  rules). Rudy's gift: a Cyndaquil instead of the Charmander.

* **v6.7** (from v6.5; the v6.6 changes are not in it) —
  * Bag (Modern / Perfect): the item icon centred in its white box.
  * Party screen (Perfect UI): names and levels centred by their real width; the
    gender follows the name in the first slot and sits on the level line in the wide
    slots; "HP/max" centred as one group.
  * Rain in towns: a reflection whose sprite went away while the flag stayed set (the
    follower after going back into its Ball) is made again.

* **v6.8** — bag item icon moved 1 px left (measured: now centred in the white box).

* **v6.9** — party screen (Perfect UI): names, levels and HP numbers raised 2-3 px.

* **v7.0** — Pokedex list screen (UI INTERFACE = Perfect): the squares background in one
  solid red, the red of the new start menu. The squares share tiles with the panel
  borders, so 76 new tiles were made (`src/data/pokedex_red_squares.h`, from the screen
  composed of both layers and flood-filled from the edges): background pixels use
  palette 9 entry 14 (red); the panels, title and labels are unchanged.

* **v7.1** — new-game defaults: every setting that has a Perfect choice starts on Perfect
  (UI INTERFACE, UI BAG, POKEMON'S ANIMATION, WEATHER SYSTEM, WEATHER SPREAD, MIST, SPAWN
  RATE, WEATHER EFFECT); ANIM SPEED 1x, BATTLE TEXT Fast, INTRO Fast, BACKGROUND Almost
  Perfect, BATTLE TERRAIN Without, SHADOWS On, LARGE FOLLOWER On. (Defaults are set when
  the save is empty.) **To do:** when a full "Perfect" BACKGROUND set is added, make it
  the new-game default.

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
  the grass). Default On (since v3.2).
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

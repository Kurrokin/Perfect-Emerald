# Changes in this build

## Battle backgrounds (HnS 2.0.6 / pokeemerald-expansion art only)
* **Crash fix** — terrains 10-15 (snow, snowy rock, snow cave, cave water, volcano,
  blue building) indexed past the end of the intro-slide, Nature Power and
  Camouflage tables. With the fast-intro option the game could jump to a garbage
  address at battle start. Every table now covers every terrain, with bounds checks.
* **Trainer & fishing battles use the area's background.** Trainer battles on a path
  (and fishing from the shore) used to show the "plain" background. The game now
  looks at the terrain around you (grass, long grass, sand, sea, pond, mountain) —
  or the water you cast into — and shows the background a wild battle there would
  have. Gym Leaders, Elite Four, Champion, Team hideouts and Frontier keep theirs.
* **Day / twilight / night** — table-driven like HnS. The HnS morning palettes
  (grass, sand, water, pond, rock) were ported earlier but never used; they now
  show at dawn (05-09) and dusk (18-21). Night 21-05. Only on maps with natural light.
* **Intro fix** — the sliding intro strip used the classic tiles under the HnS
  palette (wrong colours); it now uses the matching HnS set.
* **Never falls back to stock art** — `optionsNewBackgrounds` defaults to 1, and the
  in-battle reload path now uses the same loader, so it can't load the classic table.
* **Long grass** HnS entry had no palette pointer; fixed by the new table.
* **Per-location backgrounds** (`sMapDefaultEnvironments`, src/battle_setup.c), all
  using existing HnS art:
  Petalburg Woods → grass/forest · Route 111 sand → desert (sand art) ·
  Route 113 & Jagged Pass → mountain (was beach sand / lava cave) ·
  Mt. Chimney → mountain (outdoor, was lava-cave art) · Mt. Pyre outside → grass ·
  Mt. Pyre inside, Abandoned Ship → building · ruins (Sky Pillar, Desert Ruins,
  Island Cave, Ancient Tomb, Sealed Chamber, Mirage Tower) → cave ·
  New Mauville → blue building · all Magma Hideout floors, Fiery Path,
  Terra Cave → volcano · Seafloor Cavern, Shoal Cave, Marine Cave → cave water.
* New terrain IDs 16-24 (FOREST, DESERT, ASH, CRATER, GRAVEYARD, TOMB_HALL, RUINS,
  SHIP, POWER_PLANT) so these places get fitting Nature Power / Camouflage /
  Secret Power effects while reusing the HnS art above.

## Dynamic weather (was dormant: no map used it)
* 61 outdoor maps now use `WEATHER_DYNAMIC` (all routes, towns, cities, Safari
  Zone, Battle Frontier, Petalburg Woods, Mt. Pyre exterior).
* Hoenn split into 19 weather sections with 11 climates (temperate, forest,
  coastal, open sea, rainforest, desert, volcanic, ash-fall, mountain, misty,
  highland). All maps in a section share the same weather.
* Hash-based daily seed (in-game day + trainer ID): no extra save data, same
  weather after a soft reset, different between save files.
* 4 periods per day (morning, day, evening, night) with persistence, moving
  storm fronts that cross the region west → east, no harsh sun at night.
* Live transition when a period ends while you stand on a dynamic map.
* `COORD_EVENT_WEATHER_DYNAMIC` (Porymap) hands weather back to the system; the
  "return to sunny" triggers on Routes 111, 113, 119, 120, 123, Jagged Pass and
  Mt. Pyre now use it. Local zones (desert sandstorm, ash patches, summit fog)
  and story weather (Sootopolis, Rayquaza) are unchanged and never overridden.
* `GetDynamicWeatherForecast(mapSec, dayOffset)` for future forecast scripts.
* Weather is my own design on the pokeemerald-expansion 1.17 `WEATHER_DYNAMIC`
  idea; the XTREME repository could not be downloaded.

## Tools
* `build.sh` — one command: ROM + verified BPS.
* `tools/bps/bps.py` — BPS create/apply (tested against an independent decoder).
* `.github/workflows/build.yml` — cloud build.

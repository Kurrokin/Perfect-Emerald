// ============================================================================
// Overworld wild encounters (v3.0b) - option OWE: Off / On / Restrict
// ----------------------------------------------------------------------------
// On:       up to three wild Pokemon from the area's grass encounter table walk
//           around in the tall grass near you; touching one (walking into it,
//           or it walking into you) starts a battle with it. Random battles in
//           the grass are switched off while this is on (water, caves, sand and
//           fishing are unchanged).
// Restrict: the same, but they can't step out of the grass.
// They appear 3+ tiles away inside the view, wander, and leave after a while
// or when you get far away. The one you battle is gone afterwards.
// ============================================================================
#include "global.h"
#include "battle_setup.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "overworld_wild_encounters.h"
#include "palette.h"
#include "random.h"
#include "script.h"
#include "wild_encounter.h"
#include "constants/event_objects.h"
#include "constants/event_object_movement.h"

#define OWE_MAX           3
#define OWE_LOCALID_BASE  230
#define OWE_SPAWN_FRAMES  90      // try to add one every 1.5 s
#define OWE_LIFETIME      (60 * 60)
#define OWE_FAR           10      // tiles: gone when you are this far away

struct OweMon
{
    bool8 active;
    u16 species;
    u8 level;
    u16 age;
};

static EWRAM_DATA struct
{
    struct OweMon mons[OWE_MAX];
    u16 mapId;
    u16 spawnTimer;
    s8 pending;
} sOwe = {0};

bool8 OweChooseLandMon(u16 *species, u8 *level);
void OweCreateWildMon(u16 species, u8 level);

static bool8 IsOweGrass(u8 behavior)
{
    return MetatileBehavior_IsTallGrass(behavior) || MetatileBehavior_IsLongGrass(behavior);
}

static bool8 IsOweOn(void)
{
    return FlagGet(FLAG_OWE_ON);
}

bool8 OweIsOweObject(const struct ObjectEvent *objectEvent)
{
    return objectEvent->active
        && objectEvent->localId >= OWE_LOCALID_BASE && objectEvent->localId < OWE_LOCALID_BASE + OWE_MAX
        && objectEvent->graphicsId >= OBJ_EVENT_GFX_MON_BASE;
}

// Restrict: they never step out of the grass.
bool8 OweBlocksMove(struct ObjectEvent *objectEvent, s16 x, s16 y)
{
    return OweIsOweObject(objectEvent) && FlagGet(FLAG_OWE_RESTRICT)
        && !IsOweGrass(MapGridGetMetatileBehaviorAt(x, y));
}

// Called from the movement collision check: you walked into one, or it into you.
void OweNoteCollision(struct ObjectEvent *mover, struct ObjectEvent *other)
{
    const struct ObjectEvent *mon = NULL;

    if (mover->isPlayer && OweIsOweObject(other))
        mon = other;
    else if (other->isPlayer && OweIsOweObject(mover))
        mon = mover;
    if (mon != NULL)
        sOwe.pending = mon->localId - OWE_LOCALID_BASE;
}

bool8 OweSuppressRandomGrassEncounter(u16 behavior)
{
    return IsOweOn() && IsOweGrass(behavior);
}

static u16 CurrentMapId(void)
{
    return (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
}

static u8 GetOweObjectId(u32 i)
{
    return GetObjectEventIdByLocalIdAndMap(OWE_LOCALID_BASE + i, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
}

static void DespawnOwe(u32 i)
{
    if (GetOweObjectId(i) < OBJECT_EVENTS_COUNT)
        RemoveObjectEventByLocalIdAndMap(OWE_LOCALID_BASE + i, gSaveBlock1Ptr->location.mapNum, gSaveBlock1Ptr->location.mapGroup);
    sOwe.mons[i].active = FALSE;
}

static void TrySpawnOwe(void)
{
    s16 px, py, x, y;
    u32 i, slot, tries;
    u16 species;
    u8 level, objectEventId;

    for (slot = 0; slot < OWE_MAX && sOwe.mons[slot].active; slot++)
        ;
    if (slot == OWE_MAX || !MapHasNaturalLight(gMapHeader.mapType))
        return;
    PlayerGetDestCoords(&px, &py);
    for (tries = 0; tries < 12; tries++)
    {
        x = px + (s16)(Random() % 15) - 7;
        y = py + (s16)(Random() % 11) - 5;
        if (abs(x - px) + abs(y - py) < 3)
            continue;
        if (!IsOweGrass(MapGridGetMetatileBehaviorAt(x, y)) || MapGridGetCollisionAt(x, y) != 0)
            continue;
        if (GetObjectEventIdByXY(x, y) != OBJECT_EVENTS_COUNT)
            continue;
        if (!OweChooseLandMon(&species, &level))
            return;   // this area has no grass encounters
        objectEventId = SpawnSpecialObjectEventParameterized(OBJ_EVENT_GFX_MON_BASE + species, MOVEMENT_TYPE_WANDER_AROUND,
                                                             OWE_LOCALID_BASE + slot, x, y, MapGridGetElevationAt(x, y));
        if (objectEventId >= OBJECT_EVENTS_COUNT)
            return;
        gObjectEvents[objectEventId].rangeX = 3;
        gObjectEvents[objectEventId].rangeY = 3;
        sOwe.mons[slot].active = TRUE;
        sOwe.mons[slot].species = species;
        sOwe.mons[slot].level = level;
        sOwe.mons[slot].age = 0;
        return;
    }
    (void)i;
}

static void StartOweBattle(u32 i)
{
    u16 species = sOwe.mons[i].species;
    u8 level = sOwe.mons[i].level;

    sOwe.pending = -1;
    DespawnOwe(i);
    OweCreateWildMon(species, level);
    BattleSetup_StartWildBattle();
}

void Task_OverworldWildEncounters(u8 taskId)
{
    u32 i;
    s16 px, py;

    if (sOwe.mapId != CurrentMapId())
    {
        for (i = 0; i < OWE_MAX; i++)
            sOwe.mons[i].active = FALSE;   // the old map's objects went with it
        sOwe.mapId = CurrentMapId();
        sOwe.pending = -1;
        sOwe.spawnTimer = 0;
    }
    for (i = 0; i < OWE_MAX; i++)
        if (sOwe.mons[i].active && GetOweObjectId(i) >= OBJECT_EVENTS_COUNT)
            sOwe.mons[i].active = FALSE;

    if (!IsOweOn())
    {
        for (i = 0; i < OWE_MAX; i++)
            if (sOwe.mons[i].active)
                DespawnOwe(i);
        sOwe.pending = -1;
        return;
    }
    if (ArePlayerFieldControlsLocked() || gPaletteFade.active)
        return;

    if (sOwe.pending >= 0)
    {
        i = sOwe.pending;
        if (i < OWE_MAX && sOwe.mons[i].active && gPlayerAvatar.tileTransitionState == T_NOT_MOVING)
        {
            StartOweBattle(i);
            return;
        }
        if (i >= OWE_MAX || !sOwe.mons[i].active)
            sOwe.pending = -1;
    }

    PlayerGetDestCoords(&px, &py);
    for (i = 0; i < OWE_MAX; i++)
    {
        u8 id;
        if (!sOwe.mons[i].active)
            continue;
        id = GetOweObjectId(i);
        if (++sOwe.mons[i].age > OWE_LIFETIME
         || abs(gObjectEvents[id].currentCoords.x - px) + abs(gObjectEvents[id].currentCoords.y - py) > OWE_FAR)
            DespawnOwe(i);
    }
    if (++sOwe.spawnTimer >= OWE_SPAWN_FRAMES)
    {
        sOwe.spawnTimer = 0;
        TrySpawnOwe();
    }
}

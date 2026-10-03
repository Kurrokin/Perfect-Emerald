// ============================================================================
// Battle terrain circles
// ----------------------------------------------------------------------------
// Modern Emerald 3.5 draws a terrain platform (grass, sand, water, rock...)
// under each side when BATTLE TERRAIN is set to Modern. Perfect Emerald's New
// backgrounds are full scenes, so the circles are laid over them as sprites:
// under the Pokemon, shadows, healthboxes and the text box, above the scene.
// * Dynamic: the circle matches the terrain (snow on snow, rock on rock,
//   water on water...) and the time of day (dusk / night colours).
// * BATTLE TERRAIN: Old and CFRU never show them; neither do backgrounds that
//   are not terrain pictures (Gym Leader, Champion, legendaries, Frontier).
// * They slide in with the background during the intro, follow background
//   shakes, get the same weather / move tints as the background, and hide
//   while a move swaps the background (Night Shade, Psychic...).
// ============================================================================
#include "global.h"
#include "battle.h"
#include "battle_bg.h"
#include "battle_terrain_circles.h"
#include "decompress.h"
#include "graphics.h"
#include "gpu_regs.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "constants/battle.h"

#define TAG_BATTLE_CIRCLES 0xB0B8
#define NUM_CIRCLE_SPRITES 4

enum
{
    CIRCLE_NONE,
    CIRCLE_GRASS,
    CIRCLE_LONG_GRASS,
    CIRCLE_SAND,
    CIRCLE_WATER,
    CIRCLE_POND,
    CIRCLE_ROCK,
    CIRCLE_CAVE,
    CIRCLE_BUILDING,
    CIRCLE_PLAIN,
    CIRCLE_UNDERWATER,
    CIRCLE_SNOW,
    CIRCLE_ASH,
    CIRCLE_COUNT
};

struct CircleGfx
{
    const u32 *gfx;
    const u32 *pal;
    const u32 *palTwilight;
    const u32 *palNight;
};

static const struct CircleGfx sCircleGfx[CIRCLE_COUNT] =
{
    [CIRCLE_NONE] = {0},
    [CIRCLE_GRASS] = {gBattleCircleGfx_Grass, gBattleCirclePal_Grass, gBattleCirclePal_Grass_Twilight, gBattleCirclePal_Grass_Night},
    [CIRCLE_LONG_GRASS] = {gBattleCircleGfx_LongGrass, gBattleCirclePal_LongGrass, gBattleCirclePal_LongGrass_Twilight, gBattleCirclePal_LongGrass_Night},
    [CIRCLE_SAND] = {gBattleCircleGfx_Sand, gBattleCirclePal_Sand, gBattleCirclePal_Sand_Twilight, gBattleCirclePal_Sand_Night},
    [CIRCLE_WATER] = {gBattleCircleGfx_Water, gBattleCirclePal_Water, gBattleCirclePal_Water_Twilight, gBattleCirclePal_Water_Night},
    [CIRCLE_POND] = {gBattleCircleGfx_Pond, gBattleCirclePal_Pond, gBattleCirclePal_Pond_Twilight, gBattleCirclePal_Pond_Night},
    [CIRCLE_ROCK] = {gBattleCircleGfx_Rock, gBattleCirclePal_Rock, gBattleCirclePal_Rock_Twilight, gBattleCirclePal_Rock_Night},
    [CIRCLE_CAVE] = {gBattleCircleGfx_Cave, gBattleCirclePal_Cave, gBattleCirclePal_Cave_Twilight, gBattleCirclePal_Cave_Night},
    [CIRCLE_BUILDING] = {gBattleCircleGfx_Building, gBattleCirclePal_Building, gBattleCirclePal_Building_Twilight, gBattleCirclePal_Building_Night},
    [CIRCLE_PLAIN] = {gBattleCircleGfx_Plain, gBattleCirclePal_Plain, gBattleCirclePal_Plain_Twilight, gBattleCirclePal_Plain_Night},
    [CIRCLE_UNDERWATER] = {gBattleCircleGfx_Underwater, gBattleCirclePal_Underwater, gBattleCirclePal_Underwater_Twilight, gBattleCirclePal_Underwater_Night},
    [CIRCLE_SNOW] = {gBattleCircleGfx_Snow, gBattleCirclePal_Snow, gBattleCirclePal_Snow_Twilight, gBattleCirclePal_Snow_Night},
    [CIRCLE_ASH] = {gBattleCircleGfx_Ash, gBattleCirclePal_Ash, gBattleCirclePal_Ash_Twilight, gBattleCirclePal_Ash_Night},
};

static const u8 sTerrainCircle[BATTLE_TERRAIN_COUNT] =
{
    [BATTLE_TERRAIN_GRASS]         = CIRCLE_GRASS,
    [BATTLE_TERRAIN_LONG_GRASS]    = CIRCLE_LONG_GRASS,
    [BATTLE_TERRAIN_SAND]          = CIRCLE_SAND,
    [BATTLE_TERRAIN_UNDERWATER]    = CIRCLE_UNDERWATER,
    [BATTLE_TERRAIN_WATER]         = CIRCLE_WATER,
    [BATTLE_TERRAIN_POND]          = CIRCLE_POND,
    [BATTLE_TERRAIN_MOUNTAIN]      = CIRCLE_ROCK,
    [BATTLE_TERRAIN_CAVE]          = CIRCLE_CAVE,
    [BATTLE_TERRAIN_BUILDING]      = CIRCLE_BUILDING,
    [BATTLE_TERRAIN_PLAIN]         = CIRCLE_PLAIN,
    [BATTLE_TERRAIN_SNOW]          = CIRCLE_SNOW,
    [BATTLE_TERRAIN_ROCK_SNOW]     = CIRCLE_SNOW,
    [BATTLE_TERRAIN_SNOW_CAVE]     = CIRCLE_SNOW,
    [BATTLE_TERRAIN_CAVE_WATER]    = CIRCLE_POND,
    [BATTLE_TERRAIN_VOLCANO]       = CIRCLE_ROCK,
    [BATTLE_TERRAIN_BLUE_BUILDING] = CIRCLE_BUILDING,
    [BATTLE_TERRAIN_FOREST]        = CIRCLE_GRASS,
    [BATTLE_TERRAIN_DESERT]        = CIRCLE_SAND,
    [BATTLE_TERRAIN_ASH]           = CIRCLE_ASH,
    [BATTLE_TERRAIN_CRATER]        = CIRCLE_ROCK,
    [BATTLE_TERRAIN_GRAVEYARD]     = CIRCLE_PLAIN,
    [BATTLE_TERRAIN_TOMB_HALL]     = CIRCLE_BUILDING,
    [BATTLE_TERRAIN_RUINS]         = CIRCLE_CAVE,
    [BATTLE_TERRAIN_SHIP]          = CIRCLE_BUILDING,
    [BATTLE_TERRAIN_POWER_PLANT]   = CIRCLE_BUILDING,
    [BATTLE_TERRAIN_PATH]          = CIRCLE_PLAIN,
};

static const struct OamData sOam_Circle =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .priority = 3, // in front of the terrain (BG3), behind the Pokemon (2)
};

static const union AnimCmd sAnim_Circle0[] = {ANIMCMD_FRAME(0, 1), ANIMCMD_END};
static const union AnimCmd sAnim_Circle1[] = {ANIMCMD_FRAME(32, 1), ANIMCMD_END};
static const union AnimCmd sAnim_Circle2[] = {ANIMCMD_FRAME(64, 1), ANIMCMD_END};
static const union AnimCmd sAnim_Circle3[] = {ANIMCMD_FRAME(96, 1), ANIMCMD_END};
static const union AnimCmd *const sAnims_Circle[] = {sAnim_Circle0, sAnim_Circle1, sAnim_Circle2, sAnim_Circle3};

static void SpriteCB_Circle(struct Sprite *sprite) {}

static const struct SpriteTemplate sSpriteTemplate_Circle =
{
    .tileTag = TAG_BATTLE_CIRCLES,
    .paletteTag = TAG_BATTLE_CIRCLES,
    .oam = &sOam_Circle,
    .anims = sAnims_Circle,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_Circle,
};

// Sprite centres on screen with the background at rest: enemy platform
// (x 112-239, y 48-79) and player platform (x 0-127, y 96-127, lower half
// behind the text box).
static const s16 sCirclePos[NUM_CIRCLE_SPRITES][2] = {{144, 64}, {208, 64}, {32, 112}, {96, 112}};

static EWRAM_DATA struct
{
    u8 type;          // circle for the background currently drawn
    u8 loadedType;    // graphics in VRAM (+ tod)
    u8 tod;
    u8 loadedTod;
    bool8 hidden;     // a move background replaced the terrain
    u8 spriteIds[NUM_CIRCLE_SPRITES];
} sCircles = {0};

static bool8 IsCircleSprite(u8 id)
{
    return id < MAX_SPRITES && gSprites[id].inUse && gSprites[id].template == &sSpriteTemplate_Circle;
}

static void DestroyCircleSprites(void)
{
    u32 i;

    for (i = 0; i < NUM_CIRCLE_SPRITES; i++)
    {
        if (IsCircleSprite(sCircles.spriteIds[i]))
            DestroySprite(&gSprites[sCircles.spriteIds[i]]);
        sCircles.spriteIds[i] = MAX_SPRITES;
    }
}

static void FreeCircleGfx(void)
{
    FreeSpriteTilesByTag(TAG_BATTLE_CIRCLES);
    FreeSpritePaletteByTag(TAG_BATTLE_CIRCLES);
    sCircles.loadedType = CIRCLE_NONE;
}

void ResetBattleTerrainCircles(void)
{
    u32 i;

    for (i = 0; i < NUM_CIRCLE_SPRITES; i++)
        sCircles.spriteIds[i] = MAX_SPRITES;
    sCircles.loadedType = CIRCLE_NONE;   // sprite data was just reset
}

void ClearBattleTerrainCircles(void)
{
    sCircles.type = CIRCLE_NONE;
    sCircles.hidden = FALSE;
}

void SetBattleTerrainCircles(u8 terrain, u32 timeOfDay)
{
    sCircles.type = (terrain < BATTLE_TERRAIN_COUNT) ? sTerrainCircle[terrain] : CIRCLE_NONE;
    sCircles.tod = timeOfDay;
    sCircles.hidden = FALSE;
}

void HideBattleTerrainCircles(void)
{
    sCircles.hidden = TRUE;
}

u32 GetBattleTerrainCirclePaletteMask(void)
{
    u8 index;

    if (sCircles.loadedType == CIRCLE_NONE)
        return 0;
    index = IndexOfSpritePaletteTag(TAG_BATTLE_CIRCLES);
    return (index == 0xFF) ? 0 : (1u << (16 + index));
}

static bool8 LoadCircleGfx(u8 type, u8 tod)
{
    struct CompressedSpriteSheet sheet = {sCircleGfx[type].gfx, 64 * 128 / 2, TAG_BATTLE_CIRCLES};
    struct CompressedSpritePalette pal = {sCircleGfx[type].pal, TAG_BATTLE_CIRCLES};

    if (tod == BATTLE_BG_TIME_NIGHT)
        pal.data = sCircleGfx[type].palNight;
    else if (tod == BATTLE_BG_TIME_TWILIGHT)
        pal.data = sCircleGfx[type].palTwilight;
    FreeCircleGfx();
    LoadCompressedSpriteSheetUsingHeap(&sheet);
    LoadCompressedSpritePaletteUsingHeap(&pal);
    if (GetSpriteTileStartByTag(TAG_BATTLE_CIRCLES) == TAG_NONE || IndexOfSpritePaletteTag(TAG_BATTLE_CIRCLES) == 0xFF)
    {
        FreeCircleGfx();
        return FALSE;
    }
    sCircles.loadedType = type;
    sCircles.loadedTod = tod;
    return TRUE;
}

// Horizontal offset of the terrain on a given screen line (the intro slides
// the top and bottom halves in from opposite sides with a per-line effect).
static s16 GetTerrainOffsetX(u32 line)
{
    if (gScanlineEffect.state != 0 && gScanlineEffect.dmaDest == &REG_BG3HOFS)
        return (s16)gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer][line];
    return gBattle_BG3_X;
}

void UpdateBattleTerrainCircles(void)
{
    u32 i;
    bool8 want = (sCircles.type != CIRCLE_NONE && !sCircles.hidden);

    if (gBattleOutcome != 0)
        return; // battle over: never re-create anything while it fades out

    for (i = 0; i < NUM_CIRCLE_SPRITES; i++)
        if (!IsCircleSprite(sCircles.spriteIds[i]))
            sCircles.spriteIds[i] = MAX_SPRITES;
    if (sCircles.loadedType != CIRCLE_NONE && GetSpriteTileStartByTag(TAG_BATTLE_CIRCLES) == TAG_NONE)
        sCircles.loadedType = CIRCLE_NONE; // wiped by the Bag / party screen

    if (!want)
    {
        DestroyCircleSprites();
        if (sCircles.loadedType != CIRCLE_NONE)
            FreeCircleGfx();
        return;
    }
    if ((sCircles.loadedType != sCircles.type || sCircles.loadedTod != sCircles.tod)
     && (gPaletteFade.active || gPaletteFade.y != 0))
        return; // load once the screen is visible, so nothing pops in unfaded
    if (sCircles.loadedType != sCircles.type || sCircles.loadedTod != sCircles.tod)
    {
        DestroyCircleSprites();
        if (!LoadCircleGfx(sCircles.type, sCircles.tod))
            return;
    }

    for (i = 0; i < NUM_CIRCLE_SPRITES; i++)
    {
        struct Sprite *sprite;
        s16 x, y;

        if (sCircles.spriteIds[i] == MAX_SPRITES)
        {
            u8 id = CreateSprite(&sSpriteTemplate_Circle, 0, 0, 0xFF);
            if (id == MAX_SPRITES)
                return;
            StartSpriteAnim(&gSprites[id], i);
            sCircles.spriteIds[i] = id;
        }
        sprite = &gSprites[sCircles.spriteIds[i]];
        x = sCirclePos[i][0] - GetTerrainOffsetX(i < 2 ? 64 : 104);
        y = sCirclePos[i][1] - gBattle_BG3_Y;
        sprite->x = x;
        sprite->y = y;
        sprite->invisible = (x < -32 || x > DISPLAY_WIDTH + 32);
    }
}

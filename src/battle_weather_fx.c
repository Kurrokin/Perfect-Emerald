// ============================================================================
// Looping in-battle weather effect
// ----------------------------------------------------------------------------
// While gBattleWeather holds rain, sandstorm, harsh sunlight or hail, particles
// keep falling / blowing for the whole battle: during messages, the action
// and move menus, and on top of move animations. The effect stops when the
// weather ends (particles finish their motion, then the graphics are freed).
//
// Design notes
// * Runs from BattleMainCB2 every frame. The Bag / party / summary screens use
//   other callbacks, so the effect is naturally absent there; when the battle
//   screen is rebuilt afterwards the effect notices its sprites and graphics
//   are gone and recreates them.
// * Uses its OWN tile/palette tags (copies of the battle-anim graphics), so a
//   move animation that loads and then frees ANIM_TAG_RAIN_DROPS etc. can never
//   pull the graphics out from under the loop.
// * Keeps the weather's screen tint (dark rain, bright sun...) the whole time.
// * Never uses the battle RNG (link / recorded battles stay in sync).
// * Leaves room for move animations: no new particle is spawned unless at
//   least WFX_MIN_FREE_SPRITES sprite slots are free.
// ============================================================================
#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_weather_fx.h"
#include "battle_controllers.h"
#include "battle_main.h"
#include "pokemon.h"
#include "pokemon_animation.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "main.h"
#include "palette.h"
#include "battle_terrain_circles.h"
#include "sprite.h"
#include "constants/battle.h"
#include "constants/battle_anim.h"
#include "constants/rgb.h"

#define WFX_MAX_PARTICLES    12
#define WFX_MIN_FREE_SPRITES 20
#define WFX_MAGIC            0x5758 // 'WX' marks our sprites
#define WFX_RETRY_DELAY      30

#define TAG_WFX_RAIN 0xB0A0
#define TAG_WFX_SAND 0xB0A1
#define TAG_WFX_SUN  0xB0A2
#define TAG_WFX_HAIL 0xB0A3

enum
{
    WFX_NONE,
    WFX_RAIN,
    WFX_SAND,
    WFX_SUN,
    WFX_HAIL,
    WFX_COUNT
};

#define sTimer  data[0]
#define sEndY   data[1]
#define sVelX   data[2]
#define sVelY   data[3]
#define sFracX  data[4]
#define sFracY  data[5]
#define sMagic  data[7]

static void SpriteCB_WfxRain(struct Sprite *sprite);
static void SpriteCB_WfxSand(struct Sprite *sprite);
static void SpriteCB_WfxSun(struct Sprite *sprite);
static void SpriteCB_WfxHail(struct Sprite *sprite);

// ---- Rain (same frames as the Rain Dance / "rain continues" drop) ----------
static const union AnimCmd sAnim_WfxRainDrop[] =
{
    ANIMCMD_FRAME(0, 2),
    ANIMCMD_FRAME(8, 2),
    ANIMCMD_FRAME(16, 2),
    ANIMCMD_FRAME(24, 6),
    ANIMCMD_FRAME(32, 2),
    ANIMCMD_FRAME(40, 2),
    ANIMCMD_FRAME(48, 2),
    ANIMCMD_END,
};
static const union AnimCmd *const sAnims_WfxRainDrop[] = {sAnim_WfxRainDrop};

static const struct OamData sOam_WfxRain =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(16x32),
    .size = SPRITE_SIZE(16x32),
    .priority = 2,
};

static const struct SpriteTemplate sSpriteTemplate_WfxRain =
{
    .tileTag = TAG_WFX_RAIN,
    .paletteTag = TAG_WFX_RAIN,
    .oam = &sOam_WfxRain,
    .anims = sAnims_WfxRainDrop,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_WfxRain,
};

// ---- Sandstorm (the crescent of flying sand from Sandstorm) ----------------
static const struct OamData sOam_WfxSand =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x16),
    .size = SPRITE_SIZE(32x16),
    .priority = 1,
};

static const struct Subsprite sWfxSandSubsprites[] =
{
    {.x = -16, .y = 0, .shape = SPRITE_SHAPE(32x16), .size = SPRITE_SIZE(32x16), .tileOffset = 0, .priority = 1},
    {.x =  16, .y = 0, .shape = SPRITE_SHAPE(32x16), .size = SPRITE_SIZE(32x16), .tileOffset = 8, .priority = 1},
};

static const struct SubspriteTable sWfxSandSubspriteTable[] =
{
    {ARRAY_COUNT(sWfxSandSubsprites), sWfxSandSubsprites},
};

static const struct SpriteTemplate sSpriteTemplate_WfxSand =
{
    .tileTag = TAG_WFX_SAND,
    .paletteTag = TAG_WFX_SAND,
    .oam = &sOam_WfxSand,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_WfxSand,
};

// ---- Harsh sunlight (the translucent ray from Sunny Day) -------------------
static const struct OamData sOam_WfxSun =
{
    .affineMode = ST_OAM_AFFINE_NORMAL,
    .objMode = ST_OAM_OBJ_BLEND,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .priority = 2,
};

static const union AffineAnimCmd sAffineAnim_WfxSun[] =
{
    AFFINEANIMCMD_FRAME(0x50, 0x50, 0, 0),
    AFFINEANIMCMD_FRAME(0x2, 0x2, 10, 1),
    AFFINEANIMCMD_JUMP(1),
};
static const union AffineAnimCmd *const sAffineAnims_WfxSun[] = {sAffineAnim_WfxSun};

static const struct SpriteTemplate sSpriteTemplate_WfxSun =
{
    .tileTag = TAG_WFX_SUN,
    .paletteTag = TAG_WFX_SUN,
    .oam = &sOam_WfxSun,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = sAffineAnims_WfxSun,
    .callback = SpriteCB_WfxSun,
};

// ---- Hail (the hailstone from Hail) -----------------------------------------
static const struct OamData sOam_WfxHail =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 2,
};

static const struct SpriteTemplate sSpriteTemplate_WfxHail =
{
    .tileTag = TAG_WFX_HAIL,
    .paletteTag = TAG_WFX_HAIL,
    .oam = &sOam_WfxHail,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_WfxHail,
};

struct WfxKind
{
    u16 animTag;    // source graphics in gBattleAnimPicTable / gBattleAnimPaletteTable
    u16 tag;        // our private tile + palette tag
    const struct SpriteTemplate *template;
    u8 spawnInterval;
    u8 maxParticles;
};

static const struct WfxKind sWfxKinds[WFX_COUNT] =
{
    [WFX_NONE] = {0},
    [WFX_RAIN] = {ANIM_TAG_RAIN_DROPS,  TAG_WFX_RAIN, &sSpriteTemplate_WfxRain, 2,  10},
    [WFX_SAND] = {ANIM_TAG_FLYING_DIRT, TAG_WFX_SAND, &sSpriteTemplate_WfxSand, 10, 4},
    [WFX_SUN]  = {ANIM_TAG_SUNLIGHT,    TAG_WFX_SUN,  &sSpriteTemplate_WfxSun,  34, 2},
    [WFX_HAIL] = {ANIM_TAG_HAIL,        TAG_WFX_HAIL, &sSpriteTemplate_WfxHail, 5,  8},
};

// ---- Persistent weather tint ------------------------------------------------
// The same tints the weather animations flash (Rain Dance darkens, Sunny Day
// brightens, Hail darkens the background), held for as long as the weather
// lasts. Only the battle background (BG palettes 2-4, so the text box stays
// readable) and the battler palettes are tinted. The tint is re-applied from
// the unfaded palettes on every idle frame, so it survives switching,
// Transform and the Bag/party screens; while a move animation or a screen
// fade runs it is left alone, so animations that blend palettes still work.
#define WFX_TINT_BG_PALETTES  ((1 << 2) | (1 << 3) | (1 << 4))
#define WFX_TINT_MAX_LEVEL    16

struct WfxTint
{
    u8 bgWeight;    // 0..255 blend toward color (1/256 steps)
    u8 monWeight;
    u16 color;
};

static const struct WfxTint sWfxTints[WFX_COUNT] =
{
    [WFX_NONE] = {0, 0, RGB_BLACK},
    [WFX_RAIN] = {64, 64, RGB_BLACK},        // 4/16 = General_Rain / Rain Dance
    [WFX_SAND] = {96, 48, RGB(26, 20, 10)},  // stands in for the sandstorm overlay
    [WFX_SUN]  = {82, 82, RGB_WHITE},        // Sunny Day (the move); drought uses DROUGHT_SUN_WEIGHT
    [WFX_HAIL] = {96, 0, RGB_BLACK},         // 6/16 = Hail (background only)
};

static EWRAM_DATA struct
{
    u8 kind;          // kind whose graphics are loaded (WFX_NONE = nothing)
    u8 tintKind;      // tint currently shown (WFX_NONE = none)
    u8 tintLevel;     // 0..WFX_TINT_MAX_LEVEL, ramps in/out
    u8 spawnTimer;
    u8 retryTimer;
    bool8 blendSet;   // we set BLDCNT/BLDALPHA for the translucent sun rays
    u8 spriteIds[WFX_MAX_PARTICLES];
    u32 rng;
} sWfx = {0};

#define BLDCNT_WFX_SUN  (BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3 | BLDCNT_TGT2_OBJ | BLDCNT_TGT2_BD)

static u16 WfxRandom(void)
{
    // Private LCG: must not touch the battle RNG.
    sWfx.rng = sWfx.rng * 1103515245 + 12345 + gMain.vblankCounter1;
    return sWfx.rng >> 16;
}

static bool8 IsOurSprite(u8 spriteId)
{
    struct Sprite *sprite;
    u32 k;

    if (spriteId >= MAX_SPRITES)
        return FALSE;
    sprite = &gSprites[spriteId];
    if (!sprite->inUse || sprite->sMagic != WFX_MAGIC)
        return FALSE;
    for (k = WFX_RAIN; k < WFX_COUNT; k++)
    {
        if (sprite->template == sWfxKinds[k].template)
            return TRUE;
    }
    return FALSE;
}

// Forget slots whose sprite finished or was wiped (Bag / party screens).
static u8 ValidateParticles(void)
{
    u8 i, alive = 0;

    for (i = 0; i < WFX_MAX_PARTICLES; i++)
    {
        if (sWfx.spriteIds[i] == MAX_SPRITES)
            continue;
        if (IsOurSprite(sWfx.spriteIds[i]))
            alive++;
        else
            sWfx.spriteIds[i] = MAX_SPRITES;
    }
    return alive;
}

static void DestroyWfxSprite(struct Sprite *sprite)
{
    if (sprite->oam.affineMode != ST_OAM_AFFINE_OFF)
        FreeOamMatrix(sprite->oam.matrixNum);
    DestroySprite(sprite);
}

static void DestroyAllParticles(void)
{
    u8 i;

    for (i = 0; i < WFX_MAX_PARTICLES; i++)
    {
        if (IsOurSprite(sWfx.spriteIds[i]))
            DestroyWfxSprite(&gSprites[sWfx.spriteIds[i]]);
        sWfx.spriteIds[i] = MAX_SPRITES;
    }
}

static bool8 IsWfxGfxLoaded(u8 kind)
{
    u16 tag = sWfxKinds[kind].tag;
    return GetSpriteTileStartByTag(tag) != TAG_NONE && IndexOfSpritePaletteTag(tag) != 0xFF;
}

static void FreeWfxGfx(u8 kind)
{
    if (kind == WFX_NONE)
        return;
    FreeSpriteTilesByTag(sWfxKinds[kind].tag);
    FreeSpritePaletteByTag(sWfxKinds[kind].tag);
}

static bool8 LoadWfxGfx(u8 kind)
{
    const struct WfxKind *info = &sWfxKinds[kind];
    struct CompressedSpriteSheet sheet = gBattleAnimPicTable[GET_TRUE_SPRITE_INDEX(info->animTag)];
    struct CompressedSpritePalette pal = gBattleAnimPaletteTable[GET_TRUE_SPRITE_INDEX(info->animTag)];

    sheet.tag = info->tag;
    pal.tag = info->tag;
    if (GetSpriteTileStartByTag(info->tag) == TAG_NONE)
        LoadCompressedSpriteSheetUsingHeap(&sheet);
    if (IndexOfSpritePaletteTag(info->tag) == 0xFF)
        LoadCompressedSpritePaletteUsingHeap(&pal);

    if (!IsWfxGfxLoaded(kind))
    {
        FreeWfxGfx(kind); // don't keep half of it
        return FALSE;
    }
    return TRUE;
}

static u8 GetWantedWfxKind(void)
{
    if (gBattleWeather & B_WEATHER_RAIN)
        return WFX_RAIN;
    if (gBattleWeather & B_WEATHER_SANDSTORM)
        return WFX_SAND;
    if (gBattleWeather & B_WEATHER_SUN)
        return WFX_SUN;
    if (gBattleWeather & B_WEATHER_HAIL)
        return WFX_HAIL;
    return WFX_NONE;
}

static u8 CountFreeSprites(void)
{
    u8 i, count = 0;

    for (i = 0; i < MAX_SPRITES; i++)
    {
        if (!gSprites[i].inUse)
            count++;
    }
    return count;
}

// Translucent sun rays need a 2nd blend target. Move animations manage the
// blend registers themselves, so only touch them while no animation runs and
// nobody else is using them.
static void UpdateSunBlend(bool8 wanted)
{
    if (wanted)
    {
        if (!gAnimScriptActive && GetGpuReg(REG_OFFSET_BLDCNT) == 0)
        {
            SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_WFX_SUN);
            SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(7, 10)); // 15% dimmer rays (v1.4)
            sWfx.blendSet = TRUE;
        }
    }
    else if (sWfx.blendSet)
    {
        if (!gAnimScriptActive && GetGpuReg(REG_OFFSET_BLDCNT) == BLDCNT_WFX_SUN)
        {
            SetGpuReg(REG_OFFSET_BLDCNT, 0);
            SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        }
        sWfx.blendSet = FALSE;
    }
}

static u32 GetBattlerTintPalettes(void)
{
    u32 mask = 0;
    u8 battler, spriteId;

    for (battler = 0; battler < gBattlersCount && battler < MAX_BATTLERS_COUNT; battler++)
    {
        spriteId = gBattlerSpriteIds[battler];
        if (spriteId < MAX_SPRITES && gSprites[spriteId].inUse)
            mask |= 1 << (16 + gSprites[spriteId].oam.paletteNum);
    }
    return mask;
}

static void BlendPalettesWeighted(u32 palettes, u32 weight, u16 color)
{
    u32 p, i;
    s32 tr = color & 0x1F, tg = (color >> 5) & 0x1F, tb = (color >> 10) & 0x1F;

    for (p = 0; palettes != 0; p++, palettes >>= 1)
    {
        if (!(palettes & 1))
            continue;
        for (i = p * 16; i < p * 16 + 16; i++)
        {
            u16 c = gPlttBufferUnfaded[i];
            s32 r = c & 0x1F, g = (c >> 5) & 0x1F, b = (c >> 10) & 0x1F;
            r += ((tr - r) * (s32)weight) >> 8;
            g += ((tg - g) * (s32)weight) >> 8;
            b += ((tb - b) * (s32)weight) >> 8;
            gPlttBufferFaded[i] = r | (g << 5) | (b << 10);
        }
    }
}

// Harsh sunlight from an overworld drought (permanent sun) is 7% brighter
// than Sunny Day's (82 * 1.07 = 88).
#define DROUGHT_SUN_WEIGHT 88

static void ApplyWeatherTint(void)
{
    const struct WfxTint *tint = &sWfxTints[sWfx.tintKind];
    u32 bgWeight = tint->bgWeight, monWeight = tint->monWeight;

    if (sWfx.tintKind == WFX_SUN && (gBattleWeather & B_WEATHER_SUN_PERMANENT))
        bgWeight = monWeight = DROUGHT_SUN_WEIGHT;
    BlendPalettesWeighted(WFX_TINT_BG_PALETTES | GetBattleTerrainCirclePaletteMask(),
                          bgWeight * sWfx.tintLevel / WFX_TINT_MAX_LEVEL, tint->color);
    BlendPalettesWeighted(GetBattlerTintPalettes(), monWeight * sWfx.tintLevel / WFX_TINT_MAX_LEVEL, tint->color);
}

static void UpdateWeatherTint(u8 wanted)
{
    if (wanted != WFX_NONE && wanted != sWfx.tintKind)
    {
        // New weather (or a different one): the move/weather animation that
        // caused it has just restored the palettes, so fade in from zero.
        sWfx.tintKind = wanted;
        sWfx.tintLevel = 0;
    }
    if (sWfx.tintKind == WFX_NONE)
        return;

    // Never fight a move animation or a screen fade over the palettes, and
    // never touch them while the screen is faded out (end of battle, Bag...):
    // re-applying the tint there made the background flash (v1.5 fix).
    if (gAnimScriptActive || gPaletteFade.active || gPaletteFade.y != 0)
        return;

    if (wanted != WFX_NONE)
    {
        if (sWfx.tintLevel < WFX_TINT_MAX_LEVEL)
            sWfx.tintLevel++;
    }
    else if (sWfx.tintLevel != 0)
    {
        sWfx.tintLevel--;  // weather ended: fade back out
    }

    ApplyWeatherTint();
    if (wanted == WFX_NONE && sWfx.tintLevel == 0)
        sWfx.tintKind = WFX_NONE; // palettes are back to normal
}

static void SpawnParticle(u8 kind)
{
    const struct WfxKind *info = &sWfxKinds[kind];
    u8 slot, spriteId;
    s16 x, y;
    struct Sprite *sprite;

    for (slot = 0; slot < info->maxParticles; slot++)
    {
        if (sWfx.spriteIds[slot] == MAX_SPRITES)
            break;
    }
    if (slot >= info->maxParticles || CountFreeSprites() < WFX_MIN_FREE_SPRITES)
        return;

    switch (kind)
    {
    default:
    case WFX_RAIN:
        x = WfxRandom() % DISPLAY_WIDTH;
        y = WfxRandom() % (DISPLAY_HEIGHT / 2);
        break;
    case WFX_SAND:
        x = -64;
        y = WfxRandom() % 104;
        break;
    case WFX_SUN:
        x = (WfxRandom() % 48) - 16;
        y = (WfxRandom() % 24) - 8;
        break;
    case WFX_HAIL:
        x = (WfxRandom() % (DISPLAY_WIDTH + 40)) - 60;
        y = -8;
        break;
    }

    spriteId = CreateSprite(info->template, x, y, 4);
    if (spriteId == MAX_SPRITES)
        return;
    sprite = &gSprites[spriteId];
    sprite->sMagic = WFX_MAGIC;
    sWfx.spriteIds[slot] = spriteId;

    switch (kind)
    {
    case WFX_SAND:
        SetSubspriteTables(sprite, sWfxSandSubspriteTable);
        sprite->sVelX = 1984 + WfxRandom() % 832;  // 7.75 .. 11 px/frame (x256)
        sprite->sVelY = 64 + WfxRandom() % 64;
        break;
    case WFX_SUN:
        sprite->sVelX = 597;                       // ~140 px over 60 frames (x256)
        sprite->sVelY = 341;                       // ~80 px over 60 frames (x256)
        break;
    case WFX_HAIL:
        sprite->sEndY = 40 + WfxRandom() % 72;
        break;
    }
}

static void SpriteCB_WfxRain(struct Sprite *sprite)
{
    if (++sprite->sTimer <= 13)
    {
        sprite->x2++;
        sprite->y2 += 4;
    }
    if (sprite->animEnded)
        DestroyWfxSprite(sprite);
}

static void MoveWfxSpriteFixed(struct Sprite *sprite)
{
    sprite->sFracX += sprite->sVelX;
    sprite->sFracY += sprite->sVelY;
    sprite->x2 += sprite->sFracX >> 8;
    sprite->y2 += sprite->sFracY >> 8;
    sprite->sFracX &= 0xFF;
    sprite->sFracY &= 0xFF;
}

static void SpriteCB_WfxSand(struct Sprite *sprite)
{
    MoveWfxSpriteFixed(sprite);
    if (sprite->x + sprite->x2 > DISPLAY_WIDTH + 32)
        DestroyWfxSprite(sprite);
}

static void SpriteCB_WfxSun(struct Sprite *sprite)
{
    MoveWfxSpriteFixed(sprite);
    if (++sprite->sTimer >= 60)
        DestroyWfxSprite(sprite);
}

static void SpriteCB_WfxHail(struct Sprite *sprite)
{
    sprite->x2 += 3;
    sprite->y2 += 6;
    if (sprite->y + sprite->y2 >= sprite->sEndY)
        DestroyWfxSprite(sprite);
}


// ============================================================================
// Enemy intro animation loop
// ----------------------------------------------------------------------------
// While the player is in the Fight/Bag/Pokemon/Run menu, the move menu or
// target selection, each opposing Pokemon replays its intro (front sprite)
// animation every few seconds, without its cry. As soon as the player commits
// to an action the loop stops: an animation in progress is cut off and the
// sprite is restored exactly as it was, so the turn's normal battle
// animations run on a clean sprite. (The Bag / party screens are separate
// screens, so nothing runs there.)
// ============================================================================
#define IDLE_ANIM_FIRST_DELAY   45
#define IDLE_ANIM_REPEAT_DELAY  150
#define IDLE_ANIM_STOP_FRAMES   3   // frames out of the menu before stopping

enum
{
    IDLE_ANIM_OFF,
    IDLE_ANIM_WAITING,
    IDLE_ANIM_PLAYING,
};

struct EnemyIdleAnim
{
    u8 state;
    u8 spriteId;
    u16 timer;
    struct Sprite saved;        // the sprite exactly as it was before the loop
    struct OamMatrix matrix;    // and its affine matrix
};

static EWRAM_DATA struct EnemyIdleAnim sEnemyIdleAnims[MAX_BATTLERS_COUNT] = {0};
static EWRAM_DATA u8 sFramesOutOfMenu = 0;

static bool8 IsSpriteIdleForAnim(struct Sprite *sprite)
{
    return sprite->callback == SpriteCallbackDummy || sprite->callback == SpriteCallbackDummy_2;
}

static bool8 CanEnemyIdleAnimate(u8 battler)
{
    u8 spriteId = gBattlerSpriteIds[battler];

    if (GetBattlerSide(battler) != B_SIDE_OPPONENT || spriteId >= MAX_SPRITES)
        return FALSE;
    if (!gSprites[spriteId].inUse || gSprites[spriteId].invisible)
        return FALSE;
    if (gBattleMons[battler].hp == 0 || gBattleSpritesDataPtr == NULL
     || gBattleSpritesDataPtr->battlerData[battler].behindSubstitute)
        return FALSE;
    return TRUE;
}

// FIX (v1.2, "enemy Pokemon sometimes floats"): front animations switch the
// sprite to double-size affine mode, which moves its centre-to-corner offset,
// and swap its affine animation table. When the player picked an action in
// the middle of one, only part of that was undone, so the sprite could be left
// drawn 32 pixels up/left. Now the animation task is ended and the complete
// sprite (position offsets, OAM, affine data, animation state) and its matrix
// are put back exactly as they were before the loop touched them.
static void RestoreEnemyIdleSprite(struct EnemyIdleAnim *anim, bool8 cutShort)
{
    struct Sprite *sprite = &gSprites[anim->spriteId];
    u16 palOffset;

    StopMonAnimationForSprite(sprite);
    *sprite = anim->saved;
    if (sprite->oam.affineMode != ST_OAM_AFFINE_OFF)
        gOamMatrices[sprite->oam.matrixNum] = anim->matrix;
    if (cutShort)
    {
        palOffset = OBJ_PLTT_ID(sprite->oam.paletteNum);
        CpuCopy16(&gPlttBufferUnfaded[palOffset], &gPlttBufferFaded[palOffset], PLTT_SIZE_4BPP);
    }
}

static void StopEnemyIdleAnims(void)
{
    u32 battler;

    for (battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
    {
        struct EnemyIdleAnim *anim = &sEnemyIdleAnims[battler];
        if (anim->state == IDLE_ANIM_PLAYING && anim->spriteId < MAX_SPRITES && gSprites[anim->spriteId].inUse)
            RestoreEnemyIdleSprite(anim, !IsSpriteIdleForAnim(&gSprites[anim->spriteId]));
        anim->state = IDLE_ANIM_OFF;
    }
}

static void UpdateEnemyIdleAnims(void)
{
    u32 battler, i;

    if (!IsPlayerInBattleMenu())
    {
        if (sFramesOutOfMenu < IDLE_ANIM_STOP_FRAMES && ++sFramesOutOfMenu < IDLE_ANIM_STOP_FRAMES)
            return;
        StopEnemyIdleAnims();
        return;
    }
    sFramesOutOfMenu = 0;

    for (battler = 0; battler < gBattlersCount && battler < MAX_BATTLERS_COUNT; battler++)
    {
        struct EnemyIdleAnim *anim = &sEnemyIdleAnims[battler];
        struct Sprite *sprite;

        if (!CanEnemyIdleAnimate(battler))
        {
            if (anim->state == IDLE_ANIM_PLAYING && anim->spriteId < MAX_SPRITES && gSprites[anim->spriteId].inUse)
                RestoreEnemyIdleSprite(anim, !IsSpriteIdleForAnim(&gSprites[anim->spriteId]));
            anim->state = IDLE_ANIM_OFF;
            continue;
        }
        sprite = &gSprites[gBattlerSpriteIds[battler]];

        switch (anim->state)
        {
        case IDLE_ANIM_OFF:
            anim->state = IDLE_ANIM_WAITING;
            anim->timer = IDLE_ANIM_FIRST_DELAY;
            break;
        case IDLE_ANIM_WAITING:
            if (anim->timer != 0)
            {
                anim->timer--;
                break;
            }
            if (gAnimScriptActive || gPaletteFade.active || !IsSpriteIdleForAnim(sprite))
                break;
            anim->spriteId = gBattlerSpriteIds[battler];
            anim->saved = *sprite;
            anim->matrix = gOamMatrices[sprite->oam.matrixNum];
            StartBattleIdleFrontAnim(sprite, gBattleSpritesDataPtr->battlerData[battler].transformSpecies != SPECIES_NONE
                                             ? gBattleSpritesDataPtr->battlerData[battler].transformSpecies
                                             : gBattleMons[battler].species);
            anim->state = IDLE_ANIM_PLAYING;
            anim->timer = 2;
            break;
        case IDLE_ANIM_PLAYING:
            if (anim->timer != 0)
            {
                anim->timer--; // let the animation task start
                break;
            }
            if (IsSpriteIdleForAnim(sprite))
            {
                RestoreEnemyIdleSprite(anim, FALSE);
                anim->state = IDLE_ANIM_WAITING;
                anim->timer = IDLE_ANIM_REPEAT_DELAY;
            }
            break;
        }
    }
}

void ResetBattleWeatherFx(void)
{
    u8 i;

    for (i = 0; i < WFX_MAX_PARTICLES; i++)
        sWfx.spriteIds[i] = MAX_SPRITES;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        sEnemyIdleAnims[i].state = IDLE_ANIM_OFF;
    sFramesOutOfMenu = IDLE_ANIM_STOP_FRAMES;
    sWfx.kind = WFX_NONE;
    sWfx.tintKind = WFX_NONE;
    sWfx.tintLevel = 0;
    sWfx.spawnTimer = 0;
    sWfx.retryTimer = 0;
    sWfx.blendSet = FALSE;
    sWfx.rng ^= gMain.vblankCounter1;
}

void UpdateBattleWeatherFx(void)
{
    u8 wanted, alive;

    // Battle is over (won / lost / ran / caught): the screen fades out and the
    // game resets its fade state, so touching palettes or (re)creating sprites
    // here made the background and circles flash for a frame (v1.5 fix).
    if (gBattleOutcome != 0)
        return;
    wanted = GetWantedWfxKind();
    alive = ValidateParticles();

    UpdateEnemyIdleAnims();
    UpdateWeatherTint(wanted);

    if (sWfx.kind != WFX_NONE && !IsWfxGfxLoaded(sWfx.kind))
    {
        // Graphics were wiped (returned from the Bag / party screen).
        DestroyAllParticles();
        FreeWfxGfx(sWfx.kind);
        sWfx.kind = WFX_NONE;
        sWfx.blendSet = FALSE;
        alive = 0;
    }

    if (wanted != sWfx.kind)
    {
        if (sWfx.kind != WFX_NONE)
        {
            if (wanted == WFX_NONE && alive != 0)
            {
                // Weather ended: let the last particles finish their motion.
                UpdateSunBlend(sWfx.kind == WFX_SUN);
                return;
            }
            DestroyAllParticles();
            FreeWfxGfx(sWfx.kind);
            sWfx.kind = WFX_NONE;
        }
        if (wanted != WFX_NONE)
        {
            if (sWfx.retryTimer != 0)
            {
                sWfx.retryTimer--;
            }
            else if (gPaletteFade.active || gPaletteFade.y != 0)
            {
                // wait until the screen is visible
            }
            else if (LoadWfxGfx(wanted))
            {
                sWfx.kind = wanted;
                sWfx.spawnTimer = 0;
            }
            else
            {
                sWfx.retryTimer = WFX_RETRY_DELAY; // VRAM / palettes full, try later
            }
        }
    }

    UpdateSunBlend(sWfx.kind == WFX_SUN);
    if (sWfx.kind == WFX_NONE || gPaletteFade.active || gPaletteFade.y != 0)
        return;

    if (++sWfx.spawnTimer >= sWfxKinds[sWfx.kind].spawnInterval)
    {
        sWfx.spawnTimer = 0;
        SpawnParticle(sWfx.kind);
    }
}

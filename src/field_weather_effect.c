#include "global.h"
#include "bg.h"
#include "window.h"
#include "start_menu.h"
#include "constants/field_effects.h"
#include "malloc.h"
#include "field_player_avatar.h"
#include "field_effect.h"
#include "battle_anim.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "field_weather.h"
#include "overworld.h"
#include "random.h"
#include "script.h"
#include "constants/weather.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "trig.h"
#include "gpu_regs.h"
#include "palette.h"
#include "constants/region_map_sections.h"
#include "rtc.h"
#include "event_data.h"
#include "metatile_behavior.h"
#include "field_camera.h"
#include "constants/metatile_behaviors.h"
#include "constants/map_types.h"

EWRAM_DATA static u8 sCurrentAbnormalWeather = 0;
EWRAM_DATA static u16 sUnusedWeatherRelated = 0;

const u16 gCloudsWeatherPalette[] = INCBIN_U16("graphics/weather/cloud.gbapal");
const u16 gSandstormWeatherPalette[] = INCBIN_U16("graphics/weather/sandstorm.gbapal");
const u8 gWeatherFogDiagonalTiles[] = INCBIN_U8("graphics/weather/fog_diagonal.4bpp");
const u8 gWeatherFogHorizontalTiles[] = INCBIN_U8("graphics/weather/fog_horizontal.4bpp");
const u8 gWeatherCloudTiles[] = INCBIN_U8("graphics/weather/cloud.4bpp");
const u8 gWeatherSnow1Tiles[] = INCBIN_U8("graphics/weather/snow0.4bpp");
const u8 gWeatherSnow2Tiles[] = INCBIN_U8("graphics/weather/snow1.4bpp");
const u8 gWeatherBubbleTiles[] = INCBIN_U8("graphics/weather/bubble.4bpp");
const u8 gWeatherAshTiles[] = INCBIN_U8("graphics/weather/ash.4bpp");
const u8 gWeatherRainTiles[] = INCBIN_U8("graphics/weather/rain.4bpp");
const u8 gWeatherSandstormTiles[] = INCBIN_U8("graphics/weather/sandstorm.4bpp");

//------------------------------------------------------------------------------
// WEATHER_SUNNY_CLOUDS
//------------------------------------------------------------------------------

static void CreateCloudSprites(void);
static void DestroyCloudSprites(void);
static void UpdateCloudShadowSprite(struct Sprite *);
static bool8 RampCloudShadows(u8 target);
static void UpdateCloudReflectionSprite(struct Sprite *);

// ----------------------------------------------------------------------------
// Clouds: moving cloud SHADOWS on the ground and cloud REFLECTIONS on water.
//
// The vanilla effect placed 3 cloud sprites at fixed Route 120 map coordinates
// at BG priority 3, so on other maps they showed through buildings ("cloud
// reflections on houses"). Now:
//  * Shadows: dark translucent cloud shapes drifting with the wind across the
//    whole view. They sit behind people/objects and under roofs/tree tops.
//  * Reflections: pale translucent clouds that are only ever shown while every
//    corner of the sprite is over water (pond / sea), so they can never appear
//    on a building, path or roof. They drift slowly and hop to another patch
//    of water when they reach the shore.
// ----------------------------------------------------------------------------
#define NUM_CLOUD_SHADOWS      3
#define NUM_CLOUD_REFLECTIONS  2
// v3.1: no cloud reflections on water any more - only the shadows (they also
// darken water). Reflections had to jump away from the player, which looked like
// clouds teleporting. Set to TRUE to bring them back.
#define CLOUD_REFLECTIONS_ON   FALSE
#define CLOUD_BLEND_EVA        6   // translucent: 6/16 cloud, 12/16 ground
#define CLOUD_BLEND_EVB        12
#define PALTAG_CLOUD_SHADOW    0x1210
#define PALTAG_CLOUD_REFLECT   0x1211
// v2.9: shadows are an OBJ WINDOW with the hardware darken effect, so they shade
// every layer the same way (ground, grass tufts, roofs, tree tops, people)
// instead of being cut by the tiles drawn on the top layer.
#define CLOUD_SHADOW_DARKNESS  4    // BLDY: 4/16 darker
#define CLOUD_SHADOW_RAMP      18   // frames per darkness step (v3.3: gentler fade in / out)

static EWRAM_DATA bool8 sCloudShadowWindowOn = FALSE;
static EWRAM_DATA u16 sCloudSavedWinOut = 0;
static EWRAM_DATA u16 sCloudSavedBldCnt = 0;
static EWRAM_DATA u16 sCloudSavedWin0H = 0;
static EWRAM_DATA u8 sCloudShadowLevel = 0;
static EWRAM_DATA u8 sCloudShadowTimer = 0;

static EWRAM_DATA struct Sprite *sCloudShadowSprites[NUM_CLOUD_SHADOWS] = {0};
static EWRAM_DATA struct Sprite *sCloudReflectionSprites[NUM_CLOUD_REFLECTIONS] = {0};
static EWRAM_DATA u32 sCloudRng = 0;

static const struct SpriteSheet sCloudSpriteSheet =
{
    .data = gWeatherCloudTiles,
    .size = sizeof(gWeatherCloudTiles),
    .tag = GFXTAG_CLOUD
};

static const struct OamData sCloudSpriteOamData =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_BLEND,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 2, // above ground/water, below roofs and tree tops
};

static const union AnimCmd sCloudSpriteAnimCmd[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sCloudSpriteAnimCmds[] =
{
    sCloudSpriteAnimCmd,
};

static const struct OamData sCloudReflectionOamData =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 2,
};

static const struct SpriteTemplate sCloudReflectionSpriteTemplate =
{
    .tileTag = GFXTAG_CLOUD,
    .paletteTag = PALTAG_CLOUD_REFLECT,
    .oam = &sCloudReflectionOamData,
    .anims = sCloudSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateCloudReflectionSprite,
};

static const struct OamData sCloudShadowOamData =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_WINDOW,   // defines where the darken effect applies
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 2,
};

static const struct SpriteTemplate sCloudShadowSpriteTemplate =
{
    .tileTag = GFXTAG_CLOUD,
    .paletteTag = PALTAG_CLOUD_SHADOW,
    .oam = &sCloudShadowOamData,
    .anims = sCloudSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateCloudShadowSprite,
};

void Clouds_InitVars(void)
{
    gWeatherPtr->noShadows = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->initStep = 0;
    if (gWeatherPtr->cloudSpritesCreated == FALSE)
        Weather_SetBlendCoeffs(0, 16);
}

void Clouds_InitAll(void)
{
    Clouds_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Clouds_Main();
}

void Clouds_Main(void)
{
    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateCloudSprites();
        gWeatherPtr->initStep++;
        break;
    case 1:
        Weather_SetTargetBlendCoeffs(CLOUD_BLEND_EVA, CLOUD_BLEND_EVB, 1);
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (Weather_UpdateBlend())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    case 3:
        RampCloudShadows(CLOUD_SHADOW_DARKNESS);
        break;
    }
    // v3.0: reflections take the same day/night shade as the water they sit on
    if (CLOUD_REFLECTIONS_ON && !gPaletteFade.active && IndexOfSpritePaletteTag(PALTAG_CLOUD_REFLECT) != 0xFF)
        TintSpritePaletteLikeMap(IndexOfSpritePaletteTag(PALTAG_CLOUD_REFLECT));
}

bool8 Clouds_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 16, 1);
        gWeatherPtr->finishStep++;
        return TRUE;
    case 1:
        if (Weather_UpdateBlend() && RampCloudShadows(0))
        {
            DestroyCloudSprites();
            gWeatherPtr->finishStep++;
        }
        return TRUE;
    }
    return FALSE;
}

void Sunny_InitVars(void)
{
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    Weather_SetBlendCoeffs(8, 12);
    gWeatherPtr->noShadows = FALSE;
}

void Sunny_InitAll(void)
{
    Sunny_InitVars();
}

void Sunny_Main(void)
{
}

bool8 Sunny_Finish(void)
{
    return FALSE;
}

static u16 CloudRandom(void)
{
    // Private LCG so the effect never disturbs the overworld RNG.
    sCloudRng = sCloudRng * 1103515245 + 12345;
    return sCloudRng >> 16;
}

static void PlaceCloudSpriteOnScreen(struct Sprite *sprite, s16 screenX, s16 screenY)
{
    sprite->x = screenX - gSpriteCoordOffsetX;
    sprite->y = screenY - gSpriteCoordOffsetY;
    sprite->x2 = 0;
    sprite->y2 = 0;
}

static void GetCloudSpriteScreenPos(struct Sprite *sprite, s16 *x, s16 *y)
{
    *x = sprite->x + sprite->x2 + gSpriteCoordOffsetX;
    *y = sprite->y + sprite->y2 + gSpriteCoordOffsetY;
}

// Is the map tile under this point (in sprite coordinates) water?
static bool8 IsCloudPointOnWater(s32 spriteX, s32 spriteY)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    s16 tileX, tileY;
    s32 mapX, mapY;

    SetSpritePosToMapCoords(player->currentCoords.x, player->currentCoords.y, &tileX, &tileY);
    mapX = player->currentCoords.x + ((spriteX - tileX) >> 4);
    mapY = player->currentCoords.y + ((spriteY - tileY) >> 4);
    return MetatileBehavior_IsSurfableWaterOrUnderwater(MapGridGetMetatileBehaviorAt(mapX, mapY));
}

static bool8 IsCloudFootprintOnWater(struct Sprite *sprite)
{
    s32 x = sprite->x + sprite->x2;
    s32 y = sprite->y + sprite->y2;

    return IsCloudPointOnWater(x, y)
        && IsCloudPointOnWater(x - 26, y - 20) && IsCloudPointOnWater(x + 26, y - 20)
        && IsCloudPointOnWater(x - 26, y + 20) && IsCloudPointOnWater(x + 26, y + 20);
}

// Reflections sit outside the shadow window where no blending happens, so they
// are drawn solid in soft water blues (lighter where the cloud is brighter).
static void LoadCloudReflectionPalette(void)
{
    u16 pal[16];
    u32 i;
    struct SpritePalette spritePal = {pal, PALTAG_CLOUD_REFLECT};

    pal[0] = gCloudsWeatherPalette[0];
    for (i = 1; i < 16; i++)
    {
        u16 c = gCloudsWeatherPalette[i];
        u32 lum = ((c & 0x1F) + ((c >> 5) & 0x1F) + ((c >> 10) & 0x1F)) / 3; // 0..31
        u32 k = lum;                                                        // blend weight /31
        u32 r = 11 + (17 - 11) * k / 31, g = 20 + (24 - 20) * k / 31, b = 27 + (30 - 27) * k / 31; // just lighter than the water
        pal[i] = RGB(r, g, b);
    }
    LoadSpritePalette(&spritePal);
}

static void SetCloudShadowDarkness(u8 level)
{
    sCloudShadowLevel = level;
    SetGpuReg(REG_OFFSET_BLDY, level);
}

static void EnableCloudShadowWindow(void)
{
    if (!sCloudShadowWindowOn)
    {
        sCloudSavedWinOut = GetGpuReg(REG_OFFSET_WINOUT);
        sCloudSavedBldCnt = GetGpuReg(REG_OFFSET_BLDCNT);
        sCloudSavedWin0H = GetGpuReg(REG_OFFSET_WIN0H);
        sCloudShadowWindowOn = TRUE;
    }
    // The overworld keeps window 0 stretched over the whole screen; it outranks the
    // OBJ window, so shrink it to nothing while the cloud shadows are up.
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    // outside the clouds: everything drawn, no effect; inside: everything drawn + darken
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ
                               | WINOUT_WINOBJ_BG_ALL | WINOUT_WINOBJ_OBJ | WINOUT_WINOBJ_CLR);
    // darken everything but the text box (BG0); keep the 2nd targets for see-through sprites
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD
                               | BLDCNT_EFFECT_DARKEN
                               | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3 | BLDCNT_TGT2_OBJ);
    SetCloudShadowDarkness(sCloudShadowLevel);
    SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
}

static void DisableCloudShadowWindow(void)
{
    if (!sCloudShadowWindowOn)
        return;
    ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
    SetGpuReg(REG_OFFSET_WINOUT, sCloudSavedWinOut);
    SetGpuReg(REG_OFFSET_BLDCNT, sCloudSavedBldCnt);
    SetGpuReg(REG_OFFSET_WIN0H, sCloudSavedWin0H);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    sCloudShadowWindowOn = FALSE;
    sCloudShadowLevel = 0;
}

// Fade the shadows in/out one step at a time (subtle weather changes).
static bool8 RampCloudShadows(u8 target)
{
    if (sCloudShadowLevel == target)
        return TRUE;
    if (++sCloudShadowTimer >= CLOUD_SHADOW_RAMP)
    {
        sCloudShadowTimer = 0;
        SetCloudShadowDarkness(sCloudShadowLevel + (sCloudShadowLevel < target ? 1 : -1));
    }
    return sCloudShadowLevel == target;
}

static void LoadCloudShadowPalette(void)
{
    u16 pal[16];
    u32 i;
    struct SpritePalette spritePal = {pal, PALTAG_CLOUD_SHADOW};

    pal[0] = gCloudsWeatherPalette[0];
    for (i = 1; i < 16; i++)
        pal[i] = RGB(3, 4, 7); // blue-grey shadow
    LoadSpritePalette(&spritePal);
}

static void CreateCloudSprites(void)
{
    u16 i;
    u8 spriteId;

    if (gWeatherPtr->cloudSpritesCreated == TRUE)
        return;

    sCloudRng ^= gMain.vblankCounter1;
    LoadSpriteSheet(&sCloudSpriteSheet);
    if (CLOUD_REFLECTIONS_ON)
        LoadCloudReflectionPalette();
    LoadCloudShadowPalette();
    sCloudShadowWindowOn = FALSE;   // registers were reset with the map
    sCloudShadowLevel = 0;
    sCloudShadowTimer = 0;
    EnableCloudShadowWindow();

    for (i = 0; i < NUM_CLOUD_SHADOWS; i++)
    {
        spriteId = CreateSprite(&sCloudShadowSpriteTemplate, 0, 0, 0xFF);
        sCloudShadowSprites[i] = (spriteId != MAX_SPRITES) ? &gSprites[spriteId] : NULL;
        if (sCloudShadowSprites[i] != NULL)
        {
            // v6.4: more cloud shapes - the same cloud mirrored side to side and/or top
            // to bottom (the same size, a different outline)
            sCloudShadowSprites[i]->oam.matrixNum = (CloudRandom() & 3) << 3;
            sCloudShadowSprites[i]->coordOffsetEnabled = TRUE;
            PlaceCloudSpriteOnScreen(sCloudShadowSprites[i], i * 96 + CloudRandom() % 48, CloudRandom() % DISPLAY_HEIGHT);
        }
    }
    for (i = 0; i < (CLOUD_REFLECTIONS_ON ? NUM_CLOUD_REFLECTIONS : 0); i++)
    {
        spriteId = CreateSprite(&sCloudReflectionSpriteTemplate, 0, 0, 0xFF);
        sCloudReflectionSprites[i] = (spriteId != MAX_SPRITES) ? &gSprites[spriteId] : NULL;
        if (sCloudReflectionSprites[i] != NULL)
        {
            sCloudReflectionSprites[i]->coordOffsetEnabled = TRUE;
            sCloudReflectionSprites[i]->invisible = TRUE; // shown once it finds water
            PlaceCloudSpriteOnScreen(sCloudReflectionSprites[i], CloudRandom() % DISPLAY_WIDTH, CloudRandom() % DISPLAY_HEIGHT);
        }
    }

    gWeatherPtr->cloudSpritesCreated = TRUE;
}

static void DestroyCloudSprites(void)
{
    u16 i;

    if (!gWeatherPtr->cloudSpritesCreated)
        return;

    for (i = 0; i < NUM_CLOUD_SHADOWS; i++)
    {
        if (sCloudShadowSprites[i] != NULL)
            DestroySprite(sCloudShadowSprites[i]);
        sCloudShadowSprites[i] = NULL;
    }
    for (i = 0; i < NUM_CLOUD_REFLECTIONS; i++)
    {
        if (sCloudReflectionSprites[i] != NULL)
            DestroySprite(sCloudReflectionSprites[i]);
        sCloudReflectionSprites[i] = NULL;
    }

    FreeSpriteTilesByTag(GFXTAG_CLOUD);
    FreeSpritePaletteByTag(PALTAG_CLOUD_SHADOW);
    FreeSpritePaletteByTag(PALTAG_CLOUD_REFLECT);
    DisableCloudShadowWindow();
    gWeatherPtr->cloudSpritesCreated = FALSE;
}

static void UpdateCloudShadowSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    s16 x, y;

    // Wind: 1 pixel left every 2 frames, anchored to the ground.
    if (++sprite->data[0] >= 2)
    {
        sprite->data[0] = 0;
        sprite->x--;
    }
    GetCloudSpriteScreenPos(sprite, &x, &y);
    if (x < -40 || x > DISPLAY_WIDTH + 96 || y < -48 || y > DISPLAY_HEIGHT + 48)
        PlaceCloudSpriteOnScreen(sprite, DISPLAY_WIDTH + 32 + CloudRandom() % 48, CloudRandom() % DISPLAY_HEIGHT);
}

static void UpdateCloudReflectionSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    s16 x, y;

    if (++sprite->data[0] >= 4)
    {
        sprite->data[0] = 0;
        sprite->x--;
    }
    GetCloudSpriteScreenPos(sprite, &x, &y);
    if (x < -32 || x > DISPLAY_WIDTH + 32 || y < -32 || y > DISPLAY_HEIGHT + 32 || !IsCloudFootprintOnWater(sprite))
    {
        // Left the water (or the view): look for another patch of water.
        sprite->invisible = TRUE;
        PlaceCloudSpriteOnScreen(sprite, CloudRandom() % DISPLAY_WIDTH, CloudRandom() % DISPLAY_HEIGHT);
        if (IsCloudFootprintOnWater(sprite))
            sprite->invisible = FALSE;
        return;
    }
    // v3.0: the reflection palette isn't touched by the weather colour map, so hide
    // the reflections while the scene is still dimmed by a weather change.
    // v3.0b: and never under the player (surfing looked like standing on a cloud):
    // the player is always around the middle of the screen.
    if (x > DISPLAY_WIDTH / 2 - 48 && x < DISPLAY_WIDTH / 2 + 48 && y > DISPLAY_HEIGHT / 2 - 48 && y < DISPLAY_HEIGHT / 2 + 40)
    {
        sprite->invisible = TRUE;
        PlaceCloudSpriteOnScreen(sprite, CloudRandom() % DISPLAY_WIDTH, CloudRandom() % DISPLAY_HEIGHT);
        return;
    }
    sprite->invisible = (gWeatherPtr->colorMapIndex != 0);
}

//------------------------------------------------------------------------------
// WEATHER_DROUGHT
//------------------------------------------------------------------------------

static void UpdateDroughtBlend(u8);

void Drought_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 0;
    gWeatherPtr->noShadows = FALSE;
}

void Drought_InitAll(void)
{
    Drought_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Drought_Main();
}

void Drought_Main(void)
{
    switch (gWeatherPtr->initStep)
    {
    case 0:
        if (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_CHANGING_WEATHER)
            gWeatherPtr->initStep++;
        break;
    case 1:
        ResetDroughtWeatherPaletteLoading();
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (LoadDroughtWeatherPalettes() == FALSE)
            gWeatherPtr->initStep++;
        break;
    case 3:
        DroughtStateInit();
        gWeatherPtr->initStep++;
        break;
    case 4:
        DroughtStateRun();
        if (gWeatherPtr->droughtBrightnessStage == 6)
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    default:
        DroughtStateRun();
        break;
    }
}

bool8 Drought_Finish(void)
{
    return FALSE;
}

void StartDroughtWeatherBlend(void)
{
    CreateTask(UpdateDroughtBlend, 80);
}

#define tState      data[0]
#define tBlendY     data[1]
#define tBlendDelay data[2]
#define tWinRange   data[3]

static void UpdateDroughtBlend(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->tState)
    {
    case 0:
        task->tBlendY = 0;
        task->tBlendDelay = 0;
        task->tWinRange = REG_WININ;
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_ALL | WININ_WIN1_ALL);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ | BLDCNT_EFFECT_LIGHTEN);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        task->tState++;
        // fall through
    case 1:
        task->tBlendY += 3;
        if (task->tBlendY > 16)
            task->tBlendY = 16;
        SetGpuReg(REG_OFFSET_BLDY, task->tBlendY);
        if (task->tBlendY >= 16)
            task->tState++;
        break;
    case 2:
        task->tBlendDelay++;
        if (task->tBlendDelay > 9)
        {
            task->tBlendDelay = 0;
            task->tBlendY--;
            if (task->tBlendY <= 0)
            {
                task->tBlendY = 0;
                task->tState++;
            }
            SetGpuReg(REG_OFFSET_BLDY, task->tBlendY);
        }
        break;
    case 3:
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetGpuReg(REG_OFFSET_WININ, task->tWinRange);
        task->tState++;
        break;
    case 4:
        ScriptContext_Enable();
        DestroyTask(taskId);
        break;
    }
}

#undef tState
#undef tBlendY
#undef tBlendDelay
#undef tWinRange

//------------------------------------------------------------------------------
// WEATHER_RAIN
//------------------------------------------------------------------------------

static void LoadRainSpriteSheet(void);
static bool8 CreateRainSprite(void);
static void UpdateRainSprite(struct Sprite *sprite);
static bool8 UpdateVisibleRainSprites(void);
static void DestroyRainSprites(void);

static const struct Coords16 sRainSpriteCoords[] =
{
    {  0,   0},
    {  0, 160},
    {  0,  64},
    {144, 224},
    {144, 128},
    { 32,  32},
    { 32, 192},
    { 32,  96},
    { 72, 128},
    { 72,  32},
    { 72, 192},
    {216,  96},
    {216,   0},
    {104, 160},
    {104,  64},
    {104, 224},
    {144,   0},
    {144, 160},
    {144,  64},
    { 32, 224},
    { 32, 128},
    { 72,  32},
    { 72, 192},
    { 48,  96},
};

static const struct OamData sRainSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 2,
    .affineParam = 0,
};

static const union AnimCmd sRainSpriteFallAnimCmd[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sRainSpriteSplashAnimCmd[] =
{
    ANIMCMD_FRAME(8, 3),
    ANIMCMD_FRAME(32, 2),
    ANIMCMD_FRAME(40, 2),
    ANIMCMD_END,
};

static const union AnimCmd sRainSpriteHeavySplashAnimCmd[] =
{
    ANIMCMD_FRAME(8, 3),
    ANIMCMD_FRAME(16, 3),
    ANIMCMD_FRAME(24, 4),
    ANIMCMD_END,
};

static const union AnimCmd *const sRainSpriteAnimCmds[] =
{
    sRainSpriteFallAnimCmd,
    sRainSpriteSplashAnimCmd,
    sRainSpriteHeavySplashAnimCmd,
};

static const struct SpriteTemplate sRainSpriteTemplate =
{
    .tileTag = GFXTAG_RAIN,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sRainSpriteOamData,
    .anims = sRainSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateRainSprite,
};

// Q28.4 fixed-point format values
static const s16 sRainSpriteMovement[][2] =
{
    {-0x68,  0xD0},
    {-0xA0, 0x140},
};

// First byte is the number of frames a raindrop falls before it splashes.
// Second byte is the maximum number of frames a raindrop can "wait" before
// it appears and starts falling. (This is only for the initial raindrop spawn.)
static const u16 sRainSpriteFallingDurations[][2] =
{
    {18, 7},
    {12, 10},
};

static const struct SpriteSheet sRainSpriteSheet =
{
    .data = gWeatherRainTiles,
    .size = sizeof(gWeatherRainTiles),
    .tag = GFXTAG_RAIN,
};

void Rain_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->rainSpriteVisibleCounter = 0;
    gWeatherPtr->rainSpriteVisibleDelay = 8;
    gWeatherPtr->isDownpour = FALSE;
    gWeatherPtr->targetRainSpriteCount = 10;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    SetRainStrengthFromSoundEffect(SE_RAIN);
    Weather_SetBlendCoeffs(8, 12); // preserve shadow darkness
    gWeatherPtr->noShadows = FALSE;
}

void Rain_InitAll(void)
{
    Rain_InitVars();
    while (!gWeatherPtr->weatherGfxLoaded)
        Rain_Main();
}

void Rain_Main(void)
{
    switch (gWeatherPtr->initStep)
    {
    case 0:
        LoadRainSpriteSheet();
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (!CreateRainSprite())
            gWeatherPtr->initStep++;
        break;
    case 2:
        if (!UpdateVisibleRainSprites())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 Rain_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        if (gWeatherPtr->nextWeather == WEATHER_RAIN
         || gWeatherPtr->nextWeather == WEATHER_RAIN_THUNDERSTORM
         || gWeatherPtr->nextWeather == WEATHER_DOWNPOUR)
        {
            gWeatherPtr->finishStep = 0xFF;
            return FALSE;
        }
        else
        {
            gWeatherPtr->targetRainSpriteCount = 0;
            gWeatherPtr->finishStep++;
        }
        // fall through
    case 1:
        if (!UpdateVisibleRainSprites())
        {
            DestroyRainSprites();
            gWeatherPtr->finishStep++;
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

#define tCounter data[0]
#define tRandom  data[1]
#define tPosX    data[2]
#define tPosY    data[3]
#define tState   data[4]
#define tActive  data[5]
#define tWaiting data[6]

static void StartRainSpriteFall(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    u32 rand;
    u16 numFallingFrames;
    int tileX;
    int tileY;

    if (sprite->tRandom == 0)
        sprite->tRandom = 361;

    rand = ISO_RANDOMIZE2(sprite->tRandom);
    sprite->tRandom = ((rand & 0x7FFF0000) >> 16) % 600;

    numFallingFrames = sRainSpriteFallingDurations[gWeatherPtr->isDownpour][0];

    tileX = sprite->tRandom % 30;
    sprite->tPosX = tileX * 8; // Useless assignment, leftover from before fixed-point values were used

    tileY = sprite->tRandom / 30;
    sprite->tPosY = tileY * 8; // Useless assignment, leftover from before fixed-point values were used

    sprite->tPosX = tileX;
    sprite->tPosX <<= 7; // This is tileX * 8, using a fixed-point value with 4 decimal places

    sprite->tPosY = tileY;
    sprite->tPosY <<= 7; // This is tileX * 8, using a fixed-point value with 4 decimal places

    // "Rewind" the rain sprites, from their ending position.
    sprite->tPosX -= sRainSpriteMovement[gWeatherPtr->isDownpour][0] * numFallingFrames;
    sprite->tPosY -= sRainSpriteMovement[gWeatherPtr->isDownpour][1] * numFallingFrames;

    StartSpriteAnim(sprite, 0);
    sprite->tState = 0;
    sprite->coordOffsetEnabled = FALSE;
    sprite->tCounter = numFallingFrames;
}

static void UpdateRainSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    if (sprite->tState == 0)
    {
        // Raindrop is in its "falling" motion.
        sprite->tPosX += sRainSpriteMovement[gWeatherPtr->isDownpour][0];
        sprite->tPosY += sRainSpriteMovement[gWeatherPtr->isDownpour][1];
        sprite->x = sprite->tPosX >> 4;
        sprite->y = sprite->tPosY >> 4;

        if (sprite->tActive
         && (sprite->x >= -8 && sprite->x <= DISPLAY_WIDTH + 8)
         && sprite->y >= -16 && sprite->y <= DISPLAY_HEIGHT + 16)
            sprite->invisible = FALSE;
        else
            sprite->invisible = TRUE;

        if (--sprite->tCounter == 0)
        {
            // Make raindrop splash on the ground
            StartSpriteAnim(sprite, gWeatherPtr->isDownpour + 1);
            sprite->tState = 1;
            sprite->x -= gSpriteCoordOffsetX;
            sprite->y -= gSpriteCoordOffsetY;
            sprite->coordOffsetEnabled = TRUE;
        }
    }
    else if (sprite->animEnded)
    {
        // The splashing animation ended.
        sprite->invisible = TRUE;
        StartRainSpriteFall(sprite);
    }
}

static void WaitRainSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    if (sprite->tCounter == 0)
    {
        StartRainSpriteFall(sprite);
        sprite->callback = UpdateRainSprite;
    }
    else
    {
        sprite->tCounter--;
    }
}

static void InitRainSpriteMovement(struct Sprite *sprite, u16 val)
{
    u16 numFallingFrames = sRainSpriteFallingDurations[gWeatherPtr->isDownpour][0];
    u16 numAdvanceRng = val / (sRainSpriteFallingDurations[gWeatherPtr->isDownpour][1] + numFallingFrames);
    u16 frameVal = val % (sRainSpriteFallingDurations[gWeatherPtr->isDownpour][1] + numFallingFrames);

    while (--numAdvanceRng != 0xFFFF)
        StartRainSpriteFall(sprite);

    if (frameVal < numFallingFrames)
    {
        while (--frameVal != 0xFFFF)
            UpdateRainSprite(sprite);

        sprite->tWaiting = 0;
    }
    else
    {
        sprite->tCounter = frameVal - numFallingFrames;
        sprite->invisible = TRUE;
        sprite->tWaiting = 1;
    }
}

static void LoadRainSpriteSheet(void)
{
    LoadSpriteSheet(&sRainSpriteSheet);
}

// v6.0: rain and thunderstorms show at most 16 drops at a time, so they now make
// 22 drop sprites instead of 24 (the sprite budget); a downpour still makes all 24.
#define RAIN_SPRITES_NOT_DOWNPOUR 22
static u8 GetRainSpriteCap(void)
{
    return (gWeatherPtr->currWeather == WEATHER_DOWNPOUR || gWeatherPtr->nextWeather == WEATHER_DOWNPOUR)
         ? MAX_RAIN_SPRITES : RAIN_SPRITES_NOT_DOWNPOUR;
}

static bool8 CreateRainSprite(void)
{
    u8 spriteIndex;
    u8 spriteId;

    if (gWeatherPtr->rainSpriteCount >= GetRainSpriteCap())
        return FALSE;

    spriteIndex = gWeatherPtr->rainSpriteCount;
    spriteId = CreateSpriteAtEnd(&sRainSpriteTemplate,
      sRainSpriteCoords[spriteIndex].x, sRainSpriteCoords[spriteIndex].y, 78);

    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].tActive = FALSE;
        gSprites[spriteId].tRandom = spriteIndex * 145;
        while (gSprites[spriteId].tRandom >= 600)
            gSprites[spriteId].tRandom -= 600;

        StartRainSpriteFall(&gSprites[spriteId]);
        InitRainSpriteMovement(&gSprites[spriteId], spriteIndex * 9);
        gSprites[spriteId].invisible = TRUE;
        gWeatherPtr->sprites.s1.rainSprites[spriteIndex] = &gSprites[spriteId];
    }
    else
    {
        gWeatherPtr->sprites.s1.rainSprites[spriteIndex] = NULL;
    }

    if (++gWeatherPtr->rainSpriteCount >= GetRainSpriteCap())
    {
        u16 i;
        for (i = 0; i < gWeatherPtr->rainSpriteCount; i++)
        {
            if (gWeatherPtr->sprites.s1.rainSprites[i])
            {
                if (!gWeatherPtr->sprites.s1.rainSprites[i]->tWaiting)
                    gWeatherPtr->sprites.s1.rainSprites[i]->callback = UpdateRainSprite;
                else
                    gWeatherPtr->sprites.s1.rainSprites[i]->callback = WaitRainSprite;
            }
        }

        return FALSE;
    }

    return TRUE;
}

static bool8 UpdateVisibleRainSprites(void)
{
    // v6.0: never more visible drops than were made (and skip any drop whose sprite
    // could not be made - that used to read a missing sprite)
    if (gWeatherPtr->targetRainSpriteCount > gWeatherPtr->rainSpriteCount)
        gWeatherPtr->targetRainSpriteCount = gWeatherPtr->rainSpriteCount;
    if (gWeatherPtr->curRainSpriteIndex == gWeatherPtr->targetRainSpriteCount)
        return FALSE;

    if (++gWeatherPtr->rainSpriteVisibleCounter > gWeatherPtr->rainSpriteVisibleDelay)
    {
        gWeatherPtr->rainSpriteVisibleCounter = 0;
        if (gWeatherPtr->curRainSpriteIndex < gWeatherPtr->targetRainSpriteCount)
        {
            struct Sprite *drop = gWeatherPtr->sprites.s1.rainSprites[gWeatherPtr->curRainSpriteIndex++];
            if (drop != NULL)
                drop->tActive = TRUE;
        }
        else
        {
            struct Sprite *drop = gWeatherPtr->sprites.s1.rainSprites[--gWeatherPtr->curRainSpriteIndex];
            if (drop != NULL)
            {
                drop->tActive = FALSE;
                drop->invisible = TRUE;
            }
        }
    }
    return TRUE;
}

static void DestroyRainSprites(void)
{
    u16 i;

    for (i = 0; i < gWeatherPtr->rainSpriteCount; i++)
    {
        if (gWeatherPtr->sprites.s1.rainSprites[i] != NULL)
            DestroySprite(gWeatherPtr->sprites.s1.rainSprites[i]);
    }
    gWeatherPtr->rainSpriteCount = 0;
    FreeSpriteTilesByTag(GFXTAG_RAIN);
}

#undef tCounter
#undef tRandom
#undef tPosX
#undef tPosY
#undef tState
#undef tActive
#undef tWaiting

//------------------------------------------------------------------------------
// Snow
//------------------------------------------------------------------------------

static void UpdateSnowflakeSprite(struct Sprite *);
static bool8 UpdateVisibleSnowflakeSprites(void);
static bool8 CreateSnowflakeSprite(void);
static bool8 DestroySnowflakeSprite(void);
static void InitSnowflakeSpriteMovement(struct Sprite *);

void Snow_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->targetSnowflakeSpriteCount = 32;
    gWeatherPtr->snowflakeVisibleCounter = 0;
    Weather_SetBlendCoeffs(8, 12); // preserve shadow darkness
    gWeatherPtr->noShadows = FALSE;
}

void Snow_InitAll(void)
{
    u16 i;

    Snow_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
    {
        Snow_Main();
        for (i = 0; i < gWeatherPtr->snowflakeSpriteCount; i++)
            UpdateSnowflakeSprite(gWeatherPtr->sprites.s1.snowflakeSprites[i]);
    }
}

void Snow_Main(void)
{
    if (gWeatherPtr->initStep == 0 && !UpdateVisibleSnowflakeSprites())
    {
        gWeatherPtr->weatherGfxLoaded = TRUE;
        gWeatherPtr->initStep++;
    }
}

bool8 Snow_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        gWeatherPtr->targetSnowflakeSpriteCount = 0;
        gWeatherPtr->snowflakeVisibleCounter = 0;
        gWeatherPtr->finishStep++;
        // fall through
    case 1:
        if (!UpdateVisibleSnowflakeSprites())
        {
            gWeatherPtr->finishStep++;
            return FALSE;
        }
        return TRUE;
    }

    return FALSE;
}

static bool8 UpdateVisibleSnowflakeSprites(void)
{
    if (gWeatherPtr->snowflakeSpriteCount == gWeatherPtr->targetSnowflakeSpriteCount)
        return FALSE;

    if (++gWeatherPtr->snowflakeVisibleCounter > 36)
    {
        gWeatherPtr->snowflakeVisibleCounter = 0;
        if (gWeatherPtr->snowflakeSpriteCount < gWeatherPtr->targetSnowflakeSpriteCount)
            CreateSnowflakeSprite();
        else
            DestroySnowflakeSprite();
    }

    return gWeatherPtr->snowflakeSpriteCount != gWeatherPtr->targetSnowflakeSpriteCount;
}

static const struct OamData sSnowflakeSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(8x8),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteFrameImage sSnowflakeSpriteImages[] =
{
    {gWeatherSnow1Tiles, sizeof(gWeatherSnow1Tiles)},
    {gWeatherSnow2Tiles, sizeof(gWeatherSnow2Tiles)},
};

static const union AnimCmd sSnowflakeAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_END,
};

static const union AnimCmd sSnowflakeAnimCmd1[] =
{
    ANIMCMD_FRAME(1, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sSnowflakeAnimCmds[] =
{
    sSnowflakeAnimCmd0,
    sSnowflakeAnimCmd1,
};

static const struct SpriteTemplate sSnowflakeSpriteTemplate =
{
    .tileTag = TAG_NONE,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sSnowflakeSpriteOamData,
    .anims = sSnowflakeAnimCmds,
    .images = sSnowflakeSpriteImages,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateSnowflakeSprite,
};

#define tPosY         data[0]
#define tDeltaY       data[1]
#define tWaveDelta    data[2]
#define tWaveIndex    data[3]
#define tSnowflakeId  data[4]
#define tFallCounter  data[5]
#define tFallDuration data[6]
#define tDeltaY2      data[7]

static bool8 CreateSnowflakeSprite(void)
{
    u8 spriteId = CreateSpriteAtEnd(&sSnowflakeSpriteTemplate, 0, 0, 78);
    if (spriteId == MAX_SPRITES)
        return FALSE;

    gSprites[spriteId].tSnowflakeId = gWeatherPtr->snowflakeSpriteCount;
    InitSnowflakeSpriteMovement(&gSprites[spriteId]);
    gSprites[spriteId].coordOffsetEnabled = TRUE;
    gWeatherPtr->sprites.s1.snowflakeSprites[gWeatherPtr->snowflakeSpriteCount++] = &gSprites[spriteId];
    return TRUE;
}

static bool8 DestroySnowflakeSprite(void)
{
    if (gWeatherPtr->snowflakeSpriteCount)
    {
        DestroySprite(gWeatherPtr->sprites.s1.snowflakeSprites[--gWeatherPtr->snowflakeSpriteCount]);
        return TRUE;
    }

    return FALSE;
}

static void InitSnowflakeSpriteMovement(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    u16 rand;
    u16 x = ((sprite->tSnowflakeId * 5) & 7) * 30 + (Random() % 30);

    sprite->y = -3 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
    sprite->x = x - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);
    sprite->tPosY = sprite->y * 128;
    sprite->x2 = 0;
    rand = Random();
    sprite->tDeltaY = (rand & 3) * 5 + 64;
    sprite->tDeltaY2 = sprite->tDeltaY;
    StartSpriteAnim(sprite, (rand & 1) ? 0 : 1);
    sprite->tWaveIndex = 0;
    sprite->tWaveDelta = ((rand & 3) == 0) ? 2 : 1;
    sprite->tFallDuration = (rand & 0x1F) + 210;
    sprite->tFallCounter = 0;
}

static void WaitSnowflakeSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    // Timer is never incremented
    if (++gWeatherPtr->snowflakeTimer > 18)
    {
        sprite->invisible = FALSE;
        sprite->callback = UpdateSnowflakeSprite;
        sprite->y = 250 - (gSpriteCoordOffsetY + sprite->centerToCornerVecY);
        sprite->tPosY = sprite->y * 128;
        gWeatherPtr->snowflakeTimer = 0;
    }
}

static void UpdateSnowflakeSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    s16 x;
    s16 y;

    sprite->tPosY += sprite->tDeltaY;
    sprite->y = sprite->tPosY >> 7;
    sprite->tWaveIndex += sprite->tWaveDelta;
    sprite->tWaveIndex &= 0xFF;
    sprite->x2 = gSineTable[sprite->tWaveIndex] / 64;

    x = (sprite->x + sprite->centerToCornerVecX + gSpriteCoordOffsetX) & 0x1FF;
    if (x & 0x100)
        x |= -0x100;

    if (x < -3)
        sprite->x = 242 - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);
    else if (x > 242)
        sprite->x = -3 - (gSpriteCoordOffsetX + sprite->centerToCornerVecX);

}

#undef tPosY
#undef tDeltaY
#undef tWaveDelta
#undef tWaveIndex
#undef tSnowflakeId
#undef tFallCounter
#undef tFallDuration
#undef tDeltaY2

//------------------------------------------------------------------------------
// WEATHER_RAIN_THUNDERSTORM
//------------------------------------------------------------------------------

enum {
    // This block of states is run only once
    // when first setting up the thunderstorm
    THUNDER_STATE_LOAD_RAIN,
    THUNDER_STATE_CREATE_RAIN,
    THUNDER_STATE_INIT_RAIN,
    THUNDER_STATE_WAIT_CHANGE,

    // The thunderstorm loops through these states,
    // not necessarily in order.
    THUNDER_STATE_NEW_CYCLE,
    THUNDER_STATE_NEW_CYCLE_WAIT,
    THUNDER_STATE_INIT_CYCLE_1,
    THUNDER_STATE_INIT_CYCLE_2,
    THUNDER_STATE_SHORT_BOLT,
    THUNDER_STATE_TRY_NEW_BOLT,
    THUNDER_STATE_WAIT_BOLT_SHORT,
    THUNDER_STATE_INIT_BOLT_LONG,
    THUNDER_STATE_WAIT_BOLT_LONG,
    THUNDER_STATE_FADE_BOLT_LONG,
    THUNDER_STATE_END_BOLT_LONG,
};

void Thunderstorm_InitVars(void)
{
    gWeatherPtr->initStep = THUNDER_STATE_LOAD_RAIN;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->rainSpriteVisibleCounter = 0;
    gWeatherPtr->rainSpriteVisibleDelay = 4;
    gWeatherPtr->isDownpour = FALSE;
    gWeatherPtr->targetRainSpriteCount = 16;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->weatherGfxLoaded = FALSE;  // duplicate assignment
    gWeatherPtr->thunderEnqueued = FALSE;
    SetRainStrengthFromSoundEffect(SE_THUNDERSTORM);
    Weather_SetBlendCoeffs(8, 12); // preserve shadow darkness
    gWeatherPtr->noShadows = FALSE;
}

void Thunderstorm_InitAll(void)
{
    Thunderstorm_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Thunderstorm_Main();
}

//------------------------------------------------------------------------------
// WEATHER_DOWNPOUR
//------------------------------------------------------------------------------

static void UpdateThunderSound(void);
static void EnqueueThunder(u16);

void Downpour_InitVars(void)
{
    gWeatherPtr->initStep = THUNDER_STATE_LOAD_RAIN;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->rainSpriteVisibleCounter = 0;
    gWeatherPtr->rainSpriteVisibleDelay = 4;
    gWeatherPtr->isDownpour = TRUE;
    gWeatherPtr->targetRainSpriteCount = 24;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->weatherGfxLoaded = FALSE;  // duplicate assignment
    SetRainStrengthFromSoundEffect(SE_DOWNPOUR);
    Weather_SetBlendCoeffs(8, 12); // preserve shadow darkness
    gWeatherPtr->noShadows = FALSE;
}

void Downpour_InitAll(void)
{
    Downpour_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Thunderstorm_Main();
}

// In a given cycle, there will be some shorter bolts of lightning, potentially
// followed by a longer bolt. As a "regex", the pattern is:
//   (SHORT_BOLT){1,2}(LONG_BOLT)?
//
// Thunder only plays on the final bolt of the cycle.
void Thunderstorm_Main(void)
{
    UpdateThunderSound();
    switch (gWeatherPtr->initStep)
    {
    case THUNDER_STATE_LOAD_RAIN:
        LoadRainSpriteSheet();
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_CREATE_RAIN:
        if (!CreateRainSprite())
            gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_INIT_RAIN:
        if (!UpdateVisibleRainSprites())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    case THUNDER_STATE_WAIT_CHANGE:
        if (gWeatherPtr->palProcessingState != WEATHER_PAL_STATE_CHANGING_WEATHER)
            gWeatherPtr->initStep = THUNDER_STATE_INIT_CYCLE_1;
        break;
    case THUNDER_STATE_NEW_CYCLE:
        gWeatherPtr->thunderAllowEnd = TRUE;
        gWeatherPtr->thunderTimer = (Random() % 360) + 360;
        gWeatherPtr->initStep++;
        // fall through
    case THUNDER_STATE_NEW_CYCLE_WAIT:
        // Wait between 360-720 frames before starting a new cycle.
        // v5.3: from dawn (05:00) to the end of dusk (17:59) there is no lightning
        // unless the rain is fully falling - never a thunderstorm without rain.
        if (gLocalTime.hours >= 5 && gLocalTime.hours < 18
         && gWeatherPtr->curRainSpriteIndex < gWeatherPtr->targetRainSpriteCount)
        {
            if (gWeatherPtr->thunderTimer < 2)
                gWeatherPtr->thunderTimer = 2;
        }
        if (--gWeatherPtr->thunderTimer == 0)
            gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_INIT_CYCLE_1:
        gWeatherPtr->thunderAllowEnd = TRUE;
        gWeatherPtr->thunderLongBolt = Random() % 2;
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_INIT_CYCLE_2:
        gWeatherPtr->thunderShortBolts = (Random() & 1) + 1;
        gWeatherPtr->initStep++;
        // fall through
    case THUNDER_STATE_SHORT_BOLT:
        // Short bolt of lightning strikes.
        ApplyWeatherColorMapIfIdle(19);
        // If final lightning bolt, enqueue thunder.
        if (!gWeatherPtr->thunderLongBolt && gWeatherPtr->thunderShortBolts == 1)
            EnqueueThunder(20);

        gWeatherPtr->thunderTimer = (Random() % 3) + 6;
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_TRY_NEW_BOLT:
        if (--gWeatherPtr->thunderTimer == 0)
        {
            // Short bolt of lightning ends.
            ApplyWeatherColorMapIfIdle(3);
            gWeatherPtr->thunderAllowEnd = TRUE;
            if (--gWeatherPtr->thunderShortBolts != 0)
            {
                // Wait a little, then do another short bolt.
                gWeatherPtr->thunderTimer = (Random() % 16) + 60;
                gWeatherPtr->initStep = THUNDER_STATE_WAIT_BOLT_SHORT;
            }
            else if (!gWeatherPtr->thunderLongBolt)
            {
                // No more bolts, restart loop.
                gWeatherPtr->initStep = THUNDER_STATE_NEW_CYCLE;
            }
            else
            {
                // Set up long bolt.
                gWeatherPtr->initStep = THUNDER_STATE_INIT_BOLT_LONG;
            }
        }
        break;
    case THUNDER_STATE_WAIT_BOLT_SHORT:
        if (--gWeatherPtr->thunderTimer == 0)
            gWeatherPtr->initStep = THUNDER_STATE_SHORT_BOLT;
        break;
    case THUNDER_STATE_INIT_BOLT_LONG:
        gWeatherPtr->thunderTimer = (Random() % 16) + 60;
        gWeatherPtr->initStep++;
        break;
    case THUNDER_STATE_WAIT_BOLT_LONG:
        if (--gWeatherPtr->thunderTimer == 0)
        {
            // Do long bolt. Enqueue thunder with a potentially longer delay.
            EnqueueThunder(100);
            ApplyWeatherColorMapIfIdle(19);
            gWeatherPtr->thunderTimer = (Random() & 0xF) + 30;
            gWeatherPtr->initStep++;
        }
        break;
    case THUNDER_STATE_FADE_BOLT_LONG:
        if (--gWeatherPtr->thunderTimer == 0)
        {
            // Fade long bolt out over time.
            ApplyWeatherColorMapIfIdle_Gradual(19, 3, 5);
            gWeatherPtr->initStep++;
        }
        break;
    case THUNDER_STATE_END_BOLT_LONG:
        if (gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_IDLE)
        {
            gWeatherPtr->thunderAllowEnd = TRUE;
            gWeatherPtr->initStep = THUNDER_STATE_NEW_CYCLE;
        }
        break;
    }
}

bool8 Thunderstorm_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        gWeatherPtr->thunderAllowEnd = FALSE;
        gWeatherPtr->finishStep++;
        // fall through
    case 1:
        Thunderstorm_Main();
        if (gWeatherPtr->thunderAllowEnd)
        {
            if (gWeatherPtr->nextWeather == WEATHER_RAIN
             || gWeatherPtr->nextWeather == WEATHER_RAIN_THUNDERSTORM
             || gWeatherPtr->nextWeather == WEATHER_DOWNPOUR)
                return FALSE;

            gWeatherPtr->targetRainSpriteCount = 0;
            gWeatherPtr->finishStep++;
        }
        break;
    case 2:
        if (!UpdateVisibleRainSprites())
        {
            DestroyRainSprites();
            gWeatherPtr->thunderEnqueued = FALSE;
            gWeatherPtr->finishStep++;
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

// Enqueue a thunder sound effect for at most `waitFrames` frames from now.
static void EnqueueThunder(u16 waitFrames)
{
    if (!gWeatherPtr->thunderEnqueued)
    {
        gWeatherPtr->thunderSETimer = Random() % waitFrames;
        gWeatherPtr->thunderEnqueued = TRUE;
    }
}

static void UpdateThunderSound(void)
{
    if (gWeatherPtr->thunderEnqueued == TRUE)
    {
        if (gWeatherPtr->thunderSETimer == 0)
        {
            if (IsSEPlaying())
                return;

            if (Random() & 1)
                PlaySE(SE_THUNDER);
            else
                PlaySE(SE_THUNDER2);

            gWeatherPtr->thunderEnqueued = FALSE;
        }
        else
        {
            gWeatherPtr->thunderSETimer--;
        }
    }
}

//------------------------------------------------------------------------------
// WEATHER_FOG_HORIZONTAL and WEATHER_UNDERWATER
//------------------------------------------------------------------------------

static const u16 sUnusedData[] = {0, 6, 6, 12, 18, 42, 300, 300};

static const struct OamData sOamData_FogH =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_BLEND,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 2,
    .paletteNum = 0,
    .affineParam = 0,
};

static const union AnimCmd sAnim_FogH_0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_1[] =
{
    ANIMCMD_FRAME(32, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_2[] =
{
    ANIMCMD_FRAME(64, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_3[] =
{
    ANIMCMD_FRAME(96, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_4[] =
{
    ANIMCMD_FRAME(128, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_FogH_5[] =
{
    ANIMCMD_FRAME(160, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnims_FogH[] =
{
    sAnim_FogH_0,
    sAnim_FogH_1,
    sAnim_FogH_2,
    sAnim_FogH_3,
    sAnim_FogH_4,
    sAnim_FogH_5,
};

static const union AffineAnimCmd sAffineAnim_FogH[] =
{
    AFFINEANIMCMD_FRAME(0x200, 0x200, 0, 0),
    AFFINEANIMCMD_END,
};

static const union AffineAnimCmd *const sAffineAnims_FogH[] =
{
    sAffineAnim_FogH,
};

static void FogHorizontalSpriteCallback(struct Sprite *);
static const struct SpriteTemplate sFogHorizontalSpriteTemplate =
{
    .tileTag = GFXTAG_FOG_H,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sOamData_FogH,
    .anims = sAnims_FogH,
    .images = NULL,
    .affineAnims = sAffineAnims_FogH,
    .callback = FogHorizontalSpriteCallback,
};

void FogHorizontal_Main(void);
static void CreateFogHorizontalSprites(void);
static void DestroyFogHorizontalSprites(void);

// Within the weather palette, shadow sprites' color index
#define SHADOW_COLOR_INDEX 9

// Updates just the color of shadows to match special weather blending
u8 UpdateShadowColor(u16 color) {
    u8 paletteNum = IndexOfSpritePaletteTag(TAG_WEATHER_START);
    u16 ALIGNED(4) tempBuffer[16];
    u16 blendedColor;
    if (paletteNum < 16) {
        u16 index = OBJ_PLTT_ID(paletteNum)+SHADOW_COLOR_INDEX;
        gPlttBufferUnfaded[index] = gPlttBufferFaded[index] = color;
        // Copy to temporary buffer, blend, and keep just the shadow color index
        CpuFastCopy(&gPlttBufferFaded[index-SHADOW_COLOR_INDEX], tempBuffer, PLTT_SIZE_4BPP);
        UpdateSpritePaletteWithTime(paletteNum);
        blendedColor = gPlttBufferFaded[index];
        CpuFastCopy(tempBuffer, &gPlttBufferFaded[index-SHADOW_COLOR_INDEX], PLTT_SIZE_4BPP);
        gPlttBufferFaded[index] = blendedColor;
    }
    return paletteNum;
}

void FogHorizontal_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    if (gWeatherPtr->fogHSpritesCreated == 0)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset = 0;
        gWeatherPtr->fogHScrollPosX = 0;
        Weather_SetBlendCoeffs(0, 16);
    }
    gWeatherPtr->noShadows = FALSE;
}

void FogHorizontal_InitAll(void)
{
    FogHorizontal_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        FogHorizontal_Main();
}

void FogHorizontal_Main(void)
{
    gWeatherPtr->fogHScrollPosX = (gSpriteCoordOffsetX - gWeatherPtr->fogHScrollOffset) & 0xFF;
    if (++gWeatherPtr->fogHScrollCounter > 3)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset++;
    }
    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateFogHorizontalSprites();
        if (gWeatherPtr->currWeather == WEATHER_FOG_HORIZONTAL) {
          u8 paletteNum = IndexOfSpritePaletteTag(TAG_WEATHER_START);
          Weather_SetTargetBlendCoeffs(12, 8, 3);
          UpdateShadowColor(0x3DEF); // Gray
        } else
            Weather_SetTargetBlendCoeffs(4, 16, 0);
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 FogHorizontal_Finish(void)
{
    gWeatherPtr->fogHScrollPosX = (gSpriteCoordOffsetX - gWeatherPtr->fogHScrollOffset) & 0xFF;
    if (++gWeatherPtr->fogHScrollCounter > 3)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset++;
    }

    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 16, 3);
        gWeatherPtr->finishStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
            gWeatherPtr->finishStep++;
        break;
    case 2:
        DestroyFogHorizontalSprites();
        gWeatherPtr->finishStep++;
        break;
    default:
        UpdateShadowColor(RGB_BLACK);
        return FALSE;
    }
    return TRUE;
}

#define tSpriteColumn data[0]

static void FogHorizontalSpriteCallback(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    sprite->y2 = (u8)gSpriteCoordOffsetY;
    sprite->x = gWeatherPtr->fogHScrollPosX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = (DISPLAY_WIDTH * 2) + gWeatherPtr->fogHScrollPosX - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

static void CreateFogHorizontalSprites(void)
{
    u16 i;
    u8 spriteId;
    struct Sprite *sprite;

    if (!gWeatherPtr->fogHSpritesCreated)
    {
        struct SpriteSheet fogHorizontalSpriteSheet = {
            .data = gWeatherFogHorizontalTiles,
            .size = sizeof(gWeatherFogHorizontalTiles),
            .tag = GFXTAG_FOG_H,
        };
        LoadSpriteSheet(&fogHorizontalSpriteSheet);
        for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sFogHorizontalSpriteTemplate, 0, 0, 0xFF);
            if (spriteId != MAX_SPRITES)
            {
                sprite = &gSprites[spriteId];
                sprite->tSpriteColumn = i % 5;
                sprite->x = (i % 5) * 64 + 32;
                sprite->y = (i / 5) * 64 + 32;
                gWeatherPtr->sprites.s2.fogHSprites[i] = sprite;
            }
            else
            {
                gWeatherPtr->sprites.s2.fogHSprites[i] = NULL;
            }
        }

        gWeatherPtr->fogHSpritesCreated = TRUE;
    }
}

static void DestroyFogHorizontalSprites(void)
{
    u16 i;

    if (gWeatherPtr->fogHSpritesCreated)
    {
        for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.fogHSprites[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.fogHSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_FOG_H);
        gWeatherPtr->fogHSpritesCreated = 0;
    }
}

#undef tSpriteColumn

//------------------------------------------------------------------------------
// WEATHER_MIST
// v6.0: the same tiny drifting mist clouds (the fog texture, scrolling as in v4.5),
// now used as an OBJ window instead of being drawn: wherever a cloud is, everything
// under it - ground, tree tops, roofs, you, people, wild Pokemon - is brightened with
// the GBA's colour effect. Nothing is drawn on top, so reflections show again.
// The clouds stand still while a start menu is open; while the Perfect start menu
// (which uses the OBJ window itself) is open they are hidden.
// Most likely at dawn (04:30-06:30) through the dynamic weather.
//------------------------------------------------------------------------------
#define MIST_BLEND_EVA 4         // ramp target (the strength, in 16ths, by day)
#define MIST_HAZE_NIGHT_DIV 2    // half as strong at night

static bool8 sMistWinOn;
static u16 sMistSavedBldCnt, sMistSavedWinOut, sMistSavedWin0H;

static void UpdateMistScroll(void)
{
    gWeatherPtr->fogHScrollPosX = (gSpriteCoordOffsetX - gWeatherPtr->fogHScrollOffset) & 0xFF;
    if (++gWeatherPtr->fogHScrollCounter > 3)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset++;
    }
}

static void SetMistSprites(bool8 visible)
{
    u32 i;

    if (!gWeatherPtr->fogHSpritesCreated)
        return;
    for (i = 0; i < NUM_FOG_HORIZONTAL_SPRITES; i++)
    {
        struct Sprite *sprite = gWeatherPtr->sprites.s2.fogHSprites[i];
        if (sprite == NULL)
            continue;
        sprite->oam.objMode = ST_OAM_OBJ_WINDOW;   // a shape for the window, never drawn
        sprite->invisible = !visible;
    }
}

static void MistWindowOff(void)
{
    if (!sMistWinOn)
        return;
    ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, sMistSavedBldCnt);
    SetGpuReg(REG_OFFSET_WINOUT, sMistSavedWinOut);
    SetGpuReg(REG_OFFSET_WIN0H, sMistSavedWin0H);
    sMistWinOn = FALSE;
}

static void MistWindowOn(void)
{
    u16 bldy;

    if (!sMistWinOn)
    {
        sMistSavedBldCnt = GetGpuReg(REG_OFFSET_BLDCNT);
        sMistSavedWinOut = GetGpuReg(REG_OFFSET_WINOUT);
        sMistSavedWin0H = GetGpuReg(REG_OFFSET_WIN0H);
        sMistWinOn = TRUE;
    }
    // window 0 covers the overworld with colour effects off: shrink it; outside the
    // clouds everything shows as normal, inside them everything is brightened
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ
                               | WINOUT_WINOBJ_BG_ALL | WINOUT_WINOBJ_OBJ | WINOUT_WINOBJ_CLR);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT1_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT1_OBJ | BLDCNT_TGT1_BD
                               | BLDCNT_EFFECT_LIGHTEN);
    SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
    bldy = gWeatherPtr->currBlendEVA;
    if (gTimeOfDay == TIME_OF_DAY_NIGHT)
        bldy /= MIST_HAZE_NIGHT_DIV;
    if (gPaletteFade.active || gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_SCREEN_FADING_IN
     || gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_SCREEN_FADING_OUT)
        bldy = 0;   // or a fade to black would stop at grey
    if (GetGpuReg(REG_OFFSET_BLDY) != bldy)
        SetGpuReg(REG_OFFSET_BLDY, bldy);
}

// shared by Mist_Main and Mist_Finish: the clouds follow the start menus
static bool8 MistStepAside(void)
{
    if (Usm_IsOpen())
    {
        MistWindowOff();
        SetMistSprites(FALSE);
        return TRUE;
    }
    SetMistSprites(TRUE);
    if (!IsWeatherFrozen())
        UpdateMistScroll();
    return FALSE;
}

// ---------------------------------------------------------------------------
// v6.4 MIST = Perfect: the fog texture of the first mist (the soft drifting
// clouds) on its own layer, BG0, laid over everything with the GBA's alpha
// blending - ground, tree tops, roofs, you, people and Pokemon, and reflections
// (they are drawn underneath and show through). The texture follows the map
// as you walk and drifts slowly. BG0 is also where text boxes and menus go:
// while any is open the fog steps aside. (MIST = Old is the v6.3 mist above.)
// ---------------------------------------------------------------------------
#define MISTBG_TILE     0x2C0   // 64 tiles at the end of BG0's tile space
#define MISTBG_PAL      13
#define MISTBG_EVA      4
#define MISTBG_EVB      13
#define MISTBG_EVB_NIGHT 11

static bool8 sMistBgShown;
static s16 sMistBgOffX, sMistBgOffY;
static u16 sMistBgDrift;
static u8 sMistBgDriftTimer;
static u16 sMistBgSavedBldCnt, sMistBgSavedWinOut, sMistBgSavedWin0H, sMistBgSavedWinIn;

static bool8 UseMistBg(void)
{
    return !FlagGet(FLAG_MIST_OLD);
}

// is a text box or menu on screen? (the field message box window always exists;
// it only counts when its tiles are on the BG0 map)
static bool8 AnyBg0WindowOpen(void)
{
    u16 *tilemap = GetBgTilemapBuffer(0);
    u32 i, x, y;

    if (tilemap == NULL)
        return TRUE;
    for (i = 0; i < WINDOWS_MAX; i++)
    {
        const struct WindowTemplate *w = &gWindows[i].window;
        if (gWindows[i].tileData == NULL || w->bg != 0)
            continue;
        for (y = w->tilemapTop; y < w->tilemapTop + w->height && y < 32; y++)
        {
            for (x = w->tilemapLeft; x < w->tilemapLeft + w->width && x < 32; x++)
            {
                u16 tile = tilemap[y * 32 + x] & 0x3FF;
                if (tile != 0 && (tile < MISTBG_TILE || tile >= MISTBG_TILE + 64))
                    return TRUE;
            }
        }
    }
    return FALSE;
}

static u32 *MistBgRow(u32 tx, u32 row)
{
    return (u32 *)(BG_CHAR_ADDR(2) + (MISTBG_TILE + (row / 8) * 8 + tx) * TILE_SIZE_4BPP) + (row % 8);
}

// texture moves left by n pixels (|n| <= 7)
static void MistBgShiftX(s32 n)
{
    u32 r, i;
    if (n == 0)
        return;
    for (r = 0; r < 64; r++)
    {
        u32 w[8], out[8];
        for (i = 0; i < 8; i++)
            w[i] = *MistBgRow(i, r);
        for (i = 0; i < 8; i++)
        {
            if (n > 0)
                out[i] = (w[i] >> (4 * n)) | (w[(i + 1) & 7] << (32 - 4 * n));
            else
                out[i] = (w[i] << (4 * -n)) | (w[(i + 7) & 7] >> (32 + 4 * n));
        }
        for (i = 0; i < 8; i++)
            *MistBgRow(i, r) = out[i];
    }
}

// texture moves up by n rows (|n| <= 63)
static void MistBgShiftY(s32 n)
{
    u32 tx, r;
    if (n == 0)
        return;
    for (tx = 0; tx < 8; tx++)
    {
        u32 col[64];
        for (r = 0; r < 64; r++)
            col[r] = *MistBgRow(tx, r);
        for (r = 0; r < 64; r++)
            *MistBgRow(tx, r) = col[(r + n + 64) & 63];
    }
}

static void MistBgFollowCamera(void)
{
    s32 wantX = (sMistBgDrift - gSpriteCoordOffsetX) & 63;
    s32 wantY = (-gSpriteCoordOffsetY) & 63;
    s32 dx = ((wantX - sMistBgOffX + 32) & 63) - 32;
    s32 dy = ((wantY - sMistBgOffY + 32) & 63) - 32;

    while (dx != 0)
    {
        s32 step = dx > 7 ? 7 : (dx < -7 ? -7 : dx);
        MistBgShiftX(step);
        dx -= step;
    }
    MistBgShiftY(dy);
    sMistBgOffX = wantX;
    sMistBgOffY = wantY;
}

// fog tiles everywhere except inside windows that exist (leftover map entries of
// closed pop-ups - the map name window is removed without clearing its map -
// are covered too)
static bool8 MistBgFillMap(u16 *tilemap)
{
    u32 x, y;
    bool8 changed = FALSE;
    for (y = 0; y < 32; y++)
    {
        for (x = 0; x < 32; x++)
        {
            // everywhere except inside a window that still exists (leftover map
            // entries of closed pop-ups are overwritten)
            u32 i;
            bool8 inWindow = FALSE;
            for (i = 0; i < WINDOWS_MAX && !inWindow; i++)
            {
                const struct WindowTemplate *w = &gWindows[i].window;
                if (gWindows[i].tileData != NULL && w->bg == 0 && x >= w->tilemapLeft && x < w->tilemapLeft + w->width
                 && y >= w->tilemapTop && y < w->tilemapTop + w->height && tilemap[y * 32 + x] != 0)
                    inWindow = TRUE;
            }
            if (!inWindow && tilemap[y * 32 + x] != ((MISTBG_TILE + (y % 8) * 8 + (x % 8)) | (MISTBG_PAL << 12)))
            {
                tilemap[y * 32 + x] = (MISTBG_TILE + (y % 8) * 8 + (x % 8)) | (MISTBG_PAL << 12);
                changed = TRUE;
            }
        }
    }
    return changed;
}

static void MistBgShow(void)
{
    u16 *tilemap = GetBgTilemapBuffer(0);
    u32 x, y;

    if (sMistBgShown || tilemap == NULL)
        return;
    {
        // the texture is a byte array: copy it 16 bits at a time (VRAM takes no byte writes)
        u16 *dst = (u16 *)(BG_CHAR_ADDR(2) + MISTBG_TILE * TILE_SIZE_4BPP);
        for (x = 0; x < 64 * TILE_SIZE_4BPP / 2; x++)
            dst[x] = gWeatherFogHorizontalTiles[2 * x] | (gWeatherFogHorizontalTiles[2 * x + 1] << 8);
    }
    sMistBgOffX = sMistBgOffY = 0;
    MistBgFollowCamera();
    LoadPalette(gFogPalette, BG_PLTT_ID(MISTBG_PAL), PLTT_SIZE_4BPP);
    MistBgFillMap(tilemap);
    CopyBgTilemapBufferToVram(0);
    sMistBgSavedBldCnt = GetGpuReg(REG_OFFSET_BLDCNT);
    sMistBgSavedWinOut = GetGpuReg(REG_OFFSET_WINOUT);
    sMistBgSavedWin0H = GetGpuReg(REG_OFFSET_WIN0H);
    sMistBgSavedWinIn = GetGpuReg(REG_OFFSET_WININ);
    // windows 0 / 1 (used by the map name pop-up) have colour effects off: allow them
    SetGpuReg(REG_OFFSET_WININ, sMistBgSavedWinIn | WININ_WIN0_BG0 | WININ_WIN0_CLR | WININ_WIN1_BG0 | WININ_WIN1_CLR);
    // colour effects are off inside the overworld's window 0: shrink it
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_BLEND
                               | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_BG3 | BLDCNT_TGT2_OBJ | BLDCNT_TGT2_BD);
    sMistBgShown = TRUE;
}

static void MistBgHide(void)
{
    u16 *tilemap = GetBgTilemapBuffer(0);
    u32 i;

    if (!sMistBgShown)
        return;
    if (tilemap != NULL)
    {
        for (i = 0; i < 32 * 32; i++)
        {
            u16 tile = tilemap[i] & 0x3FF;
            if (tile >= MISTBG_TILE && tile < MISTBG_TILE + 64)
                tilemap[i] = 0;
        }
        CopyBgTilemapBufferToVram(0);
    }
    SetGpuReg(REG_OFFSET_BLDCNT, sMistBgSavedBldCnt);
    SetGpuReg(REG_OFFSET_WINOUT, sMistBgSavedWinOut);
    SetGpuReg(REG_OFFSET_WIN0H, sMistBgSavedWin0H);
    SetGpuReg(REG_OFFSET_WININ, sMistBgSavedWinIn);
    sMistBgShown = FALSE;
}

static void MistBgUpdate(void)
{
    bool8 fading = gPaletteFade.active || gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_SCREEN_FADING_IN
                || gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_SCREEN_FADING_OUT;

    if (AnyBg0WindowOpen() || fading)
    {
        MistBgHide();   // text boxes / menus use BG0; and no fog over a fade
        return;
    }
    MistBgShow();
    if ((gMain.vblankCounter1 & 31) == 0 && MistBgFillMap(GetBgTilemapBuffer(0)))
        CopyBgTilemapBufferToVram(0);
    if (!IsWeatherFrozen())
    {
        if (++sMistBgDriftTimer > 3)
        {
            sMistBgDriftTimer = 0;
            sMistBgDrift++;
        }
        MistBgFollowCamera();
    }
}

static u8 MistBgEvb(void)
{
    return gTimeOfDay == TIME_OF_DAY_NIGHT ? MISTBG_EVB_NIGHT : MISTBG_EVB;
}

void Mist_InitVars(void)
{
    if (UseMistBg())
    {
        gWeatherPtr->initStep = 0;
        gWeatherPtr->weatherGfxLoaded = FALSE;
        gWeatherPtr->targetColorMapIndex = 0;
        gWeatherPtr->colorMapStepDelay = 20;
        gWeatherPtr->noShadows = FALSE;
        sMistBgShown = FALSE;   // the layers were reset with the map
        Weather_SetBlendCoeffs(0, 16);
        return;
    }
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    if (gWeatherPtr->fogHSpritesCreated == 0)
    {
        gWeatherPtr->fogHScrollCounter = 0;
        gWeatherPtr->fogHScrollOffset = 0;
        gWeatherPtr->fogHScrollPosX = 0;
        Weather_SetBlendCoeffs(0, 16);
    }
    gWeatherPtr->noShadows = FALSE;
    sMistWinOn = FALSE;   // the display registers were reset with the map
}

void Mist_InitAll(void)
{
    if (UseMistBg())
    {
        Mist_InitVars();
        Weather_SetBlendCoeffs(MISTBG_EVA, MistBgEvb());
        gWeatherPtr->initStep = 2;
        gWeatherPtr->weatherGfxLoaded = TRUE;
        return;
    }
    // v6.3: set up at once - the old loop waited on Mist_Main, which pauses while a
    // start menu is open, so coming back from Options in mist hung the game
    Mist_InitVars();
    CreateFogHorizontalSprites();
    SetMistSprites(TRUE);
    Weather_SetBlendCoeffs(MIST_BLEND_EVA, 16);
    gWeatherPtr->initStep = 2;
    gWeatherPtr->weatherGfxLoaded = TRUE;
}

void Mist_Main(void)
{
    if (UseMistBg())
    {
        MistBgUpdate();
        if (IsWeatherFrozen())
            return;
        switch (gWeatherPtr->initStep)
        {
        case 0:
            Weather_SetTargetBlendCoeffs(MISTBG_EVA, MistBgEvb(), 3);
            gWeatherPtr->initStep++;
            break;
        case 1:
            if (Weather_UpdateBlend())
            {
                gWeatherPtr->weatherGfxLoaded = TRUE;
                gWeatherPtr->initStep++;
            }
            break;
        default:
            if (gWeatherPtr->currBlendEVB != MistBgEvb())   // follows day <-> night
            {
                Weather_SetTargetBlendCoeffs(MISTBG_EVA, MistBgEvb(), 3);
                gWeatherPtr->initStep = 1;
            }
            break;
        }
        return;
    }
    if (MistStepAside())
        return;
    if (gWeatherPtr->initStep > 0)
        MistWindowOn();
    if (IsWeatherFrozen())
        return;
    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateFogHorizontalSprites();
        SetMistSprites(TRUE);
        Weather_SetTargetBlendCoeffs(MIST_BLEND_EVA, 16, 3);
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 Mist_Finish(void)
{
    if (UseMistBg())
    {
        MistBgUpdate();
        switch (gWeatherPtr->finishStep)
        {
        case 0:
            Weather_SetTargetBlendCoeffs(0, 16, 3);
            gWeatherPtr->finishStep++;
            return TRUE;
        case 1:
            if (Weather_UpdateBlend())
                gWeatherPtr->finishStep++;
            return TRUE;
        case 2:
            MistBgHide();
            gWeatherPtr->finishStep++;
            return TRUE;
        }
        return FALSE;
    }
    if (!MistStepAside() && gWeatherPtr->finishStep < 2)
        MistWindowOn();
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 16, 3);
        gWeatherPtr->finishStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
            gWeatherPtr->finishStep++;
        break;
    case 2:
        MistWindowOff();
        DestroyFogHorizontalSprites();
        gWeatherPtr->finishStep++;
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

//------------------------------------------------------------------------------
// WEATHER_VOLCANIC_ASH
//------------------------------------------------------------------------------

static void LoadAshSpriteSheet(void);
static void CreateAshSprites(void);
static void DestroyAshSprites(void);
static void UpdateAshSprite(struct Sprite *);

void Ash_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = FALSE;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->ashUnused = 20; // Never read
    if (!gWeatherPtr->ashSpritesCreated)
    {
        Weather_SetBlendCoeffs(0, 12);
        // SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(64, 63)); // These aren't valid blend coefficients!
    }
    gWeatherPtr->noShadows = FALSE;
}

void Ash_InitAll(void)
{
    Ash_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        Ash_Main();
}

void Ash_Main(void)
{
    gWeatherPtr->ashBaseSpritesX = gSpriteCoordOffsetX & 0x1FF;
    while (gWeatherPtr->ashBaseSpritesX >= DISPLAY_WIDTH)
        gWeatherPtr->ashBaseSpritesX -= DISPLAY_WIDTH;

    switch (gWeatherPtr->initStep)
    {
    case 0:
        LoadAshSpriteSheet();
        gWeatherPtr->initStep++;
        break;
    case 1:
        if (!gWeatherPtr->ashSpritesCreated)
            CreateAshSprites();

        Weather_SetTargetBlendCoeffs(10, 12, 1);
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (Weather_UpdateBlend())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    default:
        Weather_UpdateBlend();
        break;
    }
}

bool8 Ash_Finish(void)
{
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 12, 1);
        gWeatherPtr->finishStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
        {
            DestroyAshSprites();
            gWeatherPtr->finishStep++;
        }
        break;
    case 2:
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        gWeatherPtr->finishStep++;
        return FALSE;
    default:
        return FALSE;
    }
    return TRUE;
}

static const struct SpriteSheet sAshSpriteSheet =
{
    .data = gWeatherAshTiles,
    .size = sizeof(gWeatherAshTiles),
    .tag = GFXTAG_ASH,
};

static void LoadAshSpriteSheet(void)
{
    LoadSpriteSheet(&sAshSpriteSheet);
}

static const struct OamData sAshSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_BLEND,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 15,
};

static const union AnimCmd sAshSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 60),
    ANIMCMD_FRAME(64, 60),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sAshSpriteAnimCmds[] =
{
    sAshSpriteAnimCmd0,
};

static const struct SpriteTemplate sAshSpriteTemplate =
{
    .tileTag = GFXTAG_ASH,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sAshSpriteOamData,
    .anims = sAshSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateAshSprite,
};

#define tOffsetY      data[0]
#define tCounterY     data[1]
#define tSpriteColumn data[2]
#define tSpriteRow    data[3]

static void CreateAshSprites(void)
{
    u8 i;
    u8 spriteId;
    struct Sprite *sprite;

    if (!gWeatherPtr->ashSpritesCreated)
    {
        for (i = 0; i < NUM_ASH_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sAshSpriteTemplate, 0, 0, 0x4E);
            if (spriteId != MAX_SPRITES)
            {
                sprite = &gSprites[spriteId];
                sprite->tCounterY = 0;
                sprite->tSpriteColumn = (u8)(i % 5);
                sprite->tSpriteRow = (u8)(i / 5);
                sprite->tOffsetY = sprite->tSpriteRow * 64 + 32;
                gWeatherPtr->sprites.s2.ashSprites[i] = sprite;
            }
            else
            {
                gWeatherPtr->sprites.s2.ashSprites[i] = NULL;
            }
        }

        gWeatherPtr->ashSpritesCreated = TRUE;
    }
}

static void DestroyAshSprites(void)
{
    u16 i;

    if (gWeatherPtr->ashSpritesCreated)
    {
        for (i = 0; i < NUM_ASH_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.ashSprites[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.ashSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_ASH);
        gWeatherPtr->ashSpritesCreated = FALSE;
    }
}

static void UpdateAshSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    if (++sprite->tCounterY > 5)
    {
        sprite->tCounterY = 0;
        sprite->tOffsetY++;
    }

    sprite->y = gSpriteCoordOffsetY + sprite->tOffsetY;
    sprite->x = gWeatherPtr->ashBaseSpritesX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = gWeatherPtr->ashBaseSpritesX + (DISPLAY_WIDTH * 2) - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

#undef tOffsetY
#undef tCounterY
#undef tSpriteColumn
#undef tSpriteRow

//------------------------------------------------------------------------------
// WEATHER_FOG_DIAGONAL
//------------------------------------------------------------------------------

static void UpdateFogDiagonalMovement(void);
static void CreateFogDiagonalSprites(void);
static void DestroyFogDiagonalSprites(void);
static void UpdateFogDiagonalSprite(struct Sprite *);

void FogDiagonal_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = 0;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    gWeatherPtr->fogHScrollCounter = 0;
    gWeatherPtr->fogHScrollOffset = 1;
    if (!gWeatherPtr->fogDSpritesCreated)
    {
        gWeatherPtr->fogDScrollXCounter = 0;
        gWeatherPtr->fogDScrollYCounter = 0;
        gWeatherPtr->fogDXOffset = 0;
        gWeatherPtr->fogDYOffset = 0;
        gWeatherPtr->fogDBaseSpritesX = 0;
        gWeatherPtr->fogDPosY = 0;
        Weather_SetBlendCoeffs(0, 16);
    }
    gWeatherPtr->noShadows = TRUE;
}

void FogDiagonal_InitAll(void)
{
    FogDiagonal_InitVars();
    while (gWeatherPtr->weatherGfxLoaded == FALSE)
        FogDiagonal_Main();
}

void FogDiagonal_Main(void)
{
    UpdateFogDiagonalMovement();
    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateFogDiagonalSprites();
        gWeatherPtr->initStep++;
        break;
    case 1:
        Weather_SetTargetBlendCoeffs(12, 8, 8);
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (!Weather_UpdateBlend())
            break;
        gWeatherPtr->weatherGfxLoaded = TRUE;
        gWeatherPtr->initStep++;
        break;
    }
}

bool8 FogDiagonal_Finish(void)
{
    UpdateFogDiagonalMovement();
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 16, 1);
        gWeatherPtr->finishStep++;
        break;
    case 1:
        if (!Weather_UpdateBlend())
            break;
        gWeatherPtr->finishStep++;
        break;
    case 2:
        DestroyFogDiagonalSprites();
        gWeatherPtr->finishStep++;
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

static void UpdateFogDiagonalMovement(void)
{
    if (++gWeatherPtr->fogDScrollXCounter > 2)
    {
        gWeatherPtr->fogDXOffset++;
        gWeatherPtr->fogDScrollXCounter = 0;
    }

    if (++gWeatherPtr->fogDScrollYCounter > 4)
    {
        gWeatherPtr->fogDYOffset++;
        gWeatherPtr->fogDScrollYCounter = 0;
    }

    gWeatherPtr->fogDBaseSpritesX = (gSpriteCoordOffsetX - gWeatherPtr->fogDXOffset) & 0xFF;
    gWeatherPtr->fogDPosY = gSpriteCoordOffsetY + gWeatherPtr->fogDYOffset;
}

static const struct SpriteSheet sFogDiagonalSpriteSheet =
{
    .data = gWeatherFogDiagonalTiles,
    .size = sizeof(gWeatherFogDiagonalTiles),
    .tag = GFXTAG_FOG_D,
};

static const struct OamData sFogDiagonalSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_BLEND,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 2,
    .paletteNum = 0,
};

static const union AnimCmd sFogDiagonalSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sFogDiagonalSpriteAnimCmds[] =
{
    sFogDiagonalSpriteAnimCmd0,
};

static const struct SpriteTemplate sFogDiagonalSpriteTemplate =
{
    .tileTag = GFXTAG_FOG_D,
    .paletteTag = PALTAG_WEATHER,
    .oam = &sFogDiagonalSpriteOamData,
    .anims = sFogDiagonalSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateFogDiagonalSprite,
};

#define tSpriteColumn data[0]
#define tSpriteRow    data[1]

static void CreateFogDiagonalSprites(void)
{
    u16 i;
    struct SpriteSheet fogDiagonalSpriteSheet;
    u8 spriteId;
    struct Sprite *sprite;

    if (!gWeatherPtr->fogDSpritesCreated)
    {
        fogDiagonalSpriteSheet = sFogDiagonalSpriteSheet;
        LoadSpriteSheet(&fogDiagonalSpriteSheet);
        for (i = 0; i < NUM_FOG_DIAGONAL_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sFogDiagonalSpriteTemplate, 0, (i / 5) * 64, 0xFF);
            if (spriteId != MAX_SPRITES)
            {
                sprite = &gSprites[spriteId];
                sprite->tSpriteColumn = i % 5;
                sprite->tSpriteRow = i / 5;
                gWeatherPtr->sprites.s2.fogDSprites[i] = sprite;
            }
            else
            {
                gWeatherPtr->sprites.s2.fogDSprites[i] = NULL;
            }
        }

        gWeatherPtr->fogDSpritesCreated = TRUE;
    }
}

static void DestroyFogDiagonalSprites(void)
{
    u16 i;

    if (gWeatherPtr->fogDSpritesCreated)
    {
        for (i = 0; i < NUM_FOG_DIAGONAL_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.fogDSprites[i])
                DestroySprite(gWeatherPtr->sprites.s2.fogDSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_FOG_D);
        gWeatherPtr->fogDSpritesCreated = FALSE;
    }
}

static void UpdateFogDiagonalSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    sprite->y2 = gWeatherPtr->fogDPosY;
    sprite->x = gWeatherPtr->fogDBaseSpritesX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = gWeatherPtr->fogDBaseSpritesX + (DISPLAY_WIDTH * 2) - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

#undef tSpriteColumn
#undef tSpriteRow

//------------------------------------------------------------------------------
// WEATHER_SANDSTORM
//------------------------------------------------------------------------------

static void UpdateSandstormWaveIndex(void);
static void UpdateSandstormMovement(void);
static void CreateSandstormSprites(void);
static void CreateSwirlSandstormSprites(void);
static void DestroySandstormSprites(void);
static void UpdateSandstormSprite(struct Sprite *);
static void WaitSandSwirlSpriteEntrance(struct Sprite *);
static void UpdateSandstormSwirlSprite(struct Sprite *);

#define MIN_SANDSTORM_WAVE_INDEX 0x20

void Sandstorm_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->weatherGfxLoaded = 0;
    gWeatherPtr->targetColorMapIndex = 0;
    gWeatherPtr->colorMapStepDelay = 20;
    if (!gWeatherPtr->sandstormSpritesCreated)
    {
        gWeatherPtr->sandstormXOffset = gWeatherPtr->sandstormYOffset = 0;
        gWeatherPtr->sandstormWaveIndex = 8;
        gWeatherPtr->sandstormWaveCounter = 0;
        // Dead code. How does the compiler not optimize this out?
        if (gWeatherPtr->sandstormWaveIndex >= 0x80 - MIN_SANDSTORM_WAVE_INDEX)
            gWeatherPtr->sandstormWaveIndex = 0x80 - gWeatherPtr->sandstormWaveIndex;

        Weather_SetBlendCoeffs(0, 16);
    }
    gWeatherPtr->noShadows = FALSE;
}

void Sandstorm_InitAll(void)
{
    Sandstorm_InitVars();
    while (!gWeatherPtr->weatherGfxLoaded)
        Sandstorm_Main();
}

void Sandstorm_Main(void)
{
    UpdateSandstormMovement();
    UpdateSandstormWaveIndex();
    if (gWeatherPtr->sandstormWaveIndex >= 0x80 - MIN_SANDSTORM_WAVE_INDEX)
        gWeatherPtr->sandstormWaveIndex = MIN_SANDSTORM_WAVE_INDEX;

    switch (gWeatherPtr->initStep)
    {
    case 0:
        CreateSandstormSprites();
        CreateSwirlSandstormSprites();
        gWeatherPtr->initStep++;
        break;
    case 1:
        Weather_SetTargetBlendCoeffs(16, 2, 0);
        UpdateShadowColor(0x3DEF);
        gWeatherPtr->initStep++;
        break;
    case 2:
        if (Weather_UpdateBlend())
        {
            gWeatherPtr->weatherGfxLoaded = TRUE;
            gWeatherPtr->initStep++;
        }
        break;
    }
}

bool8 Sandstorm_Finish(void)
{
    UpdateSandstormMovement();
    UpdateSandstormWaveIndex();
    switch (gWeatherPtr->finishStep)
    {
    case 0:
        Weather_SetTargetBlendCoeffs(0, 16, 0);
        gWeatherPtr->finishStep++;
        break;
    case 1:
        if (Weather_UpdateBlend())
            gWeatherPtr->finishStep++;
        if (gWeatherPtr->currBlendEVB == 12)
          UpdateShadowColor(RGB_BLACK);
        break;
    case 2:
        DestroySandstormSprites();
        UpdateShadowColor(RGB_BLACK);
        gWeatherPtr->finishStep++;
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

static void UpdateSandstormWaveIndex(void)
{
    if (gWeatherPtr->sandstormWaveCounter++ > 4)
    {
        gWeatherPtr->sandstormWaveIndex++;
        gWeatherPtr->sandstormWaveCounter = 0;
    }
}

static void UpdateSandstormMovement(void)
{
    gWeatherPtr->sandstormXOffset -= gSineTable[gWeatherPtr->sandstormWaveIndex] * 4;
    gWeatherPtr->sandstormYOffset -= gSineTable[gWeatherPtr->sandstormWaveIndex];
    gWeatherPtr->sandstormBaseSpritesX = (gSpriteCoordOffsetX + (gWeatherPtr->sandstormXOffset >> 8)) & 0xFF;
    gWeatherPtr->sandstormPosY = gSpriteCoordOffsetY + (gWeatherPtr->sandstormYOffset >> 8);
}

static void DestroySandstormSprites(void)
{
    u16 i;

    if (gWeatherPtr->sandstormSpritesCreated)
    {
        for (i = 0; i < NUM_SANDSTORM_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.sandstormSprites1[i])
                DestroySprite(gWeatherPtr->sprites.s2.sandstormSprites1[i]);
        }

        gWeatherPtr->sandstormSpritesCreated = FALSE;
        FreeSpriteTilesByTag(GFXTAG_SANDSTORM);
    }

    if (gWeatherPtr->sandstormSwirlSpritesCreated)
    {
        for (i = 0; i < NUM_SWIRL_SANDSTORM_SPRITES; i++)
        {
            if (gWeatherPtr->sprites.s2.sandstormSprites2[i] != NULL)
                DestroySprite(gWeatherPtr->sprites.s2.sandstormSprites2[i]);
        }

        gWeatherPtr->sandstormSwirlSpritesCreated = FALSE;
    }
}

static const struct OamData sSandstormSpriteOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_BLEND,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
};

static const union AnimCmd sSandstormSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 3),
    ANIMCMD_END,
};

static const union AnimCmd sSandstormSpriteAnimCmd1[] =
{
    ANIMCMD_FRAME(64, 3),
    ANIMCMD_END,
};

static const union AnimCmd *const sSandstormSpriteAnimCmds[] =
{
    sSandstormSpriteAnimCmd0,
    sSandstormSpriteAnimCmd1,
};

static const struct SpriteTemplate sSandstormSpriteTemplate =
{
    .tileTag = GFXTAG_SANDSTORM,
    .paletteTag = PALTAG_WEATHER_2,
    .oam = &sSandstormSpriteOamData,
    .anims = sSandstormSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateSandstormSprite,
};

static const struct SpriteSheet sSandstormSpriteSheet =
{
    .data = gWeatherSandstormTiles,
    .size = sizeof(gWeatherSandstormTiles),
    .tag = GFXTAG_SANDSTORM,
};

// Regular sandstorm sprites
#define tSpriteColumn  data[0]
#define tSpriteRow     data[1]

// Swirly sandstorm sprites
#define tRadius        data[0]
#define tWaveIndex     data[1]
#define tRadiusCounter data[2]
#define tEntranceDelay data[3]

static void CreateSandstormSprites(void)
{
    u16 i;
    u8 spriteId;

    if (!gWeatherPtr->sandstormSpritesCreated)
    {
        LoadSpriteSheet(&sSandstormSpriteSheet);
        LoadCustomWeatherSpritePalette(gSandstormWeatherPalette);
        for (i = 0; i < NUM_SANDSTORM_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sSandstormSpriteTemplate, 0, (i / 5) * 64, 1);
            if (spriteId != MAX_SPRITES)
            {
                gWeatherPtr->sprites.s2.sandstormSprites1[i] = &gSprites[spriteId];
                gWeatherPtr->sprites.s2.sandstormSprites1[i]->tSpriteColumn = i % 5;
                gWeatherPtr->sprites.s2.sandstormSprites1[i]->tSpriteRow = i / 5;
            }
            else
            {
                gWeatherPtr->sprites.s2.sandstormSprites1[i] = NULL;
            }
        }

        gWeatherPtr->sandstormSpritesCreated = TRUE;
    }
}

static const u16 sSwirlEntranceDelays[] = {0, 120, 80, 160, 40, 0};

static void CreateSwirlSandstormSprites(void)
{
    u16 i;
    u8 spriteId;

    if (!gWeatherPtr->sandstormSwirlSpritesCreated)
    {
        for (i = 0; i < NUM_SWIRL_SANDSTORM_SPRITES; i++)
        {
            spriteId = CreateSpriteAtEnd(&sSandstormSpriteTemplate, i * 48 + 24, 208, 1);
            if (spriteId != MAX_SPRITES)
            {
                gWeatherPtr->sprites.s2.sandstormSprites2[i] = &gSprites[spriteId];
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->oam.size = ST_OAM_SIZE_2;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tSpriteRow = i * 51;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tRadius = 8;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tRadiusCounter = 0;
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->data[4] = 0x6730; // unused value
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->tEntranceDelay = sSwirlEntranceDelays[i];
                StartSpriteAnim(gWeatherPtr->sprites.s2.sandstormSprites2[i], 1);
                CalcCenterToCornerVec(gWeatherPtr->sprites.s2.sandstormSprites2[i], SPRITE_SHAPE(32x32), SPRITE_SIZE(32x32), ST_OAM_AFFINE_OFF);
                gWeatherPtr->sprites.s2.sandstormSprites2[i]->callback = WaitSandSwirlSpriteEntrance;
            }
            else
            {
                gWeatherPtr->sprites.s2.sandstormSprites2[i] = NULL;
            }

            gWeatherPtr->sandstormSwirlSpritesCreated = TRUE;
        }
    }
}

static void UpdateSandstormSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    sprite->y2 = gWeatherPtr->sandstormPosY;
    sprite->x = gWeatherPtr->sandstormBaseSpritesX + 32 + sprite->tSpriteColumn * 64;
    if (sprite->x >= DISPLAY_WIDTH + 32)
    {
        sprite->x = gWeatherPtr->sandstormBaseSpritesX + (DISPLAY_WIDTH * 2) - (4 - sprite->tSpriteColumn) * 64;
        sprite->x &= 0x1FF;
    }
}

static void WaitSandSwirlSpriteEntrance(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    if (--sprite->tEntranceDelay == -1)
        sprite->callback = UpdateSandstormSwirlSprite;
}

static void UpdateSandstormSwirlSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.0: a start menu is open
    u32 x, y;

    if (--sprite->y < -48)
    {
        sprite->y = DISPLAY_HEIGHT + 48;
        sprite->tRadius = 4;
    }

    x = sprite->tRadius * gSineTable[sprite->tWaveIndex];
    y = sprite->tRadius * gSineTable[sprite->tWaveIndex + 0x40];
    sprite->x2 = x >> 8;
    sprite->y2 = y >> 8;
    sprite->tWaveIndex = (sprite->tWaveIndex + 10) & 0xFF;
    if (++sprite->tRadiusCounter > 8)
    {
        sprite->tRadiusCounter = 0;
        sprite->tRadius++;
    }
}

#undef tSpriteColumn
#undef tSpriteRow

#undef tRadius
#undef tWaveIndex
#undef tRadiusCounter
#undef tEntranceDelay

//------------------------------------------------------------------------------
// WEATHER_SHADE
//------------------------------------------------------------------------------

void Shade_InitVars(void)
{
    gWeatherPtr->initStep = 0;
    gWeatherPtr->targetColorMapIndex = 3;
    gWeatherPtr->colorMapStepDelay = 20;
    Weather_SetBlendCoeffs(8, 12); // preserve shadow darkness
    gWeatherPtr->noShadows = FALSE;
}

void Shade_InitAll(void)
{
    Shade_InitVars();
}

void Shade_Main(void)
{
}

bool8 Shade_Finish(void)
{
    return FALSE;
}

//------------------------------------------------------------------------------
// WEATHER_UNDERWATER_BUBBLES
//------------------------------------------------------------------------------

static void CreateBubbleSprite(u16);
static void DestroyBubbleSprites(void);
static void UpdateBubbleSprite(struct Sprite *);

static const u8 sBubbleStartDelays[] = {40, 90, 60, 90, 2, 60, 40, 30};

static const struct SpriteSheet sWeatherBubbleSpriteSheet =
{
    .data = gWeatherBubbleTiles,
    .size = sizeof(gWeatherBubbleTiles),
    .tag = GFXTAG_BUBBLE,
};

static const s16 sBubbleStartCoords[][2] =
{
    {120, 160},
    {376, 160},
    { 40, 140},
    {296, 140},
    {180, 130},
    {436, 130},
    { 60, 160},
    {436, 160},
    {220, 180},
    {476, 180},
    { 10,  90},
    {266,  90},
    {256, 160},
};

void Bubbles_InitVars(void)
{
    FogHorizontal_InitVars();
    if (!gWeatherPtr->bubblesSpritesCreated)
    {
        LoadSpriteSheet(&sWeatherBubbleSpriteSheet);
        gWeatherPtr->bubblesDelayIndex = 0;
        gWeatherPtr->bubblesDelayCounter = sBubbleStartDelays[0];
        gWeatherPtr->bubblesCoordsIndex = 0;
        gWeatherPtr->bubblesSpriteCount = 0;
    }
    gWeatherPtr->noShadows = TRUE;
}

void Bubbles_InitAll(void)
{
    Bubbles_InitVars();
    while (!gWeatherPtr->weatherGfxLoaded)
        Bubbles_Main();
}

void Bubbles_Main(void)
{
    FogHorizontal_Main();
    if (++gWeatherPtr->bubblesDelayCounter > sBubbleStartDelays[gWeatherPtr->bubblesDelayIndex])
    {
        gWeatherPtr->bubblesDelayCounter = 0;
        if (++gWeatherPtr->bubblesDelayIndex > ARRAY_COUNT(sBubbleStartDelays) - 1)
            gWeatherPtr->bubblesDelayIndex = 0;

        CreateBubbleSprite(gWeatherPtr->bubblesCoordsIndex);
        if (++gWeatherPtr->bubblesCoordsIndex > ARRAY_COUNT(sBubbleStartCoords) - 1)
            gWeatherPtr->bubblesCoordsIndex = 0;
    }
}

bool8 Bubbles_Finish(void)
{
    if (!FogHorizontal_Finish())
    {
        DestroyBubbleSprites();
        return FALSE;
    }

    return TRUE;
}

static const union AnimCmd sBubbleSpriteAnimCmd0[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_FRAME(1, 16),
    ANIMCMD_END,
};

static const union AnimCmd *const sBubbleSpriteAnimCmds[] =
{
    sBubbleSpriteAnimCmd0,
};

static const struct SpriteTemplate sBubbleSpriteTemplate =
{
    .tileTag = GFXTAG_BUBBLE,
    .paletteTag = PALTAG_WEATHER,
    .oam = &gOamData_AffineOff_ObjNormal_8x8,
    .anims = sBubbleSpriteAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = UpdateBubbleSprite,
};

#define tScrollXCounter data[0]
#define tScrollXDir     data[1]
#define tCounter        data[2]

static void CreateBubbleSprite(u16 coordsIndex)
{
    s16 x = sBubbleStartCoords[coordsIndex][0];
    s16 y = sBubbleStartCoords[coordsIndex][1] - gSpriteCoordOffsetY;
    u8 spriteId = CreateSpriteAtEnd(&sBubbleSpriteTemplate, x, y, 0);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].oam.priority = 1;
        gSprites[spriteId].coordOffsetEnabled = TRUE;
        gSprites[spriteId].tScrollXCounter = 0;
        gSprites[spriteId].tScrollXDir = 0;
        gSprites[spriteId].tCounter = 0;
        gWeatherPtr->bubblesSpriteCount++;
    }
}

static void DestroyBubbleSprites(void)
{
    u16 i;

    if (gWeatherPtr->bubblesSpriteCount)
    {
        for (i = 0; i < MAX_SPRITES; i++)
        {
            if (gSprites[i].template == &sBubbleSpriteTemplate)
                DestroySprite(&gSprites[i]);
        }

        FreeSpriteTilesByTag(GFXTAG_BUBBLE);
        gWeatherPtr->bubblesSpriteCount = 0;
    }
}

static void UpdateBubbleSprite(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    ++sprite->tScrollXCounter;
    if (++sprite->tScrollXCounter > 8) // double increment
    {
        sprite->tScrollXCounter = 0;
        if (sprite->tScrollXDir == 0)
        {
            if (++sprite->x2 > 4)
                sprite->tScrollXDir = 1;
        }
        else
        {
            if (--sprite->x2 <= 0)
                sprite->tScrollXDir = 0;
        }
    }

    sprite->y -= 3;
    if (++sprite->tCounter >= 120)
        DestroySprite(sprite);
}

#undef tScrollXCounter
#undef tScrollXDir
#undef tCounter

//------------------------------------------------------------------------------

static void UNUSED UnusedSetCurrentAbnormalWeather(u32 weather, u32 unknown)
{
    sCurrentAbnormalWeather = weather;
    sUnusedWeatherRelated = unknown;
}

#define tState         data[0]
#define tWeatherA      data[1]
#define tWeatherB      data[2]
#define tDelay         data[15]

static void Task_DoAbnormalWeather(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (tState)
    {
    case 0:
        if (tDelay-- <= 0)
        {
            SetNextWeather(tWeatherA);
            sCurrentAbnormalWeather = tWeatherA;
            tDelay = 600;
            tState++;
        }
        break;
    case 1:
        if (tDelay-- <= 0)
        {
            SetNextWeather(tWeatherB);
            sCurrentAbnormalWeather = tWeatherB;
            tDelay = 600;
            tState = 0;
        }
        break;
    }
}

static void CreateAbnormalWeatherTask(void)
{
    u8 taskId = CreateTask(Task_DoAbnormalWeather, 0);
    s16 *data = gTasks[taskId].data;

    tDelay = 600;
    if (sCurrentAbnormalWeather == WEATHER_DOWNPOUR)
    {
        // Currently Downpour, next will be Drought
        tWeatherA = WEATHER_DROUGHT;
        tWeatherB = WEATHER_DOWNPOUR;
    }
    else if (sCurrentAbnormalWeather == WEATHER_DROUGHT)
    {
        // Currently Drought, next will be Downpour
        tWeatherA = WEATHER_DOWNPOUR;
        tWeatherB = WEATHER_DROUGHT;
    }
    else
    {
        // Default to starting with Downpour
        sCurrentAbnormalWeather = WEATHER_DOWNPOUR;
        tWeatherA = WEATHER_DROUGHT;
        tWeatherB = WEATHER_DOWNPOUR;
    }
}

#undef tState
#undef tWeatherA
#undef tWeatherB
#undef tDelay

static u8 TranslateWeatherNum(u8);
static void UpdateRainCounter(u8, u8);

// The dynamic system is "in control" while the current weather comes from
// WEATHER_DYNAMIC. Any script or coord event that sets a specific weather
// takes control away, so the live refresh can never overwrite story /
// cutscene / local-zone weather. Control returns when the map is reloaded
// (header weather), or on `resetweather` / COORD_EVENT_WEATHER_DYNAMIC.
//
// FIX: this used to be an EWRAM bool, which reads FALSE after loading a save,
// so "Continue" froze the weather on the map you saved on until you left it.
// It is now a save flag, so overrides also survive saving mid-event.
static void SetDynamicWeatherInControl(bool8 inControl)
{
    if (inControl)
        FlagClear(FLAG_DYNAMIC_WEATHER_OVERRIDDEN);
    else
        FlagSet(FLAG_DYNAMIC_WEATHER_OVERRIDDEN);
}

static bool8 IsDynamicWeatherInControl(void)
{
    return !FlagGet(FLAG_DYNAMIC_WEATHER_OVERRIDDEN);
}

void SetSavedWeather(u32 weather)
{
    u8 oldWeather = gSaveBlock1Ptr->weather;
    SetDynamicWeatherInControl(weather == WEATHER_DYNAMIC);
    gSaveBlock1Ptr->weather = TranslateWeatherNum(weather);
    UpdateRainCounter(gSaveBlock1Ptr->weather, oldWeather);
}

u8 GetSavedWeather(void)
{
    return gSaveBlock1Ptr->weather;
}

#include "data/original_map_weather.h"

// v4.0 WEATHER SYSTEM = Scripted: maps use their original weather again.
static u8 GetHeaderWeatherForSystem(void)
{
    u16 mapId = (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
    u32 i;

    if (!FlagGet(FLAG_WEATHER_SCRIPTED))
        return gMapHeader.weather;
    for (i = 0; i < ARRAY_COUNT(sOriginalMapWeather); i++)
        if (sOriginalMapWeather[i].mapId == mapId)
            return sOriginalMapWeather[i].weather;
    return gMapHeader.weather == WEATHER_DYNAMIC ? WEATHER_SUNNY : gMapHeader.weather;
}

void SetSavedWeatherFromCurrMapHeader(void)
{
    u8 oldWeather = gSaveBlock1Ptr->weather;
    u8 headerWeather = GetHeaderWeatherForSystem();
    SetDynamicWeatherInControl(headerWeather == WEATHER_DYNAMIC);
    gSaveBlock1Ptr->weather = TranslateWeatherNum(headerWeather);
    UpdateRainCounter(gSaveBlock1Ptr->weather, oldWeather);
}

void SetWeather(u32 weather)
{
    SetSavedWeather(weather);
    SetNextWeather(GetSavedWeather());
}

void SetWeather_Unused(u32 weather)
{
    SetSavedWeather(weather);
    SetCurrentAndNextWeather(GetSavedWeather());
}

void DoCurrentWeather(void)
{
    u8 weather = GetSavedWeather();

    if (weather == WEATHER_ABNORMAL)
    {
        if (!FuncIsActiveTask(Task_DoAbnormalWeather))
            CreateAbnormalWeatherTask();
        weather = sCurrentAbnormalWeather;
    }
    else
    {
        if (FuncIsActiveTask(Task_DoAbnormalWeather))
            DestroyTask(FindTaskIdByFunc(Task_DoAbnormalWeather));
        sCurrentAbnormalWeather = WEATHER_DOWNPOUR;
    }
    SetNextWeather(weather);
}

void ResumePausedWeather(void)
{
    u8 weather = GetSavedWeather();

    if (weather == WEATHER_ABNORMAL)
    {
        if (!FuncIsActiveTask(Task_DoAbnormalWeather))
            CreateAbnormalWeatherTask();
        weather = sCurrentAbnormalWeather;
    }
    else
    {
        if (FuncIsActiveTask(Task_DoAbnormalWeather))
            DestroyTask(FindTaskIdByFunc(Task_DoAbnormalWeather));
        sCurrentAbnormalWeather = WEATHER_DOWNPOUR;
    }
    SetCurrentAndNextWeather(weather);
}

#define WEATHER_CYCLE_LENGTH  4

static const u8 sWeatherCycleRoute119[WEATHER_CYCLE_LENGTH] =
{
    WEATHER_SUNNY,
    WEATHER_RAIN,
    WEATHER_RAIN_THUNDERSTORM,
    WEATHER_RAIN,
};
static const u8 sWeatherCycleRoute123[WEATHER_CYCLE_LENGTH] =
{
    WEATHER_SUNNY,
    WEATHER_SUNNY,
    WEATHER_RAIN,
    WEATHER_SUNNY,
};

// ============================================================================
// DYNAMIC REGIONAL WEATHER (v2)
// ----------------------------------------------------------------------------
// Builds on pokeemerald-expansion 1.17's WEATHER_DYNAMIC idea (hash a daily
// seed together with map info to pick from a weather pool) and extends it:
//
//  * SECTIONS - Hoenn is divided into weather sections (groups of MAPSECs).
//    Every map in a section shares the same weather, so neighbouring maps and
//    route connections never disagree.
//  * CLIMATES - each section has a climate: a weighted 16-slot pool, a storm
//    weather and a persistence value.
//  * DAILY SEED - hash(in-game day, save's trainer ID). Stateless: no save data
//    is added, soft resets give the same result, different saves differ.
//  * PERIODS - the day has 4 periods (morning 5-10, day 10-17, evening 17-21,
//    night 21-5). Each period may re-roll; the climate's persistence decides
//    how often the weather simply carries on, so it evolves gradually.
//    Night (00-05) still belongs to the previous in-game day.
//  * STORM FRONTS - on some days a front crosses Hoenn west -> east over the
//    four periods; sections it passes get their climate's storm weather.
//  * NIGHT RULE - no harsh sunlight (drought) at night.
//  * LIVE REFRESH - when a period ends while you stand on a dynamic map, the
//    weather transitions in place (see TryRefreshDynamicWeather).
//
// A map opts in with WEATHER_DYNAMIC in Porymap. Coord weather events can
// hand control back to the system with COORD_EVENT_WEATHER_DYNAMIC.
// ============================================================================

enum DynamicClimate
{
    CLIMATE_NONE,
    CLIMATE_TEMPERATE,   // Littleroot / Petalburg / Mauville lowlands
    CLIMATE_FOREST,      // Petalburg Woods (under the canopy)
    CLIMATE_COASTAL,     // port towns and inshore routes
    CLIMATE_OPEN_SEA,    // Mossdeep / Sootopolis / Pacifidlog seas
    CLIMATE_RAINFOREST,  // Route 119/120, Fortree
    CLIMATE_DESERT,      // Route 111
    CLIMATE_VOLCANIC,    // Mt. Chimney, Jagged Pass, Lavaridge, Route 112
    CLIMATE_ASHFALL,     // Route 113, Fallarbor (downwind of the volcano)
    CLIMATE_MOUNTAIN,    // Route 114/115, Meteor Falls
    CLIMATE_MISTY,       // Mt. Pyre
    CLIMATE_HIGHLAND,    // Ever Grande, Victory Road
    CLIMATE_COUNT
};

struct DynamicClimateInfo
{
    u8 pool[16];      // weighted: more slots = more likely
    u8 storm;         // weather while a storm front passes
    u8 persistence;   // x/16 chance the previous period's weather carries on
};

static const struct DynamicClimateInfo sDynamicClimates[CLIMATE_COUNT] =
{
    [CLIMATE_NONE] =
    {
        .pool = {WEATHER_NONE},
        .storm = WEATHER_NONE,
        .persistence = 16,
    },
    [CLIMATE_TEMPERATE] =
    {
        .pool = {WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY,
                 WEATHER_SUNNY, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS,
                 WEATHER_SUNNY_CLOUDS, WEATHER_SHADE, WEATHER_SHADE, WEATHER_RAIN,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_FOG_HORIZONTAL, WEATHER_DROUGHT},
        .storm = WEATHER_RAIN_THUNDERSTORM,
        .persistence = 9,
    },
    [CLIMATE_FOREST] =
    {
        .pool = {WEATHER_SHADE, WEATHER_SHADE, WEATHER_SHADE, WEATHER_SHADE,
                 WEATHER_SHADE, WEATHER_SHADE, WEATHER_SHADE, WEATHER_SHADE,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_RAIN, WEATHER_FOG_HORIZONTAL,
                 WEATHER_FOG_HORIZONTAL, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_NONE},
        .storm = WEATHER_RAIN,
        .persistence = 11,
    },
    [CLIMATE_COASTAL] =
    {
        .pool = {WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY,
                 WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS,
                 WEATHER_SHADE, WEATHER_SHADE, WEATHER_RAIN, WEATHER_RAIN,
                 WEATHER_RAIN, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL, WEATHER_DROUGHT},
        .storm = WEATHER_RAIN_THUNDERSTORM,
        .persistence = 8,
    },
    [CLIMATE_OPEN_SEA] =
    {
        .pool = {WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY,
                 WEATHER_SUNNY, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_RAIN, WEATHER_DOWNPOUR,
                 WEATHER_RAIN_THUNDERSTORM, WEATHER_FOG_HORIZONTAL, WEATHER_DROUGHT, WEATHER_DROUGHT},
        .storm = WEATHER_DOWNPOUR,
        .persistence = 7,
    },
    [CLIMATE_RAINFOREST] =
    {
        .pool = {WEATHER_RAIN, WEATHER_RAIN, WEATHER_RAIN, WEATHER_RAIN,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_DOWNPOUR, WEATHER_DOWNPOUR,
                 WEATHER_RAIN_THUNDERSTORM, WEATHER_RAIN_THUNDERSTORM, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL,
                 WEATHER_SHADE, WEATHER_SHADE, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY},
        .storm = WEATHER_DOWNPOUR,
        .persistence = 10,
    },
    [CLIMATE_DESERT] =
    {
        .pool = {WEATHER_SANDSTORM, WEATHER_SANDSTORM, WEATHER_SANDSTORM, WEATHER_SANDSTORM,
                 WEATHER_SANDSTORM, WEATHER_SANDSTORM, WEATHER_DROUGHT, WEATHER_DROUGHT,
                 WEATHER_DROUGHT, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY,
                 WEATHER_SUNNY, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SHADE},
        .storm = WEATHER_SANDSTORM,
        .persistence = 10,
    },
    [CLIMATE_VOLCANIC] =
    {
        .pool = {WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH,
                 WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_DROUGHT, WEATHER_DROUGHT,
                 WEATHER_DROUGHT, WEATHER_DROUGHT, WEATHER_SUNNY, WEATHER_SUNNY,
                 WEATHER_SHADE, WEATHER_SHADE, WEATHER_SUNNY_CLOUDS, WEATHER_FOG_HORIZONTAL},
        .storm = WEATHER_VOLCANIC_ASH,
        .persistence = 11,
    },
    [CLIMATE_ASHFALL] =
    {
        .pool = {WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH,
                 WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH, WEATHER_VOLCANIC_ASH,
                 WEATHER_SHADE, WEATHER_SHADE, WEATHER_SHADE, WEATHER_SUNNY_CLOUDS,
                 WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_FOG_HORIZONTAL},
        .storm = WEATHER_VOLCANIC_ASH,
        .persistence = 12,
    },
    [CLIMATE_MOUNTAIN] =
    {
        .pool = {WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY,
                 WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SHADE, WEATHER_SHADE,
                 WEATHER_SHADE, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_DIAGONAL,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_SNOW, WEATHER_SNOW},
        .storm = WEATHER_RAIN_THUNDERSTORM,
        .persistence = 10,
    },
    [CLIMATE_MISTY] =
    {
        .pool = {WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL,
                 WEATHER_FOG_HORIZONTAL, WEATHER_FOG_HORIZONTAL, WEATHER_FOG_DIAGONAL, WEATHER_FOG_DIAGONAL,
                 WEATHER_FOG_DIAGONAL, WEATHER_SHADE, WEATHER_SHADE, WEATHER_SHADE,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_SUNNY_CLOUDS, WEATHER_NONE},
        .storm = WEATHER_RAIN,
        .persistence = 12,
    },
    [CLIMATE_HIGHLAND] =
    {
        .pool = {WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY_CLOUDS,
                 WEATHER_SUNNY_CLOUDS, WEATHER_SUNNY_CLOUDS, WEATHER_SHADE, WEATHER_SHADE,
                 WEATHER_RAIN, WEATHER_RAIN, WEATHER_RAIN_THUNDERSTORM, WEATHER_FOG_HORIZONTAL,
                 WEATHER_FOG_HORIZONTAL, WEATHER_SNOW, WEATHER_DOWNPOUR, WEATHER_DROUGHT},
        .storm = WEATHER_RAIN_THUNDERSTORM,
        .persistence = 9,
    },
};

// ---- Region sections -------------------------------------------------------
// x/y are rough positions on the Hoenn map (0..30 west->east, 0..20 north->
// south); storm fronts sweep across these coordinates.
enum DynamicWeatherSection
{
    WSEC_NONE,
    WSEC_LITTLEROOT,   // Littleroot, Oldale, Routes 101 & 103
    WSEC_PETALBURG,    // Petalburg, Routes 102 & 104
    WSEC_WOODS,        // Petalburg Woods
    WSEC_RUSTBORO,     // Rustboro, Routes 115 & 116
    WSEC_METEOR,       // Route 114, Meteor Falls
    WSEC_ASHLANDS,     // Fallarbor, Route 113
    WSEC_VOLCANO,      // Lavaridge, Route 112, Jagged Pass, Mt. Chimney, Fiery Path
    WSEC_DESERT,       // Route 111
    WSEC_DEWFORD,      // Dewford, Routes 105-107
    WSEC_SLATEPORT,    // Slateport, Routes 108-110
    WSEC_MAUVILLE,     // Mauville, Verdanturf, Routes 117 & 118
    WSEC_RAINFOREST,   // Fortree, Routes 119 & 120
    WSEC_LILYCOVE,     // Lilycove, Routes 121-123, Safari Zone
    WSEC_MT_PYRE,      // Mt. Pyre
    WSEC_MOSSDEEP,     // Mossdeep, Routes 124 & 125
    WSEC_SOOTOPOLIS,   // Sootopolis, Routes 126-128
    WSEC_SOUTH_SEA,    // Pacifidlog, Routes 129-134
    WSEC_EVER_GRANDE,  // Ever Grande, Victory Road
    WSEC_FRONTIER,     // Battle Frontier
    WSEC_COUNT
};

struct DynamicWeatherSectionInfo
{
    u8 climate;
    u8 x;
    u8 y;
};

static const struct DynamicWeatherSectionInfo sDynamicWeatherSections[WSEC_COUNT] =
{
    [WSEC_NONE]        = {CLIMATE_NONE,        0,  0},
    [WSEC_LITTLEROOT]  = {CLIMATE_TEMPERATE,   5, 15},
    [WSEC_PETALBURG]   = {CLIMATE_TEMPERATE,   2, 14},
    [WSEC_WOODS]       = {CLIMATE_FOREST,      1, 11},
    [WSEC_RUSTBORO]    = {CLIMATE_TEMPERATE,   1,  7},
    [WSEC_METEOR]      = {CLIMATE_MOUNTAIN,    3,  3},
    [WSEC_ASHLANDS]    = {CLIMATE_ASHFALL,     6,  2},
    [WSEC_VOLCANO]     = {CLIMATE_VOLCANIC,    8,  6},
    [WSEC_DESERT]      = {CLIMATE_DESERT,     11,  5},
    [WSEC_DEWFORD]     = {CLIMATE_COASTAL,     2, 19},
    [WSEC_SLATEPORT]   = {CLIMATE_COASTAL,    10, 16},
    [WSEC_MAUVILLE]    = {CLIMATE_TEMPERATE,  10, 11},
    [WSEC_RAINFOREST]  = {CLIMATE_RAINFOREST, 14,  5},
    [WSEC_LILYCOVE]    = {CLIMATE_COASTAL,    20,  7},
    [WSEC_MT_PYRE]     = {CLIMATE_MISTY,      19, 11},
    [WSEC_MOSSDEEP]    = {CLIMATE_OPEN_SEA,   25,  8},
    [WSEC_SOOTOPOLIS]  = {CLIMATE_OPEN_SEA,   22, 13},
    [WSEC_SOUTH_SEA]   = {CLIMATE_OPEN_SEA,   15, 18},
    [WSEC_EVER_GRANDE] = {CLIMATE_HIGHLAND,   28, 15},
    [WSEC_FRONTIER]    = {CLIMATE_COASTAL,    18, 19},
};

// v4.5 WEATHER SPREAD = Perfect: neighbouring areas share one weather (9 zones).
// Each area takes the weather (and climate) of its zone's first area; the
// volcano's ash and the desert's sandstorm are still added on top.
static const u8 sWideWeatherZoneLeader[WSEC_COUNT] =
{
    [WSEC_NONE]       = WSEC_NONE,
    [WSEC_LITTLEROOT] = WSEC_LITTLEROOT, [WSEC_PETALBURG]  = WSEC_LITTLEROOT,   // 1
    [WSEC_RUSTBORO]   = WSEC_RUSTBORO,   [WSEC_WOODS]      = WSEC_RUSTBORO,     // 2
    [WSEC_METEOR]     = WSEC_METEOR,     [WSEC_ASHLANDS]   = WSEC_METEOR,       // 3
    [WSEC_DESERT]     = WSEC_DESERT,     [WSEC_VOLCANO]    = WSEC_DESERT,       // 4
    [WSEC_SLATEPORT]  = WSEC_SLATEPORT,  [WSEC_DEWFORD]    = WSEC_SLATEPORT,    // 5
    [WSEC_MAUVILLE]   = WSEC_MAUVILLE,   [WSEC_RAINFOREST] = WSEC_MAUVILLE,     // 6
    [WSEC_LILYCOVE]   = WSEC_LILYCOVE,   [WSEC_MT_PYRE]    = WSEC_LILYCOVE,     // 7
    [WSEC_MOSSDEEP]   = WSEC_MOSSDEEP,   [WSEC_SOOTOPOLIS] = WSEC_MOSSDEEP,     // 8
    [WSEC_EVER_GRANDE]= WSEC_MOSSDEEP,
    [WSEC_SOUTH_SEA]  = WSEC_SOUTH_SEA,  [WSEC_FRONTIER]   = WSEC_SOUTH_SEA,    // 9
};

static u8 GetDynamicWeatherSection(u8 mapSec)
{
    switch (mapSec)
    {
    case MAPSEC_LITTLEROOT_TOWN: case MAPSEC_OLDALE_TOWN:
    case MAPSEC_ROUTE_101: case MAPSEC_ROUTE_103:
        return WSEC_LITTLEROOT;
    case MAPSEC_PETALBURG_CITY: case MAPSEC_ROUTE_102: case MAPSEC_ROUTE_104:
        return WSEC_PETALBURG;
    case MAPSEC_PETALBURG_WOODS:
        return WSEC_WOODS;
    case MAPSEC_RUSTBORO_CITY: case MAPSEC_ROUTE_115: case MAPSEC_ROUTE_116:
        return WSEC_RUSTBORO;
    case MAPSEC_ROUTE_114: case MAPSEC_METEOR_FALLS:
        return WSEC_METEOR;
    case MAPSEC_FALLARBOR_TOWN: case MAPSEC_ROUTE_113:
        return WSEC_ASHLANDS;
    case MAPSEC_LAVARIDGE_TOWN: case MAPSEC_ROUTE_112: case MAPSEC_JAGGED_PASS:
    case MAPSEC_MT_CHIMNEY: case MAPSEC_FIERY_PATH:
        return WSEC_VOLCANO;
    case MAPSEC_ROUTE_111:
        return WSEC_DESERT;
    case MAPSEC_DEWFORD_TOWN: case MAPSEC_ROUTE_105: case MAPSEC_ROUTE_106: case MAPSEC_ROUTE_107:
        return WSEC_DEWFORD;
    case MAPSEC_SLATEPORT_CITY: case MAPSEC_ROUTE_108: case MAPSEC_ROUTE_109: case MAPSEC_ROUTE_110:
        return WSEC_SLATEPORT;
    case MAPSEC_MAUVILLE_CITY: case MAPSEC_VERDANTURF_TOWN: case MAPSEC_ROUTE_117: case MAPSEC_ROUTE_118:
        return WSEC_MAUVILLE;
    case MAPSEC_FORTREE_CITY: case MAPSEC_ROUTE_119: case MAPSEC_ROUTE_120:
        return WSEC_RAINFOREST;
    case MAPSEC_LILYCOVE_CITY: case MAPSEC_ROUTE_121: case MAPSEC_ROUTE_122:
    case MAPSEC_ROUTE_123: case MAPSEC_SAFARI_ZONE:
        return WSEC_LILYCOVE;
    case MAPSEC_MT_PYRE:
        return WSEC_MT_PYRE;
    case MAPSEC_MOSSDEEP_CITY: case MAPSEC_ROUTE_124: case MAPSEC_ROUTE_125:
        return WSEC_MOSSDEEP;
    case MAPSEC_SOOTOPOLIS_CITY: case MAPSEC_ROUTE_126: case MAPSEC_ROUTE_127: case MAPSEC_ROUTE_128:
        return WSEC_SOOTOPOLIS;
    case MAPSEC_PACIFIDLOG_TOWN: case MAPSEC_ROUTE_129: case MAPSEC_ROUTE_130: case MAPSEC_ROUTE_131:
    case MAPSEC_ROUTE_132: case MAPSEC_ROUTE_133: case MAPSEC_ROUTE_134:
        return WSEC_SOUTH_SEA;
    case MAPSEC_EVER_GRANDE_CITY: case MAPSEC_VICTORY_ROAD:
        return WSEC_EVER_GRANDE;
    case MAPSEC_BATTLE_FRONTIER:
        return WSEC_FRONTIER;
    default:
        return WSEC_NONE;
    }
}

// 32-bit avalanche mix (Wang/Jenkins style). Pure function.
static u32 DynamicWeatherHash(u32 x)
{
    x += 0x9E3779B9u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

// Per-save seed so two players on the same day get different weather.
static u32 GetDynamicWeatherSaveSeed(void)
{
    u32 seed = 0;
    u8 i;

    for (i = 0; i < TRAINER_ID_LENGTH; i++)
        seed = (seed << 8) | gSaveBlock2Ptr->playerTrainerId[i];

    return seed;
}

// The "daily seed" (expansion keeps one in the save; this derives it instead).
static u32 GetDynamicWeatherDailySeed(s32 day)
{
    return DynamicWeatherHash((u32)day * 2654435761u ^ DynamicWeatherHash(GetDynamicWeatherSaveSeed()));
}

enum
{
    DYN_PERIOD_MORNING, // 05-10
    DYN_PERIOD_DAY,     // 10-17
    DYN_PERIOD_EVENING, // 17-21
    DYN_PERIOD_NIGHT,   // 21-05 (00-05 counts as the previous day's night)
    DYN_PERIOD_COUNT
};

// v4.5 WEATHER SPREAD: Normal = the table above; Extended / Perfect = five periods
// starting at 00:00, 05:00, 10:00, 14:00 and 18:00 (numbered 0-4 in that order).
enum { DYN5_NIGHT, DYN5_MORNING, DYN5_DAY, DYN5_AFTERNOON, DYN5_EVENING, DYN5_COUNT };

static bool8 UseNewWeatherTimes(void)
{
    return FlagGet(FLAG_WEATHER_NEW_TIMES);
}

static u8 GetDynamicPeriodCount(void)
{
    return UseNewWeatherTimes() ? DYN5_COUNT : DYN_PERIOD_COUNT;
}

static u8 GetDynamicDayPeriod(void)
{
    return UseNewWeatherTimes() ? DYN5_DAY : DYN_PERIOD_DAY;
}

// day / period of "now" (in the old table, 00-05 still belongs to last night)
static void GetDynamicDayAndPeriod(s32 *day, u8 *period);

static u8 GetDynamicWeatherPeriod(s32 hours)
{
    if (UseNewWeatherTimes())
    {
        if (hours < 5)  return DYN5_NIGHT;
        if (hours < 10) return DYN5_MORNING;
        if (hours < 14) return DYN5_DAY;
        if (hours < 18) return DYN5_AFTERNOON;
        return DYN5_EVENING;
    }
    if (hours >= 5 && hours < 10)
        return DYN_PERIOD_MORNING;
    if (hours >= 10 && hours < 17)
        return DYN_PERIOD_DAY;
    if (hours >= 17 && hours < 21)
        return DYN_PERIOD_EVENING;
    return DYN_PERIOD_NIGHT;
}

static u8 RollClimatePool(u8 climate, u32 dailySeed, u8 section, u8 period)
{
    u32 h = DynamicWeatherHash(dailySeed ^ (section * 0x85EBCA6Bu) ^ (period * 0xC2B2AE35u));
    return sDynamicClimates[climate].pool[h & 15];
}

// Storm front: ~5 days in 16 a front enters from the west and sweeps east
// across the four periods, soaking every section it passes over.
static bool8 IsSectionUnderStormFront(u8 section, u32 dailySeed, u8 period)
{
    u32 h = DynamicWeatherHash(dailySeed ^ 0x5F356495u);
    s32 frontX, frontY, dx, dy;

    if (((h >> 4) & 15) >= 5)
        return FALSE;

    frontX = (s32)(h & 7) + period * 32 / GetDynamicPeriodCount();   // crosses Hoenn over the day
    frontY = (s32)((h >> 8) % 21);              // anywhere north..south
    dx = (s32)sDynamicWeatherSections[section].x - frontX;
    dy = (s32)sDynamicWeatherSections[section].y - frontY;
    if (dx < -5 || dx > 5 || dy < -9 || dy > 9)
        return FALSE;

    // ragged edges: not every section under the front gets hit
    return (DynamicWeatherHash(dailySeed ^ (section * 0x27D4EB2Fu) ^ period) & 3) != 0;
}

static u8 ApplyDynamicNightRules(u8 weather, u8 period)
{
    if (UseNewWeatherTimes())
    {
        // harsh sun only 10:00-18:00 (and 12-16 at most, below); evening sun, night clear
        if (weather == WEATHER_DROUGHT && period == DYN5_NIGHT)
            return WEATHER_NONE;
        if (weather == WEATHER_DROUGHT && period != DYN5_DAY && period != DYN5_AFTERNOON)
            return WEATHER_SUNNY;
        return weather;
    }
    // v3.0b: harsh sun only by day - dusk turns it to plain sun, night to clear
    if (weather == WEATHER_DROUGHT && period == DYN_PERIOD_NIGHT)
        return WEATHER_NONE;
    if (weather == WEATHER_DROUGHT && period != DYN_PERIOD_DAY)
        return WEATHER_SUNNY;
    return weather;
}

// v3.0b: harsh sun peaks for at most four hours (12:00-15:59) inside the day
// period, so no area sits in a drought for most of the day.
static u8 LimitDroughtHours(u8 weather)
{
    if (weather == WEATHER_DROUGHT && (gLocalTime.hours < 12 || gLocalTime.hours >= 16))
        return WEATHER_SUNNY;
    return weather;
}

static u8 GetDynamicWeatherForSection(u8 section, s32 day, u8 period)
{
    const struct DynamicClimateInfo *climate;
    u32 dailySeed;
    u8 weather, p;

    if (section >= WSEC_COUNT || section == WSEC_NONE)
        return WEATHER_NONE;
    if (FlagGet(FLAG_WEATHER_WIDE_ZONES))
        section = sWideWeatherZoneLeader[section];   // v4.5 Perfect: one weather for both areas

    climate = &sDynamicClimates[sDynamicWeatherSections[section].climate];
    dailySeed = GetDynamicWeatherDailySeed(day);

    // Walk the day forward so each period can inherit the previous one.
    weather = RollClimatePool(sDynamicWeatherSections[section].climate, dailySeed, section, 0);
    for (p = 1; p <= period; p++)
    {
        u32 h = DynamicWeatherHash(dailySeed ^ (section * 0x9E3779B1u) ^ (p * 0x7FEB352Du));
        if ((h & 15) >= climate->persistence)
            weather = RollClimatePool(sDynamicWeatherSections[section].climate, dailySeed, section, p);
    }

    if (IsSectionUnderStormFront(section, dailySeed, period))
        weather = climate->storm;

    return ApplyDynamicNightRules(weather, period);
}

// ---------------------------------------------------------------------------
// Zone weather = the section's climate roll, plus:
//  * constant volcanic ash around the volcano (Lavaridge, Route 112, Jagged
//    Pass, Mt. Chimney area, Fallarbor, Route 113);
//  * dawn mist (04:30-06:30) in climates that get mist.
// ---------------------------------------------------------------------------
static bool8 IsMistCapableClimate(u8 climate)
{
    return climate != CLIMATE_NONE && climate != CLIMATE_DESERT
        && climate != CLIMATE_VOLCANIC && climate != CLIMATE_ASHFALL;
}

#define NO_DAWN (-0x7FFF)

static u8 GetZoneWeather(u8 section, s32 day, u8 period, s32 dawnDay)
{
    u8 climate, weather;

    if (section == WSEC_NONE || section >= WSEC_COUNT)
        return WEATHER_NONE;
    climate = sDynamicWeatherSections[section].climate;
    if (climate == CLIMATE_VOLCANIC || climate == CLIMATE_ASHFALL)
        return WEATHER_VOLCANIC_ASH;

    weather = GetDynamicWeatherForSection(section, day, period);
    if (dawnDay != NO_DAWN && IsMistCapableClimate(climate)
     && (weather == WEATHER_NONE || weather == WEATHER_SUNNY || weather == WEATHER_SUNNY_CLOUDS || weather == WEATHER_SHADE)
     && DynamicWeatherHash(GetDynamicWeatherDailySeed(dawnDay) ^ (section * 0x632BE5ABu)) % 100 < 65)
        weather = WEATHER_MIST;
    return weather;
}

u8 GetDynamicWeatherForMapSec(u8 mapSec, s32 day, u8 period)
{
    return GetZoneWeather(GetDynamicWeatherSection(mapSec), day, period, NO_DAWN);
}

// ---------------------------------------------------------------------------
// Which weathers can sit next to each other / lead into one another.
//   drought - sunny - cloudy - overcast(shade) - rain - downpour - thunderstorm
//   rain - thunderstorm, fog - mist - snow, sandstorm - drought/sunny
// ---------------------------------------------------------------------------
static const u8 sWeatherNeighbors[WEATHER_MIST + 1][5] =
{   //                            count, neighbours...
    [WEATHER_NONE]              = {2, WEATHER_SUNNY, WEATHER_SUNNY_CLOUDS},
    [WEATHER_SUNNY_CLOUDS]      = {3, WEATHER_SUNNY, WEATHER_SHADE, WEATHER_MIST},
    [WEATHER_SUNNY]             = {3, WEATHER_SUNNY_CLOUDS, WEATHER_SHADE, WEATHER_DROUGHT},
    [WEATHER_RAIN]              = {3, WEATHER_SHADE, WEATHER_DOWNPOUR, WEATHER_RAIN_THUNDERSTORM},
    [WEATHER_SNOW]              = {2, WEATHER_MIST, WEATHER_SHADE},
    [WEATHER_RAIN_THUNDERSTORM] = {2, WEATHER_RAIN, WEATHER_DOWNPOUR},
    [WEATHER_FOG_HORIZONTAL]    = {3, WEATHER_MIST, WEATHER_SHADE, WEATHER_FOG_DIAGONAL},
    [WEATHER_VOLCANIC_ASH]      = {0},
    [WEATHER_SANDSTORM]         = {2, WEATHER_DROUGHT, WEATHER_SUNNY},
    [WEATHER_FOG_DIAGONAL]      = {2, WEATHER_FOG_HORIZONTAL, WEATHER_MIST},
    [WEATHER_UNDERWATER]        = {0},
    [WEATHER_SHADE]             = {4, WEATHER_SUNNY_CLOUDS, WEATHER_RAIN, WEATHER_FOG_HORIZONTAL, WEATHER_MIST},
    [WEATHER_DROUGHT]           = {2, WEATHER_SUNNY, WEATHER_SANDSTORM},
    [WEATHER_DOWNPOUR]          = {2, WEATHER_RAIN, WEATHER_RAIN_THUNDERSTORM},
    [WEATHER_UNDERWATER_BUBBLES]= {0},
    [WEATHER_ABNORMAL]          = {0},
    [WEATHER_MIST]              = {4, WEATHER_FOG_HORIZONTAL, WEATHER_SNOW, WEATHER_SUNNY_CLOUDS, WEATHER_SHADE},
};

static bool8 AreWeathersNeighbors(u8 a, u8 b)
{
    u32 i;

    if (a == b)
        return TRUE;
    if (a > WEATHER_MIST)
        return FALSE;
    for (i = 0; i < sWeatherNeighbors[a][0]; i++)
    {
        if (sWeatherNeighbors[a][1 + i] == b)
            return TRUE;
    }
    return FALSE;
}

// Location logic: a place only gets weather its climate actually produces
// (sandstorm only around the desert, snow only in the mountains, ...).
static bool8 IsWeatherPlausibleInClimate(u8 weather, u8 climate)
{
    u32 i;

    if (weather == WEATHER_MIST)
        return IsMistCapableClimate(climate);
    if (weather == WEATHER_SUNNY || weather == WEATHER_SUNNY_CLOUDS || weather == WEATHER_SHADE)
        return TRUE;
    if (sDynamicClimates[climate].storm == weather)
        return TRUE;
    for (i = 0; i < ARRAY_COUNT(sDynamicClimates[climate].pool); i++)
    {
        if (sDynamicClimates[climate].pool[i] == weather)
            return TRUE;
    }
    return FALSE;
}

// A weather that differs from `anchor` but borders it, fits the climate and,
// if possible, also borders the weather on the other side (`second`).
static u8 PickNeighborWeather(u8 anchor, u8 second, u8 climate, u32 hash)
{
    u8 fit[4], both[4];
    u32 i, nFit = 0, nBoth = 0, count;

    if (anchor > WEATHER_MIST || (count = sWeatherNeighbors[anchor][0]) == 0)
        return anchor;
    for (i = 0; i < count; i++)
    {
        u8 w = sWeatherNeighbors[anchor][1 + i];
        if (!IsWeatherPlausibleInClimate(w, climate))
            continue;
        fit[nFit++] = w;
        if (second == 0xFF || AreWeathersNeighbors(second, w))
            both[nBoth++] = w;
    }
    if (nBoth != 0)
        return both[hash % nBoth];
    if (nFit != 0)
        return fit[hash % nFit];
    return sWeatherNeighbors[anchor][1 + hash % count];
}

enum
{
    DWROLE_SHARE,      // same weather as its city / zone
    DWROLE_NEIGHBOR,   // a city's other exit: different but bordering weather
    DWROLE_OWN_AVOID,  // exit into another terrain zone: own zone weather, never the city's
    DWROLE_COPY,       // route next to a same-terrain route: same weather as it
};
#define DWFLAG_DESERT_SPILL (1 << 0) // next to the Route 111 desert

struct DynamicWeatherMapRole
{
    u16 mapId;
    u8 role;
    u8 section;
    u8 anchorSection;
    u8 secondSection;
    u8 copyIndex;
    u8 flags;
};

#include "data/dynamic_weather_map_roles.h"

static u8 GetMapDynamicWeatherByIndex(u32 index, s32 day, u8 period, s32 dawnDay, u8 depth)
{
    const struct DynamicWeatherMapRole *role = &sDynamicWeatherMapRoles[index];
    u8 climate = sDynamicWeatherSections[role->section].climate;
    u32 hash = DynamicWeatherHash(GetDynamicWeatherDailySeed(day) ^ (role->mapId * 0x2545F491u) ^ (period * 0x9E3779B9u));
    u8 weather, anchor;

    switch (role->role)
    {
    default:
    case DWROLE_SHARE:
        weather = GetZoneWeather(role->anchorSection, day, period, dawnDay);
        break;
    case DWROLE_NEIGHBOR:
        anchor = GetZoneWeather(role->anchorSection, day, period, dawnDay);
        weather = PickNeighborWeather(anchor,
                                      role->secondSection != WSEC_NONE ? GetZoneWeather(role->secondSection, day, period, dawnDay) : 0xFF,
                                      climate, hash);
        break;
    case DWROLE_OWN_AVOID:
        weather = GetZoneWeather(role->section, day, period, dawnDay);
        anchor = GetZoneWeather(role->anchorSection, day, period, dawnDay);
        if (weather == anchor)
            weather = PickNeighborWeather(weather, anchor, climate, hash);
        break;
    case DWROLE_COPY:
        if (depth < 4 && role->copyIndex < ARRAY_COUNT(sDynamicWeatherMapRoles))
            return GetMapDynamicWeatherByIndex(role->copyIndex, day, period, dawnDay, depth + 1);
        weather = GetZoneWeather(role->section, day, period, dawnDay);
        break;
    }

    // A desert sandstorm spills into the maps right next to Route 111, but
    // 25% less often than in the desert itself.
    if ((role->flags & DWFLAG_DESERT_SPILL)
     && GetZoneWeather(WSEC_DESERT, day, period, dawnDay) == WEATHER_SANDSTORM
     && ((hash >> 8) & 3) != 0)
        weather = WEATHER_SANDSTORM;

    return ApplyDynamicNightRules(weather, period);
}

static void GetDynamicDayAndPeriod(s32 *day, u8 *period)
{
    *day = gLocalTime.days;
    *period = GetDynamicWeatherPeriod(gLocalTime.hours);
    if (!UseNewWeatherTimes() && *period == DYN_PERIOD_NIGHT && gLocalTime.hours < 5)
        (*day)--; // old table: after midnight is still "last night"
}

// Forecast helper for scripts / TV / Weather Institute: the daytime weather
// for a map section `dayOffset` days from today.
u8 GetDynamicWeatherForecast(u8 mapSec, s32 dayOffset)
{
    RtcCalcLocalTime();
    return GetDynamicWeatherForMapSec(mapSec, gLocalTime.days + dayOffset, GetDynamicDayPeriod());
}

static EWRAM_DATA u8 sLastDynamicWeather = WEATHER_NONE;

static EWRAM_DATA u32 sDynWeatherClockFrame = 0;
static EWRAM_DATA bool8 sDynWeatherClockValid = FALSE;

static u8 GetDynamicWeatherRaw(void);

// v6.3: the cloud shadows (sunny with clouds) never come at night - from 18:00
// until dawn (05:00) the sky is clear instead
u8 GetDynamicWeather(void)
{
    u8 weather = GetDynamicWeatherRaw();
    if (weather == WEATHER_SUNNY_CLOUDS && (gLocalTime.hours >= 18 || gLocalTime.hours < 5))
        weather = WEATHER_NONE;
    sLastDynamicWeather = weather;
    return weather;
}

static u8 GetDynamicWeatherRaw(void)
{
    s32 day;
    u8 period;

    // v2.2: read the clock at most once every half second here; map loads call this
    // right after the game's own time events already read it (a clock read
    // keeps interrupts off on real hardware).
    if ((u32)(gMain.vblankCounter1 - sDynWeatherClockFrame) > 30 || !sDynWeatherClockValid)
    {
        RtcCalcLocalTime();
        sDynWeatherClockFrame = gMain.vblankCounter1;
        sDynWeatherClockValid = TRUE;
    }
    GetDynamicDayAndPeriod(&day, &period);

    {
        u16 mapId = (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
        s32 dawnDay = NO_DAWN;
        u32 i;

        // Dawn mist window: 04:30 - 06:30.
        if ((gLocalTime.hours == 4 && gLocalTime.minutes >= 30) || gLocalTime.hours == 5
         || (gLocalTime.hours == 6 && gLocalTime.minutes < 30))
            dawnDay = gLocalTime.days;

        for (i = 0; i < ARRAY_COUNT(sDynamicWeatherMapRoles); i++)
        {
            if (sDynamicWeatherMapRoles[i].mapId == mapId)
            {
                sLastDynamicWeather = LimitDroughtHours(GetMapDynamicWeatherByIndex(i, day, period, dawnDay, 0));
                return sLastDynamicWeather;
            }
        }
        sLastDynamicWeather = LimitDroughtHours(GetZoneWeather(GetDynamicWeatherSection(gMapHeader.regionMapSectionId), day, period, dawnDay));
    }
    return sLastDynamicWeather;
}

// Called periodically from the overworld. If the current map uses dynamic
// weather and it is still in control, move to the new period's weather with
// the normal smooth weather transition.
void TryRefreshDynamicWeather(void)
{
    if (FlagGet(FLAG_WEATHER_SCRIPTED))
        return;   // v4.0 WEATHER SYSTEM = Scripted
    if (gMapHeader.weather != WEATHER_DYNAMIC || !IsDynamicWeatherInControl())
        return; // a script, cutscene or local weather zone owns the weather
    if (ArePlayerFieldControlsLocked())
        return;

    if (GetDynamicWeather() != GetSavedWeather())
        SetWeather(WEATHER_DYNAMIC);
}

// Called on "Continue" before the map's weather is started. The saved weather
// can be hours old; if the dynamic system owns this map, jump straight to the
// current period's weather (no transition). Script weather is left alone.
void UpdateDynamicWeatherOnContinue(void)
{
    if (gMapHeader.weather != WEATHER_DYNAMIC)
        return;
    if (gSaveBlock1Ptr->weather == WEATHER_ABNORMAL)
        SetDynamicWeatherInControl(FALSE); // saves from before the flag existed
    if (!IsDynamicWeatherInControl())
        return;
    gSaveBlock1Ptr->weather = GetDynamicWeather();
}

static u8 TranslateWeatherNum(u8 weather)
{
    switch (weather)
    {
    case WEATHER_NONE:               return WEATHER_NONE;
    case WEATHER_SUNNY_CLOUDS:       return WEATHER_SUNNY_CLOUDS;
    case WEATHER_SUNNY:              return WEATHER_SUNNY;
    case WEATHER_RAIN:               return WEATHER_RAIN;
    case WEATHER_SNOW:               return WEATHER_SNOW;
    case WEATHER_RAIN_THUNDERSTORM:  return WEATHER_RAIN_THUNDERSTORM;
    case WEATHER_FOG_HORIZONTAL:     return WEATHER_FOG_HORIZONTAL;
    case WEATHER_VOLCANIC_ASH:       return WEATHER_VOLCANIC_ASH;
    case WEATHER_SANDSTORM:          return WEATHER_SANDSTORM;
    case WEATHER_FOG_DIAGONAL:       return WEATHER_FOG_DIAGONAL;
    case WEATHER_UNDERWATER:         return WEATHER_UNDERWATER;
    case WEATHER_SHADE:              return WEATHER_SHADE;
    case WEATHER_DROUGHT:            return WEATHER_DROUGHT;
    case WEATHER_DOWNPOUR:           return WEATHER_DOWNPOUR;
    case WEATHER_UNDERWATER_BUBBLES: return WEATHER_UNDERWATER_BUBBLES;
    case WEATHER_ABNORMAL:           return WEATHER_ABNORMAL;
    case WEATHER_MIST:               return WEATHER_MIST;
    case WEATHER_ROUTE119_CYCLE:     return sWeatherCycleRoute119[gSaveBlock1Ptr->weatherCycleStage];
    case WEATHER_ROUTE123_CYCLE:     return sWeatherCycleRoute123[gSaveBlock1Ptr->weatherCycleStage];
    case WEATHER_DYNAMIC:            return GetDynamicWeather();
    default:                         return WEATHER_NONE;
    }
}

void UpdateWeatherPerDay(u16 increment)
{
    u16 weatherStage = gSaveBlock1Ptr->weatherCycleStage + increment;
    weatherStage %= WEATHER_CYCLE_LENGTH;
    gSaveBlock1Ptr->weatherCycleStage = weatherStage;
}

static void UpdateRainCounter(u8 newWeather, u8 oldWeather)
{
    if (newWeather != oldWeather
     && (newWeather == WEATHER_RAIN || newWeather == WEATHER_RAIN_THUNDERSTORM))
        IncrementGameStat(GAME_STAT_GOT_RAINED_ON);
}

// ============================================================================
// Rain puddles (v1.5)
// ----------------------------------------------------------------------------
// Sand-rimmed puddles of 1 to 7 tiles on open short grass (routes, towns and
// cities using the General tileset); bigger ones are rarer. Real MB_PUDDLE
// metatiles (reflections + splashes); frozen ones are MB_ICE (reflective and
// slippery).
//
// Life cycle (per map, from its weather now and in the last two periods):
//   raining (rain / thunderstorm / downpour)   full puddles, 1-7 tiles
//                                              (downpour 15% more)
//   rain -> cloudy / overcast, mist or fog     20% fewer, max 4 tiles
//   rain -> snow                               frozen, 20% fewer, max 3 tiles
//   rain -> snow -> sunny                      thawed back to water, 20% fewer
//   anything else (no rain again)              gone
// A fresh random set is spawned every time a map is entered; while you stay,
// the existing puddles shrink / freeze / thaw / dry up as the weather moves on.
// Puddles never spawn on people, items, Cut trees, Smash rocks, signs,
// hidden items or warps, and never touch each other or a pond.
// ============================================================================
#include "data/rain_puddle_shapes.h"

// Chance per ground tile, out of 10,000 (v2.3: 5% fewer than v2.2's 4.9%).
#define PUDDLE_RAIN_PERMILLE      419    // v6.0: 10% fewer (was 466)
#define PUDDLE_DOWNPOUR_PERMILLE  482    // 15% more than rain (v6.0: 10% fewer, was 536)
#define MAX_TRACKED_PUDDLES       45    // v3.5: at most 45 per map (RAM)
#define PUDDLE_ANIM_FRAMES        20    // one puddle appears / goes / changes every 20 frames

enum
{
    PUDDLES_NONE,
    PUDDLES_RAIN,
    PUDDLES_DAMP,   // after rain: cloudy, mist or fog
    PUDDLES_ICE,    // after rain: snow
    PUDDLES_THAW,   // rain -> snow -> sunny
    PUDDLES_SNOW,   // v3.0b: snowing -> snow piles on the grass
};

// v3.5: changes are queued and done one puddle at a time (see UpdatePuddleAnimations)
enum { POP_NONE, POP_DRAW, POP_ERASE, POP_REDRAW };

struct PlacedPuddle
{
    u8 x, y;        // map coordinates (maps are narrower than 256 tiles); u8 keeps EWRAM free
    u8 shape;       // index into sRainPuddleShapes (as drawn)
    u8 set;         // index into sPuddleGroundSets (which ground it sits on)
    u8 ice:1;
    u8 snow:1;      // v3.0b snow pile
    u8 drawn:1;     // on the map right now
    u8 op:2;        // pending change
    u8 newShape:3;  // shape after a POP_REDRAW
};

static EWRAM_DATA struct
{
    u16 mapId;
    u8 stage;
    u8 pendingStage;   // v4.0: applied 10 s after the weather changed
    u8 count;
    u16 pendingTimer;
    u32 rng;
    struct PlacedPuddle list[MAX_TRACKED_PUDDLES];
} sPuddles = {0};

// Chance of each size once a puddle forms (1 ... 4 tiles), out of the sum.
// v2.4: no 3-tile puddles.
static const u8 sRainPuddleSizeWeights[4] = {30, 22, 0, 12};

static u32 PuddleRandom(void)
{
    sPuddles.rng = sPuddles.rng * 1103515245 + 12345;
    return sPuddles.rng >> 8;
}

static bool8 IsRainWeather(u8 w)
{
    return w == WEATHER_RAIN || w == WEATHER_RAIN_THUNDERSTORM || w == WEATHER_DOWNPOUR;
}

static bool8 IsDampWeather(u8 w)
{
    return w == WEATHER_SUNNY_CLOUDS || w == WEATHER_SHADE || w == WEATHER_MIST
        || w == WEATHER_FOG_HORIZONTAL || w == WEATHER_FOG_DIAGONAL;
}

static u16 GetCurrentMapId(void)
{
    return (gSaveBlock1Ptr->location.mapGroup << 8) | gSaveBlock1Ptr->location.mapNum;
}

// Dynamic weather of the current map `periodsAgo` periods before now.
static u8 GetCurrentMapWeatherPeriodsAgo(u32 periodsAgo)
{
    s32 slot, day;
    u8 period;
    u16 mapId = GetCurrentMapId();
    u32 i;

    // No clock read here: the weather refresh / map load that runs just before
    // has already read it (clock reads turn interrupts off on real hardware).
    GetDynamicDayAndPeriod(&day, &period);
    slot = day * GetDynamicPeriodCount() + period - (s32)periodsAgo;
    day = slot / GetDynamicPeriodCount();
    period = slot % GetDynamicPeriodCount();
    for (i = 0; i < ARRAY_COUNT(sDynamicWeatherMapRoles); i++)
        if (sDynamicWeatherMapRoles[i].mapId == mapId)
            return GetMapDynamicWeatherByIndex(i, day, period, NO_DAWN, 0);
    return GetZoneWeather(GetDynamicWeatherSection(gMapHeader.regionMapSectionId), day, period, NO_DAWN);
}

// Which puddle stage the current map is in, and the rain rate it came from.
static u8 GetPuddleStage(u32 *permille)
{
    u8 w0 = GetSavedWeather(), w1, w2;

    *permille = (w0 == WEATHER_DOWNPOUR) ? PUDDLE_DOWNPOUR_PERMILLE : PUDDLE_RAIN_PERMILLE;
    if (IsRainWeather(w0))
        return PUDDLES_RAIN;
    // v3.0b: snow piles up on the grass (unless it fell on puddles: those freeze)
    if (w0 == WEATHER_SNOW && (gMapHeader.weather != WEATHER_DYNAMIC || FlagGet(FLAG_DYNAMIC_WEATHER_OVERRIDDEN) || FlagGet(FLAG_WEATHER_SCRIPTED)
                               || !IsRainWeather(GetCurrentMapWeatherPeriodsAgo(1))))
        return PUDDLES_SNOW;
    if (gMapHeader.weather != WEATHER_DYNAMIC || FlagGet(FLAG_DYNAMIC_WEATHER_OVERRIDDEN) || FlagGet(FLAG_WEATHER_SCRIPTED))
        return PUDDLES_NONE;   // scripted / fixed weather: puddles only while it rains

    w1 = GetCurrentMapWeatherPeriodsAgo(1);
    if (IsRainWeather(w1))
    {
        *permille = ((w1 == WEATHER_DOWNPOUR) ? PUDDLE_DOWNPOUR_PERMILLE : PUDDLE_RAIN_PERMILLE) * 8 / 10;
        if (IsDampWeather(w0))
            return PUDDLES_DAMP;
        if (w0 == WEATHER_SNOW)
            return PUDDLES_ICE;
        return PUDDLES_NONE;
    }
    if (w0 == WEATHER_SUNNY && w1 == WEATHER_SNOW)
    {
        w2 = GetCurrentMapWeatherPeriodsAgo(2);
        if (IsRainWeather(w2))
        {
            *permille = ((w2 == WEATHER_DOWNPOUR) ? PUDDLE_DOWNPOUR_PERMILLE : PUDDLE_RAIN_PERMILLE) * 64 / 100;
            return PUDDLES_THAW;
        }
    }
    return PUDDLES_NONE;
}

static u8 GetStageMaxSize(u8 stage)
{
    return (stage == PUDDLES_ICE || stage == PUDDLES_THAW) ? 3 : 4;
}

// Which ground piece set (short grass, or this city's pavement) a tile is on.
#define NO_PUDDLE_SET 0xFF
static u8 GetPuddleGroundSetAt(s32 x, s32 y)
{
    u16 metatile = MapGridGetMetatileIdAt(x, y);
    const struct MapLayout *layout = gMapHeader.mapLayout;
    u32 i;

    if (MapGridGetCollisionAt(x, y) != 0)
        return NO_PUDDLE_SET;
    for (i = 0; i < ARRAY_COUNT(sPuddleGroundSets); i++)
    {
        const struct PuddleGroundSet *set = &sPuddleGroundSets[i];
        if (metatile != set->ground)
            continue;
        if (set->primary ? layout->primaryTileset == set->tileset : layout->secondaryTileset == set->tileset)
            return i;
    }
    return NO_PUDDLE_SET;
}

static bool8 IsPuddleNearby(s32 x, s32 y)
{
    s32 dx, dy;

    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
        {
            u8 b = MapGridGetMetatileBehaviorAt(x + dx, y + dy);
            u16 m = MapGridGetMetatileIdAt(x + dx, y + dy);
            u32 k;
            if (MetatileBehavior_IsPuddle(b) || b == MB_ICE)
                return TRUE;
            for (k = 0; k < PP_COUNT; k++)
                if (sSnowPileMetatiles[k] != 0 && m == sSnowPileMetatiles[k])
                    return TRUE;
        }
    return FALSE;
}

// People, items, Cut trees, Smash rocks, boulders, signs, hidden items, warps.
static bool8 IsPuddleSpotTaken(s32 x, s32 y)
{
    const struct MapEvents *events = gMapHeader.events;
    u32 i;

    if (events != NULL)
    {
        for (i = 0; i < events->objectEventCount; i++)
            if (events->objectEvents[i].x + MAP_OFFSET == x && events->objectEvents[i].y + MAP_OFFSET == y)
                return TRUE;
        for (i = 0; i < events->bgEventCount; i++)
            if (events->bgEvents[i].x + MAP_OFFSET == x && events->bgEvents[i].y + MAP_OFFSET == y)
                return TRUE;
        for (i = 0; i < events->warpCount; i++)
            if (events->warps[i].x + MAP_OFFSET == x && events->warps[i].y + MAP_OFFSET == y)
                return TRUE;
    }
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        if (gObjectEvents[i].active && gObjectEvents[i].currentCoords.x == x && gObjectEvents[i].currentCoords.y == y)
            return TRUE;
    return FALSE;
}

static void DrawPlacedPuddle(const struct PlacedPuddle *p, bool8 redraw)
{
    const struct RainPuddleShape *shape = &sRainPuddleShapes[p->shape];
    const struct PuddleGroundSet *set = &sPuddleGroundSets[p->set];
    u32 i;

    for (i = 0; i < shape->count; i++)
    {
        u16 metatile = p->snow ? sSnowPileMetatiles[shape->pieces[i]]
                     : p->ice ? set->ice[shape->pieces[i]] : set->water[shape->pieces[i]];
        MapGridSetMetatileIdAt(p->x + shape->cells[i][0], p->y + shape->cells[i][1], metatile);
        if (redraw)
            CurrentMapDrawMetatileAt(p->x + shape->cells[i][0], p->y + shape->cells[i][1]);
    }
}

static void ErasePlacedPuddle(const struct PlacedPuddle *p, bool8 redraw)
{
    const struct RainPuddleShape *shape = &sRainPuddleShapes[p->shape];
    u32 i;

    for (i = 0; i < shape->count; i++)
    {
        MapGridSetMetatileIdAt(p->x + shape->cells[i][0], p->y + shape->cells[i][1], sPuddleGroundSets[p->set].ground);
        if (redraw)
            CurrentMapDrawMetatileAt(p->x + shape->cells[i][0], p->y + shape->cells[i][1]);
    }
}

static bool8 TryPlaceRainPuddleShape(u8 shapeId, bool8 ice, s32 x, s32 y, u8 elevation, u8 set, bool8 snow)
{
    struct PlacedPuddle p = {x, y, shapeId, set, ice, snow, TRUE, POP_NONE, shapeId};
    const struct RainPuddleShape *shape = &sRainPuddleShapes[shapeId];
    u32 i;

    if (sPuddles.count >= MAX_TRACKED_PUDDLES)
        return FALSE;   // v3.5: never more than 45 on a map
    for (i = 0; i < shape->count; i++)
    {
        s32 cx = x + shape->cells[i][0], cy = y + shape->cells[i][1];
        // v5.6: outside the map the game reads the border block (often grass), so a
        // puddle at the edge passed the checks and only its inside half was drawn.
        // Keep every cell inside the map, one tile clear of its edges.
        if (cx < MAP_OFFSET + 1 || cy < MAP_OFFSET + 1
         || cx > gMapHeader.mapLayout->width + MAP_OFFSET - 2 || cy > gMapHeader.mapLayout->height + MAP_OFFSET - 2)
            return FALSE;
        if (GetPuddleGroundSetAt(cx, cy) != set || MapGridGetElevationAt(cx, cy) != elevation
         || IsPuddleNearby(cx, cy) || IsPuddleSpotTaken(cx, cy))
            return FALSE;
    }
    DrawPlacedPuddle(&p, FALSE);
    sPuddles.list[sPuddles.count++] = p;
    return TRUE;
}

static void SpawnPuddles(u8 stage, u32 permille, bool8 redraw)
{
    s32 x, y, width = gMapHeader.mapLayout->width, height = gMapHeader.mapLayout->height;
    u8 maxSize = GetStageMaxSize(stage);
    bool8 ice = (stage == PUDDLES_ICE);
    bool8 snow = (stage == PUDDLES_SNOW);
    u32 weightSum = 0, i, first = sPuddles.count;

    for (i = 0; i < maxSize; i++)
        weightSum += sRainPuddleSizeWeights[i];

    for (y = MAP_OFFSET; y < height + MAP_OFFSET && sPuddles.count < MAX_TRACKED_PUDDLES; y++)   // v5.0: stop when full
    {
        for (x = MAP_OFFSET; x < width + MAP_OFFSET; x++)
        {
            u32 roll, size;
            u8 set;

            if (PuddleRandom() % 10000 >= permille)
                continue;
            set = GetPuddleGroundSetAt(x, y);
            if (set == NO_PUDDLE_SET || IsPuddleNearby(x, y) || IsPuddleSpotTaken(x, y) || (snow && set != 0))
                continue;
            // Pick a size (bigger = rarer), then fall back to smaller ones until
            // one fits; a 1-tile puddle always fits on this tile.
            roll = PuddleRandom() % weightSum;
            for (size = 1; size < maxSize && roll >= sRainPuddleSizeWeights[size - 1]; size++)
                roll -= sRainPuddleSizeWeights[size - 1];
            for (; size >= 1; size--)
            {
                u32 variants = 0, pick;

                for (i = 0; i < ARRAY_COUNT(sRainPuddleShapes); i++)
                    if (sRainPuddleShapes[i].size == size)
                        variants++;
                if (variants == 0)
                    continue;
                pick = PuddleRandom() % variants;
                for (i = 0; i < ARRAY_COUNT(sRainPuddleShapes); i++)
                    if (sRainPuddleShapes[i].size == size && pick-- == 0)
                        break;
                if (TryPlaceRainPuddleShape(i, ice, x, y, MapGridGetElevationAt(x, y), set, snow))
                    break;
            }
        }
    }
    // Live changes: take the new puddles back off the map (they were only put down
    // so they keep clear of each other) and let them appear one by one.
    if (redraw)
    {
        for (i = first; i < sPuddles.count; i++)
        {
            ErasePlacedPuddle(&sPuddles.list[i], FALSE);
            sPuddles.list[i].drawn = FALSE;
            sPuddles.list[i].op = POP_DRAW;
        }
    }
}

static void RemoveAllPuddles(bool8 redraw)
{
    u32 i;

    if (redraw)
    {
        for (i = 0; i < sPuddles.count; i++)
            sPuddles.list[i].op = POP_ERASE;   // they go one by one
        return;
    }
    for (i = 0; i < sPuddles.count; i++)
        if (sPuddles.list[i].drawn)
            ErasePlacedPuddle(&sPuddles.list[i], FALSE);
    sPuddles.count = 0;
}

// Keep 4 in 5 puddles (the "20% fewer" step).
static void ThinOutPuddles(void)
{
    u32 i;

    for (i = 0; i < sPuddles.count; i++)
        if (sPuddles.list[i].op != POP_ERASE && PuddleRandom() % 5 == 0)
            sPuddles.list[i].op = POP_ERASE;
}

// Frozen / thawed puddles are at most 3 tiles: a 2x2 keeps its top row.
static void ShrinkPuddles(u8 cap)
{
    u32 i;

    for (i = 0; i < sPuddles.count; i++)
    {
        struct PlacedPuddle *p = &sPuddles.list[i];
        if (p->snow || p->op == POP_ERASE || sRainPuddleShapes[p->shape].size <= cap || p->shape != PUDDLE_SHAPE_2X2)
            continue;
        p->newShape = PUDDLE_SHAPE_H2;
        p->op = (p->op == POP_DRAW) ? POP_DRAW : POP_REDRAW;
        if (p->op == POP_DRAW)
            p->shape = PUDDLE_SHAPE_H2;
    }
}

static void SetPuddlesIce(bool8 ice)
{
    u32 i;

    for (i = 0; i < sPuddles.count; i++)
    {
        struct PlacedPuddle *p = &sPuddles.list[i];
        u8 target = (p->op == POP_REDRAW) ? p->newShape : p->shape;
        if (p->snow || p->op == POP_ERASE || p->ice == ice || (ice && sRainPuddleShapes[target].size > 3))
            continue;
        p->ice = ice;
        if (p->op == POP_NONE)
        {
            p->newShape = p->shape;
            p->op = POP_REDRAW;
        }
    }
}

// Map load (after the map's ON_LOAD script): a fresh random set for the
// current stage; anything from the last visit is gone with the old map data.
void PlaceRainPuddles(void)
{
    u32 permille;

    sPuddles.count = 0;
    sPuddles.stage = PUDDLES_NONE;
    sPuddles.mapId = GetCurrentMapId();
    sPuddles.rng ^= gMain.vblankCounter1 * 0x9E3779B1u ^ sPuddles.mapId * 0x85EBCA77u;
    if (!MapHasNaturalLight(gMapHeader.mapType) || gMapHeader.mapLayout == NULL
     || gMapHeader.mapType == MAP_TYPE_TOWN || gMapHeader.mapType == MAP_TYPE_CITY
     || FlagGet(FLAG_PUDDLES_OFF))
        return;   // v2.3: towns and cities get wet ground instead of puddles; v4.0 PUDDLES Off
    sPuddles.stage = GetPuddleStage(&permille);
    sPuddles.pendingStage = sPuddles.stage;
    sPuddles.pendingTimer = 0;
    if (sPuddles.stage != PUDDLES_NONE)
        SpawnPuddles(sPuddles.stage, permille, FALSE);   // entering a map: already there
}

// ---------------------------------------------------------------------------
// v4.0 puddle fades. Puddles are map tiles and share their colours with the rest
// of the map, so they can't be made see-through themselves. Instead a patch of
// grass is laid over each changing puddle as a sprite and dissolved in or out
// through 8 dither steps (1 s with PUDDLES = Fast, 2 s with Slow):
//   going away / changing: grass dissolves in over the puddle, the tile changes
//   under it, then the grass dissolves away again over the new puddle / ground.
// Only puddles on screen get the overlay; the rest change at once, unseen.
// ---------------------------------------------------------------------------
#define PUDDLE_STAGE_DELAY     600     // react 10 s after the weather changed
#define MAX_PUDDLE_OVERLAYS    24
#define PUDDLE_FADE_LEVELS     8
#define TAG_PUDDLE_FADE        0x1238
enum { PFADE_IDLE, PFADE_COVER, PFADE_REVEAL };

static u8 sPuddleOverlayIds[MAX_PUDDLE_OVERLAYS];
static u8 sPuddleOverlayCount;
static u8 sPuddleFadePhase;
static u8 sPuddleFadeLevel;     // 0 = overlay invisible ... 8 = solid grass
static u8 sPuddleFadeTimer;
static u16 sPuddleOverlayTileStart;

static const u8 sBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static void SpriteCB_PuddleOverlay(struct Sprite *sprite)
{
    if (IsWeatherFrozen())
        return;   // v6.3: start menu open
    if (sPuddleFadeLevel == 0)
    {
        sprite->invisible = TRUE;
        return;
    }
    sprite->invisible = FALSE;
    sprite->oam.tileNum = sPuddleOverlayTileStart + (sPuddleFadeLevel - 1) * 4;
}

static const struct OamData sPuddleOverlayOam = {.shape = SPRITE_SHAPE(16x16), .size = SPRITE_SIZE(16x16), .priority = 2};
static const struct SpriteTemplate sPuddleOverlayTemplate =
{
    .tileTag = TAG_PUDDLE_FADE, .paletteTag = TAG_PUDDLE_FADE, .oam = &sPuddleOverlayOam,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_PuddleOverlay,
};

// Grass in 8 dissolve steps, built from the short-grass tiles already in VRAM.
static bool8 LoadPuddleOverlayGfx(void)
{
    static const u8 sQuadTile[4] = {2, 3, 3, 2};   // short grass 0x001: TL TR / BL BR
    const u8 *vram = (const u8 *)BG_CHAR_ADDR(0);
    u8 *buf;
    u32 level, q, i, x, y;
    struct SpriteSheet sheet;
    struct SpritePalette pal;
    u16 palBuf[16];

    if (GetSpriteTileStartByTag(TAG_PUDDLE_FADE) != TAG_NONE)
        return TRUE;
    buf = AllocZeroed(PUDDLE_FADE_LEVELS * 4 * TILE_SIZE_4BPP);
    if (buf == NULL)
        return FALSE;
    for (level = 0; level < PUDDLE_FADE_LEVELS; level++)
    {
        u32 threshold = (level + 1) * 2;   // 2/16 ... 16/16 of the pixels
        for (q = 0; q < 4; q++)
        {
            const u8 *src = vram + sQuadTile[q] * TILE_SIZE_4BPP;
            u8 *dst = buf + (level * 4 + q) * TILE_SIZE_4BPP;
            for (y = 0; y < 8; y++)
                for (x = 0; x < 8; x++)
                {
                    u32 gx = (q & 1) * 8 + x, gy = (q >> 1) * 8 + y;
                    u8 px = (src[y * 4 + x / 2] >> ((x & 1) * 4)) & 0xF;
                    if (sBayer4[gy & 3][gx & 3] < threshold)
                        dst[y * 4 + x / 2] |= px << ((x & 1) * 4);
                }
        }
    }
    sheet.data = buf;
    sheet.size = PUDDLE_FADE_LEVELS * 4 * TILE_SIZE_4BPP;
    sheet.tag = TAG_PUDDLE_FADE;
    LoadSpriteSheet(&sheet);
    Free(buf);
    for (i = 0; i < 16; i++)
        palBuf[i] = gPlttBufferFaded[BG_PLTT_ID(2) + i];
    pal.data = palBuf;
    pal.tag = TAG_PUDDLE_FADE;
    LoadSpritePalette(&pal);
    sPuddleOverlayTileStart = GetSpriteTileStartByTag(TAG_PUDDLE_FADE);
    return TRUE;
}

// Keep the overlay grass the same colour as the map grass (time of day, weather).
static void SyncPuddleOverlayPalette(void)
{
    u8 idx = IndexOfSpritePaletteTag(TAG_PUDDLE_FADE);
    u32 i;

    if (idx == 0xFF)
        return;
    for (i = 0; i < 16; i++)
    {
        gPlttBufferFaded[OBJ_PLTT_ID(idx) + i] = gPlttBufferFaded[BG_PLTT_ID(2) + i];
        gPlttBufferUnfaded[OBJ_PLTT_ID(idx) + i] = gPlttBufferFaded[BG_PLTT_ID(2) + i];
    }
}

static bool8 IsPuddleOnScreen(const struct PlacedPuddle *p)
{
    s16 px, py;
    PlayerGetDestCoords(&px, &py);
    return p->x >= px - 9 && p->x <= px + 10 && p->y >= py - 7 && p->y <= py + 8;
}

static void CoverPuddleCells(const struct PlacedPuddle *p, u8 shape)
{
    // v5.6: all of the puddle or nothing - a puddle half under the grass overlay
    // looked cut in half while it faded
    const struct RainPuddleShape *sh = &sRainPuddleShapes[shape];
    u8 start = sPuddleOverlayCount;
    u32 i;

    if (sPuddleOverlayCount + sh->count > MAX_PUDDLE_OVERLAYS || !WeatherCanUseSprites(sh->count))
        return;   // v6.0: no sprites to spare - the puddle changes at once
    for (i = 0; i < sh->count; i++)
    {
        s16 sx, sy;
        u8 id;
        SetSpritePosToMapCoords(p->x + sh->cells[i][0], p->y + sh->cells[i][1], &sx, &sy);
        id = CreateSprite(&sPuddleOverlayTemplate, sx + 8, sy + 8, 0xFE);
        if (id == MAX_SPRITES)
        {
            while (sPuddleOverlayCount > start)   // no room: take this puddle's pieces off again
                DestroySprite(&gSprites[sPuddleOverlayIds[--sPuddleOverlayCount]]);
            return;
        }
        gSprites[id].coordOffsetEnabled = TRUE;
        gSprites[id].callback(&gSprites[id]);
        sPuddleOverlayIds[sPuddleOverlayCount++] = id;
    }
}

static void EndPuddleFade(void)
{
    u32 i;

    for (i = 0; i < sPuddleOverlayCount; i++)
        if (gSprites[sPuddleOverlayIds[i]].inUse && gSprites[sPuddleOverlayIds[i]].callback == SpriteCB_PuddleOverlay)
            DestroySprite(&gSprites[sPuddleOverlayIds[i]]);
    sPuddleOverlayCount = 0;
    sPuddleFadePhase = PFADE_IDLE;
    sPuddleFadeLevel = 0;
    FreeSpriteTilesByTag(TAG_PUDDLE_FADE);
    FreeSpritePaletteByTag(TAG_PUDDLE_FADE);
}

// v5.0: one pass instead of three
static bool8 HasAnyPuddleOps(void)
{
    u32 i;
    for (i = 0; i < sPuddles.count; i++)
        if (sPuddles.list[i].op != POP_NONE)
            return TRUE;
    return FALSE;
}

// Carry out every queued change (under the grass cover, if any).
static void ApplyPuddleOps(bool8 coverNew)
{
    u32 i;

    for (i = 0; i < sPuddles.count; )
    {
        struct PlacedPuddle *p = &sPuddles.list[i];
        switch (p->op)
        {
        case POP_ERASE:
            if (p->drawn)
                ErasePlacedPuddle(p, TRUE);
            sPuddles.list[i] = sPuddles.list[--sPuddles.count];
            continue;
        case POP_DRAW:
            if (coverNew && IsPuddleOnScreen(p))
                CoverPuddleCells(p, p->shape);
            DrawPlacedPuddle(p, TRUE);
            p->drawn = TRUE;
            break;
        case POP_REDRAW:
            if (p->drawn)
                ErasePlacedPuddle(p, TRUE);
            p->shape = p->newShape;
            DrawPlacedPuddle(p, TRUE);
            p->drawn = TRUE;
            break;
        }
        p->op = POP_NONE;
        i++;
    }
}

static void StartPuddleFade(void)
{
    u32 i;
    bool8 any = FALSE;

    if (!LoadPuddleOverlayGfx())
    {
        ApplyPuddleOps(FALSE);   // no memory for the overlay: change at once
        return;
    }
    SyncPuddleOverlayPalette();
    sPuddleOverlayCount = 0;
    for (i = 0; i < sPuddles.count; i++)
    {
        struct PlacedPuddle *p = &sPuddles.list[i];
        if ((p->op == POP_ERASE || p->op == POP_REDRAW) && p->drawn && IsPuddleOnScreen(p))
        {
            CoverPuddleCells(p, p->shape);
            any = TRUE;
        }
    }
    sPuddleFadeTimer = 0;
    if (any)
    {
        sPuddleFadeLevel = 0;
        sPuddleFadePhase = PFADE_COVER;   // grass dissolves in over the old puddles
    }
    else
    {
        sPuddleFadeLevel = PUDDLE_FADE_LEVELS;
        ApplyPuddleOps(TRUE);              // new puddles start hidden under grass
        sPuddleFadePhase = PFADE_REVEAL;
    }
}

// The weather check calls this when the puddles should change: it waits 10 s
// (from the moment the weather changed on screen) before anything happens.
static void ApplyPuddleStage(u8 stage);

void UpdatePuddleAnimations(void)
{
    u32 step;

    if (sPuddles.mapId != GetCurrentMapId() || gMapHeader.mapLayout == NULL)
    {
        if (sPuddleFadePhase != PFADE_IDLE)
            EndPuddleFade();
        return;
    }
    // v6.3: when it snows, the puddles freeze only once the snow has covered every tile
    if (sPuddles.pendingTimer == 1 && (sPuddles.pendingStage == PUDDLES_ICE || sPuddles.pendingStage == PUDDLES_SNOW)
     && !IsSnowCoverComplete())
        sPuddles.pendingTimer = 2;
    if (sPuddles.pendingTimer != 0 && --sPuddles.pendingTimer == 0)
        ApplyPuddleStage(sPuddles.pendingStage);
    if (sPuddleFadePhase == PFADE_IDLE && sPuddles.count == 0)
        return;   // v5.0: the common case costs almost nothing

    step = FlagGet(FLAG_PUDDLES_FAST) ? 60 / PUDDLE_FADE_LEVELS : 120 / PUDDLE_FADE_LEVELS;
    switch (sPuddleFadePhase)
    {
    case PFADE_IDLE:
        if (HasAnyPuddleOps())
            StartPuddleFade();
        break;
    case PFADE_COVER:
        SyncPuddleOverlayPalette();
        if (++sPuddleFadeTimer < step)
            break;
        sPuddleFadeTimer = 0;
        if (++sPuddleFadeLevel < PUDDLE_FADE_LEVELS)
            break;
        ApplyPuddleOps(TRUE);             // swap the tiles while they are hidden
        sPuddleFadePhase = PFADE_REVEAL;
        break;
    case PFADE_REVEAL:
        SyncPuddleOverlayPalette();
        if (++sPuddleFadeTimer < step)
            break;
        sPuddleFadeTimer = 0;
        if (sPuddleFadeLevel > 0)
            sPuddleFadeLevel--;
        if (sPuddleFadeLevel == 0)
            EndPuddleFade();
        break;
    }
}

// Called with the time-based field events (and after weather changes):
// moves the puddles on the current map along their life cycle.
void UpdateRainPuddles(void)
{
    u32 permille;
    u8 stage;

    if (sPuddles.mapId != GetCurrentMapId() || !MapHasNaturalLight(gMapHeader.mapType)
     || gMapHeader.mapLayout == NULL || gMapHeader.mapType == MAP_TYPE_TOWN || gMapHeader.mapType == MAP_TYPE_CITY)
        return;
    if (FlagGet(FLAG_PUDDLES_OFF))
    {
        stage = PUDDLES_NONE;
        sPuddles.pendingTimer = 0;
        if (sPuddles.count)
            RemoveAllPuddles(TRUE);
        sPuddles.stage = PUDDLES_NONE;
        return;
    }
    stage = GetPuddleStage(&permille);
    if (stage == sPuddles.stage)
    {
        sPuddles.pendingTimer = 0;
        sPuddles.pendingStage = stage;
        return;
    }
    if (sPuddles.pendingTimer == 0 || sPuddles.pendingStage != stage)
    {
        sPuddles.pendingStage = stage;
        sPuddles.pendingTimer = PUDDLE_STAGE_DELAY;   // react 10 s from now
    }
}

static void ApplyPuddleStage(u8 stage)
{
    u32 permille;

    if (GetPuddleStage(&permille) != stage || stage == sPuddles.stage)
        return;   // the weather changed again meanwhile
    switch (stage)
    {
    case PUDDLES_NONE:
        RemoveAllPuddles(TRUE);
        break;
    case PUDDLES_SNOW:
        RemoveAllPuddles(TRUE);
        SpawnPuddles(PUDDLES_SNOW, permille, TRUE);
        break;
    case PUDDLES_RAIN:
        RemoveAllPuddles(TRUE);
        SpawnPuddles(PUDDLES_RAIN, permille, TRUE);
        break;
    case PUDDLES_DAMP:
        if (sPuddles.stage != PUDDLES_RAIN)
        {
            RemoveAllPuddles(TRUE);
            SpawnPuddles(stage, permille, TRUE);
            break;
        }
        ThinOutPuddles();
        ShrinkPuddles(4);
        break;
    case PUDDLES_ICE:
        if (sPuddles.stage != PUDDLES_RAIN && sPuddles.stage != PUDDLES_DAMP)
        {
            RemoveAllPuddles(TRUE);
            SpawnPuddles(stage, permille, TRUE);
            break;
        }
        ThinOutPuddles();
        ShrinkPuddles(3);
        SetPuddlesIce(TRUE);
        break;
    case PUDDLES_THAW:
        if (sPuddles.stage != PUDDLES_ICE)
        {
            RemoveAllPuddles(TRUE);
            SpawnPuddles(stage, permille, TRUE);
            break;
        }
        ThinOutPuddles();
        SetPuddlesIce(FALSE);
        break;
    }
    sPuddles.stage = stage;
}

// ============================================================================
// Wet ground in towns and cities (v2.3)
// ----------------------------------------------------------------------------
// While it rains in a town or city, every open walkable outdoor tile behaves
// like a puddle surface for the player: a see-through reflection shows on the
// ground and steps splash. No puddle tiles are placed there.
// ============================================================================
bool8 IsWetCityRain(void)
{
    u8 w = GetSavedWeather();

    return (gMapHeader.mapType == MAP_TYPE_TOWN || gMapHeader.mapType == MAP_TYPE_CITY)
        && (w == WEATHER_RAIN || w == WEATHER_RAIN_THUNDERSTORM || w == WEATHER_DOWNPOUR);
}

#include "data/grassy_metatiles.h"

// v6.0: is this map square grass (green ground)? - from tables made from the art
static bool8 IsGrassyMetatile(s16 x, s16 y)
{
    u16 metatileId = MapGridGetMetatileIdAt(x, y);
    const struct Tileset *tileset;
    u32 i;

    if (metatileId < NUM_METATILES_IN_PRIMARY)
    {
        tileset = gMapHeader.mapLayout->primaryTileset;
    }
    else
    {
        tileset = gMapHeader.mapLayout->secondaryTileset;
        metatileId -= NUM_METATILES_IN_PRIMARY;
    }
    if (tileset == NULL || metatileId >= 512)
        return FALSE;
    for (i = 0; i < ARRAY_COUNT(sGrassyTables); i++)
        if (sGrassyTables[i].metatiles == tileset->metatiles)
            return (sGrassyTables[i].grassy[metatileId >> 3] >> (metatileId & 7)) & 1;
    return FALSE;
}

bool8 IsWetCityGround(s16 x, s16 y)
{
    u8 b;

    if (!IsWetCityRain() || MapGridGetCollisionAt(x, y) != 0)
        return FALSE;
    b = MapGridGetMetatileBehaviorAt(x, y);
    // v6.0: paved ground and puddles only - never grass (short grass, or ground
    // drawn as grass, e.g. where a town meets a route)
    if (b != MB_NORMAL && b != MB_PUDDLE)
        return FALSE;
    return !IsGrassyMetatile(x, y);
}

// ============================================================================
// v4.0 water ripples
//  - ponds: now and then a ripple somewhere in open pond water (not next to the
//    edge), like a Pokemon moving underneath;
//  - rain: lots of ripples on every surface that shows reflections (ponds, sea,
//    puddles, ice, and the wet ground of towns and cities).
// Uses its own random numbers so battles / encounters are unaffected.
// ============================================================================
static u32 sRippleRng;   // (no initialised RAM data in this build)
static u8 sRippleTimer;

static u32 RippleRandom(void)
{
    sRippleRng = sRippleRng * 1103515245 + 12345 + gMain.vblankCounter1;
    return sRippleRng >> 16;
}


// v5.7: any tile of a pond's surface - pond water, deep water, and the shallow
// "puddle" water big ponds are made of (but not a small puddle on the grass:
// shallow water needs at least 5 watery tiles around it)
static bool8 IsWatery(u8 b)
{
    return b == MB_POND_WATER || b == MB_DEEP_WATER || b == MB_PUDDLE;
}

static bool8 IsPondSurface(s16 x, s16 y)
{
    u8 b = MapGridGetMetatileBehaviorAt(x, y);
    s16 dx, dy;
    u32 n = 0;

    if (b == MB_POND_WATER || b == MB_DEEP_WATER)
        return TRUE;
    if (b != MB_PUDDLE)
        return FALSE;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if ((dx || dy) && IsWatery(MapGridGetMetatileBehaviorAt(x + dx, y + dy)))
                n++;
    return n >= 5;
}

// v6.0: rain ripples used to take up to 26 of the 64 sprites; with the rain drops,
// people, wild Pokemon and grass effects the game ran out - things went missing or
// got the wrong graphics, and a battle could hang at its start (the transition had
// no sprite left). Our extras now always leave this many sprites free.
#define WEATHER_FREE_SPRITE_RESERVE 16

static u32 CountFreeSprites(void)
{
    u32 i, n = 0;

    for (i = 0; i < MAX_SPRITES; i++)
        if (!gSprites[i].inUse)
            n++;
    return n;
}

bool8 WeatherCanUseSprites(u32 count)
{
    return CountFreeSprites() >= count + WEATHER_FREE_SPRITE_RESERVE;
}

// v6.0: ripples never use more than 16 sprites (12 in a downpour, which also has
// the most rain drops and puddles)
#define MAX_RIPPLE_SPRITES          16
#define MAX_RIPPLE_SPRITES_DOWNPOUR 12
static u32 CountRippleSprites(void)
{
    const struct SpriteTemplate *rippleTemplate = gFieldEffectObjectTemplatePointers[FLDEFFOBJ_RIPPLE];
    u32 i, n = 0;

    for (i = 0; i < MAX_SPRITES; i++)
        if (gSprites[i].inUse && gSprites[i].template == rippleTemplate)
            n++;
    return n;
}

static bool8 CanStartRipple(void)
{
    u32 cap = (GetSavedWeather() == WEATHER_DOWNPOUR) ? MAX_RIPPLE_SPRITES_DOWNPOUR : MAX_RIPPLE_SPRITES;
    return CountRippleSprites() < cap;
}

static void StartMapRipple(s16 x, s16 y)
{
    if (!CanStartRipple())
        return;
    s16 sx, sy;
    SetSpritePosToMapCoords(x, y, &sx, &sy);
    gFieldEffectArguments[0] = sx + 8;
    gFieldEffectArguments[1] = sy + 8 + (s16)(RippleRandom() % 7) - 3;
    gFieldEffectArguments[2] = 151;
    gFieldEffectArguments[3] = 3;
    FieldEffectStart(FLDEFF_RIPPLE);
}

void UpdateWaterRipples(void)
{
    s16 px, py, x, y;
    u32 tries;
    bool8 raining;

    {
        extern void TryGiveRudyGift(void);
        TryGiveRudyGift();   // v6.3 (see perfect_gifts.c)
    }
    if (gMapHeader.mapLayout == NULL || ArePlayerFieldControlsLocked() || gPaletteFade.active)
        return;
    if (!WeatherCanUseSprites(1))
        return;   // v6.0: keep sprites free for everything else
    if (IsWetCityRain() && (gMain.vblankCounter1 & 31) == 0)
        TryRefreshObjectReflections();   // v6.0: people and Pokemon standing on wet city ground
    raining = IsRainWeather(GetSavedWeather()) && MapHasNaturalLight(gMapHeader.mapType);
    PlayerGetDestCoords(&px, &py);
    if (raining)
    {
        if (++sRippleTimer < 3)
            return;
        sRippleTimer = 0;
        for (tries = 0; tries < 6; tries++)
        {
            u8 b;
            x = px + (s16)(RippleRandom() % 15) - 7;
            y = py + (s16)(RippleRandom() % 11) - 5;
            b = MapGridGetMetatileBehaviorAt(x, y);
            if (MetatileBehavior_IsReflective(b) || MetatileBehavior_IsPuddle(b) || IsWetCityGround(x, y))
            {
                StartMapRipple(x, y);
                return;
            }
        }
        return;
    }
    // Clear weather: every 1-3 s, one ripple on a random open pond tile in view
    // (the tile and all 8 around it are pond water, so never at the edge).
    if (sRippleTimer == 0)
        sRippleTimer = 60 + RippleRandom() % 120;
    if (--sRippleTimer != 0)
        return;
    {
        u32 count = 0, pick;
        for (y = py - 5; y <= py + 5; y++)
            for (x = px - 7; x <= px + 7; x++)
                if (IsPondSurface(x, y))   // v5.7: the whole surface
                    count++;
        if (count == 0)
            return;
        pick = RippleRandom() % count;
        for (y = py - 5; y <= py + 5; y++)
            for (x = px - 7; x <= px + 7; x++)
                if (IsPondSurface(x, y) && pick-- == 0)
                {
                    StartMapRipple(x, y);
                    return;
                }
    }
}

// v5.1 helpers for the water tile animation (tileset_anims.c)
bool8 IsOverworldRainingOutdoors(void)
{
    return MapHasNaturalLight(gMapHeader.mapType) && IsRainWeather(GetSavedWeather());
}

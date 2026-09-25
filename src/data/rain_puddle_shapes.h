// GENERATED (v2.4): rain puddles of 1, 2 or 4 tiles (no 3-tile puddles).
// Pieces: water = single, h-left, h-mid, h-right, v-top, v-mid, v-bottom, 2x2 TL, TR, BL, BR;
//         ice   = the first seven (frozen puddles are at most 3 tiles).
// Each piece metatile: puddle art in the BOTTOM layer (reflections show on it),
// the ground with the puddle cut out in the MIDDLE layer.
enum { PP_SINGLE, PP_HL, PP_HM, PP_HR, PP_VT, PP_VM, PP_VB, PP_C2TL, PP_C2TR, PP_C2BL, PP_C2BR, PP_COUNT };
#define PP_ICE_COUNT 7

struct RainPuddleShape { u8 size; u8 count; s8 cells[4][2]; u8 pieces[4]; };

static const struct RainPuddleShape sRainPuddleShapes[] =
{
    {1, 1, {{0, 0}}, {PP_SINGLE}},
    {2, 2, {{0, 0}, {1, 0}}, {PP_HL, PP_HR}},
    {2, 2, {{0, 0}, {0, 1}}, {PP_VT, PP_VB}},
    {4, 4, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {PP_C2TL, PP_C2TR, PP_C2BL, PP_C2BR}},
};

// 2x2 puddles freeze / shrink to their top row.
#define PUDDLE_SHAPE_2X2 3
#define PUDDLE_SHAPE_H2  1

struct PuddleGroundSet { const struct Tileset *tileset; bool8 primary; u16 ground; u16 water[PP_COUNT]; u16 ice[PP_ICE_COUNT]; };

extern const struct Tileset gTileset_General;

static const struct PuddleGroundSet sPuddleGroundSets[] =
{
    {&gTileset_General, TRUE, 0x001, {0x02A, 0x05C, 0x0D7, 0x0DB, 0x0DC, 0x0DD, 0x0DE, 0x0C8, 0x0CA, 0x0D8, 0x0DA}, {0x13C, 0x13F, 0x143, 0x144, 0x160, 0x161, 0x17F}},
};

// v3.0b snow piles (grass underneath, white mound on top), same pieces as the puddles.
static const u16 sSnowPileMetatiles[PP_COUNT] = {0x135, 0x1A2, 0x000, 0x1B6, 0x1E3, 0x000, 0x1EA, 0x1EB, 0x1F8, 0x1F9, 0x199};

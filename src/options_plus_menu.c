#include "global.h"
#include "string_util.h"
#include "new_game.h"
#include "option_plus_menu.h"
#include "main.h"
#include "menu.h"
#include "scanline_effect.h"
#include "palette.h"
#include "sprite.h"
#include "task.h"
#include "malloc.h"
#include "rtc.h"
#include "bg.h"
#include "gpu_regs.h"
#include "window.h"
#include "text.h"
#include "text_window.h"
#include "international_string_util.h"
#include "strings.h"
#include "gba/m4a_internal.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "event_data.h"
#include "sound.h"

enum
{
    MENU_MAIN,
    MENU_CUSTOM,
    MENU_SOUND,
    MENU_COUNT,
};

// v6.0: the pages shown (each lists settings from the tables above, plus headers)
enum
{
    PAGE_MAIN,
    PAGE_UI,
    PAGE_WORLD,
    PAGE_BATTLE,
    PAGE_SOUND,
    PAGE_COUNT,
};

// Menu items
enum
{
    MENUITEM_MAIN_UI,
    MENUITEM_MAIN_UI_BAG,
    MENUITEM_MAIN_MIST,
    MENUITEM_MAIN_SPAWN_RATE,
    MENUITEM_MAIN_WEATHER_SYSTEM,
    MENUITEM_MAIN_POND,
    MENUITEM_MAIN_WEATHER_SPREAD,
    MENUITEM_MAIN_PUDDLES,
    MENUITEM_MAIN_TEXTSPEED,
    MENUITEM_MAIN_FONT,
    MENUITEM_MAIN_DIFFICULTY,
    MENUITEM_MAIN_BUTTONMODE,
    MENUITEM_MAIN_FOLLOWER,
    MENUITEM_MAIN_LARGE_FOLLOWER,
    MENUITEM_MAIN_SURFOVERWORLD,
    MENUITEM_MAIN_MATCHCALL,
    MENUITEM_MAIN_AUTORUN,
    MENUITEM_MAIN_AUTORUN_SURF,
    MENUITEM_MAIN_AUTORUN_DIVE,
    MENUITEM_MAIN_FISHING,
    MENUITEM_MAIN_EVEN_FASTER_JOY,
    MENUITEM_MAIN_UNIT_TYPE,
    MENUITEM_MAIN_BRIGHTER_NIGHTS,
    MENUITEM_MAIN_SKIP_INTRO,
    MENUITEM_MAIN_FRAMETYPE,
    MENUITEM_MAIN_COUNT,
};

//Menu options 2
enum
{
    MENUITEM_BATTLE_BATTLESTYLE,
    MENUITEM_BATTLE_BATTLESCENE,
    MENUITEM_BATTLE_FAST_INTRO,
    MENUITEM_BATTLE_FAST_BATTLES,
    MENUITEM_BATTLE_BATTLE_SPEED,
    MENUITEM_BATTLE_OWE,
    MENUITEM_BATTLE_WEATHER_FX,
    MENUITEM_BATTLE_BACKGROUND,
    MENUITEM_BATTLE_SHADOWS,
    MENUITEM_BATTLE_NEW_BACKGROUNDS,
    MENUITEM_BATTLE_BALL_PROMPT,
    MENUITEM_BATTLE_TYPE_EFFECTIVE,
    MENUITEM_BATTLE_RUN_TYPE,
    MENUITEM_BATTLE_LR_RUN,
    MENUITEM_BATTLE_SPLIT,
    MENUITEM_BATTLE_CURSOR_MEMORY,
    MENUITEM_BATTLE_COUNT,
};

// Menu sounds
enum
{
    MENUITEM_SOUND_SOUND,
    MENUITEM_SOUND_MUSIC,
    MENUITEM_SOUND_BIKE_MUSIC,
    MENUITEM_SOUND_SURF_MUSIC,
    MENUITEM_SOUND_WILD_MON_MUSIC,
    MENUITEM_SOUND_BATTLE_TRAINER_MUSIC,
    MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC,
    MENUITEM_SOUND_EFFECTS,
    MENUITEM_SOUND_COUNT,
};

// Window Ids
enum
{
    WIN_TOPBAR,
    WIN_OPTIONS,
    WIN_DESCRIPTION
};

static const struct WindowTemplate sOptionMenuWinTemplates[] =
{
    {//WIN_TOPBAR
        .bg = 1,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 30,
        .height = 2,
        .paletteNum = 1,
        .baseBlock = 2
    },
    {//WIN_OPTIONS
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 3,
        .width = 26,
        .height = 10,
        .paletteNum = 1,
        .baseBlock = 62
    },
    {//WIN_DESCRIPTION
        .bg = 1,
        .tilemapLeft = 2,
        .tilemapTop = 15,
        .width = 26,
        .height = 4,
        .paletteNum = 1,
        .baseBlock = 500
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sOptionMenuBgTemplates[] =
{
    {
       .bg = 0,
       .charBaseIndex = 1,
       .mapBaseIndex = 30,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 1,
       .baseTile = 0
    },
    {
       .bg = 1,
       .charBaseIndex = 1,
       .mapBaseIndex = 31,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 0,
       .baseTile = 0
    },
};

struct OptionMenu
{
    u8 submenu;
    u8 sel[MENUITEM_MAIN_COUNT];
    u8 sel_battle[MENUITEM_BATTLE_COUNT];
    u8 sel_sound[MENUITEM_SOUND_COUNT];
    int menuCursor[PAGE_COUNT];
    int visibleCursor[PAGE_COUNT];
    u8 page;
    u8 arrowTaskId;
};

#define Y_DIFF 16 // Difference in pixels between items.
#define OPTIONS_ON_SCREEN 5
#define NUM_OPTIONS_FROM_BORDER 1


// local functions
static void MainCB2(void);
static void VBlankCB(void);
static void DrawTopBarText(void); //top Option text
static void DrawLeftSideOptionText(int selection, int y);
static void DrawRightSideChoiceText(const u8 *str, int x, int y, bool8 choosen, bool8 active);
static void DrawOptionMenuTexts(void); //left side text;
static void DrawChoices(u32 id, int y); //right side draw function
static void HighlightOptionMenuItem(void);
static void Task_OptionMenuFadeIn(u8 taskId);
static void Task_OptionMenuProcessInput(u8 taskId);
static void Task_OptionMenuSave(u8 taskId);
static void Task_OptionMenuFadeOut(u8 taskId);
static void ScrollMenu(int direction);
static void SkipHeaderRows(void);
static void MoveOptionCursorDown(void);
static u8 MenuItemCount(void);
static void ScrollAll(int direction); // to bottom or top
static int GetMiddleX(const u8 *txt1, const u8 *txt2, const u8 *txt3);
static int XOptions_ProcessInput(int x, int selection);
static int ProcessInput_Options_Two(int selection);
static int ProcessInput_Options_Three(int selection);
static int ProcessInput_Options_Four(int selection);
static int ProcessInput_Options_Five(int selection);
static int ProcessInput_Options_Six(int selection);
static int ProcessInput_Options_Seven(int selection);
static int ProcessInput_Options_Eleven(int selection);
static int ProcessInput_Sound(int selection);
static int ProcessInput_FrameType(int selection);
static int ProcessInput_BattleStyle(int selection);
static int ProcessInput_Difficulty(int selection);
static const u8 *const OptionTextDescription(void);
static const u8 *const OptionTextRight(u8 menuItem);
static u8 MenuItemCount(void);
static void DrawDescriptionText(void);
static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style, bool8 active);
static void DrawChoices_Options_Four(const u8 *const *const strings, int selection, int y, bool8 active);
static void DrawChoices_Options_Seven(const u8 *const *const strings, int selection, int y, bool8 active);
static void ReDrawAll(void);
static void DrawChoices_TextSpeed(int selection, int y);
static void DrawChoices_Difficulty(int selection, int y);
static void DrawChoices_BattleScene(int selection, int y);
static void DrawChoices_BattleStyle(int selection, int y);
static void DrawChoices_Sound(int selection, int y);
static void DrawChoices_ButtonMode(int selection, int y);
static void DrawChoices_Follower(int selection, int y);
static void DrawChoices_LargeFollower(int selection, int y);
static void DrawChoices_Autorun(int selection, int y);
static void DrawChoices_FrameType(int selection, int y);
static void DrawChoices_MatchCall(int selection, int y);
static void DrawChoices_Style(int selection, int y);
static void DrawChoices_TypeEffective(int selection, int y);
static void DrawChoices_Fishing(int selection, int y);
static void DrawChoices_FastIntro(int selection, int y);
static void DrawChoices_FastBattles(int selection, int y);
static void DrawChoices_BattleSpeed(int selection, int y);
static void DrawChoices_BikeMusic(int selection, int y);
static void DrawChoices_EvenFasterJoy(int selection, int y);
static void DrawChoices_SurfMusic(int selection, int y);
static void DrawChoices_Wild_Battle_Music(int selection, int y);
static void DrawChoices_Trainer_Battle_Music(int selection, int y);
static void DrawChoices_Frontier_Trainer_Battle_Music(int selection, int y);
static void DrawChoices_Sound_Effects(int selection, int y);
static void DrawChoices_Skip_Intro(int selection, int y);
static void DrawChoices_LR_Run(int selection, int y);
static void DrawChoices_Ball_Prompt(int selection, int y);
static void DrawChoices_Unit_Type(int selection, int y);
static void DrawChoices_Music(int selection, int y);
static void DrawChoices_New_Backgrounds(int selection, int y);
static void DrawChoices_Terrain_Circles(int selection, int y);
static void DrawChoices_WeatherFx(int selection, int y);
static void DrawChoices_UI(int selection, int y);
static void DrawChoices_WeatherSystem(int selection, int y);
static void DrawChoices_UiBag(int selection, int y);
static void DrawChoices_Mist(int selection, int y);
static void DrawChoices_SpawnRate(int selection, int y);
static void DrawChoices_Pond(int selection, int y);
static void DrawChoices_WeatherSpread(int selection, int y);
static void DrawChoices_Puddles(int selection, int y);
static void DrawChoices_OWE(int selection, int y);
static void DrawChoices_Shadows(int selection, int y);
static void DrawChoices_Run_Type(int selection, int y);
static void DrawChoices_Autorun_Surf(int selection, int y);
static void DrawChoices_Autorun_Dive(int selection, int y);
static void DrawChoices_SurfOverworld(int selection, int y);
static void DrawChoices_BrighterNights(int selection, int y);
static void DrawChoices_Font(int selection, int y);
static void DrawChoices_CursorMemory(int selection, int y);
static void DrawBgWindowFrames(void);

// EWRAM vars
EWRAM_DATA static struct OptionMenu *sOptions = NULL;

// ---------------------------------------------------------------------------
// v6.0 pages: Main, UI, World and weather, Battle (with sub headers), Sounds
// ---------------------------------------------------------------------------
#define ROW_HEADER 0xFF
struct OptRow { u8 set; u8 item; };
static const u8 sText_Hdr_BattleAnimation[] = _("Battle animation:");
static const u8 sText_Hdr_BattleScene[]     = _("Battle scene:");
static const u8 sText_Hdr_BattleSettings[]  = _("Battle settings:");
static const u8 *const sHeaderTexts[] = {sText_Hdr_BattleAnimation, sText_Hdr_BattleScene, sText_Hdr_BattleSettings};
static const struct OptRow sRows_Main[] =
{
    {MENU_MAIN, MENUITEM_MAIN_DIFFICULTY}, {MENU_MAIN, MENUITEM_MAIN_BUTTONMODE}, {MENU_MAIN, MENUITEM_MAIN_AUTORUN},
    {MENU_MAIN, MENUITEM_MAIN_MATCHCALL}, {MENU_MAIN, MENUITEM_MAIN_FISHING}, {MENU_MAIN, MENUITEM_MAIN_EVEN_FASTER_JOY},
    {MENU_MAIN, MENUITEM_MAIN_SKIP_INTRO},
    {MENU_MAIN, MENUITEM_MAIN_FOLLOWER}, {MENU_MAIN, MENUITEM_MAIN_LARGE_FOLLOWER}, {MENU_MAIN, MENUITEM_MAIN_SURFOVERWORLD},
};
static const struct OptRow sRows_UI[] =
{
    {MENU_MAIN, MENUITEM_MAIN_UI}, {MENU_MAIN, MENUITEM_MAIN_UI_BAG}, {MENU_MAIN, MENUITEM_MAIN_UNIT_TYPE},
    {MENU_MAIN, MENUITEM_MAIN_FRAMETYPE}, {MENU_MAIN, MENUITEM_MAIN_TEXTSPEED}, {MENU_MAIN, MENUITEM_MAIN_FONT},
};
static const struct OptRow sRows_World[] =
{
    {MENU_CUSTOM, MENUITEM_BATTLE_OWE}, {MENU_MAIN, MENUITEM_MAIN_WEATHER_SYSTEM}, {MENU_MAIN, MENUITEM_MAIN_WEATHER_SPREAD},
    {MENU_MAIN, MENUITEM_MAIN_MIST}, {MENU_MAIN, MENUITEM_MAIN_SPAWN_RATE}, {MENU_MAIN, MENUITEM_MAIN_PUDDLES}, {MENU_MAIN, MENUITEM_MAIN_POND}, {MENU_MAIN, MENUITEM_MAIN_BRIGHTER_NIGHTS},
};
static const struct OptRow sRows_Battle[] =
{
    {ROW_HEADER, 0},
    {MENU_CUSTOM, MENUITEM_BATTLE_BATTLESCENE}, {MENU_CUSTOM, MENUITEM_BATTLE_BATTLE_SPEED},
    {MENU_CUSTOM, MENUITEM_BATTLE_FAST_BATTLES}, {MENU_CUSTOM, MENUITEM_BATTLE_FAST_INTRO},
    {ROW_HEADER, 1},
    {MENU_CUSTOM, MENUITEM_BATTLE_WEATHER_FX}, {MENU_CUSTOM, MENUITEM_BATTLE_BACKGROUND}, {MENU_CUSTOM, MENUITEM_BATTLE_NEW_BACKGROUNDS},
    {MENU_CUSTOM, MENUITEM_BATTLE_SHADOWS},
    {ROW_HEADER, 2},
    {MENU_CUSTOM, MENUITEM_BATTLE_BATTLESTYLE}, {MENU_CUSTOM, MENUITEM_BATTLE_BALL_PROMPT}, {MENU_CUSTOM, MENUITEM_BATTLE_TYPE_EFFECTIVE},
    {MENU_CUSTOM, MENUITEM_BATTLE_SPLIT}, {MENU_CUSTOM, MENUITEM_BATTLE_CURSOR_MEMORY}, {MENU_CUSTOM, MENUITEM_BATTLE_LR_RUN},
    {MENU_CUSTOM, MENUITEM_BATTLE_RUN_TYPE},
};
static const struct OptRow sRows_Sound[] =
{
    {MENU_SOUND, MENUITEM_SOUND_SOUND}, {MENU_SOUND, MENUITEM_SOUND_MUSIC}, {MENU_SOUND, MENUITEM_SOUND_BIKE_MUSIC},
    {MENU_SOUND, MENUITEM_SOUND_SURF_MUSIC}, {MENU_SOUND, MENUITEM_SOUND_WILD_MON_MUSIC}, {MENU_SOUND, MENUITEM_SOUND_BATTLE_TRAINER_MUSIC},
    {MENU_SOUND, MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC}, {MENU_SOUND, MENUITEM_SOUND_EFFECTS},
};
static const u8 sText_Page_Main[]   = _("Main Options");
static const u8 sText_Page_UI[]     = _("UI Options");
static const u8 sText_Page_World[]  = _("World and Weather");
static const u8 sText_Page_Battle[] = _("Battle Options");
static const u8 sText_Page_Sound[]  = _("Sound Options");
static const struct { const struct OptRow *rows; u8 count; const u8 *title; } sPages[PAGE_COUNT] =
{
    [PAGE_MAIN]   = {sRows_Main,   ARRAY_COUNT(sRows_Main),   sText_Page_Main},
    [PAGE_UI]     = {sRows_UI,     ARRAY_COUNT(sRows_UI),     sText_Page_UI},
    [PAGE_WORLD]  = {sRows_World,  ARRAY_COUNT(sRows_World),  sText_Page_World},
    [PAGE_BATTLE] = {sRows_Battle, ARRAY_COUNT(sRows_Battle), sText_Page_Battle},
    [PAGE_SOUND]  = {sRows_Sound,  ARRAY_COUNT(sRows_Sound),  sText_Page_Sound},
};

static bool8 IsHeaderRow(int row)
{
    return row >= 0 && row < sPages[sOptions->page].count && sPages[sOptions->page].rows[row].set == ROW_HEADER;
}

// a setting's row: makes its table the current one and returns its item
static u8 SelectRow(int row)
{
    const struct OptRow *r = &sPages[sOptions->page].rows[row];
    sOptions->submenu = r->set;
    return r->item;
}


// const data
static const u8 sEqualSignGfx[] = INCBIN_U8("graphics/interface/option_menu_equals_sign.4bpp"); // note: this is only used in the Japanese release
static const u16 sOptionMenuBg_Pal[] = {RGB(17, 18, 31)};
static const u16 sOptionMenuText_Pal[] = INCBIN_U16("graphics/interface/option_menu_text_custom.gbapal");

#define TEXT_COLOR_OPTIONS_WHITE                1
#define TEXT_COLOR_OPTIONS_GRAY_FG              2
#define TEXT_COLOR_OPTIONS_GRAY_SHADOW          3
#define TEXT_COLOR_OPTIONS_GRAY_LIGHT_FG        4
#define TEXT_COLOR_OPTIONS_ORANGE_FG            5
#define TEXT_COLOR_OPTIONS_ORANGE_SHADOW        6
#define TEXT_COLOR_OPTIONS_RED_FG               7
#define TEXT_COLOR_OPTIONS_RED_SHADOW           8
#define TEXT_COLOR_OPTIONS_GREEN_FG             9
#define TEXT_COLOR_OPTIONS_GREEN_SHADOW         10
#define TEXT_COLOR_OPTIONS_GREEN_DARK_FG        11
#define TEXT_COLOR_OPTIONS_GREEN_DARK_SHADOW    12
#define TEXT_COLOR_OPTIONS_RED_DARK_FG          13
#define TEXT_COLOR_OPTIONS_RED_DARK_SHADOW      14

// Menu draw and input functions
struct // MENU_MAIN
{
    void (*drawChoices)(int selection, int y);
    int (*processInput)(int selection);
} static const sItemFunctionsMain[MENUITEM_MAIN_COUNT] =
{
    [MENUITEM_MAIN_UI]                      = {DrawChoices_UI,               ProcessInput_Options_Two},
    [MENUITEM_MAIN_UI_BAG]                  = {DrawChoices_UiBag,            ProcessInput_Options_Three},
    [MENUITEM_MAIN_MIST]                    = {DrawChoices_Mist,             ProcessInput_Options_Two},
    [MENUITEM_MAIN_SPAWN_RATE]              = {DrawChoices_SpawnRate,        ProcessInput_Options_Three},
    [MENUITEM_MAIN_WEATHER_SYSTEM]          = {DrawChoices_WeatherSystem,    ProcessInput_Options_Two},
    [MENUITEM_MAIN_POND]                    = {DrawChoices_Pond,             ProcessInput_Options_Two},
    [MENUITEM_MAIN_WEATHER_SPREAD]          = {DrawChoices_WeatherSpread,    ProcessInput_Options_Three},
    [MENUITEM_MAIN_PUDDLES]                 = {DrawChoices_Puddles,          ProcessInput_Options_Three},
    [MENUITEM_MAIN_TEXTSPEED]               = {DrawChoices_TextSpeed,        ProcessInput_Options_Four},
    [MENUITEM_MAIN_FONT]                    = {DrawChoices_Font,             ProcessInput_Options_Two},
    [MENUITEM_MAIN_DIFFICULTY]              = {DrawChoices_Difficulty,       ProcessInput_Difficulty},
    [MENUITEM_MAIN_BUTTONMODE]              = {DrawChoices_ButtonMode,       ProcessInput_Options_Three},
    [MENUITEM_MAIN_FOLLOWER]                = {DrawChoices_Follower,         ProcessInput_Options_Two},
    [MENUITEM_MAIN_LARGE_FOLLOWER]          = {DrawChoices_LargeFollower,    ProcessInput_Options_Two},
    [MENUITEM_MAIN_AUTORUN]                 = {DrawChoices_Autorun,          ProcessInput_Options_Three},
    [MENUITEM_MAIN_AUTORUN_SURF]            = {DrawChoices_Autorun_Surf,     ProcessInput_Options_Two},
    [MENUITEM_MAIN_AUTORUN_DIVE]            = {DrawChoices_Autorun_Dive,     ProcessInput_Options_Two},
    [MENUITEM_MAIN_MATCHCALL]               = {DrawChoices_MatchCall,        ProcessInput_Options_Two},
    [MENUITEM_MAIN_FISHING]                 = {DrawChoices_Fishing,          ProcessInput_Options_Two},
    [MENUITEM_MAIN_EVEN_FASTER_JOY]         = {DrawChoices_EvenFasterJoy,    ProcessInput_Options_Two},
    [MENUITEM_MAIN_SKIP_INTRO]              = {DrawChoices_Skip_Intro,       ProcessInput_Options_Two},
    [MENUITEM_MAIN_UNIT_TYPE]               = {DrawChoices_Unit_Type,        ProcessInput_Options_Two},
    [MENUITEM_MAIN_FRAMETYPE]               = {DrawChoices_FrameType,        ProcessInput_FrameType},
    [MENUITEM_MAIN_BRIGHTER_NIGHTS]         = {DrawChoices_BrighterNights,   ProcessInput_Options_Two},
    [MENUITEM_MAIN_SURFOVERWORLD]           = {DrawChoices_SurfOverworld,    ProcessInput_Options_Two},
};

struct // MENU_CUSTOM
{
    void (*drawChoices)(int selection, int y);
    int (*processInput)(int selection);
} static const sItemFunctionsCustom[MENUITEM_BATTLE_COUNT] =
{
    [MENUITEM_BATTLE_BATTLESCENE]      = {DrawChoices_BattleScene,        ProcessInput_Options_Six},
    [MENUITEM_BATTLE_BATTLESTYLE]      = {DrawChoices_BattleStyle,        ProcessInput_BattleStyle},
    [MENUITEM_BATTLE_FAST_INTRO]       = {DrawChoices_FastIntro,          ProcessInput_Options_Two},
    [MENUITEM_BATTLE_SPLIT]            = {DrawChoices_Style,              ProcessInput_Options_Two},
    [MENUITEM_BATTLE_TYPE_EFFECTIVE]   = {DrawChoices_TypeEffective,      ProcessInput_Options_Two},
    [MENUITEM_BATTLE_FAST_BATTLES]     = {DrawChoices_FastBattles,        ProcessInput_Options_Two},
    [MENUITEM_BATTLE_BATTLE_SPEED]     = {DrawChoices_BattleSpeed,        ProcessInput_Options_Two},
    [MENUITEM_BATTLE_RUN_TYPE]         = {DrawChoices_Run_Type,           ProcessInput_Options_Four},
    [MENUITEM_BATTLE_LR_RUN]           = {DrawChoices_LR_Run,             ProcessInput_Options_Two},
    [MENUITEM_BATTLE_BALL_PROMPT]      = {DrawChoices_Ball_Prompt,        ProcessInput_Options_Two},
    [MENUITEM_BATTLE_OWE]              = {DrawChoices_OWE,                ProcessInput_Options_Three},
    [MENUITEM_BATTLE_WEATHER_FX]       = {DrawChoices_WeatherFx,          ProcessInput_Options_Two},
    [MENUITEM_BATTLE_NEW_BACKGROUNDS]  = {DrawChoices_Terrain_Circles,    ProcessInput_Options_Two},
    [MENUITEM_BATTLE_BACKGROUND]       = {DrawChoices_New_Backgrounds,    ProcessInput_Options_Four},
    [MENUITEM_BATTLE_SHADOWS]          = {DrawChoices_Shadows,            ProcessInput_Options_Two},
    [MENUITEM_BATTLE_CURSOR_MEMORY]    = {DrawChoices_CursorMemory,       ProcessInput_Options_Two},
};

struct // MENU_SOUND
{
    void (*drawChoices)(int selection, int y);
    int (*processInput)(int selection);
} static const sItemFunctionsSound[MENUITEM_SOUND_COUNT] =
{
    [MENUITEM_SOUND_SOUND]                         = {DrawChoices_Sound,                                 ProcessInput_Options_Two},
    [MENUITEM_SOUND_MUSIC]                         = {DrawChoices_Music,                                 ProcessInput_Options_Two},
    [MENUITEM_SOUND_BIKE_MUSIC]                    = {DrawChoices_BikeMusic,                             ProcessInput_Options_Two},
    [MENUITEM_SOUND_SURF_MUSIC]                    = {DrawChoices_SurfMusic,                             ProcessInput_Options_Two},
    [MENUITEM_SOUND_WILD_MON_MUSIC]                = {DrawChoices_Wild_Battle_Music,                     ProcessInput_Options_Seven},
    [MENUITEM_SOUND_BATTLE_TRAINER_MUSIC]          = {DrawChoices_Trainer_Battle_Music,                  ProcessInput_Options_Seven},
    [MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC] = {DrawChoices_Frontier_Trainer_Battle_Music,         ProcessInput_Options_Seven},
    [MENUITEM_SOUND_EFFECTS]                       = {DrawChoices_Sound_Effects,                         ProcessInput_Options_Three},
};

// Menu left side option names text
static const u8 sText_OptionTypeEffective[]       = _("STAB/EFFECTIVE");
static const u8 sText_OptionFishing[]             = _("EASIER FISHING");
static const u8 sText_OptionFastIntro[]           = _("INTRO");
static const u8 sText_OptionLargeFollower[]       = _("BIG FOLLOWERS");
static const u8 sText_OptionFastBattles[]         = _("BATTLE TEXT");
static const u8 sText_OptionBattleSpeed[]         = _("ANIM SPEED");
static const u8 sText_OptionEvenFasterJoy[]       = _("EVEN FASTER JOY");
static const u8 sText_OptionSkipIntro[]           = _("SKIP INTRO");
static const u8 sText_OptionLR_Run[]              = _("RUN PROMPT");
static const u8 sText_OptionBallPrompt[]          = _("BALL PROMPT");
static const u8 sText_OptionUnitType[]            = _("UNIT SYSTEM");
static const u8 sText_OptionNewBackgrounds[]      = _("BATTLE TERRAIN");
static const u8 sText_OptionBattleBackground[]    = _("BACKGROUND");
static const u8 sText_OptionOWE[]                 = _("OWE");
static const u8 sText_OptionWeatherFx[]           = _("WEATHER EFFECT");
static const u8 sText_OptionUI[]                  = _("UI INTERFACE");
static const u8 sText_OptionWeatherSystem[]       = _("WEATHER SYSTEM");
static const u8 sText_OptionWeatherSpread[]       = _("WEATHER SPREAD");
static const u8 sText_OptionPond[]                = _("POND BEHAVIOR");
static const u8 sText_OptionUiBag[]               = _("UI BAG");
static const u8 sText_OptionMist[]                = _("MIST");
static const u8 sText_OptionSpawnRate[]           = _("SPAWN RATE");
static const u8 sText_OptionAutoRun[]             = _("AUTO RUN");
static const u8 sText_OptionPkmnAnimation[]       = _("{PKMN}'S ANIMATION");
static const u8 sText_OptionPuddles[]             = _("PUDDLES");
static const u8 sText_OptionShadows[]             = _("SHADOWS");
static const u8 sText_OptionRunType[]             = _("QUICK RUN");
static const u8 sText_AutorunEnable_Surf[]        = _("AUTORUN (SURF)");
static const u8 sText_AutorunEnable_Dive[]        = _("AUTORUN (DIVE)");
static const u8 sText_SurfSprites[]               = _("SURF SPRITES");
static const u8 sText_BrighterNights[]            = _("BRIGHT NIGHTS");
static const u8 sText_FontType[]                  = _("FONT TYPE");
static const u8 *const sOptionMenuItemsNamesMain[MENUITEM_MAIN_COUNT] =
{
    [MENUITEM_MAIN_UI]                  = sText_OptionUI,
    [MENUITEM_MAIN_UI_BAG]              = sText_OptionUiBag,
    [MENUITEM_MAIN_MIST]                = sText_OptionMist,
    [MENUITEM_MAIN_SPAWN_RATE]          = sText_OptionSpawnRate,
    [MENUITEM_MAIN_WEATHER_SYSTEM]      = sText_OptionWeatherSystem,
    [MENUITEM_MAIN_POND]                = sText_OptionPond,
    [MENUITEM_MAIN_WEATHER_SPREAD]      = sText_OptionWeatherSpread,
    [MENUITEM_MAIN_PUDDLES]             = sText_OptionPuddles,
    [MENUITEM_MAIN_TEXTSPEED]           = gText_TextSpeed,
    [MENUITEM_MAIN_FONT]                = sText_FontType,
    [MENUITEM_MAIN_DIFFICULTY]          = gText_OptionDifficulty,
    [MENUITEM_MAIN_BUTTONMODE]          = gText_ButtonMode,
    [MENUITEM_MAIN_FOLLOWER]            = gText_FollowerEnable,
    [MENUITEM_MAIN_LARGE_FOLLOWER]      = sText_OptionLargeFollower,
    [MENUITEM_MAIN_AUTORUN]             = sText_OptionAutoRun,
    [MENUITEM_MAIN_AUTORUN_SURF]        = sText_AutorunEnable_Surf,
    [MENUITEM_MAIN_AUTORUN_DIVE]        = sText_AutorunEnable_Dive,
    [MENUITEM_MAIN_MATCHCALL]           = gText_OptionMatchCalls,
    [MENUITEM_MAIN_FISHING]             = sText_OptionFishing,
    [MENUITEM_MAIN_EVEN_FASTER_JOY]     = sText_OptionEvenFasterJoy,
    [MENUITEM_MAIN_SKIP_INTRO]          = sText_OptionSkipIntro,
    [MENUITEM_MAIN_UNIT_TYPE]           = sText_OptionUnitType,
    [MENUITEM_MAIN_FRAMETYPE]           = gText_Frame,
    [MENUITEM_MAIN_BRIGHTER_NIGHTS]     = sText_BrighterNights,
    [MENUITEM_MAIN_SURFOVERWORLD]       = sText_SurfSprites,
};

static const u8 sText_CursorMemory[]              = _("CURSOR MEMORY");
static const u8 *const sOptionMenuItemsNamesCustom[MENUITEM_BATTLE_COUNT] =
{
    [MENUITEM_BATTLE_BATTLESTYLE]      = gText_BattleStyle,
    [MENUITEM_BATTLE_BATTLESCENE]      = sText_OptionPkmnAnimation,
    [MENUITEM_BATTLE_FAST_INTRO]       = sText_OptionFastIntro,
    [MENUITEM_BATTLE_SPLIT]            = gText_OptionStyle,
    [MENUITEM_BATTLE_TYPE_EFFECTIVE]   = sText_OptionTypeEffective,
    [MENUITEM_BATTLE_FAST_BATTLES]     = sText_OptionFastBattles,
    [MENUITEM_BATTLE_BATTLE_SPEED]     = sText_OptionBattleSpeed,
    [MENUITEM_BATTLE_RUN_TYPE]         = sText_OptionRunType,
    [MENUITEM_BATTLE_LR_RUN]           = sText_OptionLR_Run,
    [MENUITEM_BATTLE_BALL_PROMPT]      = sText_OptionBallPrompt,
    [MENUITEM_BATTLE_OWE]              = sText_OptionOWE,
    [MENUITEM_BATTLE_WEATHER_FX]       = sText_OptionWeatherFx,
    [MENUITEM_BATTLE_NEW_BACKGROUNDS]  = sText_OptionNewBackgrounds,
    [MENUITEM_BATTLE_BACKGROUND]       = sText_OptionBattleBackground,
    [MENUITEM_BATTLE_SHADOWS]          = sText_OptionShadows,
    [MENUITEM_BATTLE_CURSOR_MEMORY]    = sText_CursorMemory,
};

static const u8 sText_OptionMusic[]                  = _("MUSIC");
static const u8 sText_OptionSurfMusic[]              = _("SURF MUSIC");
static const u8 sText_OptionBikeMusic[]              = _("BIKE MUSIC");
static const u8 sText_OptionWildMonMusic[]           = _("WILD MUSIC");
static const u8 sText_OptionTrainerBattleMusic[]     = _("TRAINER MUSIC");
static const u8 sText_OptionFrontierTrainerBattleMusic[]     = _("FRONTIER MUSIC");
static const u8 sText_OptionSoundEffects[]           = _("SOUND EFFECTS");
static const u8 *const sOptionMenuItemsNamesSound[MENUITEM_SOUND_COUNT] =
{
    [MENUITEM_SOUND_SOUND]                           = gText_Sound,
    [MENUITEM_SOUND_MUSIC]                           = sText_OptionMusic,
    [MENUITEM_SOUND_BIKE_MUSIC]                      = sText_OptionBikeMusic,
    [MENUITEM_SOUND_SURF_MUSIC]                      = sText_OptionSurfMusic,
    [MENUITEM_SOUND_WILD_MON_MUSIC]                  = sText_OptionWildMonMusic,
    [MENUITEM_SOUND_BATTLE_TRAINER_MUSIC]            = sText_OptionTrainerBattleMusic,
    [MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC]   = sText_OptionFrontierTrainerBattleMusic,
    [MENUITEM_SOUND_EFFECTS]                         = sText_OptionSoundEffects,
};

static const u8 *const OptionTextRight(u8 menuItem)
{
    switch (sOptions->submenu)
    {
    case MENU_MAIN:     return sOptionMenuItemsNamesMain[menuItem];
    case MENU_CUSTOM:   return sOptionMenuItemsNamesCustom[menuItem];
    case MENU_SOUND:    return sOptionMenuItemsNamesSound[menuItem];
    }
}

// Menu left side text conditions
static bool8 CheckConditions(int selection)
{
    switch (sOptions->submenu)
    {
    case MENU_MAIN:
        switch(selection)
        {
        case MENUITEM_MAIN_UI:                return TRUE;
        case MENUITEM_MAIN_WEATHER_SYSTEM:    return TRUE;
        case MENUITEM_MAIN_POND:              return TRUE;
        case MENUITEM_MAIN_UI_BAG:            return TRUE;
        case MENUITEM_MAIN_MIST:              return TRUE;
        case MENUITEM_MAIN_SPAWN_RATE:        return TRUE;
        case MENUITEM_MAIN_WEATHER_SPREAD:    return !FlagGet(FLAG_WEATHER_SCRIPTED);
        case MENUITEM_MAIN_PUDDLES:           return TRUE;
        case MENUITEM_MAIN_TEXTSPEED:         return TRUE;
        case MENUITEM_MAIN_FONT:              return TRUE;
        case MENUITEM_MAIN_DIFFICULTY:        return TRUE;

        case MENUITEM_MAIN_BUTTONMODE:        return TRUE;
        case MENUITEM_MAIN_FRAMETYPE:         return TRUE;
        case MENUITEM_MAIN_FOLLOWER:          return TRUE;
        case MENUITEM_MAIN_LARGE_FOLLOWER:    return TRUE;
        case MENUITEM_MAIN_AUTORUN:           return TRUE;
        case MENUITEM_MAIN_AUTORUN_SURF:      return TRUE;
        case MENUITEM_MAIN_AUTORUN_DIVE:      return TRUE;
        case MENUITEM_MAIN_COUNT:             return TRUE;
        case MENUITEM_MAIN_MATCHCALL:         return TRUE;
        case MENUITEM_MAIN_FISHING:           return TRUE;
        case MENUITEM_MAIN_EVEN_FASTER_JOY:   return TRUE;
        case MENUITEM_MAIN_SKIP_INTRO:        return TRUE;
        case MENUITEM_MAIN_UNIT_TYPE:         return TRUE;
        case MENUITEM_MAIN_SURFOVERWORLD:     return TRUE;
        case MENUITEM_MAIN_BRIGHTER_NIGHTS:   return TRUE;
        }
    case MENU_CUSTOM:
        switch(selection)
        {
        case MENUITEM_BATTLE_BATTLESCENE:     return TRUE;
        case MENUITEM_BATTLE_BATTLESTYLE:     return TRUE;
        case MENUITEM_BATTLE_FAST_INTRO:      return TRUE;
        case MENUITEM_BATTLE_SPLIT:           return TRUE;
        case MENUITEM_BATTLE_TYPE_EFFECTIVE:  return TRUE;
        case MENUITEM_BATTLE_FAST_BATTLES:    return TRUE;
        case MENUITEM_BATTLE_BATTLE_SPEED:    return TRUE;
        case MENUITEM_BATTLE_RUN_TYPE:        return TRUE;
        case MENUITEM_BATTLE_LR_RUN:          return sOptions->sel_battle[MENUITEM_BATTLE_RUN_TYPE] == 1 || sOptions->sel_battle[MENUITEM_BATTLE_RUN_TYPE] == 3;
        case MENUITEM_BATTLE_BALL_PROMPT:     return TRUE;
        case MENUITEM_BATTLE_OWE: return TRUE;
        case MENUITEM_BATTLE_WEATHER_FX: return TRUE;
        case MENUITEM_BATTLE_NEW_BACKGROUNDS: return TRUE;
        case MENUITEM_BATTLE_BACKGROUND: return TRUE;
        case MENUITEM_BATTLE_SHADOWS: return TRUE;
        case MENUITEM_BATTLE_CURSOR_MEMORY:   return TRUE;
        case MENUITEM_BATTLE_COUNT:           return TRUE;
        }
    case MENU_SOUND:
        switch(selection)
        {
        case MENUITEM_SOUND_SOUND:                            return TRUE;
        case MENUITEM_SOUND_MUSIC:                            return TRUE;
        case MENUITEM_SOUND_SURF_MUSIC:                       return TRUE;
        case MENUITEM_SOUND_BIKE_MUSIC:                       return TRUE;
        case MENUITEM_SOUND_WILD_MON_MUSIC:                   return TRUE;
        case MENUITEM_SOUND_BATTLE_TRAINER_MUSIC:             return TRUE;
        case MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC:    return TRUE;
        case MENUITEM_SOUND_EFFECTS:                          return TRUE;
        }
    }
}

// Descriptions
static const u8 sText_Empty[]                   = _("");
static const u8 sText_Desc_Save[]               = _("Save your settings.");
static const u8 sText_Desc_TextSpeed[]          = _("Choose one of the four text-display\nspeeds.");
static const u8 sText_Desc_Font_Em[]            = _("{COLOR 9}{COLOR 10}Emerald{COLOR 2} font type. Exit the Options\nMenu to properly apply the option.");
static const u8 sText_Desc_Font_FRLG[]          = _("{COLOR 7}{COLOR 8}FR{COLOR 9}{COLOR 10}LG{COLOR 2} font type. Exit the Options Menu\nto properly apply the option.");
static const u8 sText_Desc_BattleScene_On[]     = _("Show the Pokémon animations\nand attack animations.");
static const u8 sText_Desc_BattleScene_Off[]    = _("Skip the Pokémon animations\nand attack animations.");
static const u8 sText_Desc_Difficulty_Easy[]    = _("Change the difficulty to Easy.\nEverything is easier.");
static const u8 sText_Desc_Difficulty_Normal[]  = _("Change the difficulty to Normal.\nVanilla experience.");
static const u8 sText_Desc_Difficulty_Hard[]    = _("Change the difficulty to Hard.\nIncludes extra challenges.");
static const u8 sText_Desc_BattleStyle_Shift[]  = _("Get the option to switch your\nPokémon after the enemies faints.");
static const u8 sText_Desc_BattleStyle_Set[]    = _("No free switch after fainting the\nenemies Pokémon.");
static const u8 sText_Desc_ButtonMode[]         = _("All buttons work as normal.");
static const u8 sText_Desc_ButtonMode_LR[]      = _("On some screens the L and R buttons\nact as left and right.");
static const u8 sText_Desc_ButtonMode_LA[]      = _("The L button acts as another A\nbutton for one-handed play.");
static const u8 sText_Desc_FrameType[]          = _("Choose the frame surrounding the\nwindows. Frames 21-29 are from {COLOR 7}{COLOR 8}FR{COLOR 9}{COLOR 10}LG{COLOR 2}.");
static const u8 sText_Desc_FollowerOn[]            = _("Let the first Pokémon in your\nparty follow you.");
static const u8 sText_Desc_FollowerOff[]           = _("Walk alone.");
static const u8 sText_Desc_FollowerLargeOn[]       = _("Enable large {PKMN} followers.\n{COLOR 7}{COLOR 8}Can cause graphical issues.");
static const u8 sText_Desc_FollowerLargeOff[]      = _("Disable large {PKMN} followers.\nRecommended.");
static const u8 sText_Desc_AutorunOn[]             = _("Run without pressing B.");
static const u8 sText_Desc_AutorunOff[]            = _("Press and hold B to run.");
static const u8 sText_Desc_AutorunSurfOn[]         = _("Surf faster without pressing B.");
static const u8 sText_Desc_AutorunSurfOff[]        = _("Press and hold B to surf faster.");
static const u8 sText_Desc_AutorunDiveOn[]         = _("Surf underwater faster\nwithout pressing B.");
static const u8 sText_Desc_AutorunDiveOff[]        = _("Press and hold B to surf\nunderwater faster.");
static const u8 sText_Desc_FishingOn[]             = _("Automatically reel while fishing.");
static const u8 sText_Desc_FishingOff[]            = _("Manually reel while fishing.\nFish like you always fished!");
static const u8 sText_Desc_EvenFasterJoyOn[]       = _("Nurse Joy heals you extremely fast.\nFor those who cannot wait.");
static const u8 sText_Desc_EvenFasterJoyOff[]      = _("Nurse Joy heals you fast, but\nwith the usual animation.");
static const u8 sText_Desc_SkipIntroOn[]           = _("Skips the Copyright screen and\nintro. Applies to soft-resets.");
static const u8 sText_Desc_SkipIntroOff[]          = _("Shows the Copyright screen and\nthe game's introduction.");
static const u8 sText_Desc_OverworldCallsOn[]      = _("Trainers will be able to call you,\noffering rematches and info.");
static const u8 sText_Desc_OverworldCallsOff[]     = _("You will not receive calls.\nSpecial events will still occur.");
static const u8 sText_Desc_Units_Imperial[]        = _("Display Berry and Pokémon weight\nand size in pounds and inches.");
static const u8 sText_Desc_Units_Metric[]          = _("Display Berry and Pokémon weight\nand size in kilograms and meters.");
static const u8 sText_Desc_SurfOverworldDynamic[]       = _("Use the Pokémon's sprite when\nsurfing.");
static const u8 sText_Desc_SurfOverworldOriginal[]      = _("Use the original generic sprite when\nsurfing.");
static const u8 sText_Desc_BrighterNightsOn[]           = _("Night shading is less dark.\nEasier to see at night.");
static const u8 sText_Desc_BrighterNightsOff[]          = _("Night shading at full darkness.\nOriginal intensity.");
static const u8 sText_Desc_Weather_Scripted[]      = _("Original weather: each place\nkeeps the weather of the game.");
static const u8 sText_Desc_Weather_Perfect[]       = _("Perfect (dynamic): weather\nchanges with the real time.");
static const u8 sText_Desc_Spread_Normal[]         = _("Weather changes at 5, 10, 17\nand 21. Small weather areas.");
static const u8 sText_Desc_Spread_Wide[]           = _("Changes at 0, 5, 10, 14, 18.\nEach weather covers 2 areas.");
static const u8 sText_Desc_Spread_Perfect[]        = _("Weather changes at 0, 5, 10,\n14 and 18. Small weather areas.");
static const u8 sText_Desc_Autorun_Off[]            = _("Hold B to run.");
static const u8 sText_Desc_Autorun_Land[]           = _("Run without holding B on land;\nhold B to walk.");
static const u8 sText_Desc_Autorun_All[]            = _("Run without holding B on land,\nsurfing and diving.");
static const u8 sText_Desc_Spawn_Less[]             = _("Weather and time Pokemon are\nrare (2%).");
static const u8 sText_Desc_Spawn_More[]             = _("Weather and time Pokemon are\ncommon (10-15%).");
static const u8 sText_Desc_Spawn_Perfect[]          = _("Weather and time Pokemon now\nand then (7%).");
static const u8 sText_Desc_Mist_Old[]               = _("Mist clouds brighten what is\nunder them.");
static const u8 sText_Desc_Mist_Perfect[]           = _("Soft drifting mist over\neverything, with reflections.");
static const u8 sText_Desc_UiBag_Vanilla[]          = _("The original Bag.");
static const u8 sText_Desc_UiBag_Modern[]           = _("The new Bag look.");
static const u8 sText_Desc_UiBag_Perfect[]          = _("The new Bag look, with your\nparty beside the items.");
static const u8 sText_Desc_Pond_Dynamic[]          = _("In snow, ponds freeze over\n(the sea never does).");
static const u8 sText_Desc_Pond_Default[]          = _("Ponds stay water in every\nweather.");
static const u8 sText_Desc_Puddles_Off[]           = _("No puddles, ice or snow\npiles.");
static const u8 sText_Desc_Puddles_Slow[]          = _("Puddles, ice and snow piles\nfade in and out in 2 seconds.");
static const u8 sText_Desc_Puddles_Fast[]          = _("Puddles, ice and snow piles\nfade in and out in 1 second.");
static const u8 sText_Desc_UI_Default[]            = _("The usual start menu.");
static const u8 sText_Desc_UI_Perfect[]            = _("The Perfect start menu with\nicons, time and hints.");
static const u8 *const sOptionMenuItemDescriptionsMain[MENUITEM_MAIN_COUNT][3] =
{
    [MENUITEM_MAIN_UI]                = {sText_Desc_UI_Default,           sText_Desc_UI_Perfect,           sText_Empty},
    [MENUITEM_MAIN_WEATHER_SYSTEM]    = {sText_Desc_Weather_Scripted,     sText_Desc_Weather_Perfect,      sText_Empty},
    [MENUITEM_MAIN_WEATHER_SPREAD]    = {sText_Desc_Spread_Normal,        sText_Desc_Spread_Wide,          sText_Desc_Spread_Perfect},
    [MENUITEM_MAIN_POND]              = {sText_Desc_Pond_Dynamic,         sText_Desc_Pond_Default,         sText_Empty},
    [MENUITEM_MAIN_UI_BAG]            = {sText_Desc_UiBag_Vanilla,        sText_Desc_UiBag_Modern,         sText_Desc_UiBag_Perfect},
    [MENUITEM_MAIN_MIST]              = {sText_Desc_Mist_Old,             sText_Desc_Mist_Perfect,         sText_Empty},
    [MENUITEM_MAIN_SPAWN_RATE]        = {sText_Desc_Spawn_Less,           sText_Desc_Spawn_More,           sText_Desc_Spawn_Perfect},
    [MENUITEM_MAIN_PUDDLES]           = {sText_Desc_Puddles_Off,          sText_Desc_Puddles_Slow,         sText_Desc_Puddles_Fast},
    [MENUITEM_MAIN_TEXTSPEED]         = {sText_Desc_TextSpeed,            sText_Empty,                     sText_Empty},
    [MENUITEM_MAIN_FONT]              = {sText_Desc_Font_Em,              sText_Desc_Font_FRLG,            sText_Empty},
    [MENUITEM_MAIN_DIFFICULTY]        = {sText_Desc_Difficulty_Easy,      sText_Desc_Difficulty_Normal,    sText_Desc_Difficulty_Hard},
    [MENUITEM_MAIN_BUTTONMODE]        = {sText_Desc_ButtonMode,           sText_Desc_ButtonMode_LR,        sText_Desc_ButtonMode_LA},
    [MENUITEM_MAIN_FRAMETYPE]         = {sText_Desc_FrameType,            sText_Empty,                     sText_Empty},
    [MENUITEM_MAIN_FOLLOWER]          = {sText_Desc_FollowerOn,           sText_Desc_FollowerOff},
    [MENUITEM_MAIN_LARGE_FOLLOWER]    = {sText_Desc_FollowerLargeOn,      sText_Desc_FollowerLargeOff},
    [MENUITEM_MAIN_AUTORUN]           = {sText_Desc_Autorun_Off,          sText_Desc_Autorun_Land,         sText_Desc_Autorun_All},
    [MENUITEM_MAIN_AUTORUN_SURF]      = {sText_Desc_AutorunSurfOn,        sText_Desc_AutorunSurfOff},
    [MENUITEM_MAIN_AUTORUN_DIVE]      = {sText_Desc_AutorunDiveOn,        sText_Desc_AutorunDiveOff},
    [MENUITEM_MAIN_MATCHCALL]         = {sText_Desc_OverworldCallsOn,     sText_Desc_OverworldCallsOff},
    [MENUITEM_MAIN_FISHING]           = {sText_Desc_FishingOn,            sText_Desc_FishingOff},
    [MENUITEM_MAIN_EVEN_FASTER_JOY]   = {sText_Desc_EvenFasterJoyOn,      sText_Desc_EvenFasterJoyOff},
    [MENUITEM_MAIN_SKIP_INTRO]        = {sText_Desc_SkipIntroOn,          sText_Desc_SkipIntroOff},
    [MENUITEM_MAIN_UNIT_TYPE]         = {sText_Desc_Units_Metric,         sText_Desc_Units_Imperial},
    [MENUITEM_MAIN_SURFOVERWORLD]     = {sText_Desc_SurfOverworldDynamic, sText_Desc_SurfOverworldOriginal},
    [MENUITEM_MAIN_BRIGHTER_NIGHTS]   = {sText_Desc_BrighterNightsOff,     sText_Desc_BrighterNightsOn},
};

// Custom {PKMN}
static const u8 sText_Desc_StyleOn[]               = _("Physical and Special Moves\nare Move specific.");
static const u8 sText_Desc_StyleOff[]              = _("Physical and Special Moves\ndepend on the Pokémon Type.");
static const u8 sText_Desc_TypeEffectiveOn[]       = _("Show effectiveness and STAB ({COLOR 3}{COLOR 4}Same\n{COLOR 3}{COLOR 4}Type Attack Bonus{COLOR 2}) in battles.");
static const u8 sText_Desc_TypeEffectiveOff[]      = _("Type effectiveness and STAB\nwon't be shown in battles.");
static const u8 sText_Desc_FastIntroOn[]           = _("Skip the sliding animation\nand enter battles faster.");
static const u8 sText_Desc_FastIntroOff[]          = _("Battles load at the usual speed.");
static const u8 sText_Desc_FastBattleOn[]          = _("Skips all delays in battles, which\nmakes them faster.");
static const u8 sText_Desc_FastBattleOff[]         = _("Manual delay skipping. You can\npress A or B to skip delays.");
static const u8 sText_Desc_BattleSpeedOn[]         = _("Battle animations play at\nnormal speed.");
static const u8 sText_Desc_BattleSpeedOff[]        = _("Doubles the speed of HP bars, EXP,\nanims, faints, and switches.");
static const u8 sText_Desc_Run_Type_Off[]          = _("No quick running from battles.");
static const u8 sText_Desc_Run_Type_LR[]           = _("Hold {L_BUTTON}+{R_BUTTON}, then {A_BUTTON} to run from\nbattles before they start.");
static const u8 sText_Desc_Run_Type_B[]            = _("Press {B_BUTTON} to move the cursor to the Run\noption after the battle started.");
static const u8 sText_Desc_Run_Type_B_2[]          = _("Hold {B_BUTTON} to run from battles before\nthey start.");
static const u8 sText_Desc_LR_Run_On[]             = _("Enables a prompt to show that you\ncan run away from battles.");
static const u8 sText_Desc_LR_Run_Off[]            = _("Disables said prompt to flee.\nButton combo still works.");
static const u8 sText_Desc_Ball_Prompt_On[]        = _("Press {R_BUTTON} in battle to use Pokeballs.\nHold {L_BUTTON}/{R_BUTTON} to swap Pokéballs.");
static const u8 sText_Desc_Ball_Prompt_Off[]       = _("Disables the prompt to use\nPokéballs quickly.");
static const u8 sText_Desc_NewBackgrounds_Old[]    = _("Original battle backgrounds.");
static const u8 sText_Desc_NewBackgrounds_New[]    = _("Backgrounds from the CFRU\nFire Red backgrounds patch.");
static const u8 sText_Desc_NewBackgrounds_MPlus[]  = _("Modern + backgrounds: new\npictures by time of day.");
static const u8 sText_Desc_NewBackgrounds_Cfru[]   = _("Almost Perfect backgrounds:\nCFRU with new grass and more.");
static const u8 sText_AlmostPerfect[]              = _("Almost Perfect");
static const u8 sText_Desc_WeatherFx_Regular[]     = _("Weather only shows when it\nhits, like the original.");
static const u8 sText_Desc_WeatherFx_Perfect[]     = _("Weather keeps falling and\ntints the battle all along.");
static const u8 sText_Desc_BattleScene_Extended[]  = _("Animations on, and the foe\nPokémon keeps moving.");
static const u8 sText_Desc_OWE_Off[]               = _("Wild Pokémon only appear\nat random in the grass.");
static const u8 sText_Desc_OWE_On[]                = _("Wild Pokémon walk around in\nthe grass. Touch one to battle.");
static const u8 sText_Desc_OWE_Restrict[]          = _("Like On, but they can't\nleave the grass.");
static const u8 sText_Desc_Shadows_Off[]           = _("No shadows under the\nopposing Pokémon.");
static const u8 sText_Desc_Shadows_On[]            = _("Shadows under the\nopposing Pokémon.");
static const u8 sText_Desc_TerrainCircles_With[]   = _("Terrain circles under the\nPokémon on every background.");
static const u8 sText_Desc_TerrainCircles_Without[]= _("No terrain circles.");
static const u8 sText_Cfru[]                       = _("Perfect");
static const u8 sText_Desc_CursorMemoryOn[]        = _("The cursor in battle remembers\nthe {PKMN}'s last target.");
static const u8 sText_Desc_CursorMemoryOff[]       = _("The cursor in battle does not\nremember the last target.");
static const u8 sText_Desc_Anims_Off[]     = _("No Pokemon animations at all\nin battle.");
static const u8 sText_Desc_Anims_Partial[] = _("Only the move animations.");
static const u8 sText_Desc_Anims_On[]      = _("Move animations, and {PKMN}\nanimate when they come out.");
static const u8 sText_Desc_Anims_Modern[]  = _("All animations, the foe also\nanimates now and then.");
static const u8 sText_Desc_Anims_Always[]  = _("All animations, the foe keeps\nanimating without a pause.");
static const u8 sText_Desc_Anims_Perfect[] = _("All animations, the foe animates\nwith a very short pause.");
static const u8 sText_Desc_Anims_Perfect2[] = _("All animations, the foe animates\nwith a 1 second pause.");
static const u8 *const sOptionMenuItemDescriptionsCustom[MENUITEM_BATTLE_COUNT][6] =
{
    [MENUITEM_BATTLE_BATTLESCENE]         = {sText_Desc_Anims_Off, sText_Desc_Anims_On, sText_Desc_Anims_Always, sText_Desc_Anims_Modern, sText_Desc_Anims_Perfect, sText_Desc_Anims_Perfect2},
    [MENUITEM_BATTLE_BATTLESTYLE]         = {sText_Desc_BattleStyle_Shift,        sText_Desc_BattleStyle_Set,        sText_Empty},
    [MENUITEM_BATTLE_FAST_INTRO]          = {sText_Desc_FastIntroOn,              sText_Desc_FastIntroOff},
    [MENUITEM_BATTLE_FAST_BATTLES]        = {sText_Desc_FastBattleOn,             sText_Desc_FastBattleOff},
    [MENUITEM_BATTLE_BATTLE_SPEED]        = {sText_Desc_BattleSpeedOn,            sText_Desc_BattleSpeedOff},
    [MENUITEM_BATTLE_SPLIT]               = {sText_Desc_StyleOn,                  sText_Desc_StyleOff},
    [MENUITEM_BATTLE_TYPE_EFFECTIVE]      = {sText_Desc_TypeEffectiveOn,          sText_Desc_TypeEffectiveOff},
    [MENUITEM_BATTLE_LR_RUN]              = {sText_Desc_LR_Run_On,                sText_Desc_LR_Run_Off},
    [MENUITEM_BATTLE_BALL_PROMPT]         = {sText_Desc_Ball_Prompt_On,           sText_Desc_Ball_Prompt_Off},
    [MENUITEM_BATTLE_WEATHER_FX]          = {sText_Desc_WeatherFx_Regular,        sText_Desc_WeatherFx_Perfect},
    [MENUITEM_BATTLE_OWE]                 = {sText_Desc_OWE_Off,                  sText_Desc_OWE_On,                   sText_Desc_OWE_Restrict},
    [MENUITEM_BATTLE_NEW_BACKGROUNDS]     = {sText_Desc_TerrainCircles_With,      sText_Desc_TerrainCircles_Without},
    [MENUITEM_BATTLE_BACKGROUND]          = {sText_Desc_NewBackgrounds_Old,       sText_Desc_NewBackgrounds_New,       sText_Desc_NewBackgrounds_MPlus, sText_Desc_NewBackgrounds_Cfru},
    [MENUITEM_BATTLE_SHADOWS]             = {sText_Desc_Shadows_Off,              sText_Desc_Shadows_On},
    [MENUITEM_BATTLE_RUN_TYPE]            = {sText_Desc_Run_Type_Off,             sText_Desc_Run_Type_LR,             sText_Desc_Run_Type_B,         sText_Desc_Run_Type_B_2},
    [MENUITEM_BATTLE_CURSOR_MEMORY]       = {sText_Desc_CursorMemoryOn,           sText_Desc_CursorMemoryOff},
};

static const u8 sText_Desc_SoundMono[]                       = _("Sound is the same in all speakers.\nRecommended for original hardware.");
static const u8 sText_Desc_SoundStereo[]                     = _("Play the left and right audio channel\nseperatly. Great with headphones.");
static const u8 sText_Desc_Music_On[]                        = _("Enables music playback.\nChange maps to take effect.");
static const u8 sText_Desc_Music_Off[]                       = _("Disables music playback.\nChange maps to take effect.");
static const u8 sText_Desc_BikeMusicOn[]                     = _("Enables Bike music.");
static const u8 sText_Desc_BikeMusicOff[]                    = _("Disables Bike music.");
static const u8 sText_Desc_SurfMusicOn[]                     = _("Enables Surf music.");
static const u8 sText_Desc_SurfMusicOff[]                    = _("Disables Surf music.");
static const u8 sText_Desc_WildMonMusic_Hoenn[]              = _("Default music from Hoenn.");
static const u8 sText_Desc_WildMonMusic_Kanto_Old[]          = _("Music from {COLOR 7}{COLOR 8}Fire Red{COLOR 2} and {COLOR 9}{COLOR 10}Leaf Green{COLOR 2}.");
static const u8 sText_Desc_WildMonMusic_Sinnoh[]             = _("Music from Diamond, Pearl and\nPlatinum.");
static const u8 sText_Desc_WildMonMusic_Johto[]              = _("Music from {COLOR 5}{COLOR 6}Heart Gold{COLOR 2} and {COLOR 3}{COLOR 4}Soul Silver{COLOR 2},\nbut from Johto.");
static const u8 sText_Desc_WildMonMusic_Kanto_New[]          = _("Music from {COLOR 5}{COLOR 6}Heart Gold{COLOR 2} and {COLOR 3}{COLOR 4}Soul Silver{COLOR 2},\nbut from Kanto.");
static const u8 sText_Desc_WildMonMusic_Random[]             = _("Randomizes music from all available\ngames.");
static const u8 sText_Desc_SoundEffects_Gen3[]               = _("Default sound effects from Gen III.");
static const u8 sText_Desc_SoundEffects_DP[]                 = _("Sound effects from Diamond, Pearl\nand Platinum.");
static const u8 sText_Desc_SoundEffects_HGSS[]               = _("Sound effects from {COLOR 5}{COLOR 6}Heart Gold{COLOR 2} and\n{COLOR 3}{COLOR 4}Soul Silver{COLOR 2}.");
static const u8 sText_Desc_WildMonMusic_BW[]                 = _("Music from Black and White.");

static const u8 *const sOptionMenuItemDescriptionsSound[MENUITEM_SOUND_COUNT][7] =
{
    [MENUITEM_SOUND_SOUND]          = {sText_Desc_SoundMono,              sText_Desc_SoundStereo,                 sText_Empty,                         sText_Empty,                       sText_Empty,                           sText_Empty},
    [MENUITEM_SOUND_MUSIC]          = {sText_Desc_Music_On,               sText_Desc_Music_Off,                   sText_Empty,                         sText_Empty,                       sText_Empty,                           sText_Empty},
    [MENUITEM_SOUND_BIKE_MUSIC]     = {sText_Desc_BikeMusicOn,            sText_Desc_BikeMusicOff,                sText_Empty,                         sText_Empty,                       sText_Empty,                           sText_Empty},
    [MENUITEM_SOUND_SURF_MUSIC]     = {sText_Desc_SurfMusicOn,            sText_Desc_SurfMusicOff,                sText_Empty,                         sText_Empty,                       sText_Empty,                           sText_Empty},
    [MENUITEM_SOUND_WILD_MON_MUSIC] = {sText_Desc_WildMonMusic_Hoenn,     sText_Desc_WildMonMusic_Kanto_Old,      sText_Desc_WildMonMusic_Sinnoh,      sText_Desc_WildMonMusic_Johto,     sText_Desc_WildMonMusic_Kanto_New,     sText_Desc_WildMonMusic_BW,                  sText_Desc_WildMonMusic_Random},
    [MENUITEM_SOUND_BATTLE_TRAINER_MUSIC] = {sText_Desc_WildMonMusic_Hoenn,     sText_Desc_WildMonMusic_Kanto_Old,      sText_Desc_WildMonMusic_Sinnoh,      sText_Desc_WildMonMusic_Johto,     sText_Desc_WildMonMusic_Kanto_New,     sText_Desc_WildMonMusic_BW,            sText_Desc_WildMonMusic_Random},
    [MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC] = {sText_Desc_WildMonMusic_Hoenn,     sText_Desc_WildMonMusic_Kanto_Old,      sText_Desc_WildMonMusic_Sinnoh,      sText_Desc_WildMonMusic_Johto,     sText_Desc_WildMonMusic_Kanto_New,     sText_Desc_WildMonMusic_BW,   sText_Desc_WildMonMusic_Random},
    [MENUITEM_SOUND_EFFECTS]        = {sText_Desc_SoundEffects_Gen3,     sText_Desc_SoundEffects_DP,      sText_Desc_SoundEffects_HGSS,      sText_Empty,     sText_Empty,     sText_Empty},
};

// Disabled Descriptions
static const u8 sText_Desc_Disabled_Textspeed[]     = _("Only active if xyz.");
static const u8 sText_Desc_Disabled_BattleHPBar[]   = _("Only active if xyz.");
static const u8 *const sOptionMenuItemDescriptionsDisabledMain[MENUITEM_MAIN_COUNT] =
{
    [MENUITEM_MAIN_UI]                = sText_Empty,
    [MENUITEM_MAIN_WEATHER_SYSTEM]    = sText_Empty,
    [MENUITEM_MAIN_WEATHER_SPREAD]    = sText_Empty,
    [MENUITEM_MAIN_POND]              = sText_Empty,
    [MENUITEM_MAIN_UI_BAG]            = sText_Empty,
    [MENUITEM_MAIN_MIST]              = sText_Empty,
    [MENUITEM_MAIN_SPAWN_RATE]        = sText_Empty,
    [MENUITEM_MAIN_PUDDLES]           = sText_Empty,
    [MENUITEM_MAIN_TEXTSPEED]         = sText_Desc_Disabled_Textspeed,
    [MENUITEM_MAIN_FONT]              = sText_Empty,
    [MENUITEM_MAIN_DIFFICULTY]        = sText_Empty,
    [MENUITEM_MAIN_BUTTONMODE]        = sText_Empty,
    [MENUITEM_MAIN_FRAMETYPE]         = sText_Empty,
    [MENUITEM_MAIN_FOLLOWER]          = sText_Desc_Disabled_BattleHPBar,
    [MENUITEM_MAIN_LARGE_FOLLOWER]    = sText_Desc_Disabled_BattleHPBar,
    [MENUITEM_MAIN_AUTORUN]           = sText_Empty,
    [MENUITEM_MAIN_AUTORUN_SURF]      = sText_Empty,
    [MENUITEM_MAIN_AUTORUN_DIVE]      = sText_Empty,
    [MENUITEM_MAIN_MATCHCALL]         = sText_Empty,
    [MENUITEM_MAIN_FISHING]           = sText_Empty,
    [MENUITEM_MAIN_EVEN_FASTER_JOY]   = sText_Empty,
    [MENUITEM_MAIN_SKIP_INTRO]        = sText_Empty,
    [MENUITEM_MAIN_SURFOVERWORLD]     = sText_Empty,
    [MENUITEM_MAIN_BRIGHTER_NIGHTS]   = sText_Empty,
};

// Disabled Custom
static const u8 sText_Desc_Disabled_LR_Run[]   = _("Only active if ‘L+R+A’ or ‘B’ is\nselected above.");
static const u8 *const sOptionMenuItemDescriptionsDisabledCustom[MENUITEM_BATTLE_COUNT] =
{
    [MENUITEM_BATTLE_BATTLESCENE]         = sText_Empty,
    [MENUITEM_BATTLE_BATTLESTYLE]         = sText_Empty,
    [MENUITEM_BATTLE_FAST_INTRO]          = sText_Empty,
    [MENUITEM_BATTLE_FAST_BATTLES]        = sText_Empty,
    [MENUITEM_BATTLE_BATTLE_SPEED]        = sText_Empty,
    [MENUITEM_BATTLE_SPLIT]               = sText_Empty,
    [MENUITEM_BATTLE_TYPE_EFFECTIVE]      = sText_Empty,
    [MENUITEM_BATTLE_LR_RUN]              = sText_Desc_Disabled_LR_Run,
    [MENUITEM_BATTLE_BALL_PROMPT]         = sText_Empty,
    [MENUITEM_BATTLE_OWE]                 = sText_Empty,
    [MENUITEM_BATTLE_WEATHER_FX]          = sText_Empty,
    [MENUITEM_BATTLE_NEW_BACKGROUNDS]     = sText_Empty,
    [MENUITEM_BATTLE_SHADOWS]             = sText_Empty,
    [MENUITEM_BATTLE_RUN_TYPE]            = sText_Empty,
    [MENUITEM_BATTLE_CURSOR_MEMORY]       = sText_Empty,
};

static const u8 *const sOptionMenuItemDescriptionsDisabledSound[MENUITEM_SOUND_COUNT] =
{
    [MENUITEM_SOUND_SOUND]          = sText_Empty,
    [MENUITEM_SOUND_MUSIC]          = sText_Empty,
    [MENUITEM_SOUND_BIKE_MUSIC]     = sText_Empty,
    [MENUITEM_SOUND_SURF_MUSIC]     = sText_Empty,
    [MENUITEM_SOUND_WILD_MON_MUSIC] = sText_Empty,
    [MENUITEM_SOUND_BATTLE_TRAINER_MUSIC] = sText_Empty,
    [MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC] = sText_Empty,
    [MENUITEM_SOUND_EFFECTS]        = sText_Empty,
};

static const u8 *const OptionTextDescription(void)
{
    u8 menuItem;
    u8 selection;

    if (IsHeaderRow(sOptions->menuCursor[sOptions->page]))
        return sText_Empty;
    menuItem = SelectRow(sOptions->menuCursor[sOptions->page]);

    switch (sOptions->submenu)
    {
    case MENU_MAIN:
        if (!CheckConditions(menuItem))
            return sOptionMenuItemDescriptionsDisabledMain[menuItem];
        selection = sOptions->sel[menuItem];
        if (menuItem == MENUITEM_MAIN_TEXTSPEED || menuItem == MENUITEM_MAIN_FRAMETYPE
         || selection >= ARRAY_COUNT(sOptionMenuItemDescriptionsMain[0]))
            selection = 0;
        return sOptionMenuItemDescriptionsMain[menuItem][selection] ? sOptionMenuItemDescriptionsMain[menuItem][selection] : sText_Empty;
    case MENU_CUSTOM:
        if (!CheckConditions(menuItem))
            return sOptionMenuItemDescriptionsDisabledCustom[menuItem];
        selection = sOptions->sel_battle[menuItem];
        if (selection >= ARRAY_COUNT(sOptionMenuItemDescriptionsCustom[0]))
            selection = 0;
        return sOptionMenuItemDescriptionsCustom[menuItem][selection] ? sOptionMenuItemDescriptionsCustom[menuItem][selection] : sText_Empty;
    case MENU_SOUND:
        if (!CheckConditions(menuItem))
            return sOptionMenuItemDescriptionsDisabledSound[menuItem];
        selection = sOptions->sel_sound[menuItem];
        if (selection >= ARRAY_COUNT(sOptionMenuItemDescriptionsSound[0]))
            selection = 0;
        return sOptionMenuItemDescriptionsSound[menuItem][selection] ? sOptionMenuItemDescriptionsSound[menuItem][selection] : sText_Empty;
    }
}

static u8 MenuItemCount(void)
{
    return sPages[sOptions->page].count;
}


// Never let a stored value point past an option's choices (see SanitizeOptions).
static u8 GetOptionChoiceCount(int (*processInput)(int selection))
{
    if (processInput == ProcessInput_Options_Three)
        return 3;
    if (processInput == ProcessInput_Options_Four)
        return 4;
    if (processInput == ProcessInput_Options_Five)
        return 5;
    if (processInput == ProcessInput_Options_Six)
        return 6;
    if (processInput == ProcessInput_Options_Seven)
        return 7;
    if (processInput == ProcessInput_Options_Eleven)
        return 11;
    if (processInput == ProcessInput_FrameType)
        return WINDOW_FRAMES_COUNT;
    if (processInput == ProcessInput_Difficulty)
        return 3;
    return 2;
}

static void ClampOptionSelections(void)
{
    u32 i;

    for (i = 0; i < MENUITEM_MAIN_COUNT; i++)
        if (sItemFunctionsMain[i].processInput != NULL && sOptions->sel[i] >= GetOptionChoiceCount(sItemFunctionsMain[i].processInput))
            sOptions->sel[i] = 0;
    for (i = 0; i < MENUITEM_BATTLE_COUNT; i++)
        if (sItemFunctionsCustom[i].processInput != NULL && sOptions->sel_battle[i] >= GetOptionChoiceCount(sItemFunctionsCustom[i].processInput))
            sOptions->sel_battle[i] = 0;
    for (i = 0; i < MENUITEM_SOUND_COUNT; i++)
        if (sItemFunctionsSound[i].processInput != NULL && sOptions->sel_sound[i] >= GetOptionChoiceCount(sItemFunctionsSound[i].processInput))
            sOptions->sel_sound[i] = 0;
}

// Main code
static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static const u8 sText_TopBar_Main[]         = _("Main Options");
static const u8 sText_TopBar_Main_Right[]   = _("{R_BUTTON}");
static const u8 sText_TopBar_Custom[]       = _("Battle Options");
static const u8 sText_TopBar_Custom_Left[]  = _("{L_BUTTON}");
static const u8 sText_TopBar_Sound[]        = _("Sound Options");
static void DrawTopBarText(void)
{
    const u8 color[3] = { TEXT_DYNAMIC_COLOR_6, TEXT_COLOR_WHITE, TEXT_COLOR_OPTIONS_GRAY_FG };
    const u8 *title = sPages[sOptions->page].title;

    FillWindowPixelBuffer(WIN_TOPBAR, PIXEL_FILL(15));
    AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 120 - GetStringWidth(FONT_SMALL, title, 0) / 2, 1, color, 0, title);
    if (sOptions->page != 0)
        AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 2, 1, color, 0, sText_TopBar_Custom_Left);
    if (sOptions->page != PAGE_COUNT - 1)
        AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 222, 1, color, 0, sText_TopBar_Main_Right);
    PutWindowTilemap(WIN_TOPBAR);
    CopyWindowToVram(WIN_TOPBAR, COPYWIN_FULL);
}

static void DrawOptionMenuTexts(void) //left side text
{
    u8 i;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < optionsToDraw; i++)
        DrawLeftSideOptionText(i, (i * Y_DIFF) + 1);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}

static void DrawDescriptionText(void)
{
    u8 color_gray[3];
    color_gray[0] = TEXT_COLOR_TRANSPARENT;
    color_gray[1] = TEXT_COLOR_OPTIONS_GRAY_FG;
    color_gray[2] = TEXT_COLOR_OPTIONS_GRAY_SHADOW;

    FillWindowPixelBuffer(WIN_DESCRIPTION, PIXEL_FILL(1));
    AddTextPrinterParameterized4(WIN_DESCRIPTION, FONT_NORMAL, 8, 1, 0, 0, color_gray, TEXT_SKIP_DRAW, OptionTextDescription());
    CopyWindowToVram(WIN_DESCRIPTION, COPYWIN_FULL);
}

static void DrawLeftSideOptionTextItem(int selection, int y)
{
    u8 color_yellow[3];
    u8 color_gray[3];

    color_yellow[0] = TEXT_COLOR_TRANSPARENT;
    color_yellow[1] = TEXT_COLOR_OPTIONS_ORANGE_FG;
    color_yellow[2] = TEXT_COLOR_OPTIONS_ORANGE_SHADOW;
    color_gray[0] = TEXT_COLOR_TRANSPARENT;
    color_gray[1] = TEXT_COLOR_OPTIONS_GRAY_LIGHT_FG;
    color_gray[2] = TEXT_COLOR_OPTIONS_GRAY_SHADOW;

    if (CheckConditions(selection))
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, 8, y, 0, 0, color_yellow, TEXT_SKIP_DRAW, OptionTextRight(selection));
    else
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, 8, y, 0, 0, color_gray, TEXT_SKIP_DRAW, OptionTextRight(selection));
}

static void DrawRightSideChoiceText(const u8 *text, int x, int y, bool8 choosen, bool8 active)
{
    u8 color_red[3];
    u8 color_gray[3];

    if (active)
    {
        color_red[0] = TEXT_COLOR_TRANSPARENT;
        color_red[1] = TEXT_COLOR_OPTIONS_RED_FG;
        color_red[2] = TEXT_COLOR_OPTIONS_RED_SHADOW;
        color_gray[0] = TEXT_COLOR_TRANSPARENT;
        color_gray[1] = TEXT_COLOR_OPTIONS_GRAY_FG;
        color_gray[2] = TEXT_COLOR_OPTIONS_GRAY_SHADOW;
    }
    else
    {
        color_red[0] = TEXT_COLOR_TRANSPARENT;
        color_red[1] = TEXT_COLOR_OPTIONS_RED_DARK_FG;
        color_red[2] = TEXT_COLOR_OPTIONS_RED_DARK_SHADOW;
        color_gray[0] = TEXT_COLOR_TRANSPARENT;
        color_gray[1] = TEXT_COLOR_OPTIONS_GRAY_LIGHT_FG;
        color_gray[2] = TEXT_COLOR_OPTIONS_GRAY_SHADOW;
    }


    if (choosen)
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, x, y, 0, 0, color_red, TEXT_SKIP_DRAW, text);
    else
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, x, y, 0, 0, color_gray, TEXT_SKIP_DRAW, text);
}

static void DrawChoicesItem(u32 id, int y)
{
    switch (sOptions->submenu)
    {
        case MENU_MAIN:
            if (sItemFunctionsMain[id].drawChoices != NULL)
                sItemFunctionsMain[id].drawChoices(sOptions->sel[id], y);
            break;
        case MENU_CUSTOM:
            if (sItemFunctionsCustom[id].drawChoices != NULL)
                sItemFunctionsCustom[id].drawChoices(sOptions->sel_battle[id], y);
            break;
        case MENU_SOUND:
            if (sItemFunctionsSound[id].drawChoices != NULL)
                sItemFunctionsSound[id].drawChoices(sOptions->sel_sound[id], y);
            break;
    }
}

static void HighlightOptionMenuItem(void)
{
    int cursor = sOptions->visibleCursor[sOptions->page];

    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(Y_DIFF, 224));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(cursor * Y_DIFF + 24, cursor * Y_DIFF + 40));
}

void CB2_InitOptionPlusMenu(void)
{
    u32 i, taskId;
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sOptionMenuBgTemplates, ARRAY_COUNT(sOptionMenuBgTemplates));
        ResetBgPositions();
        InitWindows(sOptionMenuWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG0 | WININ_WIN1_BG0 | WININ_WIN0_OBJ);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_EFFECT_DARKEN | BLDCNT_TGT1_BG0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->tiles, 0x120, 0x1A2);
        gMain.state++;
        break;
    case 4:
        LoadPalette(sOptionMenuBg_Pal, 0, sizeof(sOptionMenuBg_Pal));
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->pal, 0x70, 0x20);
        gMain.state++;
        break;
    case 5:
        LoadPalette(sOptionMenuText_Pal, 16, sizeof(sOptionMenuText_Pal));
        gMain.state++;
        break;
    case 6:
        sOptions = AllocZeroed(sizeof(*sOptions));
        SanitizeOptions();
        sOptions->sel[MENUITEM_MAIN_UI]                  = FlagGet(FLAG_UI_PERFECT) ? 1 : 0;
        sOptions->sel[MENUITEM_MAIN_WEATHER_SYSTEM]      = FlagGet(FLAG_WEATHER_SCRIPTED) ? 0 : 1;
        sOptions->sel[MENUITEM_MAIN_WEATHER_SPREAD]      = FlagGet(FLAG_WEATHER_WIDE_ZONES) ? 1 : (FlagGet(FLAG_WEATHER_NEW_TIMES) ? 2 : 0);
        sOptions->sel[MENUITEM_MAIN_POND]                = FlagGet(FLAG_POND_FREEZE_DYNAMIC) ? 0 : 1;
        sOptions->sel[MENUITEM_MAIN_UI_BAG]              = FlagGet(FLAG_UI_BAG_PERFECT) ? 2 : (FlagGet(FLAG_UI_BAG_MODERN) ? 1 : 0);
        sOptions->sel[MENUITEM_MAIN_MIST]                = FlagGet(FLAG_MIST_OLD) ? 0 : 1;
        sOptions->sel[MENUITEM_MAIN_SPAWN_RATE]          = FlagGet(FLAG_SPAWN_RATE_LESS) ? 0 : (FlagGet(FLAG_SPAWN_RATE_MORE) ? 1 : 2);
        sOptions->sel[MENUITEM_MAIN_PUDDLES]             = FlagGet(FLAG_PUDDLES_OFF) ? 0 : (FlagGet(FLAG_PUDDLES_FAST) ? 2 : 1);
        sOptions->sel[MENUITEM_MAIN_TEXTSPEED]           = gSaveBlock2Ptr->optionsTextSpeed;
        sOptions->sel[MENUITEM_MAIN_FONT]                = gSaveBlock2Ptr->optionsFontType;
        sOptions->sel[MENUITEM_MAIN_DIFFICULTY]          = gSaveBlock2Ptr->optionsDifficulty;
        sOptions->sel[MENUITEM_MAIN_BUTTONMODE]          = gSaveBlock2Ptr->optionsButtonMode;
        sOptions->sel[MENUITEM_MAIN_FOLLOWER]            = gSaveBlock2Ptr->optionsfollowerEnable;
        sOptions->sel[MENUITEM_MAIN_LARGE_FOLLOWER]      = gSaveBlock2Ptr->optionsfollowerLargeEnable;
        // v6.0 AUTO RUN: Off / Land / Everywhere (0 = on in the save)
        sOptions->sel[MENUITEM_MAIN_AUTORUN]             = gSaveBlock2Ptr->optionsautoRun != 0 ? 0 : ((gSaveBlock2Ptr->optionsAutorunSurf == 0 || gSaveBlock2Ptr->optionsAutorunDive == 0) ? 2 : 1);
        sOptions->sel[MENUITEM_MAIN_AUTORUN_SURF]        = gSaveBlock2Ptr->optionsAutorunSurf;
        sOptions->sel[MENUITEM_MAIN_AUTORUN_DIVE]        = gSaveBlock2Ptr->optionsAutorunDive;
        sOptions->sel[MENUITEM_MAIN_FRAMETYPE]           = gSaveBlock2Ptr->optionsWindowFrameType;
        sOptions->sel[MENUITEM_MAIN_MATCHCALL]           = gSaveBlock2Ptr->optionsDisableMatchCall;
        sOptions->sel[MENUITEM_MAIN_FISHING]             = gSaveBlock2Ptr->optionsFishing;
        sOptions->sel[MENUITEM_MAIN_EVEN_FASTER_JOY]     = gSaveBlock2Ptr->optionsEvenFasterJoy;
        sOptions->sel[MENUITEM_MAIN_SKIP_INTRO]          = gSaveBlock2Ptr->optionsSkipIntro;
        sOptions->sel[MENUITEM_MAIN_UNIT_TYPE]           = gSaveBlock2Ptr->optionsUnitSystem;
        sOptions->sel[MENUITEM_MAIN_SURFOVERWORLD]       = gSaveBlock2Ptr->optionsSurfOverworld;
        sOptions->sel[MENUITEM_MAIN_BRIGHTER_NIGHTS]     = gSaveBlock2Ptr->optionsBrighterNights;

        sOptions->sel_battle[MENUITEM_BATTLE_BATTLESTYLE]       = gSaveBlock2Ptr->optionsBattleStyle;
        // v6.3 POKEMON'S ANIMATION: 0 Off, 1 On, 2 Always (idle, no pause), 3 Modern (idle, pause),
        // 4 Perfect (idle, very short pause), 5 Perfect 2 (idle, 1 s pause)
        sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE]       = gSaveBlock2Ptr->optionsBattleSceneOff ? 0
                                                                : FlagGet(FLAG_BATTLE_ANIMS_EXTENDED) ? 2
                                                                : FlagGet(FLAG_BATTLE_ANIMS_MODERN) ? 3
                                                                : FlagGet(FLAG_BATTLE_ANIMS_PERFECT) ? 4
                                                                : FlagGet(FLAG_BATTLE_ANIMS_PERFECT2) ? 5 : 1;
        sOptions->sel_battle[MENUITEM_BATTLE_WEATHER_FX]        = FlagGet(FLAG_BATTLE_WEATHER_REGULAR) ? 0 : 1;
        sOptions->sel_battle[MENUITEM_BATTLE_FAST_INTRO]        = gSaveBlock2Ptr->optionsFastIntro;
        sOptions->sel_battle[MENUITEM_BATTLE_FAST_BATTLES]      = gSaveBlock2Ptr->optionsFastBattle;
        sOptions->sel_battle[MENUITEM_BATTLE_BATTLE_SPEED]      = gSaveBlock2Ptr->optionsBattleSpeed;
        sOptions->sel_battle[MENUITEM_BATTLE_SPLIT]             = gSaveBlock2Ptr->optionStyle;
        sOptions->sel_battle[MENUITEM_BATTLE_TYPE_EFFECTIVE]    = gSaveBlock2Ptr->optionTypeEffective;
        sOptions->sel_battle[MENUITEM_BATTLE_LR_RUN]            = gSaveBlock2Ptr->optionsLRtoRun;
        sOptions->sel_battle[MENUITEM_BATTLE_BALL_PROMPT]       = gSaveBlock2Ptr->optionsBallPrompt;
        sOptions->sel_battle[MENUITEM_BATTLE_OWE]               = !FlagGet(FLAG_OWE_ON) ? 0 : (FlagGet(FLAG_OWE_RESTRICT) ? 2 : 1);
        sOptions->sel_battle[MENUITEM_BATTLE_SHADOWS]           = FlagGet(FLAG_BATTLE_SHADOWS_OFF) ? 0 : 1;
        sOptions->sel_battle[MENUITEM_BATTLE_NEW_BACKGROUNDS]   = FlagGet(FLAG_BATTLE_TERRAIN_CIRCLES_OFF) ? 1 : 0;
        sOptions->sel_battle[MENUITEM_BATTLE_BACKGROUND]        = !gSaveBlock2Ptr->optionsNewBackgrounds ? 0 : (FlagGet(FLAG_PERFECT_BATTLE_BACKGROUNDS) ? 3 : (FlagGet(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS) ? 2 : 1));
        sOptions->sel_battle[MENUITEM_BATTLE_RUN_TYPE]          = gSaveBlock2Ptr->optionsRunType;
        sOptions->sel_battle[MENUITEM_BATTLE_CURSOR_MEMORY]     = gSaveBlock2Ptr->optionsCursorMemory;

        sOptions->sel_sound[MENUITEM_SOUND_SOUND]                             = gSaveBlock2Ptr->optionsSound;
        sOptions->sel_sound[MENUITEM_SOUND_MUSIC]                             = gSaveBlock2Ptr->optionsMusicOnOff;
        sOptions->sel_sound[MENUITEM_SOUND_BIKE_MUSIC]                        = gSaveBlock2Ptr->optionsBikeMusic;
        sOptions->sel_sound[MENUITEM_SOUND_SURF_MUSIC]                        = gSaveBlock2Ptr->optionsSurfMusic;
        sOptions->sel_sound[MENUITEM_SOUND_WILD_MON_MUSIC]                    = gSaveBlock2Ptr->optionsWildBattleMusic;
        sOptions->sel_sound[MENUITEM_SOUND_BATTLE_TRAINER_MUSIC]              = gSaveBlock2Ptr->optionsTrainerBattleMusic;
        sOptions->sel_sound[MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC]     = gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic;
        sOptions->sel_sound[MENUITEM_SOUND_EFFECTS]                           = gSaveBlock2Ptr->optionsSoundEffects;

        ClampOptionSelections();
        sOptions->submenu = MENU_MAIN;
        sOptions->page = PAGE_MAIN;

        gMain.state++;
        break;
    case 7:
        PutWindowTilemap(WIN_TOPBAR);
        DrawTopBarText();
        gMain.state++;
        break;
    case 8:
        PutWindowTilemap(WIN_DESCRIPTION);
        DrawDescriptionText();
        gMain.state++;
        break;
    case 9:
        PutWindowTilemap(WIN_OPTIONS);
        DrawOptionMenuTexts();
        gMain.state++;
        break;
    case 10:
        taskId = CreateTask(Task_OptionMenuFadeIn, 0);

        sOptions->arrowTaskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 240 / 2, 20, 110, MenuItemCount() - 1, 110, 110, 0);

        for (i = 0; i < min(OPTIONS_ON_SCREEN, MenuItemCount()); i++)
            DrawChoices(i, i * Y_DIFF);

        HighlightOptionMenuItem();

        CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
        gMain.state++;
        break;
    case 11:
        DrawBgWindowFrames();
        gMain.state++;
        break;
    case 12:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0x10, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_OptionMenuFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_OptionMenuProcessInput;
}

static void MoveOptionCursorUp(void)
{
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

        if (sOptions->visibleCursor[sOptions->page] == NUM_OPTIONS_FROM_BORDER) // don't advance visible cursor until scrolled to the bottom
        {
            if (--sOptions->menuCursor[sOptions->page] == 0)
                sOptions->visibleCursor[sOptions->page]--;
            else
                ScrollMenu(1);
        }
        else
        {
            if (--sOptions->menuCursor[sOptions->page] < 0) // Scroll all the way to the bottom.
            {
                sOptions->visibleCursor[sOptions->page] = sOptions->menuCursor[sOptions->page] = optionsToDraw-2;
                ScrollAll(0);
                sOptions->visibleCursor[sOptions->page] = optionsToDraw-1;
                sOptions->menuCursor[sOptions->page] = MenuItemCount() - 1;
            }
            else
            {
                sOptions->visibleCursor[sOptions->page]--;
            }
        }
    }

static void MoveOptionCursorDown(void)
{
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

        if (sOptions->visibleCursor[sOptions->page] == optionsToDraw-2) // don't advance visible cursor until scrolled to the bottom
        {
            if (++sOptions->menuCursor[sOptions->page] == MenuItemCount() - 1)
                sOptions->visibleCursor[sOptions->page]++;
            else
                ScrollMenu(0);
        }
        else
        {
            if (++sOptions->menuCursor[sOptions->page] >= MenuItemCount()-1) // Scroll all the way to the top.
            {
                sOptions->visibleCursor[sOptions->page] = optionsToDraw-2;
                sOptions->menuCursor[sOptions->page] = MenuItemCount() - optionsToDraw-1;
                ScrollAll(1);
                sOptions->visibleCursor[sOptions->page] = sOptions->menuCursor[sOptions->page] = 0;
            }
            else
            {
                sOptions->visibleCursor[sOptions->page]++;
            }
        }
    }

// v6.0: the cursor never rests on a header row (a page may start with one)
static void SkipHeaderRows(void)
{
    u32 guard = 0;
    while (IsHeaderRow(sOptions->menuCursor[sOptions->page]) && guard++ < 8)
        MoveOptionCursorDown();
}

static void Task_OptionMenuProcessInput(u8 taskId)
{
    int i = 0;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN , MenuItemCount());
    if (JOY_NEW(B_BUTTON))
    {
        gTasks[taskId].func = Task_OptionMenuSave;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        MoveOptionCursorUp();
        while (IsHeaderRow(sOptions->menuCursor[sOptions->page]))
            MoveOptionCursorUp();
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        MoveOptionCursorDown();
        while (IsHeaderRow(sOptions->menuCursor[sOptions->page]))
            MoveOptionCursorDown();
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT) && !IsHeaderRow(sOptions->menuCursor[sOptions->page]))
    {
        SelectRow(sOptions->menuCursor[sOptions->page]);
        if (sOptions->submenu == MENU_MAIN)
        {
            int cursor = SelectRow(sOptions->menuCursor[sOptions->page]);
            u8 previousOption = sOptions->sel[cursor];
            if (CheckConditions(cursor))
            {
                if (sItemFunctionsMain[cursor].processInput != NULL)
                {
                    sOptions->sel[cursor] = sItemFunctionsMain[cursor].processInput(previousOption);
                    ReDrawAll();
                    DrawDescriptionText();
                }

                if (previousOption != sOptions->sel[cursor])
                    DrawChoices(sOptions->menuCursor[sOptions->page], sOptions->visibleCursor[sOptions->page] * Y_DIFF);
            }
        }
        else if (sOptions->submenu == MENU_CUSTOM)
        {
            int cursor = SelectRow(sOptions->menuCursor[sOptions->page]);
            u8 previousOption = sOptions->sel_battle[cursor];
            if (CheckConditions(cursor))
            {
                if (sItemFunctionsCustom[cursor].processInput != NULL)
                {
                    sOptions->sel_battle[cursor] = sItemFunctionsCustom[cursor].processInput(previousOption);
                    ReDrawAll();
                    DrawDescriptionText();
                }

                if (previousOption != sOptions->sel_battle[cursor])
                    DrawChoices(sOptions->menuCursor[sOptions->page], sOptions->visibleCursor[sOptions->page] * Y_DIFF);
            }
        }
        else if (sOptions->submenu == MENU_SOUND)
        {
            int cursor = SelectRow(sOptions->menuCursor[sOptions->page]);
            u8 previousOption = sOptions->sel_sound[cursor];
            if (CheckConditions(cursor))
            {
                if (sItemFunctionsSound[cursor].processInput != NULL)
                {
                    sOptions->sel_sound[cursor] = sItemFunctionsSound[cursor].processInput(previousOption);
                    ReDrawAll();
                    DrawDescriptionText();
                }

                if (previousOption != sOptions->sel_sound[cursor])
                    DrawChoices(sOptions->menuCursor[sOptions->page], sOptions->visibleCursor[sOptions->page] * Y_DIFF);
            }
        }
    }
    else if (JOY_NEW(R_BUTTON))
    {
        if (sOptions->page != PAGE_COUNT - 1)
            sOptions->page++;
        SkipHeaderRows();

        DrawTopBarText();
        ReDrawAll();
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    else if (JOY_NEW(L_BUTTON))
    {
        if (sOptions->page != 0)
            sOptions->page--;
        SkipHeaderRows();

        DrawTopBarText();
        ReDrawAll();
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    if (JOY_HELD(SELECT_BUTTON) && JOY_NEW(START_BUTTON))
    {
        if (VarGet(VAR_DEBUG_OPTIONS) == 1)
        {
            VarSet(VAR_DEBUG_OPTIONS, 0);
            PlaySE(SE_PC_OFF);
        }
        else
        {
            VarSet(VAR_DEBUG_OPTIONS, 1);
            PlaySE(SE_PC_ON);
        }
    }
}

static void Task_OptionMenuSave(u8 taskId)
{
    sOptions->sel[MENUITEM_MAIN_UI] ? FlagSet(FLAG_UI_PERFECT) : FlagClear(FLAG_UI_PERFECT);
    sOptions->sel[MENUITEM_MAIN_WEATHER_SYSTEM] ? FlagClear(FLAG_WEATHER_SCRIPTED) : FlagSet(FLAG_WEATHER_SCRIPTED);
    sOptions->sel[MENUITEM_MAIN_WEATHER_SPREAD] >= 1 ? FlagSet(FLAG_WEATHER_NEW_TIMES) : FlagClear(FLAG_WEATHER_NEW_TIMES);
    sOptions->sel[MENUITEM_MAIN_WEATHER_SPREAD] == 1 ? FlagSet(FLAG_WEATHER_WIDE_ZONES) : FlagClear(FLAG_WEATHER_WIDE_ZONES);
    sOptions->sel[MENUITEM_MAIN_POND] == 0 ? FlagSet(FLAG_POND_FREEZE_DYNAMIC) : FlagClear(FLAG_POND_FREEZE_DYNAMIC);
    sOptions->sel[MENUITEM_MAIN_UI_BAG] == 1 ? FlagSet(FLAG_UI_BAG_MODERN) : FlagClear(FLAG_UI_BAG_MODERN);
    sOptions->sel[MENUITEM_MAIN_UI_BAG] == 2 ? FlagSet(FLAG_UI_BAG_PERFECT) : FlagClear(FLAG_UI_BAG_PERFECT);
    sOptions->sel[MENUITEM_MAIN_MIST] == 0 ? FlagSet(FLAG_MIST_OLD) : FlagClear(FLAG_MIST_OLD);
    sOptions->sel[MENUITEM_MAIN_SPAWN_RATE] == 0 ? FlagSet(FLAG_SPAWN_RATE_LESS) : FlagClear(FLAG_SPAWN_RATE_LESS);
    sOptions->sel[MENUITEM_MAIN_SPAWN_RATE] == 1 ? FlagSet(FLAG_SPAWN_RATE_MORE) : FlagClear(FLAG_SPAWN_RATE_MORE);
    sOptions->sel[MENUITEM_MAIN_PUDDLES] == 0 ? FlagSet(FLAG_PUDDLES_OFF) : FlagClear(FLAG_PUDDLES_OFF);
    sOptions->sel[MENUITEM_MAIN_PUDDLES] == 2 ? FlagSet(FLAG_PUDDLES_FAST) : FlagClear(FLAG_PUDDLES_FAST);
    gSaveBlock2Ptr->optionsTextSpeed             = sOptions->sel[MENUITEM_MAIN_TEXTSPEED];
    gSaveBlock2Ptr->optionsFontType              = sOptions->sel[MENUITEM_MAIN_FONT];
    gSaveBlock2Ptr->optionsDifficulty            = sOptions->sel[MENUITEM_MAIN_DIFFICULTY];
    gSaveBlock2Ptr->optionsButtonMode            = sOptions->sel[MENUITEM_MAIN_BUTTONMODE];
    gSaveBlock2Ptr->optionsfollowerEnable        = sOptions->sel[MENUITEM_MAIN_FOLLOWER];
    gSaveBlock2Ptr->optionsfollowerLargeEnable   = sOptions->sel[MENUITEM_MAIN_LARGE_FOLLOWER];
    gSaveBlock2Ptr->optionsautoRun               = sOptions->sel[MENUITEM_MAIN_AUTORUN] == 0;   // 0 = on
    gSaveBlock2Ptr->optionsAutorunSurf           = sOptions->sel[MENUITEM_MAIN_AUTORUN] != 2;
    gSaveBlock2Ptr->optionsAutorunDive           = sOptions->sel[MENUITEM_MAIN_AUTORUN] != 2;
    gSaveBlock2Ptr->optionsDisableMatchCall      = sOptions->sel[MENUITEM_MAIN_MATCHCALL];
    gSaveBlock2Ptr->optionsWindowFrameType       = sOptions->sel[MENUITEM_MAIN_FRAMETYPE];
    gSaveBlock2Ptr->optionsFishing               = sOptions->sel[MENUITEM_MAIN_FISHING];
    gSaveBlock2Ptr->optionsEvenFasterJoy         = sOptions->sel[MENUITEM_MAIN_EVEN_FASTER_JOY];
    gSaveBlock2Ptr->optionsSkipIntro             = sOptions->sel[MENUITEM_MAIN_SKIP_INTRO];
    gSaveBlock2Ptr->optionsUnitSystem            = sOptions->sel[MENUITEM_MAIN_UNIT_TYPE];
    gSaveBlock2Ptr->optionsSurfOverworld         = sOptions->sel[MENUITEM_MAIN_SURFOVERWORLD];
    gSaveBlock2Ptr->optionsBrighterNights        = sOptions->sel[MENUITEM_MAIN_BRIGHTER_NIGHTS];

    gSaveBlock2Ptr->optionsBattleStyle      = sOptions->sel_battle[MENUITEM_BATTLE_BATTLESTYLE];
    gSaveBlock2Ptr->optionsBattleSceneOff   = (sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE] == 0);
    sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE] == 0 ? FlagSet(FLAG_BATTLE_ANIMS_NO_INTRO) : FlagClear(FLAG_BATTLE_ANIMS_NO_INTRO);
    sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE] == 2 ? FlagSet(FLAG_BATTLE_ANIMS_EXTENDED) : FlagClear(FLAG_BATTLE_ANIMS_EXTENDED);
    sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE] == 3 ? FlagSet(FLAG_BATTLE_ANIMS_MODERN) : FlagClear(FLAG_BATTLE_ANIMS_MODERN);
    sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE] == 4 ? FlagSet(FLAG_BATTLE_ANIMS_PERFECT) : FlagClear(FLAG_BATTLE_ANIMS_PERFECT);
    sOptions->sel_battle[MENUITEM_BATTLE_BATTLESCENE] == 5 ? FlagSet(FLAG_BATTLE_ANIMS_PERFECT2) : FlagClear(FLAG_BATTLE_ANIMS_PERFECT2);
    sOptions->sel_battle[MENUITEM_BATTLE_WEATHER_FX] ? FlagClear(FLAG_BATTLE_WEATHER_REGULAR) : FlagSet(FLAG_BATTLE_WEATHER_REGULAR);
    gSaveBlock2Ptr->optionsFastIntro        = sOptions->sel_battle[MENUITEM_BATTLE_FAST_INTRO];
    gSaveBlock2Ptr->optionsFastBattle       = sOptions->sel_battle[MENUITEM_BATTLE_FAST_BATTLES];
    gSaveBlock2Ptr->optionsBattleSpeed      = sOptions->sel_battle[MENUITEM_BATTLE_BATTLE_SPEED];
    gSaveBlock2Ptr->optionStyle             = sOptions->sel_battle[MENUITEM_BATTLE_SPLIT];
    gSaveBlock2Ptr->optionTypeEffective     = sOptions->sel_battle[MENUITEM_BATTLE_TYPE_EFFECTIVE];
    gSaveBlock2Ptr->optionsLRtoRun          = sOptions->sel_battle[MENUITEM_BATTLE_LR_RUN];
    gSaveBlock2Ptr->optionsBallPrompt       = sOptions->sel_battle[MENUITEM_BATTLE_BALL_PROMPT];
    sOptions->sel_battle[MENUITEM_BATTLE_OWE] ? FlagSet(FLAG_OWE_ON) : FlagClear(FLAG_OWE_ON);
    sOptions->sel_battle[MENUITEM_BATTLE_OWE] == 2 ? FlagSet(FLAG_OWE_RESTRICT) : FlagClear(FLAG_OWE_RESTRICT);
    sOptions->sel_battle[MENUITEM_BATTLE_SHADOWS] ? FlagClear(FLAG_BATTLE_SHADOWS_OFF) : FlagSet(FLAG_BATTLE_SHADOWS_OFF);
    if (sOptions->sel_battle[MENUITEM_BATTLE_NEW_BACKGROUNDS] != 0)
        FlagSet(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);
    else
        FlagClear(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);
    gSaveBlock2Ptr->optionsNewBackgrounds   = (sOptions->sel_battle[MENUITEM_BATTLE_BACKGROUND] != 0);
    sOptions->sel_battle[MENUITEM_BATTLE_BACKGROUND] == 3 ? FlagSet(FLAG_PERFECT_BATTLE_BACKGROUNDS) : FlagClear(FLAG_PERFECT_BATTLE_BACKGROUNDS);
    sOptions->sel_battle[MENUITEM_BATTLE_BACKGROUND] == 2 ? FlagSet(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS) : FlagClear(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS);
    gSaveBlock2Ptr->optionsRunType          = sOptions->sel_battle[MENUITEM_BATTLE_RUN_TYPE];
    gSaveBlock2Ptr->optionsCursorMemory     = sOptions->sel_battle[MENUITEM_BATTLE_CURSOR_MEMORY];

    gSaveBlock2Ptr->optionsSound            = sOptions->sel_sound[MENUITEM_SOUND_SOUND];
    gSaveBlock2Ptr->optionsMusicOnOff       = sOptions->sel_sound[MENUITEM_SOUND_MUSIC];
    gSaveBlock2Ptr->optionsBikeMusic        = sOptions->sel_sound[MENUITEM_SOUND_BIKE_MUSIC];
    gSaveBlock2Ptr->optionsSurfMusic        = sOptions->sel_sound[MENUITEM_SOUND_SURF_MUSIC];
    gSaveBlock2Ptr->optionsWildBattleMusic  = sOptions->sel_sound[MENUITEM_SOUND_WILD_MON_MUSIC];
    gSaveBlock2Ptr->optionsTrainerBattleMusic  = sOptions->sel_sound[MENUITEM_SOUND_BATTLE_TRAINER_MUSIC];
    gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic  = sOptions->sel_sound[MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC];
    gSaveBlock2Ptr->optionsSoundEffects  = sOptions->sel_sound[MENUITEM_SOUND_EFFECTS];

    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 0x10, RGB_BLACK);
    gTasks[taskId].func = Task_OptionMenuFadeOut;
}

static void Task_OptionMenuFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        FREE_AND_SET_NULL(sOptions);
        SetMainCallback2(gMain.savedCallback);
    }
}

static void ScrollMenu(int direction)
{
    int menuItem, pos;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    if (direction == 0) // scroll down
        menuItem = sOptions->menuCursor[sOptions->page] + NUM_OPTIONS_FROM_BORDER, pos = optionsToDraw - 1;
    else
        menuItem = sOptions->menuCursor[sOptions->page] - NUM_OPTIONS_FROM_BORDER, pos = 0;

    // Hide one
    ScrollWindow(WIN_OPTIONS, direction, Y_DIFF, PIXEL_FILL(0));
    // Show one
    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 0, Y_DIFF * pos, 26 * 8, Y_DIFF);
    // Print
    DrawChoices(menuItem, pos * Y_DIFF);
    DrawLeftSideOptionText(menuItem, (pos * Y_DIFF) + 1);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}
static void ScrollAll(int direction) // to bottom or top
{
    // v5.1: with more items than two screens the old code moved the window by more
    // than its height and left old text behind (UP from the first option). Redraw
    // the visible rows from scratch instead.
    int i, first;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    first = (direction == 0) ? MenuItemCount() - optionsToDraw : 0;
    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < optionsToDraw; i++)
    {
        DrawChoices(first + i, i * Y_DIFF);
        DrawLeftSideOptionText(first + i, (i * Y_DIFF) + 1);
    }
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

// Process Input functions ****GENERIC****
static int GetMiddleX(const u8 *txt1, const u8 *txt2, const u8 *txt3)
{
    int xMid;
    int widthLeft = GetStringWidth(1, txt1, 0);
    int widthMid = GetStringWidth(1, txt2, 0);
    int widthRight = GetStringWidth(1, txt3, 0);

    widthMid -= (198 - 104);
    xMid = (widthLeft - widthMid - widthRight) / 2 + 104;
    return xMid;
}

static int XOptions_ProcessInput(int x, int selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (++selection > (x - 1))
            selection = 0;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (--selection < 0)
            selection = (x - 1);
    }
    return selection;
}

static int ProcessInput_Options_Two(int selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
        selection ^= 1;

    return selection;
}

static int ProcessInput_Options_Three(int selection)
{
    return XOptions_ProcessInput(3, selection);
}

static int ProcessInput_Options_Four(int selection)
{
    return XOptions_ProcessInput(4, selection);
}

static int ProcessInput_Options_Five(int selection)
{
    return XOptions_ProcessInput(5, selection);
}

static int ProcessInput_Options_Six(int selection)
{
    return XOptions_ProcessInput(6, selection);
}

static int ProcessInput_Options_Seven(int selection)
{
    return XOptions_ProcessInput(7, selection);
}

static int ProcessInput_Options_Eleven(int selection)
{
    return XOptions_ProcessInput(11, selection);
}

// Process Input functions ****SPECIFIC****
static int ProcessInput_Sound(int selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        selection ^= 1;
        SetPokemonCryStereo(selection);
    }

    return selection;
}

static int ProcessInput_FrameType(int selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection < WINDOW_FRAMES_COUNT - 1)
            selection++;
        else
            selection = 0;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, 0x1A2);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, 0x70, 0x20);
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = WINDOW_FRAMES_COUNT - 1;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, 0x1A2);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, 0x70, 0x20);
    }
    return selection;
}

static int ProcessInput_BattleStyle(int selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
        if ((gSaveBlock2Ptr->optionsDifficulty == 2) && (gSaveBlock1Ptr->tx_Features_LimitDifficulty == 1) && (FlagGet(FLAG_SYS_GAME_CLEAR)))
        {
            selection ^= 1;
        }
        else if ((gSaveBlock2Ptr->optionsDifficulty == 2) && (gSaveBlock1Ptr->tx_Features_LimitDifficulty == 1))
        {
            PlaySE(SE_FAILURE);
        }
        else
        {
            selection ^= 1;
        }

    return selection;
}

static int ProcessInput_Difficulty(int selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if ((gSaveBlock1Ptr->tx_Features_LimitDifficulty == 1) && (FlagGet(FLAG_SYS_GAME_CLEAR) == 0))
            PlaySE(SE_FAILURE);
        else if (++selection > (3 - 1))
            selection = 0;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if ((gSaveBlock1Ptr->tx_Features_LimitDifficulty == 1) && (FlagGet(FLAG_SYS_GAME_CLEAR) == 0))
            PlaySE(SE_FAILURE);
        else if (--selection < 0)
            selection = (3 - 1);
    }
    return selection;
}

// Draw Choices functions ****GENERIC****
// v6.0: every setting shows only its current choice, between arrows. The old
// drawing functions still run (some also store values); their output is caught
// here and redrawn in that style.
static bool8 sChoiceCapture, sChoiceCaptured, sChoiceActive;
static u8 sChoiceText[40];

static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style, bool8 active)
{
    if (sChoiceCapture)
    {
        sChoiceActive = active;
        if (style != 0 && !sChoiceCaptured)
        {
            StringCopy(sChoiceText, text);
            sChoiceCaptured = TRUE;
        }
        return;
    }
    {
    bool8 choosen = FALSE;
    if (style != 0)
        choosen = TRUE;

    DrawRightSideChoiceText(text, x, y+1, choosen, active);
    }
}

static void DrawChoices_Options_Four(const u8 *const *const strings, int selection, int y, bool8 active)
{
    static const u8 choiceOrders[][3] =
    {
        {0, 1, 2},
        {0, 1, 2},
        {1, 2, 3},
        {1, 2, 3},
    };
    u8 styles[4] = {0};
    int xMid;
    const u8 *order;

    if (selection < 0 || selection >= 4)
        selection = 0;
    order = choiceOrders[selection];

    styles[selection] = 1;
    xMid = GetMiddleX(strings[order[0]], strings[order[1]], strings[order[2]]);

    DrawOptionMenuChoice(strings[order[0]], 104, y, styles[order[0]], active);
    DrawOptionMenuChoice(strings[order[1]], xMid, y, styles[order[1]], active);
    DrawOptionMenuChoice(strings[order[2]], GetStringRightAlignXOffset(1, strings[order[2]], 198), y, styles[order[2]], active);
}

static void DrawChoices_Options_Seven(const u8 *const *const strings, int selection, int y, bool8 active)
{
    static const u8 choiceOrders[][2] =
    {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 4},
        {4, 5},
        {5, 6},
        {6, 0},
    };
    u8 styles[7] = {0};
    const u8 *order;

    if (selection < 0 || selection >= 7)
        selection = 0;
    order = choiceOrders[selection];
    styles[selection] = 1;

    DrawOptionMenuChoice(strings[order[0]], 104, y, styles[order[0]], active);
    DrawOptionMenuChoice(strings[order[1]], GetStringRightAlignXOffset(1, strings[order[1]], 198), y, styles[order[1]], active);
}

static void ReDrawAll(void)
{
    u8 menuItem = sOptions->menuCursor[sOptions->page] - sOptions->visibleCursor[sOptions->page];
    u8 i;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    if (MenuItemCount() <= OPTIONS_ON_SCREEN) // Draw or delete the scrolling arrows based on options in the menu
    {
        if (sOptions->arrowTaskId != TASK_NONE)
        {
            RemoveScrollIndicatorArrowPair(sOptions->arrowTaskId);
            sOptions->arrowTaskId = TASK_NONE;
        }
    }
    else
    {
        if (sOptions->arrowTaskId != TASK_NONE)
            RemoveScrollIndicatorArrowPair(sOptions->arrowTaskId);
        sOptions->arrowTaskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 240 / 2, 20, 110, MenuItemCount() - 1, 110, 110, 0);
    }

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < optionsToDraw; i++)
    {
        DrawChoices(menuItem+i, i * Y_DIFF);
        DrawLeftSideOptionText(menuItem+i, (i * Y_DIFF) + 1);
    }
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

// Process Input functions ****SPECIFIC****
static const u8 sText_Faster[] = _("Faster");
static const u8 sText_Instant[] = _("Instant");
static const u8 *const sTextSpeedStrings[] = {gText_TextSpeedSlow, gText_TextSpeedMid, gText_TextSpeedFast, sText_Faster};
static void DrawChoices_TextSpeed(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_TEXTSPEED);
    DrawChoices_Options_Four(sTextSpeedStrings, selection, y, active);
}


// v6.0: choice names for the reworked settings
static const u8 sText_Choice_Off[]       = _("Off");
static const u8 sText_Choice_On[]        = _("On");
static const u8 sText_Choice_Partially[] = _("Partially");
static const u8 sText_Choice_Always[]    = _("Always");
static const u8 sText_Choice_Perfect2[]  = _("Perfect 2");
static const u8 sText_Choice_Modern[]    = _("Modern");
static const u8 sText_Choice_Perfect[]   = _("Perfect");
static const u8 sText_Choice_Vanilla[]   = _("Vanilla");
static const u8 sText_Choice_Land[]      = _("Land");
static const u8 sText_Choice_Everywhere[] = _("Everywhere");
static const u8 sText_Choice_Fast[]      = _("Fast");
static const u8 sText_Choice_Slow[]      = _("Slow");
static const u8 sText_BattleSceneExtended[] = _("Perfect");
static void DrawChoices_BattleScene(int selection, int y)
{
    const u8 *const names[6] = {sText_Choice_Off, sText_Choice_On, sText_Choice_Always, sText_Choice_Modern, sText_Choice_Perfect, sText_Choice_Perfect2};
    bool8 active = CheckConditions(MENUITEM_BATTLE_BATTLESCENE);

    if (selection < 0 || selection > 5)
        selection = 1;
    DrawOptionMenuChoice(names[selection], 104, y, 1, active);
}

static void DrawChoices_Difficulty(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_DIFFICULTY);
    u8 styles[3] = {0};
    int xMid = GetMiddleX(gText_Easy, gText_ButtonTypeNormal, gText_Hard);
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsDifficulty = 0; //Easy
        FlagClear(FLAG_DIFFICULTY_HARD);
    }
    else if (selection == 1)
    {
        gSaveBlock2Ptr->optionsDifficulty = 1; //Normal
        FlagClear(FLAG_DIFFICULTY_HARD);
    }
    else
    {
        gSaveBlock2Ptr->optionsDifficulty = 2; //Hard
        FlagSet(FLAG_DIFFICULTY_HARD);
    }

    DrawOptionMenuChoice(gText_Easy, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_ButtonTypeNormal, xMid, y, styles[1], active);
    DrawOptionMenuChoice(gText_Hard, GetStringRightAlignXOffset(1, gText_ButtonTypeLEqualsA, 198), y, styles[2], active);
}
static const u8 sText_Sound_WildMon_Hoenn[]       = _("Hoenn");
static const u8 sText_Sound_WildMon_Kanto_Old[]   = _("Kanto 1");
static const u8 sText_Sound_WildMon_Sinnoh[]      = _("Sinnoh");
static const u8 sText_Sound_WildMon_Johto[]       = _("Johto");
static const u8 sText_Sound_WildMon_Kanto_New[]   = _("Kanto 2");
static const u8 sText_Sound_WildMon_BW[]          = _("Unova");
static const u8 sText_Sound_WildMon_Random[]      = _("Random");

static const u8 *const sText_Sound_WildMonBattleMusic_Strings[] = {sText_Sound_WildMon_Hoenn,  sText_Sound_WildMon_Kanto_Old,  sText_Sound_WildMon_Sinnoh,  sText_Sound_WildMon_Johto,  sText_Sound_WildMon_Kanto_New,   sText_Sound_WildMon_BW,  sText_Sound_WildMon_Random};
static void DrawChoices_Wild_Battle_Music(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_WILD_MON_MUSIC);
    DrawChoices_Options_Seven(sText_Sound_WildMonBattleMusic_Strings, selection, y, active);

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 0; // Hoenn
    }
    else if (selection == 1)
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 1; // Kanto1
    }
    else if (selection == 2)
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 2; // Sinnoh
    }
    else if (selection == 3)
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 3; // Johto
    }
    else if (selection == 4)
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 4; // Kanto 2
    }
    else if (selection == 5)
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 5; // Unova
    }
    else
    {
        gSaveBlock2Ptr->optionsWildBattleMusic = 6; // Random
    }
}

static void DrawChoices_Trainer_Battle_Music(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_BATTLE_TRAINER_MUSIC);
    DrawChoices_Options_Seven(sText_Sound_WildMonBattleMusic_Strings, selection, y, active);

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 0; // Hoenn
    }
    else if (selection == 1)
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 1; // Kanto1
    }
    else if (selection == 2)
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 2; // Sinnoh
    }
    else if (selection == 3)
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 3; // Johto
    }
    else if (selection == 4)
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 4; // Kanto 2
    }
    else if (selection == 5)
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 5; // Unova
    }
    else
    {
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 6; // Random
    }
}

static void DrawChoices_Frontier_Trainer_Battle_Music(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_BATTLE_FRONTIER_TRAINER_MUSIC);
    DrawChoices_Options_Seven(sText_Sound_WildMonBattleMusic_Strings, selection, y, active);

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 0; // Hoenn
    }
    else if (selection == 1)
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 1; // Kanto1
    }
    else if (selection == 2)
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 2; // Sinnoh
    }
    else if (selection == 3)
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 3; // Johto
    }
    else if (selection == 4)
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 4; // Kanto 2
    }
    else if (selection == 5)
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 5; // Unova
    }
    else
    {
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 6; // Random
    }
}

static const u8 sText_Sound_Effects_Gen3[]      = _("Gen 3");
static const u8 sText_Sound_Effects_DP[]        = _("DPPL");
static const u8 sText_Sound_Effects_HGSS[]      = _("HGSS");

static void DrawChoices_Sound_Effects(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_EFFECTS);
    u8 styles[3] = {0};
    int xMid = GetMiddleX(sText_Sound_Effects_Gen3, sText_Sound_Effects_DP, sText_Sound_Effects_HGSS);
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsSoundEffects = 0; //Gen 3
    }
    else if (selection == 1)
    {
        gSaveBlock2Ptr->optionsSoundEffects = 1; //DPL
    }
    else
    {
        gSaveBlock2Ptr->optionsSoundEffects = 2; //HGSS
    }

    DrawOptionMenuChoice(sText_Sound_Effects_Gen3, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Sound_Effects_DP, xMid, y, styles[1], active);
    DrawOptionMenuChoice(sText_Sound_Effects_HGSS, GetStringRightAlignXOffset(1, sText_Sound_Effects_DP, 198), y, styles[2], active);
}

static void DrawChoices_BattleStyle(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_BATTLESTYLE);
    u8 styles[2] = {0};
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleStyleShift, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleStyleSet, GetStringRightAlignXOffset(FONT_NORMAL, gText_BattleStyleSet, 198), y, styles[1], active);
}

static void DrawChoices_Sound(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_SOUND);
    u8 styles[2] = {0};
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_SoundMono, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_SoundStereo, GetStringRightAlignXOffset(FONT_NORMAL, gText_SoundStereo, 198), y, styles[1], active);
}

static void DrawChoices_ButtonMode(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_BUTTONMODE);
    u8 styles[3] = {0};
    int xMid = GetMiddleX(gText_ButtonTypeNormal, gText_ButtonTypeLR, gText_ButtonTypeLEqualsA);
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_ButtonTypeNormal, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_ButtonTypeLR, xMid, y, styles[1], active);
    DrawOptionMenuChoice(gText_ButtonTypeLEqualsA, GetStringRightAlignXOffset(1, gText_ButtonTypeLEqualsA, 198), y, styles[2], active);
}

static void DrawChoices_FrameType(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_FRAMETYPE);
    u8 text[16];
    u8 n = selection + 1;
    u16 i;

    for (i = 0; gText_FrameTypeNumber[i] != EOS && i <= 5; i++)
        text[i] = gText_FrameTypeNumber[i];

    // Convert a number to decimal string
    if (n / 10 != 0)
    {
        text[i] = n / 10 + CHAR_0;
        i++;
        text[i] = n % 10 + CHAR_0;
        i++;
    }
    else
    {
        text[i] = n % 10 + CHAR_0;
        i++;
        text[i] = 0x77;
        i++;
    }

    text[i] = EOS;

    DrawOptionMenuChoice(gText_FrameType, 104, y, 0, active);
    DrawOptionMenuChoice(text, 128, y, 1, active);
}

static void DrawChoices_Follower(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_FOLLOWER);
    u8 styles[2] = {0};
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_LargeFollower(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_LARGE_FOLLOWER);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsfollowerLargeEnable = 0; //on
    }
    else
    {
        gSaveBlock2Ptr->optionsfollowerLargeEnable = 1; //off
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}


static void DrawChoices_Autorun(int selection, int y)
{
    const u8 *const names[3] = {sText_Choice_Off, sText_Choice_Land, sText_Choice_Everywhere};
    bool8 active = CheckConditions(MENUITEM_MAIN_AUTORUN);

    if (selection < 0 || selection > 2)
        selection = 0;
    DrawOptionMenuChoice(names[selection], 104, y, 1, active);
}

static void DrawChoices_MatchCall(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_MATCHCALL);
    u8 styles[2] = {0};
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_Style(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_SPLIT);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionStyle = 0; //Phy / sp split on
    }
    else
    {
        gSaveBlock2Ptr->optionStyle = 1; //Phy / sp split off
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_TypeEffective(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_TYPE_EFFECTIVE);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionTypeEffective = 0; //Yes
    }
    else
    {
        gSaveBlock2Ptr->optionTypeEffective = 1; //No
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_Fishing(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_FISHING);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsFishing = 0; //FRLG
    }
    else
    {
        gSaveBlock2Ptr->optionsFishing = 1; //Emerald
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_FastIntro(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_FAST_INTRO);
    DrawOptionMenuChoice(selection == 0 ? sText_Choice_Fast : sText_Choice_Slow, 104, y, 1, active);
}

static void DrawChoices_FastBattles(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_FAST_BATTLES);
    DrawOptionMenuChoice(selection == 0 ? sText_Choice_Fast : sText_Choice_Slow, 104, y, 1, active);
}

static const u8 sText_BattleSpeed1x[] = _("1x");
static const u8 sText_BattleSpeed2x[] = _("2x");

static void DrawChoices_BattleSpeed(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_BATTLE_SPEED);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsBattleSpeed = 0; // 1x (normal)
    }
    else
    {
        gSaveBlock2Ptr->optionsBattleSpeed = 1; // 2x (double speed)
    }

    DrawOptionMenuChoice(sText_BattleSpeed1x, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_BattleSpeed2x, GetStringRightAlignXOffset(1, sText_BattleSpeed2x, 198), y, styles[1], active);
}

static void DrawChoices_BikeMusic(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_BIKE_MUSIC);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsBikeMusic = 0; //music on
    }
    else
    {
        gSaveBlock2Ptr->optionsBikeMusic = 1; //music off
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_EvenFasterJoy(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_EVEN_FASTER_JOY);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsEvenFasterJoy = 0; //extremely fast joy
        FlagSet(FLAG_EVEN_FASTER_JOY);
    }
    else
    {
        gSaveBlock2Ptr->optionsEvenFasterJoy = 1; //normal joy
        FlagClear(FLAG_EVEN_FASTER_JOY);
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_SurfMusic(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_SURF_MUSIC);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsSurfMusic = 0; //music on
    }
    else
    {
        gSaveBlock2Ptr->optionsSurfMusic = 1; //music off
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_Skip_Intro(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_SKIP_INTRO);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsSkipIntro = 0; //Skips intro
    }
    else
    {
        gSaveBlock2Ptr->optionsSkipIntro = 1; //Doesn't skip intro
    }
    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_LR_Run(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_LR_RUN);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsLRtoRun = 0; //Shows prompt
    }
    else
    {
        gSaveBlock2Ptr->optionsLRtoRun = 1; //Doesn't show prompt
    }
    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_Ball_Prompt(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_BALL_PROMPT);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsBallPrompt = 0; //Shows PKBALL prompt
    }
    else
    {
        gSaveBlock2Ptr->optionsBallPrompt = 1; //Doesn't show PKBALL prompt
    }
    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static const u8 sText_Metric[]        = _("Metric");
static const u8 sText_Imperial[]      = _("Imperial");
static void DrawChoices_Unit_Type(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_UNIT_TYPE);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsUnitSystem = 0; //METRIC
    }
    else
    {
        gSaveBlock2Ptr->optionsUnitSystem = 1; //IMPERIAL
    }
    DrawOptionMenuChoice(sText_Metric, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Imperial, GetStringRightAlignXOffset(1, sText_Imperial, 198), y, styles[1], active);
}

static void DrawChoices_Music(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_SOUND_MUSIC);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsMusicOnOff = 0; //Yes music
    }
    else
    {
        gSaveBlock2Ptr->optionsMusicOnOff = 1; //No music
    }
    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}
static const u8 sText_Old[]        = _("Old");
static const u8 sText_New[]        = _("Modern");
static const u8 sText_ArrowLeft[]  = _("{LEFT_ARROW}");
static const u8 sText_ArrowRight[] = _("{RIGHT_ARROW}");
static const u8 sText_ModernPlus[] = _("Modern +");
static void DrawChoices_New_Backgrounds(int selection, int y)
{
    // BACKGROUND: Old / Modern / Modern + / Perfect, one at a time between arrows
    bool8 active = CheckConditions(MENUITEM_BATTLE_BACKGROUND);
    const u8 *names[4] = {sText_Old, sText_New, sText_ModernPlus, sText_AlmostPerfect};

    if (selection < 0 || selection > 3)
        selection = 1;
    gSaveBlock2Ptr->optionsNewBackgrounds = (selection != 0);
    selection == 3 ? FlagSet(FLAG_PERFECT_BATTLE_BACKGROUNDS) : FlagClear(FLAG_PERFECT_BATTLE_BACKGROUNDS);
    selection == 2 ? FlagSet(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS) : FlagClear(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS);

    DrawOptionMenuChoice(sText_ArrowLeft, 98, y, 0, active);   // v5.1: room for "Almost Perfect"
    DrawOptionMenuChoice(names[selection], 151 - GetStringWidth(FONT_NORMAL, names[selection], 0) / 2, y, 1, active);
    DrawOptionMenuChoice(sText_ArrowRight, 197, y, 0, active);
}

static const u8 sText_With[]    = _("With");
static const u8 sText_Without[] = _("Without");
static void DrawChoices_Terrain_Circles(int selection, int y)
{
    // BATTLE TERRAIN: With / Without terrain circles
    bool8 active = CheckConditions(MENUITEM_BATTLE_NEW_BACKGROUNDS);
    u8 styles[2] = {0};

    if (selection < 0 || selection > 1)
        selection = 0;
    styles[selection] = 1;
    if (selection)
        FlagSet(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);
    else
        FlagClear(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);
    DrawOptionMenuChoice(sText_With, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Without, GetStringRightAlignXOffset(1, sText_Without, 198), y, styles[1], active);
}

static const u8 sText_No[]        = _("No");
static const u8 sText_LR[]        = _("L+R+A");
static const u8 sText_B[]         = _("B->A");
static const u8 sText_B_2[]       = _("B");
static const u8 *const sRunTypeStrings[] = {sText_No, sText_LR, sText_B, sText_B_2};
static void DrawChoices_Run_Type(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_RUN_TYPE);
    DrawChoices_Options_Four(sRunTypeStrings, selection, y, active);

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsRunType = 0; //No
        sOptions->sel_battle[MENUITEM_BATTLE_LR_RUN]          = gSaveBlock2Ptr->optionsLRtoRun = 1;
    }
    else if (selection == 1)
    {
        gSaveBlock2Ptr->optionsRunType = 1; //LR
    }
    else if (selection == 2)
    {
        gSaveBlock2Ptr->optionsRunType = 2; //B->A
        sOptions->sel_battle[MENUITEM_BATTLE_LR_RUN]          = gSaveBlock2Ptr->optionsLRtoRun = 1;
    }
    else
    {
        gSaveBlock2Ptr->optionsRunType = 3; //Hold B (before battle)
    }
}

static void DrawChoices_Autorun_Surf(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_AUTORUN_SURF);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsAutorunSurf = 0; //yes
    }
    else
    {
        gSaveBlock2Ptr->optionsAutorunSurf = 1; //no
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static void DrawChoices_Autorun_Dive(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_AUTORUN_DIVE);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsAutorunDive = 0; //yes
    }
    else
    {
        gSaveBlock2Ptr->optionsAutorunDive = 1; //no
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

static const u8 sText_Dynamic[]        = _("Dynamic");
static void DrawChoices_SurfOverworld(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_SURFOVERWORLD);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsSurfOverworld = 0; //yeS
    }
    else
    {
        gSaveBlock2Ptr->optionsSurfOverworld = 1; //no
    }

    DrawOptionMenuChoice(sText_Dynamic, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Old, GetStringRightAlignXOffset(1, sText_Old, 198), y, styles[1], active);
}

static void DrawChoices_BrighterNights(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_BRIGHTER_NIGHTS);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsBrighterNights = 0; // Off (original)
    }
    else
    {
        gSaveBlock2Ptr->optionsBrighterNights = 1; // On (brighter)
    }

    DrawOptionMenuChoice(gText_BattleSceneOff, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOn, GetStringRightAlignXOffset(1, gText_BattleSceneOn, 198), y, styles[1], active);
}

static const u8 sText_Em[]          = _("Emerald");
static const u8 sText_FRLG[]        = _("FRLG");
static void DrawChoices_Font(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_FONT);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsFontType = 0; //Emerald Font
    }
    else
    {
        gSaveBlock2Ptr->optionsFontType = 1; //FRLG Font
    }

    DrawOptionMenuChoice(sText_Em, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_FRLG, GetStringRightAlignXOffset(1, sText_FRLG, 198), y, styles[1], active);
}

static void DrawChoices_CursorMemory(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_CURSOR_MEMORY);
    u8 styles[2] = {0};
    styles[selection] = 1;

    if (selection == 0)
    {
        gSaveBlock2Ptr->optionsCursorMemory = 0; //On
    }
    else
    {
        gSaveBlock2Ptr->optionsCursorMemory = 1; //Off
    }

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, y, styles[0], active);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(1, gText_BattleSceneOff, 198), y, styles[1], active);
}

// Background tilemap
#define TILE_TOP_CORNER_L 0x1A2 // 418
#define TILE_TOP_EDGE     0x1A3 // 419
#define TILE_TOP_CORNER_R 0x1A4 // 420
#define TILE_LEFT_EDGE    0x1A5 // 421
#define TILE_RIGHT_EDGE   0x1A7 // 423
#define TILE_BOT_CORNER_L 0x1A8 // 424
#define TILE_BOT_EDGE     0x1A9 // 425
#define TILE_BOT_CORNER_R 0x1AA // 426

static void DrawBgWindowFrames(void)
{
    //                     bg, tile,              x, y, width, height, palNum
    // Option Texts window
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  2,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  2, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  2,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  3,  1, 16,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  3,  1, 16,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 13,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 13, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 13,  1,  1,  7);

    // Description window
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1, 14,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2, 14, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28, 14,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1, 15,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28, 15,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 19,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 19, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 19,  1,  1,  7);

    CopyBgTilemapBufferToVram(1);
}

static const u8 sText_OWE_Off[]      = _("Off");
static const u8 sText_OWE_On[]       = _("On");
static const u8 sText_OWE_Restrict[] = _("Restrict");
static void DrawChoices_OWE(int selection, int y)
{
    // OWE: overworld wild encounters - Off / On / Restrict (stay in the grass)
    bool8 active = CheckConditions(MENUITEM_BATTLE_OWE);
    u8 styles[3] = {0};
    int xMid;

    if (selection < 0 || selection > 2)
        selection = 0;
    styles[selection] = 1;
    xMid = 104 + GetStringWidth(1, sText_OWE_Off, 0) + 10;
    DrawOptionMenuChoice(sText_OWE_Off, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_OWE_On, xMid, y, styles[1], active);
    DrawOptionMenuChoice(sText_OWE_Restrict, GetStringRightAlignXOffset(1, sText_OWE_Restrict, 198), y, styles[2], active);
}

static const u8 sText_Shadows_Off[] = _("Off");
static const u8 sText_Shadows_On[]  = _("On");
static void DrawChoices_Shadows(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_SHADOWS);
    u8 styles[2] = {0};

    if (selection < 0 || selection > 1)
        selection = 1;
    styles[selection] = 1;
    DrawOptionMenuChoice(sText_Shadows_Off, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Shadows_On, GetStringRightAlignXOffset(1, sText_Shadows_On, 198), y, styles[1], active);
}

static const u8 sText_WeatherFx_Regular[] = _("Regular");
static const u8 sText_WeatherFx_Perfect[] = _("Perfect");
static void DrawChoices_WeatherFx(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_WEATHER_FX);
    u8 styles[2] = {0};

    if (selection < 0 || selection > 1)
        selection = 1;
    styles[selection] = 1;
    DrawOptionMenuChoice(sText_WeatherFx_Regular, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_WeatherFx_Perfect, GetStringRightAlignXOffset(FONT_NORMAL, sText_WeatherFx_Perfect, 198), y, styles[1], active);
}

static const u8 sText_UI_Default[] = _("Default");
static const u8 sText_UI_Perfect[] = _("Perfect");
static void DrawChoices_UI(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_UI);
    u8 styles[2] = {0};

    if (selection < 0 || selection > 1)
        selection = 0;
    styles[selection] = 1;
    DrawOptionMenuChoice(sText_UI_Default, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_UI_Perfect, GetStringRightAlignXOffset(FONT_NORMAL, sText_UI_Perfect, 198), y, styles[1], active);
}

static const u8 sText_Weather_Scripted[] = _("Scripted");
static const u8 sText_Weather_Perfect[]  = _("Perfect");
static void DrawChoices_WeatherSystem(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_WEATHER_SYSTEM);
    u8 styles[2] = {0};

    if (selection < 0 || selection > 1)
        selection = 1;
    styles[selection] = 1;
    DrawOptionMenuChoice(sText_Weather_Scripted, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Weather_Perfect, GetStringRightAlignXOffset(FONT_NORMAL, sText_Weather_Perfect, 198), y, styles[1], active);
}

static const u8 sText_Puddles_Off[]  = _("Off");
static const u8 sText_Puddles_Slow[] = _("Slow");
static const u8 sText_Puddles_Fast[] = _("Fast");
static void DrawChoices_Puddles(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_PUDDLES);
    u8 styles[3] = {0};

    if (selection < 0 || selection > 2)
        selection = 1;
    styles[selection] = 1;
    DrawOptionMenuChoice(sText_Puddles_Off, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Puddles_Slow, GetMiddleX(sText_Puddles_Off, sText_Puddles_Slow, sText_Puddles_Fast), y, styles[1], active);
    DrawOptionMenuChoice(sText_Puddles_Fast, GetStringRightAlignXOffset(FONT_NORMAL, sText_Puddles_Fast, 198), y, styles[2], active);
}

static const u8 sText_Spread_Normal[]   = _("Normal");
static const u8 sText_Spread_Wide[]     = _("Wide");
static const u8 sText_Spread_Perfect[]  = _("Perfect");
static void DrawChoices_WeatherSpread(int selection, int y)
{
    // WEATHER SPREAD: Normal / Extended / Perfect, one at a time between arrows
    bool8 active = CheckConditions(MENUITEM_MAIN_WEATHER_SPREAD);
    const u8 *names[3] = {sText_Spread_Normal, sText_Spread_Wide, sText_Spread_Perfect};

    if (selection < 0 || selection > 2)
        selection = 2;
    DrawOptionMenuChoice(sText_ArrowLeft, 104, y, 0, active);
    DrawOptionMenuChoice(names[selection], 151 - GetStringWidth(FONT_NORMAL, names[selection], 0) / 2, y, 1, active);
    DrawOptionMenuChoice(sText_ArrowRight, 192, y, 0, active);
}

static const u8 sText_Pond_Dynamic[] = _("Dynamic");
static const u8 sText_Pond_Default[] = _("Default");
static void DrawChoices_Pond(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_POND);
    u8 styles[2] = {0};

    if (selection < 0 || selection > 1)
        selection = 1;
    styles[selection] = 1;
    DrawOptionMenuChoice(sText_Pond_Dynamic, 104, y, styles[0], active);
    DrawOptionMenuChoice(sText_Pond_Default, GetStringRightAlignXOffset(FONT_NORMAL, sText_Pond_Default, 198), y, styles[1], active);
}

static void DrawChoices_UiBag(int selection, int y)
{
    const u8 *const names[3] = {sText_Choice_Vanilla, sText_Choice_Modern, sText_Choice_Perfect};
    bool8 active = CheckConditions(MENUITEM_MAIN_UI_BAG);

    if (selection < 0 || selection > 2)
        selection = 0;
    DrawOptionMenuChoice(names[selection], 104, y, 1, active);
}

// v6.0: rows - a header (bold small text, nothing else on the line) or a setting
// drawn as  <  current choice  >  with the arrows always in the same columns
static void DrawChoices(u32 row, int y)
{
    u8 item;

    if (IsHeaderRow(row))
        return;
    item = SelectRow(row);
    sChoiceCapture = TRUE;
    sChoiceCaptured = FALSE;
    sChoiceActive = TRUE;
    DrawChoicesItem(item, y);
    sChoiceCapture = FALSE;
    if (!sChoiceCaptured)
        return;
    DrawRightSideChoiceText(sText_ArrowLeft, 102, y + 1, FALSE, sChoiceActive);
    DrawRightSideChoiceText(sChoiceText, 151 - GetStringWidth(FONT_NORMAL, sChoiceText, 0) / 2, y + 1, TRUE, sChoiceActive);
    DrawRightSideChoiceText(sText_ArrowRight, 194, y + 1, FALSE, sChoiceActive);
}

static void DrawLeftSideOptionText(int row, int y)
{
    if (IsHeaderRow(row))
    {
        static const u8 color[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_OPTIONS_GRAY_FG, TEXT_COLOR_OPTIONS_GRAY_SHADOW};
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_SMALL, 4, y + 1, 0, 0, color, TEXT_SKIP_DRAW,
                                     sHeaderTexts[sPages[sOptions->page].rows[row].item]);
        return;
    }
    DrawLeftSideOptionTextItem(SelectRow(row), y);
}

static const u8 sText_Choice_Old[] = _("Old");
static void DrawChoices_Mist(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_MAIN_MIST);
    DrawOptionMenuChoice(selection == 0 ? sText_Choice_Old : sText_Choice_Perfect, 104, y, 1, active);
}

static const u8 sText_Choice_Less[] = _("Less");
static const u8 sText_Choice_More[] = _("More");
static void DrawChoices_SpawnRate(int selection, int y)
{
    const u8 *const names[3] = {sText_Choice_Less, sText_Choice_More, sText_Choice_Perfect};
    bool8 active = CheckConditions(MENUITEM_MAIN_SPAWN_RATE);

    if (selection < 0 || selection > 2)
        selection = 2;
    DrawOptionMenuChoice(names[selection], 104, y, 1, active);
}

#include "global.h"
#include "text_window.h"
#include "new_game.h"
#include "random.h"
#include "pokemon.h"
#include "roamer.h"
#include "pokemon_size_record.h"
#include "script.h"
#include "lottery_corner.h"
#include "play_time.h"
#include "mauville_old_man.h"
#include "match_call.h"
#include "lilycove_lady.h"
#include "load_save.h"
#include "pokeblock.h"
#include "dewford_trend.h"
#include "berry.h"
#include "rtc.h"
#include "easy_chat.h"
#include "event_data.h"
#include "money.h"
#include "trainer_hill.h"
#include "tv.h"
#include "coins.h"
#include "text.h"
#include "overworld.h"
#include "mail.h"
#include "battle_records.h"
#include "item.h"
#include "pokedex.h"
#include "apprentice.h"
#include "frontier_util.h"
#include "pokedex.h"
#include "save.h"
#include "link_rfu.h"
#include "main.h"
#include "contest.h"
#include "item_menu.h"
#include "pokemon_storage_system.h"
#include "pokemon_jump.h"
#include "decoration_inventory.h"
#include "secret_base.h"
#include "player_pc.h"
#include "field_specials.h"
#include "berry_powder.h"
#include "mystery_gift.h"
#include "union_room_chat.h"
#include "constants/items.h"
#include "tx_randomizer_and_challenges.h"

extern const u8 EventScript_ResetAllMapFlags[];

static void ClearFrontierRecord(void);
static void WarpToTruck(void);
static void ResetMiniGamesRecords(void);

EWRAM_DATA bool8 gDifferentSaveFile = FALSE;
EWRAM_DATA bool8 gEnableContestDebugging = FALSE;

static const struct ContestWinner sContestWinnerPicDummy =
{
    .monName = _(""),
    .trainerName = _("")
};

void SetTrainerId(u32 trainerId, u8 *dst)
{
    dst[0] = trainerId;
    dst[1] = trainerId >> 8;
    dst[2] = trainerId >> 16;
    dst[3] = trainerId >> 24;
}

u32 GetTrainerId(u8 *trainerId)
{
    return (trainerId[3] << 24) | (trainerId[2] << 16) | (trainerId[1] << 8) | (trainerId[0]);
}

void CopyTrainerId(u8 *dst, u8 *src)
{
    s32 i;
    for (i = 0; i < TRAINER_ID_LENGTH; i++)
        dst[i] = src[i];
}

static void InitPlayerTrainerId(void)
{
    u32 trainerId = (Random() << 16) | GetGeneratedTrainerIdLower();
    SetTrainerId(trainerId, gSaveBlock2Ptr->playerTrainerId);
}

// L=A isnt set here for some reason.
static void SetDefaultOptions(void)
{
    gSaveBlock2Ptr->optionsTextSpeed = OPTIONS_TEXT_SPEED_FAST;
    gSaveBlock2Ptr->optionsWindowFrameType = 0;
    gSaveBlock2Ptr->optionsSound = OPTIONS_SOUND_STEREO;
    gSaveBlock2Ptr->optionsBattleStyle = OPTIONS_BATTLE_STYLE_SET;
    gSaveBlock2Ptr->optionsBattleSceneOff = FALSE;
    gSaveBlock2Ptr->regionMapZoom = FALSE;
    gSaveBlock2Ptr->optionsDifficulty = 1;
    gSaveBlock2Ptr->optionsfollowerEnable = 0;
    gSaveBlock2Ptr->optionsfollowerLargeEnable = 0;   // v7.1: LARGE FOLLOWER On (0 = on)
    gSaveBlock2Ptr->optionsautoRun = 1;
    gSaveBlock2Ptr->optionsAutorunDive = 1;
    gSaveBlock2Ptr->optionsAutorunSurf = 1;
    gSaveBlock2Ptr->optionsDisableMatchCall = 0;
    gSaveBlock2Ptr->optionStyle = 0;
    gSaveBlock2Ptr->optionTypeEffective = 0;
    gSaveBlock2Ptr->optionsFishing = 1;
    gSaveBlock2Ptr->optionsFastIntro = 0; // v3.0b: FAST INTRO on by default (0 = on)
    gSaveBlock2Ptr->optionsFastBattle = 0;            // v7.1: BATTLE TEXT Fast (0 = fast)
    gSaveBlock2Ptr->optionsBattleSpeed = 0;           // ANIM SPEED 1x
    gSaveBlock2Ptr->optionsBikeMusic = 0;
    gSaveBlock2Ptr->optionsEvenFasterJoy = 1;
    gSaveBlock2Ptr->optionsSurfMusic = 0;
    gSaveBlock2Ptr->optionsWildBattleMusic = 0;
    gSaveBlock2Ptr->optionsTrainerBattleMusic = 0;
    gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 0;
    gSaveBlock2Ptr->optionsSoundEffects = 0;
    gSaveBlock2Ptr->optionsSkipIntro = 1;
    gSaveBlock2Ptr->optionsLRtoRun = 0;
    gSaveBlock2Ptr->optionsBallPrompt = 1;
    gSaveBlock2Ptr->optionsUnitSystem = 0;
    gSaveBlock2Ptr->optionsMusicOnOff = 0;
    // v2.8 defaults for a first start: BACKGROUND = Perfect, BATTLE TERRAIN = With
    gSaveBlock2Ptr->optionsNewBackgrounds = 1;
    FlagSet(FLAG_PERFECT_BATTLE_BACKGROUNDS);
    FlagClear(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS);
    FlagSet(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);   // v7.1: BATTLE TERRAIN Without
    FlagSet(FLAG_OWE_ON);              // v3.2: OWE On by default
    FlagClear(FLAG_OWE_RESTRICT);
    FlagClear(FLAG_BATTLE_SHADOWS_OFF); // SHADOWS: On
    FlagClear(FLAG_BATTLE_WEATHER_REGULAR); // v3.3: WEATHER EFFECT Perfect
    FlagClear(FLAG_BATTLE_ANIMS_EXTENDED);  // v7.1: POKEMON'S ANIMATION Perfect
    FlagSet(FLAG_BATTLE_ANIMS_PERFECT);
    FlagClear(FLAG_BATTLE_ANIMS_PERFECT2);
    FlagSet(FLAG_UI_PERFECT);               // v7.1: UI INTERFACE Perfect
    FlagClear(FLAG_WEATHER_SCRIPTED);       // v4.0: WEATHER SYSTEM Perfect
    FlagClear(FLAG_PUDDLES_OFF);            // v4.0: PUDDLES Slow
    FlagClear(FLAG_PUDDLES_FAST);
    FlagSet(FLAG_WEATHER_NEW_TIMES);        // v5.1: WEATHER SPREAD Perfect (new times, normal areas)
    FlagClear(FLAG_WEATHER_WIDE_ZONES);
    FlagClear(FLAG_POND_FREEZE_DYNAMIC);    // v5.1: POND BEHAVIOR Default
    FlagClear(FLAG_MIST_OLD);               // v6.4: MIST Perfect
    FlagClear(FLAG_SPAWN_RATE_LESS);        // v6.5: SPAWN RATE Perfect
    FlagClear(FLAG_SPAWN_RATE_MORE);
    FlagClear(FLAG_UI_BAG_MODERN);          // v7.1: UI BAG Perfect
    FlagSet(FLAG_UI_BAG_PERFECT);
    FlagClear(FLAG_BATTLE_ANIMS_NO_INTRO);
    FlagClear(FLAG_BATTLE_ANIMS_MODERN);
    gSaveBlock2Ptr->optionsRunType = 1;
    gSaveBlock2Ptr->optionsSurfOverworld = 0;
    gSaveBlock2Ptr->optionsCursorMemory = 1;
    gSaveBlock2Ptr->optionsBrighterNights = 0;
}

static void ClearPokedexFlags(void)
{
    gUnusedPokedexU8 = 0;
    memset(&gSaveBlock2Ptr->pokedex.owned, 0, sizeof(gSaveBlock2Ptr->pokedex.owned));
    memset(&gSaveBlock2Ptr->pokedex.seen, 0, sizeof(gSaveBlock2Ptr->pokedex.seen));
}

void ClearAllContestWinnerPics(void)
{
    s32 i;

    ClearContestWinnerPicsInContestHall();

    // Clear Museum paintings
    for (i = MUSEUM_CONTEST_WINNERS_START; i < NUM_CONTEST_WINNERS; i++)
        gSaveBlock1Ptr->contestWinners[i] = sContestWinnerPicDummy;
}

static void ClearFrontierRecord(void)
{
    CpuFill32(0, &gSaveBlock2Ptr->frontier, sizeof(gSaveBlock2Ptr->frontier));

    gSaveBlock2Ptr->frontier.opponentNames[0][0] = EOS;
    gSaveBlock2Ptr->frontier.opponentNames[1][0] = EOS;
}

static void WarpToTruck(void)
{
    SaveData_TxRandomizerAndChallenges();
    SetWarpDestination(MAP_GROUP(INSIDE_OF_TRUCK), MAP_NUM(INSIDE_OF_TRUCK), WARP_ID_NONE, -1, -1);
    WarpIntoMap();
}

// FIX (Options crash on real hardware, Sound page / "Wild Music" line):
// options are only set to defaults when the save is empty. A save written by
// another game or version (e.g. one left on a flash cart) can hold values in
// these bits that no option allows, e.g. Wild Music = 19 of 0-6. The Options
// menu then read past its tables and wrote past a small array on the stack,
// which shows garbage or freezes on hardware (the exact damage depends on
// what the stray value points at). Every multi-choice option is now pulled
// back into range when a save is loaded, on New Game, and in the menu.
void SanitizeOptions(void)
{
    if (gSaveBlock2Ptr->optionsTextSpeed > 3)
        gSaveBlock2Ptr->optionsTextSpeed = OPTIONS_TEXT_SPEED_FAST;
    if (gSaveBlock2Ptr->optionsWindowFrameType >= WINDOW_FRAMES_COUNT)
        gSaveBlock2Ptr->optionsWindowFrameType = 0;
    if (gSaveBlock2Ptr->optionsDifficulty > 2)
        gSaveBlock2Ptr->optionsDifficulty = 1;
    if (gSaveBlock2Ptr->optionsButtonMode > 2)
        gSaveBlock2Ptr->optionsButtonMode = 0;
    if (gSaveBlock2Ptr->optionsRunType > 3)
        gSaveBlock2Ptr->optionsRunType = 0;
    if (gSaveBlock2Ptr->optionsWildBattleMusic > 6)
        gSaveBlock2Ptr->optionsWildBattleMusic = 0;
    if (gSaveBlock2Ptr->optionsTrainerBattleMusic > 6)
        gSaveBlock2Ptr->optionsTrainerBattleMusic = 0;
    if (gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic > 6)
        gSaveBlock2Ptr->optionsFrontierTrainerBattleMusic = 0;
    if (gSaveBlock2Ptr->optionsSoundEffects > 2)
        gSaveBlock2Ptr->optionsSoundEffects = 0;
    if (gSaveBlock2Ptr->optionsautoRun > 1)
        gSaveBlock2Ptr->optionsautoRun = 1;
}

void Sav2_ClearSetDefault(void)
{
    ClearSav2();
    SetDefaultOptions();
}

void ResetMenuAndMonGlobals(void)
{
    gDifferentSaveFile = FALSE;
    ResetPokedexScrollPositions();
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    ResetBagScrollPositions();
    ResetPokeblockScrollPositions();
}

void NewGameInitData(void)
{
    SanitizeOptions();
    bool8 HardPrev = FlagGet(FLAG_DIFFICULTY_HARD);
    bool8 TMPrev = FlagGet(FLAG_FINITE_TMS);
    bool8 UnlimitedWT = FlagGet(FLAG_UNLIMITIED_WONDERTRADE);
    bool8 EnableMints = FlagGet(FLAG_MINTS_ENABLED);
    bool8 EnableExtraLegendaries = FlagGet(FLAG_EXTRA_LEGENDARIES);
    bool8 FasterJoy = FlagGet(FLAG_EVEN_FASTER_JOY);
    // the two battle-background options live in flags: keep them through the reset
    bool8 PerfectBgPrev = FlagGet(FLAG_PERFECT_BATTLE_BACKGROUNDS);
    bool8 ModernPlusPrev = FlagGet(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS);
    bool8 CirclesOffPrev = FlagGet(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);
    bool8 OweOnPrev = FlagGet(FLAG_OWE_ON), OweRestrictPrev = FlagGet(FLAG_OWE_RESTRICT);
    bool8 ShadowsOffPrev = FlagGet(FLAG_BATTLE_SHADOWS_OFF);
    bool8 WeatherRegularPrev = FlagGet(FLAG_BATTLE_WEATHER_REGULAR), AnimsExtendedPrev = FlagGet(FLAG_BATTLE_ANIMS_EXTENDED);
    bool8 UiPerfectPrev = FlagGet(FLAG_UI_PERFECT);
    bool8 NewTimesPrev = FlagGet(FLAG_WEATHER_NEW_TIMES), WideZonesPrev = FlagGet(FLAG_WEATHER_WIDE_ZONES), PondPrev = FlagGet(FLAG_POND_FREEZE_DYNAMIC);
    bool8 BagModernPrev = FlagGet(FLAG_UI_BAG_MODERN), BagPerfectPrev = FlagGet(FLAG_UI_BAG_PERFECT), NoIntroPrev = FlagGet(FLAG_BATTLE_ANIMS_NO_INTRO), AnimsModernPrev = FlagGet(FLAG_BATTLE_ANIMS_MODERN), AnimsP1Prev = FlagGet(FLAG_BATTLE_ANIMS_PERFECT), AnimsP2Prev = FlagGet(FLAG_BATTLE_ANIMS_PERFECT2), MistOldPrev = FlagGet(FLAG_MIST_OLD), SpawnLessPrev = FlagGet(FLAG_SPAWN_RATE_LESS), SpawnMorePrev = FlagGet(FLAG_SPAWN_RATE_MORE);
    bool8 WeatherScriptedPrev = FlagGet(FLAG_WEATHER_SCRIPTED), PuddlesOffPrev = FlagGet(FLAG_PUDDLES_OFF), PuddlesFastPrev = FlagGet(FLAG_PUDDLES_FAST);

    if (gSaveFileStatus == SAVE_STATUS_EMPTY || gSaveFileStatus == SAVE_STATUS_CORRUPT)
        RtcReset();

    gDifferentSaveFile = TRUE;
    gSaveBlock2Ptr->encryptionKey = 0;
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    ResetPokedex();
    ClearFrontierRecord();
    ClearSav1();
    ClearAllMail();
    gSaveBlock2Ptr->specialSaveWarpFlags = 0;
    gSaveBlock2Ptr->gcnLinkFlags = 0;
    InitPlayerTrainerId();
    PlayTimeCounter_Reset();
    ClearPokedexFlags();
    InitEventData();
    ClearTVShowData();
    ResetGabbyAndTy();
    ClearSecretBases();
    ClearBerryTrees();
    SetMoney(&gSaveBlock1Ptr->money, 3000);
    SetCoins(0);
    ResetLinkContestBoolean();
    ResetGameStats();
    ClearAllContestWinnerPics();
    ClearPlayerLinkBattleRecords();
    InitSeedotSizeRecord();
    InitLotadSizeRecord();
    gPlayerPartyCount = 0;
    ZeroPlayerPartyMons();
    ResetPokemonStorageSystem();
    ClearRoamerData();
    ClearRoamerLocationData();
    gSaveBlock1Ptr->registeredItem = ITEM_NONE;
    gSaveBlock1Ptr->registeredLongItem = 0;
    ClearBag();
    NewGameInitPCItems();
    ClearPokeblocks();
    ClearDecorationInventories();
    InitEasyChatPhrases();
    SetMauvilleOldMan();
    InitDewfordTrend();
    ResetFanClub();
    ResetLotteryCorner();
    WarpToTruck();
    RunScriptImmediately(EventScript_ResetAllMapFlags);
    ResetMiniGamesRecords();
    InitUnionRoomChatRegisteredTexts();
    InitLilycoveLady();
    ResetAllApprenticeData();
    ClearRankingHallRecords();
    InitMatchCallCounters();
    ClearMysteryGift();
    WipeTrainerNameRecords();
    ResetTrainerHillResults();
    ResetContestLinkResults();
    gSaveBlock1Ptr->typeRandomizerSeed = Random32() & 0xFFFF;
    RandomizeTypeEffectivenessListEWRAM(gSaveBlock1Ptr->typeRandomizerSeed);
    if ((gSaveBlock1Ptr->tx_Nuzlocke_EasyMode) && (gSaveBlock1Ptr->tx_Challenges_Nuzlocke))
        gSaveBlock1Ptr->tx_Nuzlocke_EasyMode = 0;

    HardPrev ? FlagSet(FLAG_DIFFICULTY_HARD) : FlagClear(FLAG_DIFFICULTY_HARD);
    PerfectBgPrev ? FlagSet(FLAG_PERFECT_BATTLE_BACKGROUNDS) : FlagClear(FLAG_PERFECT_BATTLE_BACKGROUNDS);
    ModernPlusPrev ? FlagSet(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS) : FlagClear(FLAG_MODERN_PLUS_BATTLE_BACKGROUNDS);
    CirclesOffPrev ? FlagSet(FLAG_BATTLE_TERRAIN_CIRCLES_OFF) : FlagClear(FLAG_BATTLE_TERRAIN_CIRCLES_OFF);
    OweOnPrev ? FlagSet(FLAG_OWE_ON) : FlagClear(FLAG_OWE_ON);
    OweRestrictPrev ? FlagSet(FLAG_OWE_RESTRICT) : FlagClear(FLAG_OWE_RESTRICT);
    ShadowsOffPrev ? FlagSet(FLAG_BATTLE_SHADOWS_OFF) : FlagClear(FLAG_BATTLE_SHADOWS_OFF);
    WeatherRegularPrev ? FlagSet(FLAG_BATTLE_WEATHER_REGULAR) : FlagClear(FLAG_BATTLE_WEATHER_REGULAR);
    AnimsExtendedPrev ? FlagSet(FLAG_BATTLE_ANIMS_EXTENDED) : FlagClear(FLAG_BATTLE_ANIMS_EXTENDED);
    UiPerfectPrev ? FlagSet(FLAG_UI_PERFECT) : FlagClear(FLAG_UI_PERFECT);
    WeatherScriptedPrev ? FlagSet(FLAG_WEATHER_SCRIPTED) : FlagClear(FLAG_WEATHER_SCRIPTED);
    NewTimesPrev ? FlagSet(FLAG_WEATHER_NEW_TIMES) : FlagClear(FLAG_WEATHER_NEW_TIMES);
    WideZonesPrev ? FlagSet(FLAG_WEATHER_WIDE_ZONES) : FlagClear(FLAG_WEATHER_WIDE_ZONES);
    PondPrev ? FlagSet(FLAG_POND_FREEZE_DYNAMIC) : FlagClear(FLAG_POND_FREEZE_DYNAMIC);
    BagModernPrev ? FlagSet(FLAG_UI_BAG_MODERN) : FlagClear(FLAG_UI_BAG_MODERN);
    BagPerfectPrev ? FlagSet(FLAG_UI_BAG_PERFECT) : FlagClear(FLAG_UI_BAG_PERFECT);
    NoIntroPrev ? FlagSet(FLAG_BATTLE_ANIMS_NO_INTRO) : FlagClear(FLAG_BATTLE_ANIMS_NO_INTRO);
    AnimsModernPrev ? FlagSet(FLAG_BATTLE_ANIMS_MODERN) : FlagClear(FLAG_BATTLE_ANIMS_MODERN);
    AnimsP1Prev ? FlagSet(FLAG_BATTLE_ANIMS_PERFECT) : FlagClear(FLAG_BATTLE_ANIMS_PERFECT);
    AnimsP2Prev ? FlagSet(FLAG_BATTLE_ANIMS_PERFECT2) : FlagClear(FLAG_BATTLE_ANIMS_PERFECT2);
    MistOldPrev ? FlagSet(FLAG_MIST_OLD) : FlagClear(FLAG_MIST_OLD);
    SpawnLessPrev ? FlagSet(FLAG_SPAWN_RATE_LESS) : FlagClear(FLAG_SPAWN_RATE_LESS);
    SpawnMorePrev ? FlagSet(FLAG_SPAWN_RATE_MORE) : FlagClear(FLAG_SPAWN_RATE_MORE);
    PuddlesOffPrev ? FlagSet(FLAG_PUDDLES_OFF) : FlagClear(FLAG_PUDDLES_OFF);
    PuddlesFastPrev ? FlagSet(FLAG_PUDDLES_FAST) : FlagClear(FLAG_PUDDLES_FAST);
    TMPrev ? FlagSet(FLAG_FINITE_TMS) : FlagClear(FLAG_FINITE_TMS);
    UnlimitedWT ? FlagSet(FLAG_UNLIMITIED_WONDERTRADE) : FlagClear(FLAG_UNLIMITIED_WONDERTRADE);
    EnableMints ? FlagSet(FLAG_MINTS_ENABLED) : FlagClear(FLAG_MINTS_ENABLED);
    EnableExtraLegendaries ? FlagSet(FLAG_EXTRA_LEGENDARIES) : FlagClear(FLAG_EXTRA_LEGENDARIES);
    FasterJoy ? FlagSet(FLAG_EVEN_FASTER_JOY) : FlagClear(FLAG_EVEN_FASTER_JOY);

    /*if (difficultyPrev == DIFFICULTY_EASY)
        VarSet(VAR_DIFFICULTY, DIFFICULTY_EASY);
    else if (difficultyPrev == DIFFICULTY_NORMAL)
        VarSet(VAR_DIFFICULTY, DIFFICULTY_NORMAL);
    else if (difficultyPrev == DIFFICULTY_HARD)
        VarSet(VAR_DIFFICULTY, DIFFICULTY_HARD);*/
    
}
void CheckIfChallengesAreActive(void)
{
    if (FlagGet(FLAG_SYS_GAME_CLEAR) == FALSE)
    {
        if (((gSaveBlock1Ptr->tx_Challenges_Nuzlocke) == 1)
        || (gSaveBlock1Ptr->tx_Nuzlocke_EasyMode == 1)
        || (gSaveBlock1Ptr->tx_Challenges_EvoLimit == 1)
        || (gSaveBlock1Ptr->tx_Challenges_BaseStatEqualizer == 1)
        || (gSaveBlock1Ptr->tx_Challenges_Mirror == 1)
        || (gSaveBlock1Ptr->tx_Challenges_Mirror_Thief == 1)
        || (gSaveBlock1Ptr->tx_Challenges_PkmnCenter == 1)
        || (IsOneTypeChallengeActive()))
            FlagSet(FLAG_NO_WT_BECAUSE_CHALLENGE);
    }
    else
        FlagClear(FLAG_NO_WT_BECAUSE_CHALLENGE);
}

void CheckIfRandomizerIsActive(void)
{
    if (((gSaveBlock1Ptr->tx_Random_Chaos == 1)
        || (gSaveBlock1Ptr->tx_Random_WildPokemon == 1)
        || (gSaveBlock1Ptr->tx_Random_Similar == 1)
        || (gSaveBlock1Ptr->tx_Random_MapBased == 1)
        || (gSaveBlock1Ptr->tx_Random_IncludeLegendaries == 1)
        || (gSaveBlock1Ptr->tx_Random_Type == 1)
        || (gSaveBlock1Ptr->tx_Random_TypeEffectiveness == 1)
        || (gSaveBlock1Ptr->tx_Random_Abilities == 1)
        || (gSaveBlock1Ptr->tx_Random_Moves == 1)
        || (gSaveBlock1Ptr->tx_Random_Trainer == 1)
        || (gSaveBlock1Ptr->tx_Random_Evolutions == 1)
        || (gSaveBlock1Ptr->tx_Random_EvolutionMethods == 1)
        || (gSaveBlock1Ptr->tx_Random_Items == 1)))
            FlagSet(FLAG_WT_ENABLED_RANDOMIZER);
}

static void ResetMiniGamesRecords(void)
{
    CpuFill16(0, &gSaveBlock2Ptr->berryCrush, sizeof(struct BerryCrush));
    SetBerryPowder(&gSaveBlock2Ptr->berryCrush.berryPowderAmount, 0);
    ResetPokemonJumpRecords();
    CpuFill16(0, &gSaveBlock2Ptr->berryPick, sizeof(struct BerryPickingResults));
}

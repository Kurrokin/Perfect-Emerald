#ifndef GUARD_BATTLE_BG_H
#define GUARD_BATTLE_BG_H

void BattleInitBgsAndWindows(void);
void InitBattleBgsVideo(void);
void LoadBattleMenuWindowGfx(void);
void DrawMainBattleBackground(void);
void LoadBattleTextboxAndBackground(void);
void InitLinkBattleVsScreen(u8 taskId);
void DrawBattleEntryBackground(void);
bool8 LoadChosenBattleElement(u8 caseId);

enum
{
    BATTLE_BG_TIME_DAY,
    BATTLE_BG_TIME_TWILIGHT,
    BATTLE_BG_TIME_NIGHT,
};

#endif // GUARD_BATTLE_BG_H

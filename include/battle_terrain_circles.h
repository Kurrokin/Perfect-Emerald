#ifndef GUARD_BATTLE_TERRAIN_CIRCLES_H
#define GUARD_BATTLE_TERRAIN_CIRCLES_H

// Terrain circles (platforms) under the battlers, BATTLE TERRAIN: New only.
void ResetBattleTerrainCircles(void);
void ClearBattleTerrainCircles(void);                         // a background without circles is drawn
void SetBattleTerrainCircles(u8 terrain, u32 timeOfDay);      // a New terrain background is drawn
void HideBattleTerrainCircles(void);                          // a move replaced the background
void UpdateBattleTerrainCircles(void);                        // every frame (BattleMainCB2)
u32 GetBattleTerrainCirclePaletteMask(void);                  // for background tints / blends

#endif // GUARD_BATTLE_TERRAIN_CIRCLES_H

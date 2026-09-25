#ifndef GUARD_OVERWORLD_WILD_ENCOUNTERS_H
#define GUARD_OVERWORLD_WILD_ENCOUNTERS_H

// OWE option (Battle options): wild Pokemon walking around in the grass.
void Task_OverworldWildEncounters(u8 taskId);
bool8 OweIsOweObject(const struct ObjectEvent *objectEvent);
bool8 OweBlocksMove(struct ObjectEvent *objectEvent, s16 x, s16 y);
void OweNoteCollision(struct ObjectEvent *mover, struct ObjectEvent *other);
bool8 OweSuppressRandomGrassEncounter(u16 behavior);

#endif // GUARD_OVERWORLD_WILD_ENCOUNTERS_H

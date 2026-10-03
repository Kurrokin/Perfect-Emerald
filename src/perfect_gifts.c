// v6.3: a gift for a player named Rudy - after the first Gym badge, a Dratini, a
// Cyndaquil and a Gastly (level 5, male, his trainer ID, maximum friendship) are
// put in the last PC box.
#include "global.h"
#include "event_data.h"
#include "malloc.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "pokedex.h"
#include "random.h"
#include "string_util.h"
#include "constants/species.h"

static const u8 sText_Rudy[] = _("Rudy");

void TryGiveRudyGift(void)
{
    static const u16 sSpecies[] = {SPECIES_DRATINI, SPECIES_CYNDAQUIL, SPECIES_GASTLY};
    struct Pokemon *mon;
    u32 i, pos, box = TOTAL_BOXES_COUNT - 1;

    if (FlagGet(FLAG_RUDY_GIFT_GIVEN) || !FlagGet(FLAG_BADGE01_GET))
        return;
    FlagSet(FLAG_RUDY_GIFT_GIVEN);   // checked once, whatever the name
    if (StringCompare(gSaveBlock2Ptr->playerName, sText_Rudy) != 0)
        return;
    mon = AllocZeroed(sizeof(struct Pokemon));
    if (mon == NULL)
        return;
    for (i = 0; i < ARRAY_COUNT(sSpecies); i++)
    {
        u8 friendship = MAX_FRIENDSHIP;
        u16 dexNum = SpeciesToNationalPokedexNum(sSpecies[i]);

        CreateMonWithGenderNatureLetter(mon, sSpecies[i], 5, 32, MON_MALE, Random() % NUM_NATURES, 0);
        SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship);
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            if (GetBoxMonData(GetBoxedMonPtr(box, pos), MON_DATA_SPECIES) == SPECIES_NONE)
            {
                SetBoxMonAt(box, pos, &mon->box);
                break;
            }
        }
        GetSetPokedexFlag(dexNum, FLAG_SET_SEEN);
        GetSetPokedexFlag(dexNum, FLAG_SET_CAUGHT);
    }
    Free(mon);
}

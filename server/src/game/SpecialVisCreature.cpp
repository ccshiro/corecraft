#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"

SpecialVisCreature::SpecialVisCreature()
  : Creature(CREATURE_SUBTYPE_SPECIAL_VIS)
{
}

void SpecialVisCreature::Update(uint32 update_diff, uint32 diff)
{
    Creature::Update(update_diff, diff);
}

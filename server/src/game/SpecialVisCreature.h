#ifndef GAME__SPECIAL_VIS_CREATURE_H
#define GAME__SPECIAL_VIS_CREATURE_H

#include "Creature.h"
#include "ObjectAccessor.h"

class SpecialVisCreature : public Creature
{
public:
    explicit SpecialVisCreature();
    void Update(uint32 update_diff, uint32 time);

private:
};
#endif

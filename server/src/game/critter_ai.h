#ifndef MANGOS__CRITTER_AI_H
#define MANGOS__CRITTER_AI_H

#include "CreatureAI.h"

class critter_ai : public CreatureAI
{
public:
    critter_ai(Creature* creature) : CreatureAI(creature), begun_fleeing_(false)
    {
    }

    void AttackStart(Unit* attacker) override;
    void AttackedBy(Unit* attacker) override;
    void UpdateAI(const uint32) override;
    void EnterEvadeMode(bool by_group = false) override;

private:
    bool begun_fleeing_;
};

#endif // MANGOS__CRITTER_AI_H

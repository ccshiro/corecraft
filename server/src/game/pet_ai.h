#ifndef MANGOS__PET_AI_H
#define MANGOS__PET_AI_H

#include "CreatureAI.h"

class Creature;

class pet_ai : public CreatureAI
{
public:
    pet_ai(Creature* pet);
    ~pet_ai();

    /* Overrides from Creature AI */
    void GetAIInformation(ChatHandler& reader) override;
    void EnterEvadeMode(bool by_group = false) override;
    void AttackedBy(Unit* attacker) override;
    void UpdateAI(const uint32 diff) override;
    void JustDied(Unit* attacker) override;
    void DamageTaken(Unit* attacker, uint32& damage) override;

private:
    Creature* pet_;
};

#endif

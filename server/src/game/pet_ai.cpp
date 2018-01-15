#include "pet_ai.h"
#include "Chat.h"
#include "Creature.h"
#include "pet_behavior.h"

pet_ai::pet_ai(Creature* pet) : CreatureAI(pet), pet_(pet)
{
    /* Empty */
}

pet_ai::~pet_ai()
{
}

void pet_ai::GetAIInformation(ChatHandler& reader)
{
    if (pet_->behavior())
        reader.PSendSysMessage("%s", pet_->behavior()->debug().c_str());
}

void pet_ai::EnterEvadeMode(bool)
{
    if (pet_->behavior())
        pet_->behavior()->evade();
}

void pet_ai::AttackedBy(Unit* attacker)
{
    if (pet_->behavior())
        pet_->behavior()->attacked(attacker);
}

void pet_ai::UpdateAI(const uint32 diff)
{
    if (pet_->behavior())
        pet_->behavior()->update(diff);
}

void pet_ai::JustDied(Unit*)
{
    if (pet_->behavior())
        pet_->behavior()->died();
}

void pet_ai::DamageTaken(Unit* attacker, uint32& damage)
{
    if (pet_->behavior())
        pet_->behavior()->damaged(attacker, damage);
}

#ifndef MANGOS__PET_TEMPLATE_H
#define MANGOS__PET_TEMPLATE_H

#include "Policies/Singleton.h"
#include <map>

class Creature;

enum pet_template_behavior
{
    PET_BEHAVIOR_MELEE = 0,
    PET_BEHAVIOR_RANGED = 1,
    PET_BEHAVIOR_RANGED_NO_MELEE = 2,
    PET_BEHAVIOR_MELEE_NO_AUTO_ATTACK =
        3, // Melee mob, but doesn't use auto-attack
};

enum pet_template_flags
{
    PET_FLAGS_DISABLE_FOLLOW = 0x1,
    PET_FLAGS_SPAWN_AGGRESSIVE = 0x2, // Pets spawn with defensive as default
    PET_FLAGS_SPAWN_PASSIVE = 0x4,
    PET_FLAGS_RETURN_HOME = 0x8, // Returns to last OOC position when evading
};

enum pet_template_cflags
{
    PET_CFLAGS_USE_AI = 0x1, // Uses the AI specified in creature template
    PET_CFLAGS_IGNORE_PET_BEHAVIOR =
        0x2, // If we have another AI we can toggle this flag to ignore our pet
             // behaviors completely (use pause for temporarily doing this)
    PET_CFLAGS_USE_HEALTH = 0x4,
    PET_CFLAGS_USE_MANA = 0x8,
    PET_CFLAGS_USE_ARMOR = 0x10,
    PET_CFLAGS_USE_FACTION = 0x20,
    PET_CFLAGS_USE_DAMAGE = 0x40,
    PET_CLFAGS_USE_DAMAGE_SCHOOL = 0x80,
    PET_CFLAGS_USE_UNIT_FLAGS = 0x100,
    PET_CFLAGS_USE_LEVEL = 0x200,
    PET_CFLAGS_USE_RESISTANCE = 0x400,
    PET_CLFAGS_ALLOW_LOOTING = 0x800,
    PET_CFLAGS_USE_ATTACK_SPEED = 0x1000,
};

/* spell_immunity:
Bit     Immunity
1	    physical
2	    holy
3	    fire
4	    nature
5	    frost
6	    shadow
7	    arcane
8	    magic
9	    curses
10	    diseases
11	    poison
*/

struct pet_template
{
    unsigned int cid;
    unsigned int behavior;
    unsigned int ctemplate_flags;
    unsigned int pet_flags;
    unsigned int behavior_flags;
    float spell_dist;
    signed int spell_oom; // // If > 0 we use this as oom mark, if < 0 we use
                          // this as the spell id of the spell that decided our
                          // oom mark
    unsigned int spell_immunity;

    void apply_spell_immunity(Creature* pet) const;
};

class pet_template_store
{
public:
    pet_template_store()
      : default_template_(nullptr), minipet_template_(nullptr)
    {
    }
    ~pet_template_store()
    {
        delete default_template_;
        delete minipet_template_;
    }

    // Calling load more than once will throw
    void load();

    const pet_template* get(unsigned int id) const;
    const pet_template* get_minipet(unsigned int id) const;

    // Get template adequate for enslaved creature
    const pet_template* enslaved_template() const;

private:
    pet_template* default_template_;
    pet_template* minipet_template_;
    typedef std::map<unsigned int, pet_template> templates;
    templates templates_;
};

#define sPetTemplates MaNGOS::UnlockedSingleton<pet_template_store>

#endif

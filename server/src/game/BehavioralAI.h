#ifndef MANGOS_BEHAVIORALAI_H
#define MANGOS_BEHAVIORALAI_H

#include "Common.h"
#include "logging.h"
#include "Object.h"
#include "SpellMgr.h"
#include "Platform/Define.h"
#include <unordered_map>
#include <vector>

class Creature;
class Unit;

enum SpellType
{
    AI_SPELL_OFFENSIVE = 0,
    AI_SPELL_HEALING = 1,
    AI_SPELL_BENEFICIAL = 2,
    AI_SPELL_CROWD_CONTROL = 3,
    AI_SPELL_SELF_ENHANCEMENT = 4,
    AI_SPELL_SILENCE = 5,
    AI_SPELL_DISPEL = 6,
    AI_SPELL_AREA_OF_EFFECT = 7,
    AI_SPELL_MY_PET = 8,
    AI_SPELL_DOT = 9, // Same target selection rules as OFFENSIVE

    AI_SPELL_MAX
};

enum TargetSettings
{
    // Some flags cancel out eachother (when it makes no sense to have them
    // coexist), in which case one of them is used in the order they appear
    // Flags that cancel eachother out are grouped together

    AI_SPELL_TS_MANA_TARGETS = 0x1,
    AI_SPELL_TS_RAGE_TARGETS = 0x2,
    AI_SPELL_TS_ENERGY_TARGETS = 0x4,

    AI_SPELL_TS_RANDOM_AGGRO = 0x8,
    AI_SPELL_TS_SECOND_AGGRO = 0x10,
    AI_SPELL_TS_RANDOM_NOT_TOP_AGGRO = 0x20,
    AI_SPELL_TS_BOTTOM_AGGRO = 0x40,

    AI_SPELL_TS_IN_MELEE_RANGE = 0x80,
    AI_SPELL_TS_NOT_IN_MELEE_RANGE = 0x100,

    AI_SPELL_IGNORE_LOS = 0x200,

    AI_SPELL_CC_INCLUDE_MAIN_TANK = 0x400,
    AI_SPELL_CC_ONLY_MAIN_TANK = 0x800,

    AI_SPELL_HEROIC_ONLY =
        0x1000, // Spell is only used in heroic (put spell id in normal)

    AI_SPELL_IGNORE_FOR_MAX_RANGE = 0x2000, // Do not use spell for max range
                                            // calculation (offensive spells
                                            // only)

    AI_SPELL_IGNORE_FOR_MIN_MANA = 0x4000, // Do not use spell for min mana
                                           // calculation (offensive spells
                                           // only)

    // These two dispel flags do NOT cancel eachother out; using dispel with no
    // flag will assume friendly
    AI_SPELL_DISPEL_FRIENDLY = 0x8000,
    AI_SPELL_DISPEL_HOSTILE = 0x10000,

    AI_SPELL_EXCLUDE_SELF =
        0x20000, // Spells cannot be cast on the caster (type 1 and 2)

    AI_SPELL_USE_TARGET_COORDS =
        0x40000, // Casted at target's coordinates instead of at target directly

    AI_SPELL_AOE_SPELL = 0x80000, // Spell is only casted when at least 2
                                  // enemies are nearby (type 1 and 4 only).

    AI_SPELL_AOE_MAIN_TARGET =
        0x100000, // Spell needs to be in range of main-tank to be casted, but
                  // cares not if other targets are nearby (type 7 only).

    AI_SPELL_AOE_ANYONE_IN_RANGE = 0x200000, // As long as at least one target
                                             // is in range, we can use the AoE
                                             // spell (type 7 only).

    AI_SPELL_TS_IN_FRONT = 0x400000, // Selected targets must be in front

    AI_SPELL_VICTIM_IN_RANGE =
        0x800000, // Spell is only casted when victim is in range (type 4 only)
};

enum BehaviorAIType
{
    BEHAVIOR_WARRIOR,
    BEHAVIOR_PALADIN,
    BEHAVIOR_MAGE,
    BEHAVIOR_ROGUE, // More of a hunter really, considering the wow type of
                    // rogue would be a warrior

    AI_BEHAVIOR_MAX
};

struct CreatureAISpell
{
    uint32 spellId;
    uint32 heroicSpellId; // If 0 defaults to spellId
    SpellType type;
    uint32 priority;
    uint32 cooldown_min;
    uint32 cooldown_max;
    uint32 target_settings;
    uint32 phase_mask;

    // For sorting
    bool operator<(const CreatureAISpell& rhs) const
    {
        return priority < rhs.priority;
    }
};

// Milliseconds that cooldown_min must be of a spell for the initial cooldown to
// happen
#define INTIAL_COOLDOWN_BREAK_POINT 6000

class MANGOS_DLL_SPEC BehavioralAI
{
public:
    static void LoadBehaviors();

    BehavioralAI(Creature* creature);

    bool TurnedOn() const { return toggledOn_; }
    void ToggleBehavior(uint32 state);
    void ChangeBehavior(uint32 behavior);

    // Returns true if we intend to take care of movement
    bool OnAttackStart();
    void OnReset();
    void Update(uint32 diff)
    {
        if (toggledOn_)
            InternalUpdate(diff);
    }

    int GetPhase() const { return phase_; }
    void SetPhase(int phase) { phase_ = phase; }

    bool IgnoreTarget(Unit* target) const;

    // Sets a cooldown for a given spell id in phase, if phase_mask is 0 it sets
    // all entries with that spell id
    void SetCooldown(uint32 spell_id, uint32 phase_mask, uint32 cooldown);

    std::string debug() const;

private:
    typedef std::vector<CreatureAISpell> AISpellVector;
    typedef std::unordered_map<uint32 /* creature id */, AISpellVector>
        AISpellMap;
    static const AISpellMap aiSpellMap_; // constness casted away during startup
                                         // for insertional purposes

    void InternalUpdate(uint32 diff);
    void CastSpell();
    Unit* GetSpellTarget(const CreatureAISpell& spell) const;
    bool HaveAvailableSpells(bool now, uint32* mask_out = nullptr) const;
    void InitMovement();
    void UpdateMovement();
    inline bool IsChasing() const;
    inline bool IsMoving() const;

    uint32 GetSpellCost(const CreatureAISpell& spell) const;
    float GetSpellMinimumRange(const CreatureAISpell& spell) const;
    float GetSpellMaximumRange(const CreatureAISpell& spell) const;
    inline int GetSpellId(const CreatureAISpell& spell) const;

    uint32 NearbyEnemiesCount(float radius) const;

    void SummonMyPet();

    bool toggledOn_;
    Creature* owner_;
    bool shouldDoMovement_;
    bool isHeroic_;
    uint32 globalCooldown_;
    std::vector<uint32> cooldownEndTimestamps_; // Timestamp of when the spell
                                                // is no longer on cooldown
    float maxRange_;
    float runX_;
    float runY_;
    float runZ_;
    int phase_;
    ObjectGuid oldVictim_;

    // Spells could have multiple schools so we cannot save our available
    // schools in one single bitmask:
    std::vector<uint32> availableSchools_;
    uint32 minMana_;
    BehaviorAIType behavior_;

    bool chaseMana_;
    bool chaseLoS_;
    bool chaseNear_;
    bool chaseRange_;
    bool chaseSchoolsLocked_;
    bool running_;
};

inline bool BehavioralAI::IsChasing() const
{
    return chaseMana_ || chaseLoS_ || chaseNear_ || chaseRange_ ||
           chaseSchoolsLocked_;
}

inline bool BehavioralAI::IsMoving() const
{
    return IsChasing() || running_;
}

inline int BehavioralAI::GetSpellId(const CreatureAISpell& spell) const
{
    if (spell.target_settings & AI_SPELL_HEROIC_ONLY)
        return isHeroic_ ?
                   spell.spellId != 0 ? spell.spellId : spell.heroicSpellId :
                   0;
    return isHeroic_ && spell.heroicSpellId ? spell.heroicSpellId :
                                              spell.spellId;
}

#endif

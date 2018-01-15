#ifndef MANGOS__PET_BEHAVIOR_H
#define MANGOS__PET_BEHAVIOR_H

#include "Common.h"
#include "movement/generators.h"
#include "ObjectGuid.h"
#include "G3D/Vector3.h"
#include <map>
#include <string>

class Creature;
class Player;
class Spell;
struct SpellEntry;
class Unit;

#define PET_UPDATE_FRIENDLY_TARGETS \
    10 * IN_MILLISECONDS // How often valid friendly targets are scanned for

enum class petai_cmd
{
    none,
    stay,
    follow,
    attack,
    passive
};
std::ostream& operator<<(std::ostream&, const petai_cmd);

/* Key points:
- This class exists one for each pet.
- This class is independent of the underlying AI class.
- Each AI that is intended to be used for pets must invoke this
  class as expected. See pet_ai.h for how to implement it for an AI.
*/
class pet_behavior
{
public:
    pet_behavior(Creature* owner);
    ~pet_behavior();

    /* AI Callbacks */
    void evade(bool target_died = false);
    void attacked(Unit* attacker); // Mangos calls this one for us when our
                                   // owner is attacked as well
    void update(const uint32 diff);
    void died();
    // Callback used to implement Imp's Fire Shield, is only invoked for
    // pet_behavior (not CreatureAI)
    void struck_party_member(Unit* attacker, Player* attackee);
    void damaged(Unit* by, uint32 damage);

    /* Player Initiated Commands */
    void attempt_attack(Unit* target);
    void attempt_follow();
    void attempt_stay();
    void attempt_passive();
    bool attempt_queue_spell(
        const SpellEntry* info, Unit* target, bool switch_target);

    /* Main functionality */
    bool try_evade(); // calls evade() only if we actually have a target
    bool try_attack(Unit* target);
    void attack(Unit* target);
    bool paused() const { return paused_; }
    void pause() { paused_ = true; }
    void resume() { paused_ = false; }

    /* Debugging */
    std::string debug() const;

private:
    void update_command_state(Unit* owner);
    void update_victim(
        Unit* owner, Unit* target);     // ::update() when we're attacking
    void update_no_victim(Unit* owner); // ::update() when we're not attacking
    void update_player_controlled();    // ::update() when possessed by a player

    void update_queued_spells(); // does work when queued_spell_ is not null

    bool is_passive() const;

    /* Spell casting helper functions */
    std::vector<const SpellEntry*> build_available_spells();
    // Player pets have conditions for certain spells
    bool can_use(const SpellEntry* spell_info) const;
    Unit* find_target(const SpellEntry* spell_info) const;
    bool can_cast_on(const SpellEntry* spell_info, Unit* target) const;
    bool cast(const SpellEntry* spell_info, Unit* target, bool can_ban = true);
    void cast_ooc_spells();
    uint32 offensive_spell_cost() const; // Returns cost of our offensive spell
    float
    offensive_spell_dist() const; // Returns distance of our offensive spell

    bool can_initiate_attack(Unit* target) const;
    bool can_attack(Unit* target) const;
    void update_friendly_targets(Unit* owner);
    bool in_control() const; // False if AI is unable to act

    Unit* get_owner() const;

    void clear_queued_spell();

    void set_chase_on();

    Creature* pet_;
    movement::Generator* stay_gen_; // NOTE: Don't dereference!
    bool paused_;                   // True if AI is paused
    bool stay_;                     // True if ordered to stay
    bool chasing_;                  // True if currently combat moving
    ObjectGuid target_;             // Our current victim
    petai_cmd issued_command_;      // Command to process (-1 if no new command)
    std::map<uint32, time_t> banned_spells_; // Spells that could be casted but
                                             // has no valid target is timed out
                                             // for 10 seconds before tried
                                             // again
    uint32 update_friendly_; // Timer to update friendly targets again
    std::vector<ObjectGuid>
        friendly_targets_; // Targets valid for casting friendly spells on

    // Queued spell data
    uint32 queued_id_;
    ObjectGuid queued_target_;

    // Patch 2.3: attack from behind. If target isn't moving, move to its back
    G3D::Vector3 target_last_pos_;
};

#endif

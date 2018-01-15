/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Creature.h"
#include "BattleGroundMgr.h"
#include "CreatureAI.h"
#include "CreatureGroup.h"
#include "Formulas.h"
#include "GameEventMgr.h"
#include "GossipDef.h"
#include "InstanceData.h"
#include "logging.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "PoolManager.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "SmartAI.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "loot_distributor.h"
#include "pet_ai.h"
#include "pet_behavior.h"
#include "pet_template.h"
#include "Database/DatabaseEnv.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Policies/Singleton.h"
#include "movement/MoveSplineInit.h"
#include "movement/FleeingMovementGenerator.h"
#include "movement/HomeMovementGenerator.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "movement/WaypointMovementGenerator.h"
#include "maps/visitors.h"

#define SPAWN_AGGRO_DELAY \
    2000 // ms delay when spawning/respawning before creature can attack
         // anything

TrainerSpell const* TrainerSpellData::Find(uint32 spell_id) const
{
    auto itr = spellList.find(spell_id);
    if (itr != spellList.end())
        return &itr->second;

    return nullptr;
}

void VendorItemData::AddItem(uint32 item, uint32 maxcount, uint32 ptime,
    uint32 ExtendedCost, uint32 weight)
{
    m_items.emplace_back(item, maxcount, ptime, ExtendedCost, weight);
    std::stable_sort(m_items.begin(), m_items.end(),
        [](const VendorItem& a, const VendorItem& b)
        {
            return a.weight < b.weight;
        });
}

bool VendorItemData::RemoveItem(uint32 item_id)
{
    for (auto i = m_items.begin(); i != m_items.end(); ++i)
    {
        if ((*i).item == item_id)
        {
            m_items.erase(i);
            return true;
        }
    }
    return false;
}

size_t VendorItemData::FindItemSlot(uint32 item_id) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
        if (m_items[i].item == item_id)
            return i;
    return m_items.size();
}

VendorItem const* VendorItemData::FindItem(uint32 item_id) const
{
    for (const auto& elem : m_items)
        if ((elem).item == item_id)
            return &elem;
    return nullptr;
}

bool ForcedDespawnDelayEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    m_owner.ForcedDespawn();
    return true;
}

void CreatureCreatePos::SelectFinalPoint(Creature* /*cr*/)
{
    // if object provided then selected point at specific dist/angle from object
    // forward look
    if (m_closeObject)
    {
        if (m_dist == 0.0f)
        {
            m_pos.x = m_closeObject->GetX();
            m_pos.y = m_closeObject->GetY();
            m_pos.z = m_closeObject->GetZ();
        }
        else
        {
            // The client does not seem to interpolate spawned positions as it
            // does with spline positions, so we normalize_z to get a better
            // spawn point
            auto pos = m_closeObject->GetPoint(m_angle, m_dist, true);
            m_pos.x = pos.x;
            m_pos.y = pos.y;
            m_pos.z = pos.z;
        }
    }
}

bool CreatureCreatePos::Relocate(Creature* cr) const
{
    cr->Relocate(m_pos.x, m_pos.y, m_pos.z);
    cr->SetOrientation(m_pos.o);

    if (!cr->IsPositionValid())
    {
        logging.error(
            "%s not created. Suggested coordinates isn't valid (X: %f Y: %f)",
            cr->GetGuidStr().c_str(), cr->GetX(), cr->GetY());
        return false;
    }

    return true;
}

uint32 CreatureInfo::GetRandomValidModelId() const
{
    uint8 c = 0;
    uint32 modelIDs[4];

    if (ModelId[0])
        modelIDs[c++] = ModelId[0];
    if (ModelId[1])
        modelIDs[c++] = ModelId[1];
    if (ModelId[2])
        modelIDs[c++] = ModelId[2];
    if (ModelId[3])
        modelIDs[c++] = ModelId[3];

    return ((c > 0) ? modelIDs[urand(0, c - 1)] : 0);
}

uint32 CreatureInfo::GetFirstValidModelId() const
{
    if (ModelId[0])
        return ModelId[0];
    if (ModelId[1])
        return ModelId[1];
    if (ModelId[2])
        return ModelId[2];
    if (ModelId[3])
        return ModelId[3];
    return 0;
}

Creature::Creature(CreatureSubtype subtype)
  : Unit(), i_AI(nullptr), legit_dmg_taken(0), player_dmg_taken(0),
    total_dmg_taken(0), m_corpseDecayTimer(0), m_respawnTime(0),
    m_respawnDelay(25), m_corpseDelay(60), m_respawnradius(5.0f),
    m_subtype(subtype), m_defaultMovementGen(movement::gen::idle),
    m_equipmentId(0), m_regenHealth(true), m_regenMana(true),
    m_AI_locked(false), m_isDeadByDefault(false),
    m_temporaryFactionFlags(TEMPFACTION_NONE),
    m_meleeDamageSchoolMask(SPELL_SCHOOL_MASK_NORMAL), m_originalEntry(0),
    m_lastCombatX(0), m_lastCombatY(0), m_lastCombatZ(0), m_canRespawn(true),
    m_forcedRespawn(false), creature_group_(nullptr), m_canRun(true),
    m_checkBossLink(false), m_focusSpellId(0), m_aggroPulseTimer(0),
    m_lastLowHpUpdate(0), m_inhabitType(INHABIT_GROUND), pet_template_(nullptr),
    pet_behavior_(nullptr), m_creatureInfo(nullptr),
    m_spellSchoolCooldowns(MAX_SPELL_SCHOOL, 0), m_spellSchoolCooldownMask(0),
    lootPickpocket(false), lootSkin(false), lootBody(false),
    m_skinningLootDist(nullptr), m_pickpocketLootDist(nullptr),
    in_evade_(false), evade_timer_(0), evade_tick_timer_(0), aggro_delay_(0),
    leash_coords{0, 0, 0}, leash_radius(0)
{
    m_regenTimer = 200;
    m_valuesCount = UNIT_END;

    for (auto& elem : m_spells)
        elem = 0;

    m_CreatureSpellCooldowns.clear();
    m_CreatureCategoryCooldowns.clear();

    aggro_radius = NPC_DEFAULT_AGGRO_DIST;
    chain_radius = NPC_DEFAULT_CHAIN_DIST;

    SetWalk(true);
}

Creature::~Creature()
{
    m_vendorItemCounts.clear();

    delete i_AI;

    delete m_skinningLootDist;
    delete m_pickpocketLootDist;
    // m_lootDistributor destroyed in WorldObject's destructor

    delete pet_behavior_;
}

void Creature::AddToWorld()
{
    bool in_world = IsInWorld();

    // We need to do Object::AddToWorld before AI::OnSpawn(), so that the unit
    // is considered In World before the callback is executed.
    Unit::AddToWorld();

    ///- Register the creature for guid lookup
    if (!in_world && GetObjectGuid().GetHigh() == HIGHGUID_UNIT)
    {
        GetMap()->GetCreatureGroupMgr().OnAddToWorld(this);
        if (AI() && isAlive())
            AI()->OnSpawn();

        // Reset inhabit type when added to world
        ResetInhabitType();
        if (m_inhabitType & INHABIT_WATER)
        {
            float x, y, z;
            GetPosition(x, y, z);
            if (m_inhabitType == INHABIT_WATER || IsUnderWater())
                SetSwim(true);
        }
        // The script can force on flying in AIM_Initialize() or OnSpawn(), in
        // which case we ignore resetting it
        if (!m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING))
            SetLevitate(CanFly());

        if (!IsTemporarySummon())
            aggro_delay_ = WorldTimer::getMSTime() + SPAWN_AGGRO_DELAY;

        if (m_bossLink && !isAlive())
        {
            if (Creature* boss = GetMap()->GetCreature(m_bossLink))
                boss->AddBossLinkedMob(GetObjectGuid());
        }

        // Remove from group marks. This is a hacky solution to make sure marks
        // don't stay on instance reset.
        if (auto state = dynamic_cast<DungeonPersistentState*>(
                GetMap()->GetPersistentState()))
        {
            for (auto& group : state->GetBoundGroups())
                group->ClearTargetIcon(GetObjectGuid());
        }
    }
}

void Creature::RemoveFromWorld()
{
    ///- Remove the creature from the accessor
    if (IsInWorld() && GetObjectGuid().GetHigh() == HIGHGUID_UNIT)
    {
        auto map = GetMap();

        if (creature_group_)
            map->GetCreatureGroupMgr().OnRemoveFromWorld(this);
    }

    Unit::RemoveFromWorld();
}

void Creature::RemoveCorpse()
{
    if ((getDeathState() != CORPSE && !m_isDeadByDefault) ||
        (getDeathState() != ALIVE && m_isDeadByDefault))
        return;

    m_corpseDecayTimer = 0;
    SetDeathState(DEAD);
    UpdateObjectVisibility();

    if (m_lootDistributor)
        m_lootDistributor->cancel_loot_session();

    uint32 respawnDelay = 0;

    if (AI())
        AI()->CorpseRemoved(respawnDelay);

    // script can set time (in seconds) explicit, override the original
    if (respawnDelay)
        m_respawnTime = WorldTimer::time_no_syscall() + respawnDelay;

    // Remove all movement gens (fall gen might still exist from SetDeathState)
    movement_gens.reset();

    float x, y, z, o;
    GetRespawnCoord(x, y, z, &o);
    GetMap()->relocate(this, x, y, z, o);

    // forced recreate creature object at clients
    UnitVisibility currentVis = GetVisibility();
    if (currentVis == VISIBILITY_REMOVE_CORPSE) // VISIBILITY_REMOVE_CORPSE in
                                                // Creature::ForcedDespawn()
                                                // case
        currentVis = VISIBILITY_ON;
    SetVisibility(VISIBILITY_REMOVE_CORPSE);
    UpdateObjectVisibility();
    SetVisibility(currentVis); // restore visibility state
    UpdateObjectVisibility();
}

/**
 * change the entry of creature until respawn
 */
bool Creature::InitEntry(uint32 Entry, CreatureData const* data /*=NULL*/,
    GameEventCreatureData const* eventData /*=NULL*/)
{
    // use game event entry if any instead default suggested
    if (eventData && eventData->entry_id)
        Entry = eventData->entry_id;

    CreatureInfo const* normalInfo = ObjectMgr::GetCreatureTemplate(Entry);
    if (!normalInfo)
    {
        logging.error(
            "Creature::UpdateEntry creature entry %u does not exist.", Entry);
        return false;
    }

    // difficulties for dungeons/battleground ordered in normal way
    // and if more high version not exist must be used lesser version
    CreatureInfo const* cinfo = normalInfo;
    if (normalInfo->HeroicEntry)
    {
        // we already have valid Map pointer for current creature!
        if (!GetMap()->IsRegularDifficulty())
        {
            cinfo = ObjectMgr::GetCreatureTemplate(normalInfo->HeroicEntry);
            if (!cinfo)
            {
                logging.error(
                    "Creature::UpdateEntry creature heroic entry %u does not "
                    "exist.",
                    normalInfo->HeroicEntry);
                return false;
            }
        }
    }

    SetEntry(Entry);        // normal entry always
    m_creatureInfo = cinfo; // map mode related always

    SetObjectScale(cinfo->scale);

    // equal to player Race field, but creature does not have race
    SetByteValue(UNIT_FIELD_BYTES_0, 0, 0);

    // known valid are: CLASS_WARRIOR,CLASS_PALADIN,CLASS_ROGUE,CLASS_MAGE
    SetByteValue(UNIT_FIELD_BYTES_0, 1, uint8(cinfo->unit_class));

    uint32 display_id = ChooseDisplayId(GetCreatureInfo(), data, eventData);
    if (!display_id) // Cancel load if no display id
    {
        logging.error(
            "Creature (Entry: %u) has no model defined in table "
            "`creature_template`, can't load.",
            Entry);
        return false;
    }

    CreatureModelInfo const* minfo =
        sObjectMgr::Instance()->GetCreatureModelRandomGender(display_id);
    if (!minfo) // Cancel load if no model defined
    {
        logging.error(
            "Creature (Entry: %u) has no model info defined in table "
            "`creature_model_info`, can't load.",
            Entry);
        return false;
    }

    display_id = minfo->modelid; // it can be different (for another gender)

    SetNativeDisplayId(display_id);

    if (minfo->los_height)
        SetLosHeight(minfo->los_height);

    // normally the same as native, but some has exceptions
    // (Spell::DoSummonTotem)
    SetDisplayId(display_id);

    SetByteValue(UNIT_FIELD_BYTES_0, 2, minfo->gender);

    // Load creature equipment
    if (eventData && eventData->equipment_id)
    {
        LoadEquipment(
            eventData
                ->equipment_id); // use event equipment if any for active event
    }
    else if (!data || data->equipmentId == 0)
    {
        if (cinfo->equipmentId == 0)
            LoadEquipment(normalInfo->equipmentId); // use default from normal
                                                    // template if diff does not
                                                    // have any
        else
            LoadEquipment(cinfo->equipmentId); // else use from diff template
    }
    else if (data && data->equipmentId != -1)
    { // override, -1 means no equipment
        LoadEquipment(data->equipmentId);
    }

    SetName(normalInfo->Name); // at normal entry always

    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    // update speed for the new CreatureInfo base speed mods
    UpdateSpeed(MOVE_WALK, false);
    UpdateSpeed(MOVE_RUN, false);

    SetLevitate(CanFly());

    // checked at loading
    m_defaultMovementGen = movement::gen(cinfo->MovementType);
    ResetInhabitType(cinfo->InhabitType);

    // mark as active object if creature has special visibility
    if (special_vis_mob())
        SetActiveObjectState(true);

    return true;
}

bool Creature::UpdateEntry(uint32 Entry, Team team,
    const CreatureData* data /*=NULL*/,
    GameEventCreatureData const* eventData /*=NULL*/,
    bool preserveHPAndPower /*=true*/)
{
    if (!InitEntry(Entry, data, eventData))
        return false;

    m_regenHealth = GetCreatureInfo()->RegenHealth;
    m_regenMana = GetCreatureInfo()->RegenMana;

    // creatures always have melee weapon ready if any
    SetSheath(SHEATH_STATE_MELEE);
    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_POSITIVE_AURAS);

    SelectLevel(GetCreatureInfo(),
        preserveHPAndPower ? GetHealthPercent() : 100.0f, 100.0f, true);

    if (team == HORDE)
        setFaction(GetCreatureInfo()->faction_H);
    else
        setFaction(GetCreatureInfo()->faction_A);

    SetUInt32Value(UNIT_NPC_FLAGS, GetCreatureInfo()->npcflag);

    uint32 attackTimer = GetCreatureInfo()->baseattacktime;

    SetAttackTime(BASE_ATTACK, attackTimer);
    SetAttackTime(OFF_ATTACK, attackTimer - attackTimer / 4);
    SetAttackTime(RANGED_ATTACK, GetCreatureInfo()->rangeattacktime);

    uint32 unitFlags = GetCreatureInfo()->unit_flags;

    // we may need to append or remove additional flags
    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT))
        unitFlags |= UNIT_FLAG_IN_COMBAT;

    SetUInt32Value(UNIT_FIELD_FLAGS, unitFlags);

    // preserve all current dynamic flags if exist
    uint32 dynFlags = GetUInt32Value(UNIT_DYNAMIC_FLAGS);
    SetUInt32Value(UNIT_DYNAMIC_FLAGS,
        dynFlags ? dynFlags : GetCreatureInfo()->dynamicflags);

    SetModifierValue(
        UNIT_MOD_ARMOR, BASE_VALUE, float(GetCreatureInfo()->armor));
    SetModifierValue(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE,
        float(GetCreatureInfo()->resistance1));
    SetModifierValue(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE,
        float(GetCreatureInfo()->resistance2));
    SetModifierValue(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE,
        float(GetCreatureInfo()->resistance3));
    SetModifierValue(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE,
        float(GetCreatureInfo()->resistance4));
    SetModifierValue(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE,
        float(GetCreatureInfo()->resistance5));
    SetModifierValue(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE,
        float(GetCreatureInfo()->resistance6));

    SetCanModifyStats(true);
    UpdateAllStats();

    // Toggle pvp flag for self and all summoned minions
    auto faction =
        sFactionTemplateStore.LookupEntry(GetCreatureInfo()->faction_A);
    bool pvp_flagged = HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP);
    if (!pvp_flagged && faction &&
        faction->factionFlags & FACTION_TEMPLATE_FLAG_PVP)
        pvp_flagged = true;
    SetPvP(pvp_flagged);

    for (int i = 0; i < CREATURE_MAX_SPELLS; ++i)
        m_spells[i] = GetCreatureInfo()->spells[i];

    // if eventData set then event active and need apply spell_start
    if (eventData)
        ApplyGameEventSpells(eventData, true);

    return true;
}

uint32 Creature::ChooseDisplayId(const CreatureInfo* cinfo,
    const CreatureData* data /*= NULL*/,
    GameEventCreatureData const* eventData /*=NULL*/)
{
    // Use creature event model explicit, override any other static models
    if (eventData && eventData->modelid)
        return eventData->modelid;

    // Use creature model explicit, override template (creature.modelid)
    if (data && data->modelid_override)
        return data->modelid_override;

    // use defaults from the template
    uint32 display_id = 0;

    // Choose from the available models in creature_template
    std::vector<uint32> indices;
    indices.reserve(4);
    if (cinfo->ModelId[3])
        indices.push_back(3);
    if (cinfo->ModelId[2])
        indices.push_back(2);
    if (cinfo->ModelId[1])
        indices.push_back(1);
    if (cinfo->ModelId[0])
        indices.push_back(0);

    if (!indices.empty())
    {
        uint32 index = indices[urand(0, indices.size() - 1)];
        display_id = cinfo->ModelId[index];
        // If model has a registered alternative, we add a 50% chance to use
        // that instead
        uint32 alternative =
            sObjectMgr::Instance()->GetCreatureModelAlternativeModel(
                cinfo->ModelId[index]);
        if (alternative && urand(0, 1))
            display_id = alternative;
    }

    // Fail safe, we use creature entry 1 and make error
    if (!display_id)
    {
        logging.error(
            "ChooseDisplayId can not select native model for creature entry "
            "%u, model from creature entry 1 will be used instead.",
            cinfo->Entry);

        if (const CreatureInfo* creatureDefault =
                ObjectMgr::GetCreatureTemplate(1))
            display_id = creatureDefault->ModelId[0];
    }

    return display_id;
}

void Creature::Update(uint32 update_diff, uint32 diff)
{
    if (unlikely(has_queued_actions()))
        update_queued_actions(update_diff);

    switch (m_deathState)
    {
    case JUST_ALIVED:
        // Don't must be called, see Creature::SetDeathState JUST_ALIVED ->
        // ALIVE promoting.
        logging.error(
            "Creature (GUIDLow: %u Entry: %u ) in wrong state: JUST_ALIVED (4)",
            GetGUIDLow(), GetEntry());
        break;
    case JUST_DIED:
        // Don't must be called, see Creature::SetDeathState JUST_DIED -> CORPSE
        // promoting.
        logging.error(
            "Creature (GUIDLow: %u Entry: %u ) in wrong state: JUST_DEAD (1)",
            GetGUIDLow(), GetEntry());
        break;
    case DEAD:
    {
        if (CanRespawn())
        {
            LOG_DEBUG(logging, "Respawning...");
            m_forcedRespawn = false;
            m_respawnTime = 0;
            lootPickpocket = false;
            lootBody = false;
            lootSkin = false;
            delete m_lootDistributor;
            delete m_skinningLootDist;
            delete m_pickpocketLootDist;
            m_pickpocketLootDist =
                new loot_distributor(this, LOOT_PICKPOCKETING);
            m_skinningLootDist = new loot_distributor(this, LOOT_SKINNING);
            m_lootDistributor = new loot_distributor(this, LOOT_CORPSE);

            // Teleport to respawn point
            float x, y, z, o;
            if (GetGroup() == nullptr ||
                !GetMap()->GetCreatureGroupMgr().GetRespawnPositionOfCreature(
                    GetGroup()->GetId(), this, x, y, z, o))
                GetRespawnCoord(x, y, z, &o);
            NearTeleportTo(x, y, z, o);

            // Set aggro delay on respawning
            aggro_delay_ = WorldTimer::getMSTime() + SPAWN_AGGRO_DELAY;

            // Clear possible auras having IsDeathPersistent() attribute
            remove_auras();

            if (AI())
                AI()->Pacify(false);

            if (m_originalEntry != GetEntry())
            {
                // need preserver gameevent state
                GameEventCreatureData const* eventData =
                    sGameEventMgr::Instance()
                        ->GetCreatureUpdateDataForActiveEvent(GetGUIDLow());
                UpdateEntry(m_originalEntry, TEAM_NONE, nullptr, eventData);
            }

            CreatureInfo const* cinfo = GetCreatureInfo();

            SelectLevel(cinfo);
            SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
            if (m_isDeadByDefault)
            {
                SetDeathState(JUST_DIED);
                SetHealth(0);
                clearUnitState(UNIT_STAT_ALL_STATE);
                LoadCreatureAddon(true);
            }
            else
                SetDeathState(JUST_ALIVED);

            // Remove mount id
            Unmount();
            // Apply creature_addon's mount id if needed
            if (auto addon = GetCreatureAddon())
                if (addon->mount != 0)
                    Mount(addon->mount);

            // Call AI respawn virtual function
            if (AI())
                AI()->JustRespawned();

            if (creature_group_)
                GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                    creature_group_->GetId(), CREATURE_GROUP_EVENT_RESPAWN,
                    this);

            // Notify the outdoor pvp script
            UpdateZoneAreaCache();
            if (OutdoorPvP* outdoorPvP =
                    sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
                outdoorPvP->HandleCreatureRespawn(this);

            if (auto id = GetMap()->GetInstanceData())
                id->OnCreatureRespawn(this);

            if (!IsInWorld())
            {
                bool b = GetMap()->insert(this);
                if (!b)
                {
                    logging.error(
                        "Map::insert failed in respawn, shouldn't be "
                        "possible!");
                    assert(false);
                }
            }
        }
        break;
    }
    case CORPSE:
    {
        Unit::Update(update_diff, diff);

        if (m_isDeadByDefault)
            break;

        if (m_corpseDecayTimer <= update_diff)
        {
            // since pool system can fail to roll unspawned object, this one can
            // remain spawned, so must set respawn nevertheless
            if (auto poolid =
                    sPoolMgr::Instance()->IsPartOfAPool<Creature>(GetGUIDLow()))
            {
                if (GetMap()->GetPersistentState())
                    sPoolMgr::Instance()->UpdatePool<Creature>(
                        *GetMap()->GetPersistentState(), poolid, GetGUIDLow());
            }

            if (IsInWorld()) // can be despawned by update pool
            {
                RemoveCorpse();
                LOG_DEBUG(logging, "Removing corpse... %u ", GetEntry());
            }
        }
        else
        {
            m_corpseDecayTimer -= update_diff;
            if (m_lootDistributor)
                m_lootDistributor->update_rolls(diff);
        }

        break;
    }
    case ALIVE:
    {
        // If our owning boss is dead and we have m_checkBossLink set to true we
        // despawn
        // This will cause a recreated map to despawn any creatures owned by
        // dead bosses
        // to be non-present
        if (m_checkBossLink)
        {
            if (Creature* boss = GetMap()->GetCreature(m_bossLink))
            {
                boss->AddBossLinkedMob(GetObjectGuid());
                m_checkBossLink = false;

                if (!boss->isAlive())
                {
                    ForcedDespawn();
                    return;
                }
            }
        }

        if (m_isDeadByDefault)
        {
            if (m_corpseDecayTimer <= update_diff)
            {
                // since pool system can fail to roll unspawned object, this one
                // can remain spawned, so must set respawn nevertheless
                if (auto poolid = sPoolMgr::Instance()->IsPartOfAPool<Creature>(
                        GetGUIDLow()))
                {
                    if (GetMap()->GetPersistentState())
                        sPoolMgr::Instance()->UpdatePool<Creature>(
                            *GetMap()->GetPersistentState(), poolid,
                            GetGUIDLow());
                }

                if (IsInWorld()) // can be despawned by update pool
                {
                    RemoveCorpse();
                    LOG_DEBUG(
                        logging, "Removing alive corpse... %u ", GetEntry());
                }
                else
                    return;
            }
            else
            {
                m_corpseDecayTimer -= update_diff;
            }
        }

        Unit::Update(update_diff, diff);

        // creature can be dead after Unit::Update call
        // CORPSE/DEAD state will processed at next tick (in other case death
        // timer will be updated unexpectedly)
        if (!isAlive())
            break;

        // Keep setting orientation to casted at target
        if (m_castedAtTarget && !HasAuraType(SPELL_AURA_MOD_TAUNT))
        {
            if (auto target = GetMap()->GetUnit(m_castedAtTarget))
            {
                SetInFront(target);
                SetTargetGuid(m_castedAtTarget);
            }
        }

        if (isInCombat())
        {
            if (m_aggroPulseTimer <= update_diff)
            {
                // Pulse aggro to nearby friendly creatures
                if (!(m_creatureInfo->flags_extra &
                        CREATURE_FLAG_EXTRA_NO_AGGRO_PULSE))
                    AggroPulse();

                // Do zone-wide aggro at the same time
                if (GetMap()->IsDungeon() &&
                    m_creatureInfo->flags_extra &
                        CREATURE_FLAG_EXTRA_AGGRO_ZONE)
                    SetInCombatWithZone();

                m_aggroPulseTimer = 1000;
            }
            else
                m_aggroPulseTimer -= update_diff;

            if (evading())
                update_evade(update_diff);

            // Mobs on low health decrease their speed (don't update run speed
            // if swimming; will make mob glitch client-side. TODO: maybe we
            // should update the swim speed)
            if (!IsPlayerPet() &&
                !m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING))
                UpdateLowHealthSpeed();

            // Leashing
            if (leash_radius > 0)
            {
                if (!IsWithinDist3d(leash_coords[0], leash_coords[1],
                        leash_coords[2], leash_radius) &&
                    AI())
                {
                    AI()->EnterEvadeMode();
                    AI()->Leashed();
                }
            }

            // Kiting
            if (!isCharmed() && !IsPet() &&
                (!GetMap()->Instanceable() ||
                    GetMap()->IsBattleGroundOrArena()) &&
                movement_gens.top_id() == movement::gen::chase)
            {
                if (GetDistance(m_lastCombatX, m_lastCombatY, m_lastCombatZ) >=
                        KITING_LEASH_RADIUS &&
                    AI())
                {
                    if (!GetMap()->Instanceable())
                    {
                        if (auto gen =
                                dynamic_cast<movement::HomeMovementGenerator*>(
                                    movement_gens.get(movement::gen::home)))
                        {
                            float x, y, z;
                            gen->get_combat_start_pos(x, y, z);
                            if (GetDistance(x, y, z) >= KITING_LEASH_TELEPORT)
                                KitingLeashTeleportHome();
                        }
                    }
                    if (AI())
                    {
                        AI()->EnterEvadeMode();
                        AI()->Leashed();
                    }
                    if (GetGroup() != nullptr &&
                        !GetGroup()->HasFlag(
                            CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
                    {
                        GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                            GetGroup()->GetId(), CREATURE_GROUP_EVENT_EVADE,
                            this);
                    }
                    if (Pet* pet = GetPet())
                        if (pet->AI())
                            pet->AI()->EnterEvadeMode();
                }
            }
        }

        if (!IsInEvadeMode())
        {
            // Mobs that aren't pet do not get a pet_behavior when they get mind
            // controlled,
            // so we need to update melee attacking outside of any AI code for
            // them
            if (hasUnitState(UNIT_STAT_CONTROLLED) && !IsPet() &&
                GetCharmInfo() && hasUnitState(UNIT_STAT_MELEE_ATTACKING))
                UpdateMeleeAttackingState();

            if (AI())
            {
                // do not allow the AI to be changed during update
                m_AI_locked = true;
                AI()->UpdateAI(diff); // AI not react good at real update delays
                                      // (while freeze in non-active part of
                                      // map)
                m_AI_locked = false;
            }
        }

        // creature can be dead after UpdateAI call
        // CORPSE/DEAD state will processed at next tick (in other case death
        // timer will be updated unexpectedly)
        if (!isAlive())
            break;
        RegenerateAll(update_diff);
        break;
    }
    default:
        break;
    }

    // Update spell school lockouts if we have any
    if (m_spellSchoolCooldownMask)
        for (int i = 1; i < MAX_SPELL_SCHOOL; i++)
        {
            if (m_spellSchoolCooldowns[i])
            {
                if (m_spellSchoolCooldowns[i] <= update_diff)
                {
                    m_spellSchoolCooldowns[i] = 0;
                    m_spellSchoolCooldownMask &= ~(1 << i);
                }
                else
                {
                    m_spellSchoolCooldowns[i] -= update_diff;
                }
            }
        }
}

void Creature::RegenerateAll(uint32 update_diff)
{
    if (m_regenTimer > 0)
    {
        if (update_diff >= m_regenTimer)
            m_regenTimer = 0;
        else
            m_regenTimer -= update_diff;
    }
    if (m_regenTimer != 0)
        return;

    if (!isInCombat())
        RegenerateHealth();

    RegenerateMana();

    m_regenTimer = REGEN_TIME_FULL;
}

void Creature::RegenerateMana()
{
    if (!IsRegeneratingMana())
        return;

    uint32 curValue = GetPower(POWER_MANA);
    uint32 maxValue = GetMaxPower(POWER_MANA);

    if (curValue >= maxValue)
        return;

    uint32 addvalue = 0;

    // Combat and any controlled creature
    if (isInCombat() || GetCharmerOrOwnerGuid())
    {
        if (!IsUnderLastManaUseEffect())
        {
            float ManaIncreaseRate =
                sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_POWER_MANA);
            float Spirit = GetStat(STAT_SPIRIT);

            addvalue = uint32((Spirit / 5.0f + 17.0f) * ManaIncreaseRate);
        }
    }
    else
        addvalue = maxValue / 3;

    if (Unit* owner = GetCharmerOrOwner())
    {
        float mp5 = 0.0f;

        // mp5 based on owner's intellect
        // TODO: Although not the most reliable source, the code below is based
        // on:
        // http://forums.elitistjerks.com/index.php?/topic/40226-pet-mana-regen-how-does-it-scale/
        // ~40 mp5 naked; ~150 mp5 with good gear
        mp5 += owner->GetStat(STAT_INTELLECT) * 0.3f;

        // mp5 buffs
        mp5 += GetTotalAuraModifierByMiscValue(
            SPELL_AURA_MOD_POWER_REGEN, POWER_MANA);

        // RegenerateMana() invoked every 4 sec, so we need to recalc the value
        addvalue += mp5 / 1.25f;
    }

    ModifyPower(POWER_MANA, addvalue);
}

void Creature::RegenerateHealth()
{
    if (!IsRegeneratingHealth())
        return;

    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    uint32 addvalue = 0;

    // Not only pet, but any controlled creature
    if (GetCharmerOrOwnerGuid())
    {
        float HealthIncreaseRate =
            sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_HEALTH);
        float Spirit = GetStat(STAT_SPIRIT);

        if (GetPower(POWER_MANA) > 0)
            addvalue = uint32(Spirit * 0.25 * HealthIncreaseRate);
        else
            addvalue = uint32(Spirit * 0.80 * HealthIncreaseRate);
    }
    else
        addvalue = maxValue / 3;

    ModifyHealth(addvalue);
}

void Creature::RunAwayInFear(bool do_emote)
{
    if (hasUnitState(UNIT_STAT_CONTROLLED))
        return;

    if (do_emote)
        MonsterTextEmote("%s attempts to run away in fear!", nullptr);

    if (movement_gens.has(movement::gen::confused) ||
        movement_gens.has(movement::gen::fleeing) ||
        movement_gens.has(movement::gen::charge))
        return;

    if (hasUnitState(
            UNIT_STAT_CAN_NOT_MOVE | UNIT_STAT_STUNNED | UNIT_STAT_ROOT))
        return;

    if (HasAuraType(SPELL_AURA_PREVENTS_FLEEING))
        return;

    if (auto victim = getVictim())
    {
        InterruptNonMeleeSpells(false);
        movement_gens.push(new movement::RunInFearMovementGenerator(
                               victim->GetObjectGuid(), urand(8000, 12000)),
            movement::EVENT_LEAVE_COMBAT);
    }
}

bool Creature::AIM_Initialize()
{
    // make sure nothing can change the AI during AI update
    if (m_AI_locked)
    {
        LOG_DEBUG(logging, "AIM_Initialize: failed to init, locked.");
        return false;
    }

    delete i_AI;
    i_AI = nullptr;

    i_AI = make_ai_for(this);
    i_AI->InitializeAI();

    if (movement_gens.empty())
        movement_gens.reset();

    return true;
}

bool Creature::Create(uint32 guidlow, CreatureCreatePos& cPos,
    CreatureInfo const* cinfo, Team team /*= TEAM_NONE*/,
    const CreatureData* data /*= NULL*/,
    GameEventCreatureData const* eventData /*= NULL*/)
{
    SetMap(cPos.GetMap());

    if (!CreateFromProto(guidlow, cinfo, team, data, eventData))
        return false;

    cPos.SelectFinalPoint(this);

    if (!cPos.Relocate(this))
        return false;

    // Notify the outdoor pvp script
    UpdateZoneAreaCache();
    if (OutdoorPvP* outdoorPvP =
            sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
        outdoorPvP->HandleCreatureCreate(this);

    // Notify the map's instance data (done in LoadFromDB if we have cdata)
    if (!data)
        if (InstanceData* iData = GetMap()->GetInstanceData())
            iData->OnCreatureCreate(this);

    switch (GetCreatureInfo()->rank)
    {
    case CREATURE_ELITE_RARE:
        m_corpseDelay =
            sWorld::Instance()->getConfig(CONFIG_UINT32_CORPSE_DECAY_RARE);
        break;
    case CREATURE_ELITE_ELITE:
        m_corpseDelay =
            sWorld::Instance()->getConfig(CONFIG_UINT32_CORPSE_DECAY_ELITE);
        break;
    case CREATURE_ELITE_RAREELITE:
        m_corpseDelay =
            sWorld::Instance()->getConfig(CONFIG_UINT32_CORPSE_DECAY_RAREELITE);
        break;
    case CREATURE_ELITE_WORLDBOSS:
        m_corpseDelay =
            sWorld::Instance()->getConfig(CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS);
        break;
    default:
        m_corpseDelay =
            sWorld::Instance()->getConfig(CONFIG_UINT32_CORPSE_DECAY_NORMAL);
        break;
    }

    LoadCreatureAddon(false);

    m_skinningLootDist = new loot_distributor(this, LOOT_SKINNING);
    m_pickpocketLootDist = new loot_distributor(this, LOOT_PICKPOCKETING);
    m_lootDistributor = new loot_distributor(this, LOOT_CORPSE);

    return true;
}

bool Creature::IsTrainerOf(Player* pPlayer, bool msg) const
{
    if (!isTrainer())
        return false;

    TrainerSpellData const* cSpells = GetTrainerSpells();
    TrainerSpellData const* tSpells = GetTrainerTemplateSpells();

    // for not pet trainer expected not empty trainer list always
    if ((!cSpells || cSpells->spellList.empty()) &&
        (!tSpells || tSpells->spellList.empty()))
    {
        logging.error(
            "Creature %u (Entry: %u) have UNIT_NPC_FLAG_TRAINER but have empty "
            "trainer spell list.",
            GetGUIDLow(), GetEntry());
        return false;
    }

    switch (GetCreatureInfo()->trainer_type)
    {
    case TRAINER_TYPE_CLASS:
        if (pPlayer->getClass() != GetCreatureInfo()->trainer_class)
        {
            if (msg)
            {
                pPlayer->PlayerTalkClass->ClearMenus();
                switch (GetCreatureInfo()->trainer_class)
                {
                case CLASS_DRUID:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        4913, GetObjectGuid());
                    break;
                case CLASS_HUNTER:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        10090, GetObjectGuid());
                    break;
                case CLASS_MAGE:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        328, GetObjectGuid());
                    break;
                case CLASS_PALADIN:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        1635, GetObjectGuid());
                    break;
                case CLASS_PRIEST:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        4436, GetObjectGuid());
                    break;
                case CLASS_ROGUE:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        4797, GetObjectGuid());
                    break;
                case CLASS_SHAMAN:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5003, GetObjectGuid());
                    break;
                case CLASS_WARLOCK:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5836, GetObjectGuid());
                    break;
                case CLASS_WARRIOR:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        4985, GetObjectGuid());
                    break;
                }
            }
            return false;
        }
        break;
    case TRAINER_TYPE_PETS:
        if (pPlayer->getClass() != CLASS_HUNTER)
        {
            if (msg)
            {
                pPlayer->PlayerTalkClass->ClearMenus();
                pPlayer->PlayerTalkClass->SendGossipMenu(3620, GetObjectGuid());
            }
            return false;
        }
        break;
    case TRAINER_TYPE_MOUNTS:
        if (GetCreatureInfo()->trainer_race &&
            pPlayer->getRace() != GetCreatureInfo()->trainer_race)
        {
            // Allowed to train if exalted
            if (FactionTemplateEntry const* faction_template =
                    getFactionTemplateEntry())
            {
                if (pPlayer->GetReputationRank(faction_template->faction) ==
                    REP_EXALTED)
                    return true;
            }

            if (msg)
            {
                pPlayer->PlayerTalkClass->ClearMenus();
                switch (GetCreatureInfo()->trainer_class)
                {
                case RACE_DWARF:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5865, GetObjectGuid());
                    break;
                case RACE_GNOME:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        4881, GetObjectGuid());
                    break;
                case RACE_HUMAN:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5861, GetObjectGuid());
                    break;
                case RACE_NIGHTELF:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5862, GetObjectGuid());
                    break;
                case RACE_ORC:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5863, GetObjectGuid());
                    break;
                case RACE_TAUREN:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5864, GetObjectGuid());
                    break;
                case RACE_TROLL:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5816, GetObjectGuid());
                    break;
                case RACE_UNDEAD:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        624, GetObjectGuid());
                    break;
                case RACE_BLOODELF:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5862, GetObjectGuid());
                    break;
                case RACE_DRAENEI:
                    pPlayer->PlayerTalkClass->SendGossipMenu(
                        5864, GetObjectGuid());
                    break;
                }
            }
            return false;
        }
        break;
    case TRAINER_TYPE_TRADESKILLS:
        if (GetCreatureInfo()->trainer_spell &&
            !pPlayer->HasSpell(GetCreatureInfo()->trainer_spell))
        {
            if (msg)
            {
                pPlayer->PlayerTalkClass->ClearMenus();
                pPlayer->PlayerTalkClass->SendGossipMenu(
                    11031, GetObjectGuid());
            }
            return false;
        }
        break;
    default:
        return false; // checked and error output at creature_template loading
    }
    return true;
}

bool Creature::CanInteractWithBattleMaster(Player* pPlayer, bool msg) const
{
    if (!isBattleMaster())
        return false;

    BattleGroundTypeId bgTypeId =
        sBattleGroundMgr::Instance()->GetBattleMasterBG(GetEntry());
    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
        return false;

    if (!msg)
        return pPlayer->GetBGAccessByLevel(bgTypeId);

    if (!pPlayer->GetBGAccessByLevel(bgTypeId))
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        switch (bgTypeId)
        {
        case BATTLEGROUND_AV:
            pPlayer->PlayerTalkClass->SendGossipMenu(7616, GetObjectGuid());
            break;
        case BATTLEGROUND_WS:
            pPlayer->PlayerTalkClass->SendGossipMenu(7599, GetObjectGuid());
            break;
        case BATTLEGROUND_AB:
            pPlayer->PlayerTalkClass->SendGossipMenu(7642, GetObjectGuid());
            break;
        case BATTLEGROUND_EY:
        case BATTLEGROUND_NA:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_AA:
        case BATTLEGROUND_RL:
            pPlayer->PlayerTalkClass->SendGossipMenu(10024, GetObjectGuid());
            break;
        default:
            break;
        }
        return false;
    }
    return true;
}

bool Creature::CanTrainAndResetTalentsOf(Player* pPlayer) const
{
    return pPlayer->getLevel() >= 10 &&
           GetCreatureInfo()->trainer_type == TRAINER_TYPE_CLASS &&
           pPlayer->getClass() == GetCreatureInfo()->trainer_class;
}

void Creature::PrepareCorpseLoot()
{
    if (isAlive() || lootBody)
        return;

    if (IsTemporarySummon() && !((TemporarySummon*)this)->CanDropLoot())
        return;

    if (GetCreatureType() == CREATURE_TYPE_CRITTER)
    {
        lootBody = true;
        PrepareSkinningLoot();
        return;
    }

    if (!m_lootDistributor)
        return;

    lootBody = true;

    bool hasLoot = GetCreatureInfo()->maxgold > 0 || GetCreatureInfo()->lootid;
    if (hasLoot)
    {
        m_lootDistributor->generate_loot();
        m_lootDistributor->start_loot_session();
        SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
    }
    else if (!sWorld::Instance()->getConfig(CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW))
        PrepareSkinningLoot();
}

void Creature::PrepareSkinningLoot()
{
    if (isAlive() || lootSkin || !lootBody || !m_skinningLootDist)
        return;

    if (GetCreatureInfo()->SkinLootId)
    {
        m_skinningLootDist->generate_loot();
        RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
        lootSkin = true;
    }
    else
        FinishedLooting();
}

void Creature::FinishedLooting()
{
    uint32 corpseLootDelay;

    RemoveLootFlags();

    // Despawn immediately if skinned
    if (lootSkin)
        corpseLootDelay = 0;
    else
        corpseLootDelay =
            sWorld::Instance()->getConfig(CONFIG_UINT32_CORPSE_DECAY_LOOTED) *
            1000;

    // if m_respawnTime is not expired already
    if (m_respawnTime >= WorldTimer::time_no_syscall())
    {
        // if spawntimesecs is larger than default corpse delay always use
        // corpseLootedDelay
        if (m_respawnDelay > m_corpseDelay)
            m_corpseDecayTimer = corpseLootDelay;
        else
            // if m_respawnDelay is relatively short and corpseDecayTimer is
            // larger than corpseLootedDelay
            if (m_corpseDecayTimer > corpseLootDelay)
            m_corpseDecayTimer = corpseLootDelay;
    }
    else
    {
        m_corpseDecayTimer = 0;

        // TODO: reaching here, means mob will respawn at next tick.
        // This might be a place to set some aggro delay so creature has
        // ~5 seconds before it can react to hostile surroundings.

        // It's worth noting that it will not be fully correct either way.
        // At this point another "instance" of the creature are presumably
        // expected to
        // be spawned already, while this corpse will not appear in respawned
        // form.
    }
}

void Creature::RemoveLootFlags()
{
    RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
}

loot_distributor* Creature::GetLootDistributor() const
{
    if (isAlive() && !isInCombat())
        return m_pickpocketLootDist;
    else if (isAlive())
        return m_lootDistributor;
    else if (lootSkin)
        return m_skinningLootDist;
    return m_lootDistributor;
}

void Creature::OnLootOpen(LootType lootType, Player* looter)
{
    // Only pickpocketing is handled here
    if (lootType != LOOT_PICKPOCKETING || isInCombat())
        return;

    // Only generate loot if none exists and we're alive
    if (lootPickpocket || lootBody || lootSkin || !isAlive())
        return;

    assert(m_pickpocketLootDist);

    m_pickpocketLootDist->generate_loot(looter);
    lootPickpocket = true;
}

void Creature::ResetLootRecipients()
{
    if (m_lootDistributor)
        m_lootDistributor->recipient_mgr()->reset();
    legit_dmg_taken = 0;
    player_dmg_taken = 0;
}

bool Creature::IsTappedBy(const Player* player) const
{
    if (!m_lootDistributor)
        return false;
    return m_lootDistributor->recipient_mgr()->has_tap(player);
}

void Creature::abandon_taps(ObjectGuid guid) const
{
    // If all other taps but us are out of range or dead, the taps reset when we
    // leave combat (run away, die, etc.) This makes the loot re-taggable, even
    // though the NPC is already engaged.

    if (hasUnitState(UNIT_STAT_CONTROLLED))
        return;

    if (!m_lootDistributor)
        return;

    for (auto& other : *m_lootDistributor->recipient_mgr()->taps())
        if (guid != other)
        {
            auto player = GetMap()->GetPlayer(other);
            if (player && player->isAlive() &&
                player->IsWithinDistInMap(
                    this, GetMap()->GetVisibilityDistance()))
                return;
        }

    m_lootDistributor->recipient_mgr()->reset();
}

void Creature::SaveToDB()
{
    // this should only be used when the creature has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    CreatureData const* data =
        sObjectMgr::Instance()->GetCreatureData(GetGUIDLow());
    if (!data)
    {
        logging.error("Creature::SaveToDB failed, cannot get creature data!");
        return;
    }

    SaveToDB(GetMapId(), data->spawnMask);
}

void Creature::SaveToDB(uint32 mapid, uint8 spawnMask)
{
    // TODO: remove this check
    if (IsPet())
    {
        assert(false);
        return;
    }

    // update in loaded data
    CreatureData& data =
        sObjectMgr::Instance()->NewOrExistCreatureData(GetGUIDLow());

    uint32 displayId = GetNativeDisplayId();

    // check if it's a custom model and if not, use 0 for displayId
    CreatureInfo const* cinfo = GetCreatureInfo();
    if (cinfo)
    {
        if (displayId != cinfo->ModelId[0] && displayId != cinfo->ModelId[1] &&
            displayId != cinfo->ModelId[2] && displayId != cinfo->ModelId[3])
        {
            for (int i = 0; i < MAX_CREATURE_MODEL && displayId; ++i)
                if (cinfo->ModelId[i])
                    if (CreatureModelInfo const* minfo =
                            sObjectMgr::Instance()->GetCreatureModelInfo(
                                cinfo->ModelId[i]))
                        if (displayId == minfo->modelid_other_gender)
                            displayId = 0;
        }
        else
            displayId = 0;
    }

    // data->guid = guid don't must be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.spawnMask = spawnMask;
    data.modelid_override = displayId;
    data.equipmentId = GetEquipmentId();
    data.posX = GetX();
    data.posY = GetY();
    data.posZ = GetZ();
    data.orientation = GetO();
    data.spawntimesecs = m_respawnDelay;
    // prevent add data integrity problems
    data.spawndist =
        get_default_movement_gen() == movement::gen::idle ? 0 : m_respawnradius;
    data.currentwaypoint = 0;
    data.curhealth = 0;
    data.curmana = 0;
    data.is_dead = m_isDeadByDefault;
    // prevent add data integrity problems
    data.movementType =
        uint8(!m_respawnradius &&
                      (get_default_movement_gen() == movement::gen::random ||
                          get_default_movement_gen() ==
                              movement::gen::random_waterair) ?
                  movement::gen::idle :
                  get_default_movement_gen());

    // updated in DB
    WorldDatabase.BeginTransaction();

    WorldDatabase.PExecuteLog(
        "DELETE FROM creature WHERE guid=%u", GetGUIDLow());

    std::ostringstream ss;
    ss << "INSERT INTO creature VALUES (" << GetGUIDLow() << "," << data.id
       << "," << data.mapid << "," << uint32(data.spawnMask)
       << "," // cast to prevent save as symbol
       << data.modelid_override << "," << data.equipmentId << "," << data.posX
       << "," << data.posY << "," << data.posZ << "," << data.orientation << ","
       << data.spawntimesecs << ","     // respawn time
       << (float)data.spawndist << ","  // spawn distance (float)
       << data.currentwaypoint << ","   // currentwaypoint
       << data.curhealth << ","         // curhealth
       << data.curmana << ","           // curmana
       << (data.is_dead ? 1 : 0) << "," // is_dead
       << uint32(data.movementType)
       << "," // default movement generator type, cast to prevent save as symbol
       << (m_bossLink ? m_bossLink.GetEntry() : 0) << ","   // bosslink entry
       << (m_bossLink ? m_bossLink.GetCounter() : 0) << "," // bosslink guid
       << leash_coords[0] << ","                            // leash_x
       << leash_coords[1] << ","                            // leash_y
       << leash_coords[2] << ","                            // leash_z
       << leash_radius << ","                               // leash_radius
       << aggro_radius << ","                               // aggro_radius
       << chain_radius << ")";                              // chain_radius

    WorldDatabase.PExecuteLog("%s", ss.str().c_str());

    WorldDatabase.CommitTransaction();
}

void Creature::SelectLevel(const CreatureInfo* cinfo, float percentHealth,
    float /*percentMana*/, bool keepLevel)
{
    uint32 rank = IsPet() ? 0 : cinfo->rank;

    // level
    uint32 minlevel = std::min(cinfo->maxlevel, cinfo->minlevel);
    uint32 maxlevel = std::max(cinfo->maxlevel, cinfo->minlevel);
    uint32 level;
    if (keepLevel && estd::in_range(minlevel, maxlevel, getLevel()))
        level = getLevel();
    else
        level = minlevel == maxlevel ? minlevel : urand(minlevel, maxlevel);
    SetLevel(level);

    float rellevel = maxlevel == minlevel ? 0 : (float(level - minlevel)) /
                                                    (maxlevel - minlevel);

    // health
    float healthmod = _GetHealthMod(rank);

    uint32 minhealth = std::min(cinfo->maxhealth, cinfo->minhealth);
    uint32 maxhealth = std::max(cinfo->maxhealth, cinfo->minhealth);
    uint32 health = uint32(
        healthmod * (minhealth + uint32(rellevel * (maxhealth - minhealth))));

    SetCreateHealth(health);
    SetMaxHealth(health);

    if (percentHealth == 100.0f)
        SetHealth(health);
    else
        SetHealthPercent(percentHealth);

    // mana
    uint32 minmana = std::min(cinfo->maxmana, cinfo->minmana);
    uint32 maxmana = std::max(cinfo->maxmana, cinfo->minmana);
    uint32 mana = minmana + uint32(rellevel * (maxmana - minmana));

    SetCreateMana(mana);
    SetMaxPower(POWER_MANA, mana); // MAX Mana
    SetPower(POWER_MANA, mana);

    // TODO: set UNIT_FIELD_POWER*, for some creature class case (energy, etc)

    SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, float(health));
    SetModifierValue(UNIT_MOD_MANA, BASE_VALUE, float(mana));

    // damage
    float damagemod = _GetDamageMod(rank);

    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, cinfo->mindmg * damagemod);
    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, cinfo->maxdmg * damagemod);

    SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, cinfo->mindmg * damagemod);
    SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, cinfo->maxdmg * damagemod);

    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, cinfo->minrangedmg * damagemod);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, cinfo->maxrangedmg * damagemod);

    uint32 create_attackpower;
    if (cinfo->attackpower > 0)
        create_attackpower = cinfo->attackpower;
    else
        create_attackpower = level * 4 + 48;

    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, create_attackpower);
}

float Creature::_GetHealthMod(int32 Rank)
{
    switch (Rank) // define rates for each elite rank
    {
    case CREATURE_ELITE_NORMAL:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP);
    case CREATURE_ELITE_ELITE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP);
    case CREATURE_ELITE_RAREELITE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP);
    case CREATURE_ELITE_WORLDBOSS:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP);
    case CREATURE_ELITE_RARE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP);
    default:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP);
    }
}

float Creature::_GetDamageMod(int32 Rank)
{
    switch (Rank) // define rates for each elite rank
    {
    case CREATURE_ELITE_NORMAL:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE);
    case CREATURE_ELITE_ELITE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE);
    case CREATURE_ELITE_RAREELITE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE);
    case CREATURE_ELITE_WORLDBOSS:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE);
    case CREATURE_ELITE_RARE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE);
    default:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE);
    }
}

float Creature::GetSpellDamageMod(int32 Rank)
{
    switch (Rank) // define rates for each elite rank
    {
    case CREATURE_ELITE_NORMAL:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE);
    case CREATURE_ELITE_ELITE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
    case CREATURE_ELITE_RAREELITE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE);
    case CREATURE_ELITE_WORLDBOSS:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE);
    case CREATURE_ELITE_RARE:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE);
    default:
        return sWorld::Instance()->getConfig(
            CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
    }
}

bool Creature::CreateFromProto(uint32 guidlow, CreatureInfo const* cinfo,
    Team team, const CreatureData* data /*=NULL*/,
    GameEventCreatureData const* eventData /*=NULL*/)
{
    m_originalEntry = cinfo->Entry;

    Object::_Create(guidlow, cinfo->Entry, cinfo->GetHighGuid());

    if (!UpdateEntry(cinfo->Entry, team, data, eventData, false))
        return false;

    return true;
}

bool Creature::LoadFromDB(uint32 guidlow, Map* map)
{
    CreatureData const* data = sObjectMgr::Instance()->GetCreatureData(guidlow);

    if (!data)
    {
        logging.error(
            "Creature (GUID: %u) not found in table `creature`, can't load. ",
            guidlow);
        return false;
    }

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(data->id);
    if (!cinfo)
    {
        logging.error(
            "Creature (Entry: %u) not found in table `creature_template`, "
            "can't load. ",
            data->id);
        return false;
    }

    m_creatureInfo = cinfo;

    GameEventCreatureData const* eventData =
        sGameEventMgr::Instance()->GetCreatureUpdateDataForActiveEvent(guidlow);

    // Creature can be loaded already in map if grid has been unloaded while
    // creature walk to another grid
    if (map->GetCreature(cinfo->GetObjectGuid(guidlow)))
        return false;

    CreatureCreatePos pos(
        map, data->posX, data->posY, data->posZ, data->orientation);

    if (!Create(guidlow, pos, cinfo, TEAM_NONE, data, eventData))
        return false;

    leash_coords[0] = data->leash_x;
    leash_coords[1] = data->leash_y;
    leash_coords[2] = data->leash_z;
    leash_radius = data->leash_radius;

    // The aggro radius we have is for our maximum level, subtract 1 for each
    // level we're above the maximum we can spawn as
    if (data->aggro_radius > 0)
        aggro_radius = data->aggro_radius -
                       ((int)m_creatureInfo->maxlevel - (int)getLevel());
    else if (m_creatureInfo->aggro_radius > 0)
        aggro_radius = m_creatureInfo->aggro_radius -
                       ((int)GetCreatureInfo()->maxlevel - (int)getLevel());
    else
        aggro_radius = NPC_DEFAULT_AGGRO_DIST;

    if (data->chain_radius > 0)
        chain_radius = data->chain_radius;
    else if (m_creatureInfo->chain_radius > 0)
        chain_radius = m_creatureInfo->chain_radius;
    else
        chain_radius = NPC_DEFAULT_CHAIN_DIST;

    m_respawnradius = data->spawndist;

    m_respawnDelay = data->spawntimesecs;
    m_corpseDelay = std::min(m_respawnDelay * 9 / 10,
        m_corpseDelay); // set corpse delay to 90% of the respawn delay
    m_isDeadByDefault = data->is_dead;
    m_deathState = m_isDeadByDefault ? DEAD : ALIVE;

    if (data->boss_link_entry)
    {
        m_bossLink = ObjectGuid(
            HIGHGUID_UNIT, data->boss_link_entry, data->boss_link_guid);
        m_checkBossLink = true;
    }

    m_respawnTime =
        map->GetPersistentState() ?
            map->GetPersistentState()->GetCreatureRespawnTime(GetGUIDLow()) :
            0;

    if (m_respawnTime > WorldTimer::time_no_syscall()) // not ready to respawn
    {
        m_deathState = DEAD;
        if (CanFly())
        {
            float tz = GetTerrain()->GetHeightStatic(
                data->posX, data->posY, data->posZ, false);
            if (data->posZ - tz > 0.1)
                Relocate(data->posX, data->posY, tz);
        }
    }
    else if (m_respawnTime) // respawn time set but expired
    {
        m_respawnTime = 0;

        if (GetMap()->GetPersistentState())
            GetMap()->GetPersistentState()->SaveCreatureRespawnTime(
                GetGUIDLow(), 0, m_respawnDelay);
    }

    uint32 curhealth = data->curhealth == 0 ? GetMaxHealth() : data->curhealth;
    uint32 curmana = data->curmana == 0 && cinfo->RegenMana == true ?
                         GetMaxPower(POWER_MANA) :
                         data->curmana;
    if (curhealth)
    {
        curhealth = uint32(curhealth * _GetHealthMod(GetCreatureInfo()->rank));
        if (curhealth < 1)
            curhealth = 1;
    }

    SetHealth(m_deathState == ALIVE ? curhealth : 0);
    SetPower(POWER_MANA, curmana);

    SetMeleeDamageSchool(SpellSchools(GetCreatureInfo()->dmgschool));

    // checked at creature_template loading
    m_defaultMovementGen = movement::gen(data->movementType);
    ResetInhabitType(cinfo->InhabitType);

    AIM_Initialize();

    // Notify instance (do it after saved state has been properly loaded)
    if (InstanceData* iData = GetMap()->GetInstanceData())
        iData->OnCreatureCreate(this);

    return true;
}

void Creature::LoadEquipment(uint32 equip_entry, bool force)
{
    if (equip_entry == 0)
    {
        if (force)
        {
            for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
                SetVirtualItem(VirtualItemSlot(i), 0);
            m_equipmentId = 0;
        }
        return;
    }

    if (EquipmentInfo const* einfo =
            sObjectMgr::Instance()->GetEquipmentInfo(equip_entry))
    {
        m_equipmentId = equip_entry;
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
            SetVirtualItem(VirtualItemSlot(i), einfo->equipentry[i]);
    }
    else if (EquipmentInfoRaw const* einfo =
                 sObjectMgr::Instance()->GetEquipmentInfoRaw(equip_entry))
    {
        m_equipmentId = equip_entry;
        for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
            SetVirtualItemRaw(VirtualItemSlot(i), einfo->equipmodel[i],
                einfo->equipinfo[i], einfo->equipslot[i]);
    }

    // Update damage on equipment change
    UpdateDamagePhysical(BASE_ATTACK);
    UpdateDamagePhysical(OFF_ATTACK);
    UpdateDamagePhysical(RANGED_ATTACK);
}

bool Creature::HasQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds =
        sObjectMgr::Instance()->GetCreatureQuestRelationsMapBounds(GetEntry());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
            return true;
    }
    return false;
}

bool Creature::HasInvolvedQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds =
        sObjectMgr::Instance()->GetCreatureQuestInvolvedRelationsMapBounds(
            GetEntry());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
            return true;
    }
    return false;
}

struct CreatureRespawnDeleteWorker
{
    explicit CreatureRespawnDeleteWorker(uint32 guid) : i_guid(guid) {}

    void operator()(MapPersistentState* state)
    {
        state->SaveCreatureRespawnTime(i_guid, 0,
            SAVE_CREATURE_MIN_RESPAWN_DELAY); // Always cause a delete
    }

    uint32 i_guid;
};

void Creature::DeleteFromDB()
{
    CreatureData const* data =
        sObjectMgr::Instance()->GetCreatureData(GetGUIDLow());
    if (!data)
    {
        LOG_DEBUG(logging, "Trying to delete not saved creature!");
        return;
    }

    DeleteFromDB(GetGUIDLow(), data);
}

void Creature::DeleteFromDB(uint32 lowguid, CreatureData const* data)
{
    CreatureRespawnDeleteWorker worker(lowguid);
    sMapPersistentStateMgr::Instance()->DoForAllStatesWithMapId(
        data->mapid, worker);

    sObjectMgr::Instance()->DeleteCreatureData(lowguid);

    WorldDatabase.BeginTransaction();
    WorldDatabase.PExecuteLog("DELETE FROM creature WHERE guid=%u", lowguid);
    WorldDatabase.PExecuteLog(
        "DELETE FROM creature_addon WHERE guid=%u", lowguid);
    WorldDatabase.PExecuteLog(
        "DELETE FROM creature_movement WHERE id=%u", lowguid);
    WorldDatabase.PExecuteLog(
        "DELETE FROM game_event_creature WHERE guid=%u", lowguid);
    WorldDatabase.PExecuteLog(
        "DELETE FROM game_event_creature_data WHERE guid=%u", lowguid);
    WorldDatabase.PExecuteLog(
        "DELETE FROM creature_battleground WHERE guid=%u", lowguid);
    WorldDatabase.CommitTransaction();
}

float Creature::GetAggroDistance(const Unit* target) const
{
    if (auto owner = target->GetCharmerOrOwner())
        return GetAggroDistance(owner);

    float dist = aggro_radius;

    int target_level = target->GetLevelForTarget(this);
    int my_level = GetLevelForTarget(this);

    dist += (my_level - target_level);

    dist += GetTotalAuraModifier(SPELL_AURA_MOD_DETECT_RANGE);
    dist += target->GetTotalAuraModifier(SPELL_AURA_MOD_DETECTED_RANGE);

    if (dist > 40.0f)
        dist = 40.0f;
    else if (dist < 5.0f)
        dist = 5.0f;

    return dist;
}

void Creature::SetDeathState(DeathState s)
{
    if ((s == JUST_DIED && !m_isDeadByDefault) ||
        (s == JUST_ALIVED && m_isDeadByDefault))
    {
        m_corpseDecayTimer =
            m_corpseDelay * IN_MILLISECONDS; // the max/default time for corpse
                                             // decay (before creature is
                                             // looted)
        m_respawnTime = WorldTimer::time_no_syscall() +
                        m_respawnDelay; // respawn delay (spawntimesecs)

        SaveRespawnTime();
    }

    Unit::SetDeathState(s);

    if (s == JUST_DIED)
    {
        if (GetMap()->IsCreatureInCombatList(this))
        {
            SetActiveObjectState(false);
            GetMap()->CreatureLeaveCombat(this);
        }

        SetTargetGuid(ObjectGuid()); // remove target selection in any cases
                                     // (can be set at aura remove in
                                     // Unit::SetDeathState)
        SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

        ResetLowHealthSpeed();
        UpdateSpeed(MOVE_RUN, false);

        // Fishes (water only npcs) stay where they died
        if (m_inhabitType != INHABIT_WATER)
        {
            m_movementInfo.RemoveMovementFlag(
                MOVEFLAG_SWIMMING | MOVEFLAG_FLYING);

            // Make NPCs in the air fall down when they die
            if (m_inhabitType & INHABIT_AIR ||
                m_movementInfo.HasMovementFlag(MOVEFLAG_LEVITATING))
            {
                m_movementInfo.RemoveMovementFlag(MOVEFLAG_LEVITATING);
                movement_gens.push(new movement::FallMovementGenerator());
            }
        }

        // Don't despawn combat summons; just clear our list
        combat_summons.clear();

        // Despawn dedicated pet
        if (Pet* pet = GetPet())
            pet->Unsummon(PET_SAVE_AS_DELETED, this);

        Unit::SetDeathState(CORPSE);

        // Cancel any charms we have
        InterruptCharms();
    }

    if (s == JUST_ALIVED)
    {
        CreatureInfo const* cinfo = GetCreatureInfo();

        SetHealth(GetMaxHealth());
        ResetLootRecipients();
        SetWalk(true);

        if (getFaction() != cinfo->faction_A)
            setFaction(cinfo->faction_A);

        Unit::SetDeathState(ALIVE);

        clearUnitState(UNIT_STAT_ALL_STATE);
        movement_gens.reset();

        SetMeleeDamageSchool(SpellSchools(cinfo->dmgschool));

        SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
        SetUInt32Value(UNIT_FIELD_FLAGS, cinfo->unit_flags);
        SetStandState(UNIT_STAND_STATE_STAND);

        auto prev_vis = GetVisibility();

        LoadCreatureAddon(true);

        if (GetVisibility() == prev_vis &&
            (cinfo->flags_extra & CREATURE_FLAG_EXTRA_INVISIBLE) == 0)
            SetVisibility(VISIBILITY_ON);

        // Flags after LoadCreatureAddon. Any spell in *addon
        // will not be able to adjust these.
        SetUInt32Value(UNIT_NPC_FLAGS, cinfo->npcflag);
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
    }
}

void Creature::Respawn(bool force /* = false */)
{
    m_forcedRespawn = force;

    RemoveCorpse();

    if (IsDespawned())
    {
        if (HasStaticDBSpawnData() && GetMap()->GetPersistentState())
            GetMap()->GetPersistentState()->SaveCreatureRespawnTime(
                GetGUIDLow(), 0, m_respawnDelay);
        m_respawnTime = WorldTimer::time_no_syscall(); // respawn at next tick
    }
}

void Creature::ForcedDespawn(uint32 timeMSToDespawn)
{
    if (timeMSToDespawn)
    {
        auto pEvent = new ForcedDespawnDelayEvent(*this);

        m_Events.AddEvent(pEvent, m_Events.CalculateTime(timeMSToDespawn));
        return;
    }

    if (auto sum = dynamic_cast<TemporarySummon*>(this))
        sum->ForcefullyDespawned();

    // SmartAI callback: Clear creature group used in scripts
    if (auto ai = dynamic_cast<SmartAI*>(AI()))
        ai->ClearGroup();

    if (isAlive())
    {
        if (GetVisibility() != VISIBILITY_OFF)
            SetVisibility(VISIBILITY_REMOVE_CORPSE); // Reset to VISIBILITY_ON
                                                     // in
                                                     // Creature::RemoveCorpse()
        SetDeathState(JUST_DIED);
    }

    m_corpseDecayTimer = 1; // Properly remove corpse on next tick (also pool
                            // system requires Creature::Update call with CORPSE
                            // state
    SetHealth(0);           // just for nice GM-mode view
}

bool Creature::IsImmuneToSpell(SpellEntry const* spellInfo)
{
    if (!spellInfo)
        return false;

    if (GetCreatureInfo()->MechanicImmuneMask &
        (1 << (spellInfo->Mechanic - 1)))
        return true;

    return Unit::IsImmuneToSpell(spellInfo);
}

bool Creature::IsImmuneToSpellEffect(
    SpellEntry const* spellInfo, SpellEffectIndex index) const
{
    if (GetCreatureInfo()->MechanicImmuneMask &
        (1 << (spellInfo->EffectMechanic[index] - 1)))
        return true;

    // Taunt immunity special flag check
    if (GetCreatureInfo()->flags_extra & CREATURE_FLAG_EXTRA_NOT_TAUNTABLE)
    {
        // Taunt aura apply check
        if (spellInfo->Effect[index] == SPELL_EFFECT_APPLY_AURA)
        {
            if (spellInfo->EffectApplyAuraName[index] == SPELL_AURA_MOD_TAUNT)
                return true;
        }
        // Spell effect taunt check
        else if (spellInfo->Effect[index] == SPELL_EFFECT_ATTACK_ME)
            return true;
    }

    return Unit::IsImmuneToSpellEffect(spellInfo, index);
}

SpellEntry const* Creature::ReachWithSpellAttack(Unit* pVictim)
{
    if (!pVictim)
        return nullptr;

    for (auto& elem : m_spells)
    {
        if (!elem)
            continue;
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(elem);
        if (!spellInfo)
        {
            logging.error("WORLD: unknown spell id %i", elem);
            continue;
        }

        bool bcontinue = true;
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if ((spellInfo->Effect[j] == SPELL_EFFECT_SCHOOL_DAMAGE) ||
                (spellInfo->Effect[j] == SPELL_EFFECT_INSTAKILL) ||
                (spellInfo->Effect[j] == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE) ||
                (spellInfo->Effect[j] == SPELL_EFFECT_HEALTH_LEECH))
            {
                bcontinue = false;
                break;
            }
        }
        if (bcontinue)
            continue;

        if (spellInfo->manaCost > GetPower(POWER_MANA))
            continue;
        SpellRangeEntry const* srange =
            sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
        float range = GetSpellMaxRange(srange);
        float minrange = GetSpellMinRange(srange);

        float dist = GetCombatDistance(pVictim);

        // if(!isInFront( pVictim, range ) && spellInfo->AttributesEx )
        //    continue;
        if (dist > range || dist < minrange)
            continue;
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE &&
            HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
            continue;
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY &&
            HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
            continue;
        return spellInfo;
    }
    return nullptr;
}

SpellEntry const* Creature::ReachWithSpellCure(Unit* pVictim)
{
    if (!pVictim)
        return nullptr;

    for (auto& elem : m_spells)
    {
        if (!elem)
            continue;
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(elem);
        if (!spellInfo)
        {
            logging.error("WORLD: unknown spell id %i", elem);
            continue;
        }

        bool bcontinue = true;
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if ((spellInfo->Effect[j] == SPELL_EFFECT_HEAL))
            {
                bcontinue = false;
                break;
            }
        }
        if (bcontinue)
            continue;

        if (spellInfo->manaCost > GetPower(POWER_MANA))
            continue;
        SpellRangeEntry const* srange =
            sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
        float range = GetSpellMaxRange(srange);
        float minrange = GetSpellMinRange(srange);

        float dist = GetCombatDistance(pVictim);

        // if(!isInFront( pVictim, range ) && spellInfo->AttributesEx )
        //    continue;
        if (dist > range || dist < minrange)
            continue;
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE &&
            HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED))
            continue;
        if (spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY &&
            HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
            continue;
        return spellInfo;
    }
    return nullptr;
}

bool Creature::IsVisibleInGridForPlayer(Player* pl) const
{
    // gamemaster in GM mode see all, including ghosts
    if (pl->isGameMaster())
        return true;

    if (GetCreatureInfo()->flags_extra & CREATURE_FLAG_EXTRA_INVISIBLE)
        return false;

    // Live player (or with not release body see live creatures or death
    // creatures with corpse disappearing time > 0
    if (pl->isAlive() || pl->GetDeathTimer() > 0)
    {
        return (isAlive() || m_corpseDecayTimer > 0 ||
                (m_isDeadByDefault && m_deathState == CORPSE));
    }

    // Dead player see live creatures near own corpse
    if (isAlive())
    {
        Corpse* corpse = pl->GetCorpse();
        if (corpse)
        {
            // 20 - aggro distance for same level, 25 - max additional distance
            // if player level less that creature level
            if (corpse->IsWithinDistInMap(this, (20 + 25)))
                return true;
        }
    }

    // Dead player can see ghosts
    if (GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_GHOST_VISIBLE)
        return true;

    // and not see any other
    return false;
}

void Creature::SendAIReaction(AiReaction reactionType)
{
    WorldPacket data(SMSG_AI_REACTION, 12);

    data << GetObjectGuid();
    data << uint32(reactionType);

    ((WorldObject*)this)->SendMessageToSet(&data, true);
}

void Creature::CallForHelp(float fRadius)
{
    if (fRadius <= 0.0f || !getVictim() || IsPet() || isCharmed())
        return;

    CustomAggroPulse(fRadius);
}

bool Creature::CanAssist(const Unit* u, const Unit* enemy, bool sparring) const
{
    if (!isAlive() || GetCharmerOrOwnerGuid() ||
        u->GetTypeId() != TYPEID_UNIT ||
        (!isInCombat() &&
            (IsCivilian() || HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE))) ||
        IsInEvadeMode() || IsAffectedByThreatIgnoringCC())
        return false;

    const Creature* c_u = static_cast<const Creature*>(u);

    // Logic to not let players pull mobs onto other players
    if ((enemy->GetTypeId() == TYPEID_PLAYER ||
            static_cast<const Creature*>(enemy)->IsPet()) &&
        !GetMap()->Instanceable()) // Does not apply to instanced maps
    {
        const Player* p = enemy->GetTypeId() == TYPEID_PLAYER ?
                              static_cast<const Player*>(enemy) :
                              enemy->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (p)
        {
            // u is always tapped with enemy, so if we are tapped as well, enemy
            // player must have a tap on us
            if (HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED))
            {
                if (!p->HasTapOn(this))
                    return false;
            }
            else if (isInCombat() && !sparring)
            {
                // We're in combat but not tapped, so we ignore assistance
                return false;
            }
        }
    }

    // Ignore group asssist flags if target is under a fear or flee effect
    if (!u->hasUnitState(UNIT_STAT_FLEEING))
    {
        // We can't assist if our CreatureGroup prevents us from doing so
        if (creature_group_)
        {
            if (creature_group_->HasFlag(CREATURE_GROUP_FLAG_CANNOT_ASSIST))
                return false;
            else if (creature_group_->HasFlag(
                         CREATURE_GROUP_FLAG_CANNOT_ASSIST_OTHER_GRPS) &&
                     c_u->GetGroup() != nullptr)
                return false;
        }

        if (c_u->GetGroup())
        {
            if (c_u->GetGroup()->HasFlag(
                    CREATURE_GROUP_FLAG_CANNOT_BE_ASSISTED))
                return false;
        }
    }

    return (IsFriendlyTo(u) || getFaction() == u->getFaction()) &&
           IsHostileTo(enemy);
}

bool Creature::CanInitiateAttack() const
{
    if (IsInEvadeMode())
        return false;

    if (hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED))
        return false;

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE))
        return false;

    if (isPassiveToHostile())
        return false;

    return true;
}

bool Creature::CanStartAttacking(const Unit* who) const
{
    if (IsCivilian() || IsNeutralToAll())
        return false;

    if (!CanInitiateAttack())
        return false;

    if (!who->isTargetableForAttack())
        return false;

    if (!IsHostileTo(who))
        return false;

    if (!who->isInAccessablePlaceFor(this))
        return false;

    if (GetTypeId() == TYPEID_UNIT)
    {
        auto me = static_cast<const Creature*>(this);

        if (me->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_EXTRA_AGGRESSIVE_PLAYER_DEMON &&
            !const_cast<Creature*>(me)->getThreatManager().hasTarget(
                const_cast<Unit*>(who)) &&
            !who->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP) &&
            (who->GetTypeId() == TYPEID_PLAYER ||
                static_cast<const Creature*>(who)->IsPlayerPet()))
            return false;
    }

    if (who->GetTypeId() == TYPEID_UNIT)
    {
        auto cwho = static_cast<const Creature*>(who);

        if (cwho->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_IGNORED_BY_NPCS &&
            who->GetCharmerGuid().IsEmpty())
            return false;

        if (cwho->IsInEvadeMode())
            return false;
    }

    if (who->GetTypeId() == TYPEID_PLAYER && !GetMap()->Instanceable() &&
        !CanFly() && who->m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING2) &&
        !CanReachWithMeleeAttack(who, -1.0f))
        return false;

    return true;
}

void Creature::SaveRespawnTime()
{
    if (IsPet() || !HasStaticDBSpawnData() || !GetMap()->GetPersistentState())
        return;

    time_t respawn_time;
    if (m_respawnTime > WorldTimer::time_no_syscall()) // dead (no corpse)
        respawn_time = m_respawnTime;
    else if (m_corpseDecayTimer > 0) // dead (corpse)
        respawn_time = WorldTimer::time_no_syscall() + m_respawnDelay +
                       m_corpseDecayTimer / IN_MILLISECONDS;
    else // alive
        return;

    GetMap()->GetPersistentState()->SaveCreatureRespawnTime(
        GetGUIDLow(), respawn_time, m_respawnDelay);

    // If we have boss-linked mobs, save the same respawn timer for them to the
    // database as well
    for (auto guid : m_linkedMobs)
    {
        if (Creature* c = GetMap()->GetCreature(guid))
        {
            if (c->isAlive())
                c->SetRespawnDelay(m_respawnDelay);
            else
                c->SetRespawnTime(respawn_time);
        }

        GetMap()->GetPersistentState()->SaveCreatureRespawnTime(
            guid.GetCounter(), respawn_time, m_respawnDelay);
    }
}

bool Creature::IsOutOfThreatArea(Unit* pVictim) const
{
    if (!pVictim)
        return true;

    if (!pVictim->IsInMap(this))
        return true;

    if (!GetMap()->IsDungeon() &&
        !IsWithinDistInMap(pVictim, GetMap()->GetVisibilityDistance()))
        return true;

    if (!pVictim->isTargetableForAttack())
        return true;

    if (pVictim->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(pVictim)->IsInEvadeMode())
        return true;

    return false;
}

CreatureDataAddon const* Creature::GetCreatureAddon() const
{
    if (CreatureDataAddon const* addon =
            ObjectMgr::GetCreatureAddon(GetGUIDLow()))
        return addon;

    // dependent from difficulty mode entry
    if (GetEntry() != GetCreatureInfo()->Entry)
    {
        // If CreatureTemplateAddon for heroic exist, it's there for a reason
        if (CreatureDataAddon const* addon =
                ObjectMgr::GetCreatureTemplateAddon(GetCreatureInfo()->Entry))
            return addon;
    }

    // Return CreatureTemplateAddon when nothing else exist
    return ObjectMgr::GetCreatureTemplateAddon(GetEntry());
}

// creature_addon table
bool Creature::LoadCreatureAddon(bool reload)
{
    CreatureDataAddon const* cainfo = GetCreatureAddon();
    if (!cainfo)
        return false;

    if (cainfo->mount != 0)
        Mount(cainfo->mount);

    if (cainfo->bytes1 != 0)
    {
        // 0 StandState
        // 1 LoyaltyLevel  Pet only, so always 0 for default creature
        // 2 StandFlags
        // 3 StandMiscFlags

        SetByteValue(UNIT_FIELD_BYTES_1, 0, uint8(cainfo->bytes1 & 0xFF));
        // SetByteValue(UNIT_FIELD_BYTES_1, 1, uint8((cainfo->bytes1 >> 8) &
        // 0xFF));
        SetByteValue(UNIT_FIELD_BYTES_1, 1, 0);
        SetByteValue(
            UNIT_FIELD_BYTES_1, 2, uint8((cainfo->bytes1 >> 16) & 0xFF));
        SetByteValue(
            UNIT_FIELD_BYTES_1, 3, uint8((cainfo->bytes1 >> 24) & 0xFF));
    }

    // UNIT_FIELD_BYTES_2
    // 0 SheathState
    // 1 Bytes2Flags, in 3.x used UnitPVPStateFlags, that have different meaning
    // 2 UnitRename         Pet only, so always 0 for default creature
    // 3 ShapeshiftForm     Must be determined/set by shapeshift spell/aura
    SetByteValue(UNIT_FIELD_BYTES_2, 0, cainfo->sheath_state);

    if (cainfo->flags != 0)
        SetByteValue(UNIT_FIELD_BYTES_2, 1, cainfo->flags);
    else
        SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_POSITIVE_AURAS);

    // SetByteValue(UNIT_FIELD_BYTES_2, 2, 0);
    // SetByteValue(UNIT_FIELD_BYTES_2, 3, 0);

    if (cainfo->emote != 0)
        SetUInt32Value(UNIT_NPC_EMOTESTATE, cainfo->emote);

    if (cainfo->auras)
    {
        for (uint32 const* cAura = cainfo->auras; *cAura; ++cAura)
        {
            if (has_aura(*cAura))
            {
                if (!reload)
                    logging.error(
                        "Creature (GUIDLow: %u Entry: %u) has spell %u in "
                        "`auras` field, but aura is already applied.",
                        GetGUIDLow(), GetEntry(), *cAura);

                continue;
            }

            CastSpell(this, *cAura, true);
        }
    }
    return true;
}

/// Send a message to LocalDefense channel for players opposition team in the
/// zone
void Creature::SendZoneUnderAttackMessage(Player* attacker)
{
    Team enemy_team = attacker->GetTeam();

    WorldPacket data(SMSG_ZONE_UNDER_ATTACK, 4);
    data << uint32(GetZoneId());
    sWorld::Instance()->SendGlobalMessage(
        &data, nullptr, (enemy_team == ALLIANCE ? HORDE : ALLIANCE));
}

void Creature::SetInCombatWithZone()
{
    if (!CanHaveThreatList())
        return;

    Map* pMap = GetMap();

    if (!pMap->IsDungeon())
    {
        logging.error(
            "Creature entry %u call SetInCombatWithZone for map (id: %u) that "
            "isn't an instance.",
            GetEntry(), pMap->GetId());
        return;
    }

    auto can_attack = [this](Player* p)
    {
        if (p->isGameMaster())
            return false;

        if (!p->isAlive() || getThreatManager().hasTarget(p) || IsFriendlyTo(p))
            return false;

        return true;
    };

    Map::PlayerList const& PlList = pMap->GetPlayers();

    if (PlList.isEmpty())
        return;

    // Attack start closest target if no victim
    if (getVictim() == nullptr && AI())
    {
        float dist = std::numeric_limits<float>::max();
        Player* player = nullptr;
        for (Map::PlayerList::const_iterator i = PlList.begin();
             i != PlList.end(); ++i)
        {
            if (Player* p = i->getSource())
            {
                if (can_attack(p))
                {
                    float d = GetDistance(p);
                    if (d < dist)
                    {
                        player = p;
                        dist = d;
                    }
                }
            }
        }

        if (player)
            AI()->AttackStart(player);
    }

    for (Map::PlayerList::const_iterator i = PlList.begin(); i != PlList.end();
         ++i)
    {
        if (Player* pPlayer = i->getSource())
        {
            if (can_attack(pPlayer))
            {
                pPlayer->SetInCombatWith(this);
                AddThreat(pPlayer);

                // NOTE: This is not blizzlike, but it solves a non-blizzlike
                // problem Remove all soulstones that are not applied by people
                // currently in the instance

                AuraHolder* soul_stone = nullptr;

                auto& al = pPlayer->GetAurasByType(SPELL_AURA_DUMMY);
                for (auto aura : al)
                {
                    switch (aura->GetId())
                    {
                    case 20707:
                    case 20762:
                    case 20763:
                    case 20764:
                    case 20765:
                    case 27239:
                        soul_stone = aura->GetHolder();
                        break;
                    }
                    if (soul_stone)
                        break;
                }

                if (soul_stone)
                {
                    bool found = false;
                    for (auto itr = PlList.begin();
                         itr != PlList.end() && !found; ++itr)
                    {
                        if (auto plr = itr->getSource())
                            if (plr->GetObjectGuid() ==
                                soul_stone->GetCasterGuid())
                                found = true;
                    }

                    if (!found)
                        pPlayer->RemoveAuraHolder(soul_stone);
                }

                // end of non-blizzlike functionality
            }
        }
    }
}

float Creature::GetAggroPulsateRange() const
{
    if (IsAffectedByThreatIgnoringCC())
        return chain_radius * 0.5f;

    return chain_radius;
}

void Creature::AggroPulse()
{
    if (hasUnitState(UNIT_STAT_SAPPED) || IsInEvadeMode())
        return;

    float range = GetAggroPulsateRange();
    float rangesq = range * range;
    maps::visitors::simple<Creature, TemporarySummon>{}(this, range,
        [this, rangesq](Creature* c)
        {
            if (c == this)
                return;

            // Make sure 2d point to point range is less than range (it can be
            // more given a big bounding box)
            auto dx = GetX() - c->GetX();
            auto dy = GetY() - c->GetY();
            if (dx * dx + dy * dy > rangesq)
                return;

            AggroPulsateOnCreature(c);
        });
}

void Creature::CustomAggroPulse(float range)
{
    float rangesq = range * range;
    maps::visitors::simple<Creature>{}(this, range, [this, rangesq](Creature* c)
        {
            if (c == this)
                return;

            // Make sure 2d point to point range is less than range (it can be
            // more given a big bounding box)
            auto dx = GetX() - c->GetX();
            auto dy = GetY() - c->GetY();
            if (dx * dx + dy * dy > rangesq)
                return;

            AggroPulsateOnCreature(c);
        });
}

void Creature::AggroPulsateOnCreature(Creature* target)
{
    // Set the pulse target in combat with our threat targets if he isn't
    // already
    const ThreatList& tl = getThreatManager().getThreatList();
    for (const auto& elem : tl)
    {
        Unit* victim = (elem)->getTarget();
        if (!victim || !target->IsHostileTo(victim))
            continue;

        bool sparring =
            (target->isInCombat() &&
                dynamic_cast<const SmartAI*>(target->AI())) ?
                static_cast<const SmartAI*>(target->AI())->IsSparring() :
                false;

        if (!target->CanAssist(this, victim, sparring))
            continue;

        // Skip if target already has this in his threat list
        bool found = false;
        for (const auto& _titr : target->getThreatManager().getThreatList())
        {
            Unit* u = (_titr)->getTarget();
            if (u == victim)
            {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // Add combat states if we got this far
        if ((!target->isInCombat() || sparring) && target->AI())
        {
            // Verify LoS on NPCs not in combat
            if (!IsWithinWmoLOSInMap(target))
                continue;

            // If we were running away in fear previously to this, it means
            // we've now
            // found a companion to help us, and we can go back to attacking our
            // victim
            if (movement_gens.top_id() == movement::gen::run_in_fear)
                movement_gens.pop_top();

            // Attack start sets our combat and adds our intial threat for us
            // this should always happen on our most appropriate threat target,
            // which will also be the target we're attacking
            target->AI()->AttackStart(victim);
        }
        else
        {
            // Set combat states and add initial threat
            target->SetInCombatWith(victim);
            victim->SetInCombatWith(target);
            target->AddThreat(victim, 0);
        }
    }
}

float Creature::GetLowHealthSpeedRate() const
{
    if (!GetMaxHealth() || !isInCombat())
        return 1.0f;

    float healthPct = GetHealthPercent();
    if (healthPct > 25)
        return 1.0f;

    if (healthPct <= 5)
        return 0.50f;
    else if (healthPct <= 10)
        return 0.60f;
    else if (healthPct <= 15)
        return 0.70f;
    else if (healthPct <= 20)
        return 0.80f;
    else if (healthPct <= 25)
        return 0.90f;

    return 1.0f; // Only to avoid warnings
}

void Creature::UpdateLowHealthSpeed()
{
    if (IsWorldBoss() || IsDungeonBoss() || hasUnitState(UNIT_STAT_NOT_MOVE))
        return;

    float lastHealthPct = m_lastLowHpUpdate > 0 ? m_lastLowHpUpdate : 100;
    float healthPct = GetHealthPercent();
    if (healthPct > 25 && lastHealthPct > 25)
        return;

    // Update speed will query Creature::GetLowHealthSpeedRate to get the rate
    // it needs
    if (healthPct <= 5 && lastHealthPct >= 5)
        UpdateSpeed(MOVE_RUN, false);
    else if (healthPct <= 10 && lastHealthPct >= 10)
        UpdateSpeed(MOVE_RUN, false);
    else if (healthPct <= 15 && lastHealthPct >= 15)
        UpdateSpeed(MOVE_RUN, false);
    else if (healthPct <= 20 && lastHealthPct >= 20)
        UpdateSpeed(MOVE_RUN, false);
    else if (healthPct >= 25)
        UpdateSpeed(MOVE_RUN, false);

    m_lastLowHpUpdate = (healthPct == 0 ? 1 : healthPct);
}

bool Creature::MeetsSelectAttackingRequirement(
    Unit* pTarget, SpellEntry const* pSpellInfo, uint32 selectFlags) const
{
    if (selectFlags & SELECT_FLAG_PLAYER &&
        pTarget->GetTypeId() != TYPEID_PLAYER)
        return false;

    if (selectFlags & SELECT_FLAG_POWER_MANA &&
        pTarget->getPowerType() != POWER_MANA)
        return false;
    else if (selectFlags & SELECT_FLAG_POWER_RAGE &&
             pTarget->getPowerType() != POWER_RAGE)
        return false;
    else if (selectFlags & SELECT_FLAG_POWER_ENERGY &&
             pTarget->getPowerType() != POWER_ENERGY)
        return false;

    if (selectFlags & SELECT_FLAG_IN_MELEE_RANGE &&
        !CanReachWithMeleeAttack(pTarget))
        return false;
    if (selectFlags & SELECT_FLAG_NOT_IN_MELEE_RANGE &&
        CanReachWithMeleeAttack(pTarget))
        return false;

    if (selectFlags & SELECT_FLAG_IN_FRONT && !HasInArc(M_PI_F, pTarget))
        return false;

    if (selectFlags & SELECT_FLAG_IN_LOS && !IsWithinWmoLOSInMap(pTarget))
        return false;

    if (pSpellInfo)
    {
        if (selectFlags & SELECT_FLAG_IGNORE_TARGETS_WITH_AURA &&
            pTarget->has_aura(pSpellInfo->Id))
            return false;

        switch (pSpellInfo->rangeIndex)
        {
        case SPELL_RANGE_IDX_SELF_ONLY:
            return false;
        case SPELL_RANGE_IDX_ANYWHERE:
            return true;
        case SPELL_RANGE_IDX_COMBAT:
            return CanReachWithMeleeAttack(pTarget);
        }

        SpellRangeEntry const* srange =
            sSpellRangeStore.LookupEntry(pSpellInfo->rangeIndex);
        float max_range = GetSpellMaxRange(srange);
        float min_range = GetSpellMinRange(srange);
        float dist = GetCombatDistance(pTarget);

        return dist < max_range && dist >= min_range;
    }

    return true;
}

Unit* Creature::SelectAttackingTarget(AttackingTarget target, uint32 position,
    uint32 uiSpellEntry, uint32 selectFlags) const
{
    return SelectAttackingTarget(
        target, position, sSpellStore.LookupEntry(uiSpellEntry), selectFlags);
}

Unit* Creature::SelectAttackingTarget(AttackingTarget target, uint32 position,
    SpellEntry const* pSpellInfo /*= NULL*/, uint32 selectFlags /*= 0*/) const
{
    if (!CanHaveThreatList())
        return nullptr;

    // ThreatList m_threatlist;
    ThreatList const& threatlist = getThreatManager().getThreatList();
    auto itr = threatlist.begin();
    auto ritr = threatlist.rbegin();

    if (position >= threatlist.size() || !threatlist.size())
        return nullptr;

    switch (target)
    {
    case ATTACKING_TARGET_RANDOM:
    {
        std::vector<Unit*> suitableUnits;
        suitableUnits.reserve(threatlist.size() - position);
        advance(itr, position);

        Unit* farthest = nullptr;
        float dist = -1.0f;
        for (; itr != threatlist.end(); ++itr)
        {
            if (Unit* pTarget = GetMap()->GetUnit((*itr)->getUnitGuid()))
                // Skip totems and pets
                if (!(pTarget->GetTypeId() == TYPEID_UNIT &&
                        (((Creature*)pTarget)->IsPet() ||
                            ((Creature*)pTarget)->IsTotem())))
                    if (MeetsSelectAttackingRequirement(
                            pTarget, pSpellInfo, selectFlags))
                    {
                        if (selectFlags & SELECT_FLAG_FARTHEST_AWAY)
                        {
                            float d = GetDistance(pTarget);
                            if (d > dist)
                            {
                                farthest = pTarget;
                                dist = d;
                            }
                        }
                        else
                            suitableUnits.push_back(pTarget);
                    }
        }

        if (selectFlags & SELECT_FLAG_FARTHEST_AWAY)
            return farthest;
        if (!suitableUnits.empty())
            return suitableUnits[urand(0, suitableUnits.size() - 1)];

        break;
    }
    case ATTACKING_TARGET_TOPAGGRO:
    {
        advance(itr, position);
        for (; itr != threatlist.end(); ++itr)
            if (Unit* pTarget = GetMap()->GetUnit((*itr)->getUnitGuid()))
                if (MeetsSelectAttackingRequirement(
                        pTarget, pSpellInfo, selectFlags))
                    return pTarget;

        break;
    }
    case ATTACKING_TARGET_BOTTOMAGGRO:
    {
        advance(ritr, position);
        for (; ritr != threatlist.rend(); ++ritr)
            if (Unit* pTarget = GetMap()->GetUnit((*itr)->getUnitGuid()))
                if (MeetsSelectAttackingRequirement(
                        pTarget, pSpellInfo, selectFlags))
                    return pTarget;

        break;
    }
    }

    return nullptr;
}

void Creature::_AddCreatureSpellCooldown(uint32 spell_id, time_t end_time)
{
    m_CreatureSpellCooldowns[spell_id] = end_time;
}

void Creature::_AddCreatureCategoryCooldown(uint32 category, time_t apply_time)
{
    m_CreatureCategoryCooldowns[category] = apply_time;
}

void Creature::AddCreatureSpellCooldown(uint32 spellid)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
        return;

    uint32 cooldown = GetSpellRecoveryTime(spellInfo);
    if (cooldown)
    {
        // Here we apply mods so that pets get affected by auras that the master
        // has that are
        // supposed to modify the cooldown the pet gets on its spells (Lash of
        // Pain for example)
        if (IsPet())
        {
            Unit* owner = GetOwner();
            if (owner && owner->GetTypeId() == TYPEID_PLAYER)
                ((Player*)owner)
                    ->ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, cooldown);
        }
        _AddCreatureSpellCooldown(spellid,
            WorldTimer::time_no_syscall() + cooldown / IN_MILLISECONDS);
    }

    if (spellInfo->Category)
        _AddCreatureCategoryCooldown(
            spellInfo->Category, WorldTimer::time_no_syscall());
}

bool Creature::HasCategoryCooldown(uint32 spell_id) const
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
        return false;

    auto itr = m_CreatureCategoryCooldowns.find(spellInfo->Category);

    uint32 recoveryTime = spellInfo->CategoryRecoveryTime;

    // Here we apply mods so that pets get affected by auras that the master has
    // that are
    // supposed to modify the cooldown the pet gets on its spells (Lash of Pain
    // for example)
    if (itr != m_CreatureCategoryCooldowns.end() && IsPet())
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            ((Player*)owner)
                ->ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, recoveryTime);
    }

    return (itr != m_CreatureCategoryCooldowns.end() &&
            time_t(itr->second + (recoveryTime / IN_MILLISECONDS)) >
                WorldTimer::time_no_syscall());
}

bool Creature::HasSpellCooldown(uint32 spell_id) const
{
    auto itr = m_CreatureSpellCooldowns.find(spell_id);
    return (itr != m_CreatureSpellCooldowns.end() &&
               itr->second > WorldTimer::time_no_syscall()) ||
           HasCategoryCooldown(spell_id);
}

void Creature::ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 timeMs)
{
    if (timeMs == 0 || idSchoolMask == 0)
        return;

    for (int i = 1; i < MAX_SPELL_SCHOOL; i++)
    {
        if ((1 << i) & idSchoolMask && m_spellSchoolCooldowns[i] < timeMs)
        {
            m_spellSchoolCooldowns[i] = timeMs;
            m_spellSchoolCooldownMask |= 1 << i;
        }
    }
}

bool Creature::IsInEvadeMode() const
{
    if (evading() || movement_gens.top_id() == movement::gen::home)
        return true;

    // We're considered in evade mode if creatures in our group is evading
    if (GetGroup() &&
        !GetGroup()->HasFlag(CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
    {
        for (auto m : GetGroup()->GetMembers())
        {
            if (m == this)
                continue;
            if (m->evading() ||
                (m->movement_gens.top_id() == movement::gen::home))
                return true;
        }
    }

    return false;
}

bool Creature::HasSpell(uint32 spellID) const
{
    uint8 i;
    for (i = 0; i < CREATURE_MAX_SPELLS; ++i)
        if (spellID == m_spells[i])
            break;
    return i <
           CREATURE_MAX_SPELLS; // break before end of iteration of known spells
}

time_t Creature::GetRespawnTimeEx() const
{
    time_t now = WorldTimer::time_no_syscall();
    if (m_respawnTime > now) // dead (no corpse)
        return m_respawnTime;
    else if (m_corpseDecayTimer > 0) // dead (corpse)
        return now + m_respawnDelay + m_corpseDecayTimer / IN_MILLISECONDS;
    else
        return now;
}

void Creature::GetRespawnCoord(
    float& x, float& y, float& z, float* ori, float* dist) const
{
    if (!IsTemporarySummon() && !IsPet())
    {
        auto data = sObjectMgr::Instance()->GetCreatureData(GetGUIDLow());
        if (data)
        {
            x = data->posX;
            y = data->posY;
            z = data->posZ;
            if (ori)
                *ori = data->orientation;
            if (dist)
                *dist = GetRespawnRadius();
        }
    }
    else
    {
        float orient;

        GetSummonPoint(x, y, z, orient);

        if (ori)
            *ori = orient;
        if (dist)
            *dist = GetRespawnRadius();
    }

    // lets check if our creatures have valid spawn coordinates
    assert(
        maps::verify_coords(x, y) || PrintCoordinatesError(x, y, z, "respawn"));
}

uint32 Creature::GetLevelForTarget(Unit const* target) const
{
    if (!IsWorldBoss())
        return Unit::GetLevelForTarget(target);

    uint32 level =
        target->getLevel() +
        sWorld::Instance()->getConfig(CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF);
    if (level < 1)
        return 1;
    if (level > 255)
        return 255;
    return level;
}

std::string Creature::GetAIName() const
{
    return ObjectMgr::GetCreatureTemplate(GetEntry())->AIName;
}

std::string Creature::GetScriptName() const
{
    return sScriptMgr::Instance()->GetScriptName(GetScriptId());
}

uint32 Creature::GetScriptId() const
{
    return ObjectMgr::GetCreatureTemplate(GetEntry())->ScriptID;
}

VendorItemData const* Creature::GetVendorItems() const
{
    return sObjectMgr::Instance()->GetNpcVendorItemList(GetEntry());
}

VendorItemData const* Creature::GetVendorTemplateItems() const
{
    uint32 vendorId = GetCreatureInfo()->vendorId;
    return vendorId ?
               sObjectMgr::Instance()->GetNpcVendorTemplateItemList(vendorId) :
               nullptr;
}

uint32 Creature::GetVendorItemCurrentCount(VendorItem const* vItem)
{
    if (!vItem->maxcount)
        return vItem->maxcount;

    auto itr = m_vendorItemCounts.begin();
    for (; itr != m_vendorItemCounts.end(); ++itr)
        if (itr->itemId == vItem->item)
            break;

    if (itr == m_vendorItemCounts.end())
        return vItem->maxcount;

    VendorItemCount* vCount = &*itr;

    time_t ptime = WorldTimer::time_no_syscall();

    if (vCount->lastIncrementTime + vItem->incrtime <= ptime)
    {
        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(vItem->item);

        uint32 diff =
            uint32((ptime - vCount->lastIncrementTime) / vItem->incrtime);
        if ((vCount->count + diff * pProto->BuyCount) >= vItem->maxcount)
        {
            m_vendorItemCounts.erase(itr);
            return vItem->maxcount;
        }

        vCount->count += diff * pProto->BuyCount;
        vCount->lastIncrementTime = ptime;
    }

    return vCount->count;
}

uint32 Creature::UpdateVendorItemCurrentCount(
    VendorItem const* vItem, uint32 used_count)
{
    if (!vItem->maxcount)
        return 0;

    auto itr = m_vendorItemCounts.begin();
    for (; itr != m_vendorItemCounts.end(); ++itr)
        if (itr->itemId == vItem->item)
            break;

    if (itr == m_vendorItemCounts.end())
    {
        uint32 new_count =
            vItem->maxcount > used_count ? vItem->maxcount - used_count : 0;
        m_vendorItemCounts.push_back(VendorItemCount(vItem->item, new_count));
        return new_count;
    }

    VendorItemCount* vCount = &*itr;

    time_t ptime = WorldTimer::time_no_syscall();

    if (vCount->lastIncrementTime + vItem->incrtime <= ptime)
    {
        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(vItem->item);

        uint32 diff =
            uint32((ptime - vCount->lastIncrementTime) / vItem->incrtime);
        if ((vCount->count + diff * pProto->BuyCount) < vItem->maxcount)
            vCount->count += diff * pProto->BuyCount;
        else
            vCount->count = vItem->maxcount;
    }

    vCount->count = vCount->count > used_count ? vCount->count - used_count : 0;
    vCount->lastIncrementTime = ptime;
    return vCount->count;
}

TrainerSpellData const* Creature::GetTrainerTemplateSpells() const
{
    uint32 trainerId = GetCreatureInfo()->trainerId;
    return trainerId ?
               sObjectMgr::Instance()->GetNpcTrainerTemplateSpells(trainerId) :
               nullptr;
}

TrainerSpellData const* Creature::GetTrainerSpells() const
{
    return sObjectMgr::Instance()->GetNpcTrainerSpells(GetEntry());
}

// overwrite WorldObject function for proper name localization
const char* Creature::GetNameForLocaleIdx(int32 loc_idx) const
{
    char const* name = GetName();
    sObjectMgr::Instance()->GetCreatureLocaleStrings(
        GetEntry(), loc_idx, &name);
    return name;
}

void Creature::SetFactionTemporary(uint32 factionId, uint32 tempFactionFlags)
{
    m_temporaryFactionFlags = tempFactionFlags;
    setFaction(factionId);
}

void Creature::ClearTemporaryFaction()
{
    // No restore if creature is charmed/possessed.
    // For later we may consider extend to restore to charmer faction where
    // charmer is creature.
    // This can also be done by update any pet/charmed of creature at any
    // faction change to charmer.
    if (isCharmed())
        return;

    m_temporaryFactionFlags = TEMPFACTION_NONE;
    setFaction(GetCreatureInfo()->faction_A);
}

void Creature::KitingLeashTeleportHome()
{
    auto home = dynamic_cast<movement::HomeMovementGenerator*>(
        movement_gens.get(movement::gen::home));
    if (!home)
        return;
    SetVisibility(VISIBILITY_OFF);
    DisableSpline();
    float x, y, z;
    home->get_combat_start_pos(x, y, z);
    GetMap()->relocate(this, x, y, z, 0);
    // update visibility again next tick
    queue_action(10, [this]()
        {
            SetVisibility(VISIBILITY_ON);
        });
}

void Creature::SendAreaSpiritHealerQueryOpcode(Player* pl)
{
    uint32 next_resurrect = 0;
    if (Spell* pcurSpell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        next_resurrect = pcurSpell->GetCastedTime();
    WorldPacket data(SMSG_AREA_SPIRIT_HEALER_TIME, 8 + 4);
    data << ObjectGuid(GetObjectGuid());
    data << uint32(next_resurrect);
    pl->SendDirectMessage(std::move(data));
}

void Creature::ApplyGameEventSpells(
    GameEventCreatureData const* eventData, bool activated)
{
    uint32 cast_spell =
        activated ? eventData->spell_id_start : eventData->spell_id_end;
    uint32 remove_spell =
        activated ? eventData->spell_id_end : eventData->spell_id_start;

    if (remove_spell)
        if (SpellEntry const* spellEntry =
                sSpellStore.LookupEntry(remove_spell))
            if (IsSpellAppliesAura(spellEntry))
                remove_auras(remove_spell);

    if (cast_spell)
        CastSpell(this, cast_spell, true);
}

void Creature::FillGuidsListFromThreatList(
    std::vector<ObjectGuid>& guids, uint32 maxamount /*= 0*/)
{
    if (!CanHaveThreatList())
        return;

    ThreatList const& threats = getThreatManager().getThreatList();

    maxamount = maxamount > 0 ? std::min(maxamount, uint32(threats.size())) :
                                threats.size();

    guids.reserve(guids.size() + maxamount);

    for (auto itr = threats.begin(); maxamount && itr != threats.end();
         ++itr, --maxamount)
        guids.push_back((*itr)->getUnitGuid());
}

struct AddCreatureToRemoveListInMapsWorker
{
    AddCreatureToRemoveListInMapsWorker(ObjectGuid guid)
      : i_guid(std::move(guid))
    {
    }

    void operator()(Map* map)
    {
        if (Creature* pCreature = map->GetCreature(i_guid))
            pCreature->AddObjectToRemoveList();
    }

    ObjectGuid i_guid;
};

void Creature::AddToRemoveListInMaps(uint32 db_guid, CreatureData const* data)
{
    AddCreatureToRemoveListInMapsWorker worker(data->GetObjectGuid(db_guid));
    sMapMgr::Instance()->DoForAllMapsWithMapId(data->mapid, worker);
}

struct SpawnCreatureInMapsWorker
{
    SpawnCreatureInMapsWorker(uint32 guid, const CreatureData* data)
      : i_guid(guid), i_data(data)
    {
    }

    void operator()(Map* map)
    {
        if (i_data->special_visibility)
        {
            auto creature = new SpecialVisCreature;
            if (!creature->LoadFromDB(i_guid, map))
                delete creature;
            else
            {
                if (!map->insert(creature))
                    delete creature;
            }
        }
        else
        {
            auto creature = new Creature;
            if (!creature->LoadFromDB(i_guid, map))
                delete creature;
            else
            {
                if (!map->insert(creature))
                    delete creature;
            }
        }
    }

    uint32 i_guid;
    const CreatureData* i_data;
};

void Creature::SpawnInMaps(uint32 db_guid, CreatureData const* data)
{
    SpawnCreatureInMapsWorker worker(db_guid, data);
    sMapMgr::Instance()->DoForAllMapsWithMapId(data->mapid, worker);
}

bool Creature::HasStaticDBSpawnData() const
{
    return !IsPet() &&
           sObjectMgr::Instance()->GetCreatureData(GetGUIDLow()) != nullptr;
}

void Creature::SetVirtualItem(VirtualItemSlot slot, uint32 item_id)
{
    if (item_id == 0)
    {
        SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + slot, 0);
        SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, 0);
        SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1, 0);
        return;
    }

    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);
    if (!proto)
    {
        logging.error(
            "Not listed in 'item_template' item (ID:%u) used as virtual item "
            "for %s",
            item_id, GetGuidStr().c_str());
        return;
    }

    SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + slot, proto->DisplayInfoID);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0,
        VIRTUAL_ITEM_INFO_0_OFFSET_CLASS, proto->Class);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0,
        VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS, proto->SubClass);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0,
        VIRTUAL_ITEM_INFO_0_OFFSET_UNK0, proto->Unk0);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0,
        VIRTUAL_ITEM_INFO_0_OFFSET_MATERIAL, proto->Material);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1,
        VIRTUAL_ITEM_INFO_1_OFFSET_INVENTORYTYPE, proto->InventoryType);
    SetByteValue(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1,
        VIRTUAL_ITEM_INFO_1_OFFSET_SHEATH, proto->Sheath);
}

void Creature::SetVirtualItemRaw(
    VirtualItemSlot slot, uint32 display_id, uint32 info0, uint32 info1)
{
    SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_DISPLAY + slot, display_id);
    SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 0, info0);
    SetUInt32Value(UNIT_VIRTUAL_ITEM_INFO + (slot * 2) + 1, info1);
}

void Creature::SetWalk(bool enable)
{
    if (enable)
        m_movementInfo.AddMovementFlag(MOVEFLAG_WALK_MODE);
    else
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_WALK_MODE);
    WorldPacket data(
        enable ? SMSG_SPLINE_MOVE_SET_WALK_MODE : SMSG_SPLINE_MOVE_SET_RUN_MODE,
        9);
    data << GetPackGUID();
    SendMessageToSet(&data, true);
}

void Creature::SetLevitate(bool enable)
{
    if (enable)
        m_movementInfo.AddMovementFlag(MOVEFLAG_LEVITATING);
    else
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_LEVITATING);
    SendHeartBeat();
    // TODO: there should be analogic opcode for 2.43
    // WorldPacket data(enable ? SMSG_SPLINE_MOVE_GRAVITY_DISABLE :
    // SMSG_SPLINE_MOVE_GRAVITY_ENABLE, 9);
    // data << GetPackGUID();
    // SendMessageToSet(&data, true);
}

void Creature::SetSwim(bool enable)
{
    if (enable)
        m_movementInfo.AddMovementFlag(MOVEFLAG_SWIMMING | MOVEFLAG_LEVITATING);
    else if (m_inhabitType & INHABIT_AIR)
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_SWIMMING);
    else
        m_movementInfo.RemoveMovementFlag(
            MOVEFLAG_SWIMMING | MOVEFLAG_LEVITATING);

    WorldPacket data(
        enable ? SMSG_SPLINE_MOVE_START_SWIM : SMSG_SPLINE_MOVE_STOP_SWIM);
    data << GetPackGUID();
    SendMessageToSet(&data, true);
}

void Creature::SetKeepTargetEmptyDueToCC(bool apply)
{
    if (!isAlive())
        return;

    if (apply)
    {
        SetTargetGuid(ObjectGuid());
    }
    else
    {
        // Check so we don't still have a CC effect on us (the calling CC effect
        // is already removed at this point)
        static const AuraType auras[4] = {SPELL_AURA_MOD_CONFUSE,
            SPELL_AURA_MOD_CHARM, SPELL_AURA_MOD_FEAR, SPELL_AURA_MOD_STUN};
        for (auto& aura : auras)
            if (HasAuraType(aura))
                return;

        if (getVictim())
            SetTargetGuid(getVictim()->GetObjectGuid());
    }
}

void Creature::SetAggroDistance(float dist)
{
    aggro_radius = dist;
}

bool Creature::GetSpawnPosition(float& X, float& Y, float& Z, float& O) const
{
    CreatureData const* data =
        sObjectMgr::Instance()->GetCreatureData(GetGUIDLow());
    if (!data)
        return false;

    X = data->posX;
    Y = data->posY;
    Z = data->posZ;
    O = data->orientation;
    return true;
}

void Creature::OnMapCreatureRelocation()
{
    remove_auras_if([](AuraHolder* h)
        {
            return h->GetSpellProto()->AuraInterruptFlags &
                   AURA_INTERRUPT_FLAG_MOVE;
        });

    // Update swim flags if need be
    if (m_inhabitType == INHABIT_WATER)
    {
        SetSwim(true);
    }
    else if (m_inhabitType & INHABIT_WATER)
    {
        bool inWater = IsUnderWater();
        if (m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) && !inWater)
            SetSwim(false);
        else if (!m_movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) && inWater)
            SetSwim(true);
    }
}

bool Creature::CanRespawn()
{
    if (!m_canRespawn && !m_forcedRespawn)
        return false;

    auto now = WorldTimer::time_no_syscall();
    if (m_respawnTime > now)
        return false;

    if (m_bossLink)
    {
        if (Creature* boss = GetMap()->GetCreature(m_bossLink))
        {
            if (boss->isInCombat())
            {
                m_respawnTime = now + 30;
                return false;
            }

            if (boss->isDead())
            {
                m_respawnTime = boss->GetRespawnTime();
                return false;
            }
        }
        // not returning false here
    }

    if (creature_group_)
    {
        int64_t max_respawntime = 0;
        for (auto& c : creature_group_->GetMembers())
        {
            if (c == this)
                continue;

            // Try again in 30 secs if group is in combat
            if (c->isInCombat())
            {
                m_respawnTime = now + 30;
                return false;
            }

            // If all of the group is dead, respawn all of it at once
            if (max_respawntime >= 0)
            {
                if (c->isAlive())
                {
                    // If some of the group is alive, we can respawn asap
                    max_respawntime = -1;
                }
                else
                {
                    auto time = c->GetRespawnTime();
                    if (max_respawntime < time)
                        max_respawntime = time;
                }
            }
        }

        if (max_respawntime > 0 && max_respawntime > now + 5)
        {
            m_respawnTime = max_respawntime;
            return false;
        }
    }

    return true;
}

void Creature::AddBossLinkedMob(ObjectGuid guid)
{
    auto itr = std::find(m_linkedMobs.begin(), m_linkedMobs.end(), guid);
    if (itr == m_linkedMobs.end())
        m_linkedMobs.push_back(guid);
}

void Creature::SetFocusTarget(Unit* unit)
{
    getThreatManager().setFocusTarget(unit);
}

void Creature::remove_auras_on_evade()
{
    remove_auras_if([](AuraHolder* holder)
        {
            auto i = holder->GetSpellProto();
            return !holder->IsPositive() ||
                   i->HasAttribute(SPELL_ATTR_CUSTOM1_CANCEL_ON_EVADE) ||
                   i->HasApplyAuraName(SPELL_AURA_MOD_STUN) ||
                   i->HasApplyAuraName(SPELL_AURA_MOD_CONFUSE) ||
                   i->HasApplyAuraName(SPELL_AURA_MOD_ROOT) ||
                   i->HasApplyAuraName(SPELL_AURA_MOD_FEAR);
        });
}

void Creature::OnEvadeActions(bool by_group)
{
    total_dmg_taken = 0;
    legit_dmg_taken = 0;
    player_dmg_taken = 0;

    if (AI())
        AI()->Pacify(false);

    if (InstanceData* mapInstance = GetInstanceData())
        mapInstance->OnCreatureEvade(this);

    // despawn any combat summons
    for (auto guid : combat_summons)
    {
        if (Creature* c = GetMap()->GetAnyTypeCreature(guid))
        {
            if (c->IsPet())
                static_cast<Pet*>(c)->Unsummon(PET_SAVE_NOT_IN_SLOT, this);
            else
                c->ForcedDespawn();
        }
    }
    combat_summons.clear();

    // invoke evade for group
    if (!by_group && GetGroup() != nullptr &&
        !GetGroup()->HasFlag(CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
    {
        GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
            GetGroup()->GetId(), CREATURE_GROUP_EVENT_EVADE, this);
    }

    // Evade all controlled units
    CallForAllControlledUnits(
        [](Unit* c)
        {
            if (c->GetTypeId() == TYPEID_UNIT &&
                static_cast<Creature*>(c)->AI())
                static_cast<Creature*>(c)->AI()->EnterEvadeMode();
        },
        CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

void Creature::SetFocusSpellTarget(Unit* target, const SpellEntry* spellInfo)
{
    if (IsAffectedByThreatIgnoringCC() || (AI() && AI()->IsPacified()))
    {
        m_castedAtTarget.Clear();
        m_focusSpellId = 0;
        return;
    }

    if (spellInfo &&
        spellInfo->HasAttribute(SPELL_ATTR_EX5_DONT_TARGET_WHILE_CASTING))
        return;

    if (target)
    {
        SetTargetGuid(target->GetObjectGuid());
        m_castedAtTarget = target->GetObjectGuid();
        if (spellInfo)
            m_focusSpellId = spellInfo->Id;
    }
    else
    {
        if (spellInfo && m_focusSpellId && m_focusSpellId != spellInfo->Id)
            return;

        if (getVictim())
            SetTargetGuid(getVictim()->GetObjectGuid());
        else
        {
            SetTargetGuid(ObjectGuid());
            if (!isInCombat() && movement_gens.top_id() == movement::gen::idle)
            {
                if (const CreatureData* data =
                        sObjectMgr::Instance()->GetCreatureData(GetGUIDLow()))
                    SetFacingTo(data->orientation);
            }
        }

        m_castedAtTarget.Clear();
        m_focusSpellId = 0;
    }
}

bool Creature::IsPlayerPet() const
{
    if (!IsPet())
        return false;

    // TODO: might want to have a boolean marking if our owner is a player or
    // not in Pet
    return GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr;
}

void Creature::AddInhabitType(InhabitTypeValues inhabit)
{
    m_inhabitType |= inhabit;
    if (inhabit & INHABIT_WATER && IsUnderWater())
        SetSwim(true);
    if (inhabit & INHABIT_AIR)
        SetLevitate(true);
}

void Creature::RemoveInhabitType(InhabitTypeValues inhabit)
{
    m_inhabitType = m_inhabitType & ~inhabit;
    if (inhabit & INHABIT_WATER)
        SetSwim(false);
    if (inhabit & INHABIT_AIR)
        SetLevitate(false);
}

void Creature::ResetInhabitType(uint32 inhabit)
{
    if (IsPet() && GetOwnerGuid().IsPlayer())
        m_inhabitType = INHABIT_GROUND | INHABIT_WATER;
    else if (inhabit)
        m_inhabitType = inhabit;
    else if (auto info = GetCreatureInfo())
        m_inhabitType = info->InhabitType;
}

bool Creature::MeetsQuestVisibility(Player* player) const
{
    /* Documentation of the quest visibility string:
     * Format:
     * "(identifier)(quest_id) [(identifier)(quest_id)...]"
     " or NULL to signify no quest visibility
     *
     * Identifier Explanation
     * @          If I have this quest, I might be able to see the creature
     * ~          If I can take the quest, I might be able to see the creature
     * =          If I have completed quest, I might be able to see creature
     * !          If I have completed this quest, I can never see the creature
     */

    const CreatureDataAddon* addon = GetCreatureAddon();
    if (!addon || !addon->quest_vis)
        return true;

    std::string str(addon->quest_vis);

    if (str.empty())
        return true;

    std::stringstream ss(str);

    bool can_see = false;

    while (true)
    {
        char identifier;
        ss >> identifier;

        if (!ss)
            break; // no more identifiers

        uint32 quest;
        ss >> quest;

        if (!ss)
        {
            logging.error(
                "QuestVisibility in creature_(template)_addon has a syntax "
                "error for creature %s",
                GetObjectGuid().GetString().c_str());
            return true;
        }

        switch (identifier)
        {
        case '@':
            if (player->IsCurrentQuest(quest, 0))
                can_see = true;
            break;
        case '~':
            // FIXME: Remove const_cast
            if (const Quest* q =
                    sObjectMgr::Instance()->GetQuestTemplate(quest))
                if (player->CanTakeQuest(q, false, const_cast<Creature*>(this)))
                    can_see = true;
            break;
        case '=':
            if (player->GetQuestRewardStatus(quest))
                can_see = true;
            break;
        case '!': // takes precedence
            if (player->GetQuestRewardStatus(quest))
                return false;
            break;
        }
    }

    return can_see;
}

uint8 Creature::expansion_level() const
{
    if (getLevel() > 63)
        return 1;

    // TBC Map ids. Not the cleanest of solutions.
    switch (GetMapId())
    {
    case 269: // CoT, Dark Portal
    case 530: // Outlands
    case 532: // Karazhan
    case 534: // CoT, Mount Hyjal
    case 540: // HF, Shattered Halls
    case 542: // HF, Blood Furnace
    case 543: // HF, Ramparts
    case 544: // HF, Magtheridon's Lair
    case 545: // CF, Steamvault
    case 546: // CF, Underbog
    case 547: // CF, Slave Pens
    case 548: // CF, Serpentshrine Cavern
    case 550: // TK, Tempest Keep
    case 552: // TK, Arcatraz
    case 553: // TK, Botanica
    case 554: // TK, Mechanar
    case 555: // Auch, Shadow Labyrinth
    case 556: // Auch, Sethekk Halls
    case 557: // Auch, Mana-Tombs
    case 558: // Auch, Auchenai Crypts
    case 560: // CoT, Durnholde
    case 564: // Black Temple
    case 565: // Gruul's Lair
    case 568: // Zul'Aman
    case 580: // Sunwell
    case 585: // Magister's Terrace
        return 1;
    }

    return 0;
}

void Creature::start_evade()
{
    in_evade_ = true;
    evade_timer_ = MOB_EVADE_FULL_RESET_TIMER;
    evade_tick_timer_ = MOB_EVADE_HP_TICK_TIMER;
}

void Creature::stop_evade()
{
    in_evade_ = false;
}

void Creature::update_evade(const uint32 diff)
{
    assert(in_evade_);

    // If threat manager goes empty while evading (i.e. target dies), go into
    // full evade
    if (evade_timer_ <= diff || getThreatManager().isThreatListEmpty())
    {
        in_evade_ = false;
        AI()->EnterEvadeMode();
        if (GetGroup() != nullptr &&
            !GetGroup()->HasFlag(CREATURE_GROUP_FLAG_MOVEMENT_NO_COMBAT))
        {
            GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(
                GetGroup()->GetId(), CREATURE_GROUP_EVENT_EVADE, this);
        }
    }
    else
        evade_timer_ -= diff;

    // NOTE: We can end up leaving evade mode in the code above
    if (in_evade_)
    {
        if (evade_tick_timer_ <= diff)
        {
            RegenerateHealth();
            evade_tick_timer_ = MOB_EVADE_HP_TICK_TIMER;
        }
        else
            evade_tick_timer_ -= diff;
    }
}

Team Creature::tapping_team() const
{
    Player* first;
    if (GetLootDistributor() &&
        (first = GetLootDistributor()->recipient_mgr()->first_valid_player()) !=
            nullptr)
        return first->GetTeam();
    return TEAM_NONE;
}

bool Creature::special_vis_mob() const
{
    return GetCreatureInfo()->special_visibility;
}

float Creature::special_vis_dist() const
{
    return GetCreatureInfo()->special_visibility;
}

void Creature::stealth_reaction(const Unit* target)
{
    // Aura 18950: Is the stealth detection eye aura; mobs with this aura do not
    // perform the stealth reaction
    if (movement_gens.top_id() == movement::gen::distract || has_aura(18950) ||
        !IsHostileTo(target))
        return;

    // Send "grunt"-like sound
    SendAIReaction(AI_REACTION_ALERT);

    // Do a distract movement towards target for a short period of time
    movement_gens.push(new movement::DistractMovementGenerator(4000),
        movement::EVENT_ENTER_COMBAT);
    if (GetGroup() != nullptr)
        GetMap()->GetCreatureGroupMgr().ProcessGroupEvent(GetGroup()->GetId(),
            CREATURE_GROUP_EVENT_MOVEMENT_PAUSE, nullptr, 4000);

    SetFacingTo(GetAngle(target->GetX(), target->GetY()));
}

void Creature::install_pet_behavior()
{
    if (m_AI_locked)
        throw std::runtime_error(
            "Creature::install_pet_behavior cannot be invoked during an "
            "AI-update.");

    // Keep current AI if pet_template has an entry that says PET_CFLAGS_USE_AI
    const pet_template* tmp = sPetTemplates::Instance()->get(GetEntry());
    if (!tmp || (tmp->ctemplate_flags & PET_CFLAGS_USE_AI) == 0)
    {
        delete i_AI;
        i_AI = nullptr;

        // Save home generator for restoring later on
        if (auto home = movement_gens.get(movement::gen::home))
            movement_gens.mod_priority(home, -10);

        // Pop movement gens from current AI
        movement_gens.remove_if([](const movement::Generator* gen)
            {
                return gen->id() == movement::gen::chase ||
                       gen->id() == movement::gen::stopped ||
                       gen->id() == movement::gen::point ||
                       gen->id() == movement::gen::follow;
            });

        i_AI = new pet_ai(this);
    }

    if (!pet_behavior_)
    {
        pet_behavior_ = new pet_behavior(this);
        pet_template_ =
            tmp ? tmp : sPetTemplates::Instance()->enslaved_template();
    }
}

void Creature::uninstall_pet_behavior()
{
    if (m_AI_locked)
        throw std::runtime_error(
            "Creature::uninstall_pet_behavior cannot be invoked during an "
            "AI-update.");

    bool reinit_ai = (pet_template_->ctemplate_flags & PET_CFLAGS_USE_AI) == 0;
    if (reinit_ai)
    {
        delete i_AI;
        i_AI = nullptr;
    }

    if (!IsPet() || !GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        delete pet_behavior_;
        pet_behavior_ = nullptr;
        pet_template_ = nullptr;
    }

    if (reinit_ai)
    {
        // Pop movement gens from current AI
        movement_gens.remove_if([](const movement::Generator* gen)
            {
                return gen->id() == movement::gen::chase ||
                       gen->id() == movement::gen::stopped ||
                       gen->id() == movement::gen::point ||
                       gen->id() == movement::gen::follow;
            });

        AIM_Initialize();
    }
}

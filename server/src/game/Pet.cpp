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

#include "Pet.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "SmartAI.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Transport.h"
#include "Unit.h"
#include "Util.h"
#include "WorldPacket.h"
#include "loot_distributor.h"
#include "pet_ai.h"
#include "pet_behavior.h"
#include "pet_template.h"
#include "Database/DatabaseEnv.h"

// numbers represent minutes * 100 while happy (you get 100 loyalty points per
// min while happy)
uint32 const LevelUpLoyalty[6] = {
    5500, 11500, 17000, 23500, 31000, 39500,
};

uint32 const LevelStartLoyalty[6] = {
    2000, 4500, 7000, 10000, 13500, 17500,
};

Pet::Pet(PetType type)
  : Creature(CREATURE_SUBTYPE_PET), SetHappiness(0), m_TrainingPoints(0),
    m_resetTalentsCost(0), m_resetTalentsTime(0), m_removed(false),
    m_happinessTimer(10000), m_loyaltyTimer(12000), m_petType(type),
    m_duration(0), m_loyaltyPoints(0), spell_bonus_(0), m_auraUpdateMask(0),
    m_loading(false), cinfo_(nullptr), m_declinedname(nullptr),
    m_petModeFlags(PET_MODE_DEFAULT)
{
    m_name = "Pet";
    m_regenTimer = 4000;

    // pets always have a charminfo, even if they are not actually charmed
    CharmInfo* charmInfo = InitCharmInfo(this);

    if (type == MINI_PET) // always passive
        charmInfo->SetReactState(REACT_PASSIVE);
    else if (type == GUARDIAN_PET) // always aggressive
        charmInfo->SetReactState(REACT_AGGRESSIVE);
}

Pet::~Pet()
{
    delete m_declinedname;
}

void Pet::AddToWorld()
{
    Unit::AddToWorld();
}

void Pet::RemoveFromWorld()
{
    ///- Don't call the function for Creature, normal mobs + totems go in a
    /// different storage
    if (auto owner = GetOwner())
    {
        switch (getPetType())
        {
        case MINI_PET:
            if (owner->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(owner)->_SetMiniPet(nullptr);
            break;
        case GUARDIAN_PET:
            owner->RemoveGuardian(this);
            break;
        default:
            if (owner->GetPetGuid() == GetObjectGuid())
                owner->SetPet(nullptr);
            break;
        }
    }
    Unit::RemoveFromWorld();
}

bool Pet::LoadPetFromDB(
    Player* owner, uint32 petentry, uint32 pet_guid, bool current)
{
    if (petentry)
    {
        if (!ShouldDBSavePet(petentry))
            return false;
    }

    m_loading = true;

    PetDbData* data = nullptr;

    // We know the pet's low guid
    if (pet_guid)
    {
        for (auto& d : owner->_pet_store)
            if (d.guid == pet_guid && !d.deleted)
            {
                data = &d;
                break;
            }
    }
    // We want to summon the current active pet
    else if (current)
    {
        for (auto& d : owner->_pet_store)
            if (d.slot == PET_SAVE_AS_CURRENT && !d.deleted)
            {
                data = &d;
                break;
            }
    }
    // We know the creature id of the pet (ignore stabled pets)
    else if (petentry)
    {
        for (auto& d : owner->_pet_store)
            if (d.id == petentry && (d.slot == PET_SAVE_AS_CURRENT ||
                                        d.slot > PET_SAVE_LAST_STABLE_SLOT) &&
                !d.deleted)
            {
                data = &d;
                break;
            }
    }
    // We just want something... (will get current or unsummoned; ignores
    // stabled pets; used for hunter's call pet for example)
    else
    {
        for (auto& d : owner->_pet_store)
            if ((d.slot == PET_SAVE_AS_CURRENT ||
                    d.slot > PET_SAVE_LAST_STABLE_SLOT) &&
                !d.deleted)
            {
                data = &d;
                break;
            }
    }

    if (!data)
        return false;

    petentry = data->id;

    const CreatureInfo* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        logging.error(
            "Pet entry %u does not exist but used at pet load (owner: %s).",
            petentry, owner->GetGuidStr().c_str());
        return false;
    }

    uint32 summon_spell_id = data->create_spell;
    const SpellEntry* spellInfo = sSpellStore.LookupEntry(summon_spell_id);

    bool is_temporary_summoned = spellInfo && GetSpellDuration(spellInfo) > 0;

    // check temporary summoned pets like mage water elemental
    if (current && is_temporary_summoned)
        return false;

    PetType pet_type = PetType(data->pet_type);
    if (pet_type == HUNTER_PET)
    {
        if (!creatureInfo->isTameable())
            return false;
    }

    if (current && owner->IsPetNeedBeTemporaryUnsummoned())
    {
        owner->SetTemporaryUnsummonedPetNumber(data->guid);
        return false;
    }

    Map* map = owner->GetMap();

    CreatureCreatePos pos(
        owner, owner->GetO(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    uint32 guid = pos.GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);
    if (!Create(guid, pos, creatureInfo, data->guid))
        return false;

    // Add pet to transport if owner is on a transport
    if (Transport* trans = owner->GetTransport())
    {
        trans->AddPassenger(this);

        // Fill in transport positions right away so pet won't look weird after
        // map load
        float x, y, z, o;
        owner->m_movementInfo.transport.pos.Get(x, y, z, o);
        m_movementInfo.transport.pos.Set(x, y, z, o);
    }

    setPetType(pet_type);
    setFaction(owner->getFaction());
    SetUInt32Value(UNIT_CREATED_BY_SPELL, summon_spell_id);

    // reget for sure use real creature info selected for Pet at load/creating
    const CreatureInfo* cinfo = GetCreatureInfo();
    if (cinfo->type == CREATURE_TYPE_CRITTER)
    {
        AIM_Initialize();
        pos.GetMap()->insert(this);
        return true;
    }

    m_charmInfo->SetPetNumber(data->guid, IsPermanentPetFor(owner));

    SetOwnerGuid(owner->GetObjectGuid());
    SetDisplayId(data->model_id);
    SetNativeDisplayId(data->model_id);
    uint32 petlevel = data->level;
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    SetName(data->name);

    switch (getPetType())
    {
    case SUMMON_PET:
        petlevel = owner->getLevel();
        break;
    case HUNTER_PET:
        // loyalty
        SetByteValue(UNIT_FIELD_BYTES_1, 1, data->loyalty);

        SetByteFlag(UNIT_FIELD_BYTES_2, 2,
            data->renamed ? UNIT_CAN_BE_ABANDONED :
                            UNIT_CAN_BE_RENAMED | UNIT_CAN_BE_ABANDONED);
        SetTP(data->training_points);
        SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
        SetPower(POWER_HAPPINESS, data->happiness);
        setPowerType(POWER_FOCUS);
        break;
    default:
        logging.error(
            "Pet have incorrect type (%u) for pet loading.", getPetType());
    }

    if (owner->IsPvP())
        SetPvP(true);

    InitStatsForLevel(petlevel);
    SetUInt32Value(
        UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(WorldTimer::time_no_syscall()));
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, data->exp);
    SetCreatorGuid(owner->GetObjectGuid());

    m_charmInfo->SetReactState(ReactStates(data->react_state));
    m_loyaltyPoints = data->loyalty_points;
    m_resetTalentsCost = data->reset_talents_cost;
    m_resetTalentsTime = data->reset_talents_time;

    uint32 savedhealth = data->health;
    uint32 savedmana = data->mana;

    if (!is_temporary_summoned)
    {
        // init teach spells
        Tokens tokens = StrSplit(data->teach_spells_raw, " ");
        Tokens::const_iterator iter;
        int index;
        for (iter = tokens.begin(), index = 0; index < 4; ++iter, ++index)
        {
            uint32 tmp = atol((*iter).c_str());

            ++iter;

            if (tmp)
                AddTeachSpell(tmp, atol((*iter).c_str()));
            else
                break;
        }
    }

    // since last save (in seconds)
    uint32 timediff = uint32(WorldTimer::time_no_syscall() - data->save_time);

    // load spells/cooldowns/auras
    _LoadAuras(data, timediff, map->IsBattleArena());

    // init AB
    if (is_temporary_summoned)
    {
        // Temporary summoned pets always have initial spell list at load
        auto action_bar = data->action_bar_raw;
        queue_action(0, [this, action_bar]()
            {
                InitPetCreateSpells();
                m_charmInfo->LoadPetActionBar(action_bar);
            });
    }
    else
    {
        LearnPetPassives();
        CastPetAuras(current);
    }

    if (getPetType() == SUMMON_PET && !current) // all (?) summon pets come with
                                                // full health when called, but
                                                // not when they are current
    {
        SetHealth(GetMaxHealth());
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));
    }
    else
    {
        SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
        SetPower(POWER_MANA, savedmana > GetMaxPower(POWER_MANA) ?
                                 GetMaxPower(POWER_MANA) :
                                 savedmana);
    }

    if (getPetType() == MINI_PET)
        pet_template_ = sPetTemplates::Instance()->get_minipet(petentry);
    else
        pet_template_ = sPetTemplates::Instance()->get(petentry);

    AIM_Initialize();
    map->insert(this);

    // Spells should be loaded after pet is added to map, because in CheckCast
    // is check on it
    _LoadSpells(data);

    // load pet action bar, needs to be done after _LoadSpells to permit
    // removing spells from the pet's action bar altogether
    // NOTE: for temporary summons done above
    if (!is_temporary_summoned)
        m_charmInfo->LoadPetActionBar(data->action_bar_raw);

    CleanupActionBar(); // remove unknown spells from action bar after load

    _LoadSpellCooldowns(data);

    owner->SetPet(this); // in DB stored only full controlled creature
    LOG_DEBUG(logging, "New Pet has guid %u", GetGUIDLow());

    if (owner->GetTypeId() == TYPEID_PLAYER)
    {
        auto owner_guid = owner->GetObjectGuid();
        queue_action(0, [this, owner_guid]()
            {
                if (auto player = GetMap()->GetPlayer(owner_guid))
                {
                    player->PetSpellInitialize();
                    // HACK: Voidstar Talisman
                    if (player->has_aura(37386))
                        player->CastSpell(player, 150045, true);
                }
            });
        if (((Player*)owner)->GetGroup())
            ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_PET);
    }

    if (owner->GetTypeId() == TYPEID_PLAYER && getPetType() == HUNTER_PET)
    {
        bool has_declined = false;
        for (auto& str : data->declined_name.name)
            if (!str.empty())
                has_declined = true;
        if (has_declined)
        {
            if (m_declinedname)
                delete m_declinedname;

            m_declinedname = new DeclinedName;
            for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
                m_declinedname->name[i] = data->declined_name.name[i];
        }
    }

    m_loading = false;

    SynchronizeLevelWithOwner();

    // Adds water if pet is owned by a player
    ResetInhabitType(INHABIT_GROUND);

    // Make this pet current; if current exists move that to not in slot
    if (data->slot != PET_SAVE_AS_CURRENT)
    {
        for (auto& mod : owner->_pet_store)
        {
            if (data == &mod)
                mod.slot = PET_SAVE_AS_CURRENT;
            else if (mod.slot == PET_SAVE_AS_CURRENT)
                mod.slot = PET_SAVE_NOT_IN_SLOT;
            else
                continue;
            mod.needs_save = true;
        }
    }

    return true;
}

bool Pet::ShouldDBSavePet(uint32 entry) const
{
    // FIXME: Only hunter pets, warlock pets & mage's water elemental should be
    // saved to the database (afaik). We need to verify that's working as
    // intended.
    switch (entry ? entry : GetEntry())
    {
    case 19668: // Shadowfiend
        return false;
    default:
        break;
    }

    return true;
}

void Pet::SavePetToDB(PetSaveMode mode)
{
    if (!GetEntry())
        return;

    // save only fully controlled creature
    if (!isControlled())
        return;

    // not save not player pets
    if (!GetOwnerGuid().IsPlayer())
        return;

    // not all pets should be saved to the database
    if (!ShouldDBSavePet())
        return;

    Player* pOwner = (Player*)GetOwner();
    if (!pOwner)
        return;

    // Find Player's PetDbData cache of *this
    PetDbData* data = nullptr;
    for (auto& d : pOwner->_pet_store)
    {
        if (d.guid == m_charmInfo->GetPetNumber())
        {
            data = &d;
            break;
        }
    }

    // No cache exists, create one
    if (!data)
    {
        pOwner->_pet_store.emplace_back();
        data = &pOwner->_pet_store.back();
        data->guid = m_charmInfo->GetPetNumber();
        data->id = GetEntry();
        data->owner_guid = pOwner->GetGUIDLow();
    }

    data->needs_save = true;

    // current/stable/not_in_slot
    if (mode >= PET_SAVE_AS_CURRENT)
    {
        // reagents must be returned before save call
        if (mode == PET_SAVE_REAGENTS)
            mode = PET_SAVE_NOT_IN_SLOT;
        // not save pet as current if another pet temporary unsummoned
        else if (mode == PET_SAVE_AS_CURRENT &&
                 pOwner->GetTemporaryUnsummonedPetNumber() &&
                 pOwner->GetTemporaryUnsummonedPetNumber() !=
                     m_charmInfo->GetPetNumber())
        {
            // pet will lost anyway at restore temporary unsummoned
            if (getPetType() == HUNTER_PET)
                return;

            // for warlock case
            mode = PET_SAVE_NOT_IN_SLOT;
        }

        uint32 curhealth = GetHealth();
        uint32 curmana = GetPower(POWER_MANA);

        // stable and not in slot saves
        if (mode != PET_SAVE_AS_CURRENT && mode != PET_SAVE_DISMISS_PET)
            remove_auras();
        if (mode == PET_SAVE_DISMISS_PET)
            mode = PET_SAVE_NOT_IN_SLOT;

        _SaveSpells(data);
        _SaveSpellCooldowns(data);
        _SaveAuras(data);

        // prevent duplicate using slot (except PET_SAVE_NOT_IN_SLOT)
        if (mode <= PET_SAVE_LAST_STABLE_SLOT)
        {
            for (auto& d : pOwner->_pet_store)
            {
                if (&d != data && d.slot == data->slot)
                {
                    d.slot = PET_SAVE_NOT_IN_SLOT;
                    d.needs_save = true;
                }
            }
        }

        // prevent existence another hunter pet in PET_SAVE_AS_CURRENT and
        // PET_SAVE_NOT_IN_SLOT
        if (getPetType() == HUNTER_PET &&
            (mode == PET_SAVE_AS_CURRENT || mode > PET_SAVE_LAST_STABLE_SLOT))
        {
            for (auto& d : pOwner->_pet_store)
            {
                if (&d != data && (d.slot == PET_SAVE_AS_CURRENT ||
                                      d.slot > PET_SAVE_LAST_STABLE_SLOT))
                {
                    d.deleted = true;
                }
            }
        }

        data->model_id = GetNativeDisplayId();
        data->level = getLevel();
        data->exp = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
        data->react_state = m_charmInfo->GetReactState();
        data->loyalty_points = m_loyaltyPoints;
        data->loyalty = GetLoyaltyLevel();
        data->training_points = m_TrainingPoints;
        data->slot = mode;
        data->name = m_name;
        data->renamed = uint32(
            HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ? 0 : 1);
        data->health = curhealth < 1 ? 1 : curhealth;
        data->mana = curmana;
        data->happiness = GetPower(POWER_HAPPINESS);

        std::ostringstream ss;
        for (uint32 i = ACTION_BAR_INDEX_START; i < ACTION_BAR_INDEX_END; ++i)
        {
            ss << uint32(m_charmInfo->GetActionBarEntry(i)->GetType()) << " "
               << uint32(m_charmInfo->GetActionBarEntry(i)->GetAction()) << " ";
        };
        data->action_bar_raw = ss.str();

        // Save spells the pet can teach to its Master
        ss.str("");
        ss.clear();
        {
            int i = 0;
            for (TeachSpellMap::const_iterator itr = m_teachspells.begin();
                 i < 4 && itr != m_teachspells.end(); ++i, ++itr)
                ss << itr->first << " " << itr->second << " ";
            for (; i < 4; ++i)
                ss << uint32(0) << " " << uint32(0) << " ";
        }
        data->teach_spells_raw = ss.str();

        data->save_time = WorldTimer::time_no_syscall();
        data->reset_talents_cost = m_resetTalentsCost;
        data->reset_talents_time = m_resetTalentsTime;
        data->create_spell = GetUInt32Value(UNIT_CREATED_BY_SPELL);
        data->pet_type = getPetType();
        data->dead = (m_deathState != ALIVE) ? true : false;

        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            if (m_declinedname)
                data->declined_name.name[i] = m_declinedname->name[i];
            else
                data->declined_name.name[i].clear();
        }
    }
    else
    {
        remove_auras(AURA_REMOVE_BY_DELETE);
        data->deleted = true;
    }
}

void Pet::SetDeathState(DeathState
        s) // overwrite virtual Creature::SetDeathState and Unit::SetDeathState
{
    Creature::SetDeathState(s);
    if (getDeathState() == CORPSE)
    {
        if (Unit* owner = GetOwner())
        {
            if (owner->GetTypeId() == TYPEID_PLAYER &&
                static_cast<Player*>(owner)->InBattleGround())
                static_cast<Player*>(owner)->SetBgResummonGuid(GetObjectGuid());
        }

        // pet corpse non lootable and non skinnable
        SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);

        // pets will be desummoned in Pet::Update
        // summoned pets fade on death after a few seconds
        if (getPetType() == SUMMON_PET)
        {
            // queueing a desummon action is easier than fiddling with corpse
            // decay, etc
            queue_action(3 * IN_MILLISECONDS, [this]()
                {
                    Unsummon(PET_SAVE_NOT_IN_SLOT);
                });
        }
        // other pets despawn more slowly (use the standard corpse delay)
        else
        {
            // lose happiness when died and not in BG/Arena
            const MapEntry* mapEntry = sMapStore.LookupEntry(GetMapId());
            if (!mapEntry || (mapEntry->map_type != MAP_ARENA &&
                                 mapEntry->map_type != MAP_BATTLEGROUND))
                ModifyPower(POWER_HAPPINESS, -HAPPINESS_LEVEL_SIZE);
        }
    }
    else if (getDeathState() == ALIVE)
    {
        if (auto owner = GetOwner())
        {
            if (owner->GetTypeId() == TYPEID_PLAYER)
                for (auto& data : static_cast<Player*>(owner)->_pet_store)
                    if (data.guid == m_charmInfo->GetPetNumber())
                        data.dead = false;
        }
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
        CastPetAuras(true);
    }
}

void Pet::Update(uint32 update_diff, uint32 diff)
{
    if (m_removed) // pet already removed, just wait in remove queue, no updates
        return;

    if (unlikely(has_queued_actions()))
        update_queued_actions(update_diff);

    switch (m_deathState)
    {
    case CORPSE:
    {
        if (m_corpseDecayTimer <= update_diff)
        {
            if (auto owner = GetOwner())
            {
                if (owner->GetTypeId() == TYPEID_PLAYER)
                    for (auto& data : static_cast<Player*>(owner)->_pet_store)
                        if (data.guid == m_charmInfo->GetPetNumber())
                            data.dead = true;
            }
            Unsummon(PET_SAVE_NOT_IN_SLOT);
            return;
        }
        break;
    }
    case ALIVE:
    {
        Unit* owner = GetOwner();
        if (!owner || !owner->isAlive())
        {
            Unsummon(getPetType() == HUNTER_PET ? PET_SAVE_AS_CURRENT :
                                                  PET_SAVE_REAGENTS);
            return;
        }

        //  Pet's base damage changes depending on happiness
        if (getPetType() == HUNTER_PET && GetHappinessState() != SetHappiness)
        {
            if (has_aura(8875))
            {
                SetHappiness = GetHappinessState();
                remove_auras(8875);
                CastSpell(this, 8875, true);
            }
        }

        if (isControlled())
        {
            if (owner->GetPetGuid() != GetObjectGuid())
            {
                Unsummon(getPetType() == HUNTER_PET ? PET_SAVE_AS_DELETED :
                                                      PET_SAVE_NOT_IN_SLOT,
                    owner);
                return;
            }
        }

        if (m_duration > 0)
        {
            if (m_duration > (int32)update_diff)
                m_duration -= (int32)update_diff;
            else
            {
                Unsummon(getPetType() != SUMMON_PET ? PET_SAVE_AS_DELETED :
                                                      PET_SAVE_NOT_IN_SLOT,
                    owner);
                return;
            }
        }
        break;
    }
    default:
        break;
    }

    Creature::Update(update_diff, diff);
}

void Pet::RegenerateAll(uint32 update_diff)
{
    // regenerate focus
    if (m_regenTimer <= update_diff)
    {
        if (getPetType() == HUNTER_PET)
            RegenerateFocus();

        if (!isInCombat() || IsPolymorphed())
            RegenerateHealth();

        RegenerateMana();

        m_regenTimer = 4000;
    }
    else
        m_regenTimer -= update_diff;

    if (getPetType() != HUNTER_PET)
        return;

    if (m_happinessTimer <= update_diff)
    {
        LooseHappiness();
        m_happinessTimer = 10000;
    }
    else
        m_happinessTimer -= update_diff;

    if (m_loyaltyTimer <= update_diff)
    {
        TickLoyaltyChange();
        m_loyaltyTimer = 12000;
    }
    else
        m_loyaltyTimer -= update_diff;
}

void Pet::RegenerateFocus()
{
    uint32 curValue = GetPower(POWER_FOCUS);
    uint32 maxValue = GetMaxPower(POWER_FOCUS);

    if (curValue >= maxValue)
        return;

    float addvalue =
        24 * sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_POWER_FOCUS);

    // Apply modifiers (if any).
    const Auras& modPowerRegenPct =
        GetAurasByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (const auto& elem : modPowerRegenPct)
        if ((elem)->GetModifier()->m_miscvalue == int32(POWER_FOCUS))
            addvalue *= ((elem)->GetModifier()->m_amount + 100) / 100.0f;

    ModifyPower(POWER_FOCUS, (int32)addvalue);
}

void Pet::LooseHappiness()
{
    uint32 curValue = GetPower(POWER_HAPPINESS);
    if (curValue <= 0)
        return;
    // Measured references: loyalty 6, 500 happiness per hour; loyalty 2 1000
    // happiness per hour
    int32 addvalue =
        1250 - 125 * GetLoyaltyLevel(); // range: 1125/1000/875/750/625/500
                                        // happiness lost per hour
    addvalue *= 1000;                   // happiness is * 1000
    addvalue /= 360;                    // ticks every 10 seconds
    ModifyPower(POWER_HAPPINESS, -addvalue);
}

void Pet::ModifyLoyalty(int32 addvalue)
{
    uint32 loyaltylevel = GetLoyaltyLevel();

    if (addvalue > 0) // only gain influenced, not loss
        addvalue =
            int32((float)addvalue *
                  sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_LOYALTY));

    if (loyaltylevel >= BEST_FRIEND &&
        (addvalue + m_loyaltyPoints) > int32(GetMaxLoyaltyPoints(loyaltylevel)))
        return;

    m_loyaltyPoints += addvalue;

    if (m_loyaltyPoints < 0)
    {
        if (loyaltylevel > REBELLIOUS)
        {
            // level down
            --loyaltylevel;
            SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
            m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
            SetTP(m_TrainingPoints - int32(getLevel()));
        }
        else
        {
            m_loyaltyPoints = 0;
            Unit* owner = GetOwner();
            if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            {
                WorldPacket data(SMSG_PET_BROKEN, 0);
                ((Player*)owner)->GetSession()->send_packet(std::move(data));

                // run away
                Unsummon(PET_SAVE_AS_DELETED, owner);
            }
        }
    }
    // level up
    else if (m_loyaltyPoints > int32(GetMaxLoyaltyPoints(loyaltylevel)))
    {
        ++loyaltylevel;
        SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
        m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
        SetTP(m_TrainingPoints + getLevel());
    }
}

void Pet::TickLoyaltyChange()
{
    int32 addvalue;

    switch (GetHappinessState())
    {
    case HAPPY:
        addvalue = 20;
        break;
    case CONTENT:
        addvalue = 10;
        break;
    case UNHAPPY:
        addvalue = -20;
        break;
    default:
        return;
    }
    ModifyLoyalty(addvalue);
}

void Pet::KillLoyaltyBonus(uint32 level)
{
    if (level > 100)
        return;

    // at lower levels gain is faster | the lower loyalty the more loyalty is
    // gained
    uint32 bonus = uint32(((100 - level) / 10) + (6 - GetLoyaltyLevel()));
    ModifyLoyalty(bonus);
}

HappinessState Pet::GetHappinessState()
{
    if (GetPower(POWER_HAPPINESS) < HAPPINESS_LEVEL_SIZE)
        return UNHAPPY;
    else if (GetPower(POWER_HAPPINESS) >= HAPPINESS_LEVEL_SIZE * 2)
        return HAPPY;
    else
        return CONTENT;
}

void Pet::SetLoyaltyLevel(LoyaltyLevel level)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 1, level);
}

bool Pet::CanTakeMoreActiveSpells(uint32 spellid)
{
    uint8 activecount = 1;
    uint32 chainstartstore[ACTIVE_SPELLS_MAX];

    if (IsPassiveSpell(spellid))
        return true;

    chainstartstore[0] = sSpellMgr::Instance()->GetFirstSpellInChain(spellid);

    for (PetSpellMap::const_iterator itr = m_spells.begin();
         itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
            continue;

        if (IsPassiveSpell(itr->first))
            continue;

        uint32 chainstart =
            sSpellMgr::Instance()->GetFirstSpellInChain(itr->first);

        uint8 x;

        for (x = 0; x < activecount; x++)
        {
            if (chainstart == chainstartstore[x])
                break;
        }

        if (x == activecount) // spellchain not yet saved -> add active count
        {
            ++activecount;
            if (activecount > ACTIVE_SPELLS_MAX)
                return false;
            chainstartstore[x] = chainstart;
        }
    }
    return true;
}

bool Pet::HasTPForSpell(uint32 spellid)
{
    int32 neededtrainp = GetTPForSpell(spellid);
    if ((m_TrainingPoints - neededtrainp < 0 || neededtrainp < 0) &&
        neededtrainp != 0)
        return false;
    return true;
}

int32 Pet::GetTPForSpell(uint32 spellid)
{
    uint32 basetrainp = 0;

    SkillLineAbilityMapBounds bounds =
        sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(spellid);
    for (auto _spell_idx = bounds.first; _spell_idx != bounds.second;
         ++_spell_idx)
    {
        if (!_spell_idx->second->reqtrainpoints)
            return 0;

        basetrainp = _spell_idx->second->reqtrainpoints;
        break;
    }

    uint32 spenttrainp = 0;
    uint32 chainstart = sSpellMgr::Instance()->GetFirstSpellInChain(spellid);

    for (auto& elem : m_spells)
    {
        if (elem.second.state == PETSPELL_REMOVED)
            continue;

        if (sSpellMgr::Instance()->GetFirstSpellInChain(elem.first) ==
            chainstart)
        {
            SkillLineAbilityMapBounds _bounds =
                sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(elem.first);

            for (auto _spell_idx2 = _bounds.first;
                 _spell_idx2 != _bounds.second; ++_spell_idx2)
            {
                if (_spell_idx2->second->reqtrainpoints > spenttrainp)
                {
                    spenttrainp = _spell_idx2->second->reqtrainpoints;
                    break;
                }
            }
        }
    }

    return int32(basetrainp) - int32(spenttrainp);
}

uint32 Pet::GetMaxLoyaltyPoints(uint32 level)
{
    if (level < 1)
        level = 1; // prevent SIGSEGV (out of range)
    if (level > 7)
        level = 7; // prevent SIGSEGV (out of range)
    return LevelUpLoyalty[level - 1];
}

uint32 Pet::GetStartLoyaltyPoints(uint32 level)
{
    if (level < 1)
        level = 1; // prevent SIGSEGV (out of range)
    if (level > 7)
        level = 7; // prevent SIGSEGV (out of range)
    return LevelStartLoyalty[level - 1];
}

void Pet::SetTP(int32 TP)
{
    m_TrainingPoints = TP;
    SetUInt32Value(UNIT_TRAINING_POINTS, (uint32)GetDispTP());
}

int32 Pet::GetDispTP()
{
    if (getPetType() != HUNTER_PET)
        return (0);
    if (m_TrainingPoints < 0)
        return -m_TrainingPoints;
    else
        return -(m_TrainingPoints + 1);
}

void Pet::Unsummon(PetSaveMode mode, Unit* owner /*= NULL*/)
{
    if (!owner)
        owner = GetOwner();

    CombatStop();

    if (owner)
    {
        assert(GetOwnerGuid() == owner->GetObjectGuid());

        // reset infinity cooldown when pet expires
        if (const SpellEntry* info =
                sSpellStore.LookupEntry(GetUInt32Value(UNIT_CREATED_BY_SPELL)))
        {
            if (info->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE) &&
                !info->HasEffect(SPELL_EFFECT_APPLY_AURA) &&
                owner->GetTypeId() == TYPEID_PLAYER)
                static_cast<Player*>(owner)->SendCooldownEvent(info);
        }

        Player* p_owner =
            owner->GetTypeId() == TYPEID_PLAYER ? (Player*)owner : nullptr;

        if (p_owner)
        {
            // not save secondary permanent pet as current
            if (mode == PET_SAVE_AS_CURRENT &&
                p_owner->GetTemporaryUnsummonedPetNumber() &&
                p_owner->GetTemporaryUnsummonedPetNumber() !=
                    GetCharmInfo()->GetPetNumber())
                mode = PET_SAVE_NOT_IN_SLOT;

            if (mode == PET_SAVE_REAGENTS)
            {
                // returning of reagents only for players, so best done here
                uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL);
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

                if (spellInfo)
                {
                    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                    {
                        if (spellInfo->Reagent[i] > 0)
                        {
                            inventory::transaction trans;
                            trans.add(spellInfo->Reagent[i],
                                spellInfo->ReagentCount[i]);
                            p_owner->storage().finalize(trans);
                            // XXX: p_owner->SendNewItem(item,
                            // spellInfo->ReagentCount[i], true, false);
                        }
                    }
                }
            }

            if (isControlled())
            {
                p_owner->RemovePetActionBar();

                if (p_owner->GetGroup())
                    p_owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);
            }
        }

        // only if current pet in slot
        switch (getPetType())
        {
        case MINI_PET:
            if (p_owner)
                p_owner->_SetMiniPet(nullptr);
            break;
        case GUARDIAN_PET:
            owner->RemoveGuardian(this);
            break;
        default:
            if (owner->GetPetGuid() == GetObjectGuid())
                owner->SetPet(nullptr);
            break;
        }
    }

    // Remove from transport
    if (Transport* trans = GetTransport())
        trans->RemovePassenger(this);

    SavePetToDB(mode);
    AddObjectToRemoveList();
    m_removed = true;
}

void Pet::GivePetXP(uint32 xp)
{
    if (getPetType() != HUNTER_PET)
        return;

    if (xp < 1)
        return;

    if (!isAlive())
        return;

    uint32 level = getLevel();
    uint32 maxlevel =
        std::min(sWorld::Instance()->getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL),
            GetOwner()->getLevel());

    // pet not receive xp for level equal to owner level
    if (level >= maxlevel)
        return;

    uint32 nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    uint32 curXP = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
    uint32 newXP = curXP + xp;

    while (newXP >= nextLvlXP && level < maxlevel)
    {
        newXP -= nextLvlXP;
        ++level;

        GivePetLevel(level); // also update UNIT_FIELD_PETNEXTLEVELEXP and
                             // UNIT_FIELD_PETEXPERIENCE to level start

        nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    }

    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, level < maxlevel ? newXP : 0);

    if (getPetType() == HUNTER_PET)
        KillLoyaltyBonus(level);
}

void Pet::GivePetLevel(uint32 level)
{
    if (!level || level == getLevel())
        return;

    if (getPetType() == HUNTER_PET)
    {
        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP,
            sObjectMgr::Instance()->GetXPForPetLevel(level));
    }

    InitStatsForLevel(level);
    SetTP(m_TrainingPoints + (GetLoyaltyLevel() - 1));
}

bool Pet::CreateBaseAtCreature(Creature* creature)
{
    if (!creature)
    {
        logging.error(
            "CRITICAL: NULL pointer passed into CreateBaseAtCreature()");
        return false;
    }

    CreatureCreatePos pos(creature, creature->GetO());

    uint32 guid = creature->GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);

    LOG_DEBUG(logging, "Create pet");
    uint32 pet_number = sObjectMgr::Instance()->GeneratePetNumber();
    if (!Create(guid, pos, creature->GetCreatureInfo(), pet_number))
        return false;

    CreatureInfo const* cinfo = GetCreatureInfo();
    if (!cinfo)
    {
        logging.error(
            "CreateBaseAtCreature() failed, creatureInfo is missing!");
        return false;
    }
    cinfo_ = cinfo;

    if (cinfo->type == CREATURE_TYPE_CRITTER)
    {
        setPetType(MINI_PET);
        return true;
    }
    SetDisplayId(creature->GetDisplayId());
    SetNativeDisplayId(creature->GetNativeDisplayId());
    SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
    SetPower(POWER_HAPPINESS, 166500);
    setPowerType(POWER_FOCUS);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP,
        sObjectMgr::Instance()->GetXPForPetLevel(creature->getLevel()));
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    if (CreatureFamilyEntry const* cFamily =
            sCreatureFamilyStore.LookupEntry(cinfo->family))
        SetName(cFamily->Name[sWorld::Instance()->GetDefaultDbcLocale()]);
    else
        SetName(creature->GetNameForLocaleIdx(
            sObjectMgr::Instance()->GetDBCLocaleIndex()));

    m_loyaltyPoints = 1000;
    if (cinfo->type == CREATURE_TYPE_BEAST)
    {
        SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
        SetByteValue(UNIT_FIELD_BYTES_0, 2, GENDER_NONE);
        SetByteValue(UNIT_FIELD_BYTES_0, 3, POWER_FOCUS);
        SetSheath(SHEATH_STATE_MELEE);
        SetByteValue(UNIT_FIELD_BYTES_2, 1,
            UNIT_BYTE2_FLAG_SANCTUARY | UNIT_BYTE2_FLAG_POSITIVE_AURAS);
        SetByteFlag(
            UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED | UNIT_CAN_BE_ABANDONED);

        SetUInt32Value(
            UNIT_MOD_CAST_SPEED, creature->GetUInt32Value(UNIT_MOD_CAST_SPEED));
        SetLoyaltyLevel(REBELLIOUS);
    }
    return true;
}

void Pet::InitGuardianStatsFromOwner(Unit* owner)
{
    // Some pets are summoned by totems and should have "real" owners
    Unit* real_owner = (owner->GetTypeId() == TYPEID_UNIT &&
                           static_cast<Creature*>(owner)->IsTotem()) ?
                           owner->GetOwner() :
                           owner;
    if (!real_owner)
        real_owner = owner;

    switch (m_originalEntry)
    {
    case 15438: // Greater Fire Elemental
    {
        SetCreateHealth(
            GetCreateHealth() + real_owner->GetStat(STAT_STAMINA) * 10 * 0.35f);
        SetCreateMana(
            GetCreateMana() + real_owner->GetStat(STAT_INTELLECT) * 10 * 0.35f);
        // Research showed that 35% (the same as stat scaling) of master's spell
        // power should affect the fire elemental
        float dmgIncrease =
            real_owner->GetUInt32Value(
                PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE) *
            0.35f;
        spell_bonus(int32(dmgIncrease));
        break;
    }
    case 15352: // Earth Elemental
    {
        SetCreateHealth(
            GetCreateHealth() + real_owner->GetStat(STAT_STAMINA) * 10 * 0.35f);
        // Earth elemental seems to have the _slightest_ of scaling from attack
        // power, around 2-4%, as well as patch 3.1.0
        // saying it no longer scales with attack power but rather spell power,
        // which indicates there was some scaling
        float dmgIncrease =
            real_owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.04f;
        spell_bonus(int32(dmgIncrease));
        break;
    }
    default:
        break;
    }
}

bool Pet::InitStatsForLevel(uint32 petlevel, Unit* owner)
{
    SetCanModifyStats(true);

    CreatureInfo const* cinfo = GetCreatureInfo();
    assert(cinfo);

    if (!owner)
    {
        owner = GetOwner();
        if (!owner)
        {
            logging.error(
                "attempt to summon pet (Entry %u) without owner! Attempt "
                "terminated.",
                cinfo->Entry);
            return false;
        }
    }

    uint32 creature_ID = (getPetType() == HUNTER_PET) ? 1 : cinfo->Entry;

    switch (getPetType())
    {
    case SUMMON_PET:
        SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_MAGE);

        // this enables popup window (pet dismiss, cancel)
        if (owner->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr)
            SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
        break;
    case HUNTER_PET:
        SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_WARRIOR);
        SetByteValue(UNIT_FIELD_BYTES_0, 2, GENDER_NONE);
        SetSheath(SHEATH_STATE_MELEE);
        SetByteValue(UNIT_FIELD_BYTES_2, 1,
            UNIT_BYTE2_FLAG_SANCTUARY | UNIT_BYTE2_FLAG_POSITIVE_AURAS);

        // this enables popup window (pet abandon, cancel)
        if (owner->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr)
            SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
        break;
    default:
        break;
    }

    SetLevel(petlevel);

    SetMeleeDamageSchool(SpellSchools(cinfo->dmgschool));

    SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, float(petlevel * 50));

    SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
    SetAttackTime(OFF_ATTACK, BASE_ATTACK_TIME);
    SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);

    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0);

    CreatureFamilyEntry const* cFamily =
        sCreatureFamilyStore.LookupEntry(cinfo->family);
    if (cFamily && cFamily->minScale > 0.0f && getPetType() == HUNTER_PET)
    {
        float scale;
        if (getLevel() >= cFamily->maxScaleLevel)
            scale = cFamily->maxScale;
        else if (getLevel() <= cFamily->minScaleLevel)
            scale = cFamily->minScale;
        else
            scale = cFamily->minScale +
                    float(getLevel() - cFamily->minScaleLevel) /
                        cFamily->maxScaleLevel *
                        (cFamily->maxScale - cFamily->minScale);

        // FIXME: For some reason pets are always a lot smaller, even if you put
        // them to the same scale as the
        // mob they're modelled off. This is obviously not the way to solve it,
        // but it makes it look good enough.
        scale *= 1.5f;
        if (scale > 1.0f)
            scale = 1.0f;

        SetObjectScale(scale);
        UpdateModelData();
    }

    int32 createResistance[MAX_SPELL_SCHOOL] = {0, 0, 0, 0, 0, 0, 0};

    if (getPetType() != HUNTER_PET)
    {
        createResistance[SPELL_SCHOOL_HOLY] = cinfo->resistance1;
        createResistance[SPELL_SCHOOL_FIRE] = cinfo->resistance2;
        createResistance[SPELL_SCHOOL_NATURE] = cinfo->resistance3;
        createResistance[SPELL_SCHOOL_FROST] = cinfo->resistance4;
        createResistance[SPELL_SCHOOL_SHADOW] = cinfo->resistance5;
        createResistance[SPELL_SCHOOL_ARCANE] = cinfo->resistance6;
    }

    switch (getPetType())
    {
    case SUMMON_PET:
    {
        SetBaseWeaponDamage(
            BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
        SetBaseWeaponDamage(
            BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));

        PetLevelInfo const* pInfo =
            sObjectMgr::Instance()->GetPetLevelInfo(creature_ID, petlevel);
        if (pInfo) // level dependant values exist in DB
        {
            SetCreateHealth(pInfo->health);
            SetCreateMana(pInfo->mana);

            if (pInfo->armor > 0)
                SetModifierValue(
                    UNIT_MOD_ARMOR, BASE_VALUE, float(pInfo->armor));

            for (int stat = 0; stat < MAX_STATS; ++stat)
            {
                SetCreateStat(Stats(stat), float(pInfo->stats[stat]));
            }
        }
        else // level dependent values do not exist in DB, use creature_template
             // data
        {
            SetCreateHealth(cinfo->maxhealth);
            SetCreateMana(cinfo->maxmana);

            // If no level dependent stats exist, we use cinfo level
            uint32 level = urand(std::min(cinfo->minlevel, cinfo->maxlevel),
                std::max(cinfo->minlevel, cinfo->maxlevel));
            if (level > 0)
                SetLevel(level);
            else
                logging.error(
                    "Summoned pet with entry %u has invalid level data in "
                    "creature_template.",
                    cinfo->Entry);

            // We also use cinfo damage if no level dependent data exists
            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(cinfo->mindmg));
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(cinfo->maxdmg));
            SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, float(cinfo->mindmg));
            SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, float(cinfo->maxdmg));

            // FIXME: Creatures too should have stats, not just pets, this needs
            // a general fix though
            for (int stat = 0; stat < MAX_STATS; ++stat)
                SetCreateStat(Stats(stat), 0);
            /*SetCreateStat(STAT_STRENGTH, 22);
            SetCreateStat(STAT_AGILITY, 22);
            SetCreateStat(STAT_STAMINA, 25);
            SetCreateStat(STAT_INTELLECT, 28);
            SetCreateStat(STAT_SPIRIT, 27);*/
        }
        break;
    }
    case HUNTER_PET:
    {
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP,
            sObjectMgr::Instance()->GetXPForPetLevel(petlevel));
        // these formula may not be correct; however, it is designed to be close
        // to what it should be
        // this makes dps 0.5 of pets level
        SetBaseWeaponDamage(
            BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
        // damage range is then petlevel / 2
        SetBaseWeaponDamage(
            BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));
        // damage is increased afterwards as strength and pet scaling modify
        // attack power

        // stored standard pet stats are entry 1 in pet_levelinfo
        PetLevelInfo const* pInfo =
            sObjectMgr::Instance()->GetPetLevelInfo(creature_ID, petlevel);
        if (pInfo) // exist in DB
        {
            SetCreateHealth(pInfo->health);
            SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, float(pInfo->armor));

            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            {
                SetCreateStat(Stats(i), float(pInfo->stats[i]));
            }
        }
        else // not exist in DB, use some default fake data
        {
            logging.error("Hunter pet levelstats missing in DB");

            // remove elite bonuses included in DB values
            SetCreateHealth(
                uint32(((float(cinfo->maxhealth) / cinfo->maxlevel) /
                           (1 + 2 * cinfo->rank)) *
                       petlevel));

            SetCreateStat(STAT_STRENGTH, 22);
            SetCreateStat(STAT_AGILITY, 22);
            SetCreateStat(STAT_STAMINA, 25);
            SetCreateStat(STAT_INTELLECT, 28);
            SetCreateStat(STAT_SPIRIT, 27);
        }
        break;
    }
    case GUARDIAN_PET:
        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);

        // Use creature_template data for guardians
        SetCreateHealth(cinfo->maxhealth);
        SetCreateMana(cinfo->maxmana);

        SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, float(cinfo->armor));

        SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(cinfo->mindmg));
        SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(cinfo->maxdmg));
        // Some guardian pets have an off-hand weapon
        SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, float(cinfo->mindmg));
        SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, float(cinfo->maxdmg));

        // Some guardians have damage and stats modified by owner's stats
        if (owner)
            InitGuardianStatsFromOwner(owner);

        break;
    default:
        logging.error(
            "Pet have incorrect type (%u) for levelup.", getPetType());
        break;
    }

    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE,
            float(createResistance[i]));

    UpdateAllStats();

    SetHealth(GetMaxHealth());
    SetPower(POWER_MANA, GetMaxPower(POWER_MANA));

    // Always use creature_template.unit_flags for NPC owned pets
    if (owner->GetTypeId() == TYPEID_UNIT)
    {
        if (auto info = GetCreatureInfo())
            SetFlag(UNIT_FIELD_FLAGS, info->unit_flags);
    }

    init_pet_template_data();

    return true;
}

bool Pet::HaveInDiet(ItemPrototype const* item) const
{
    if (!item->FoodType)
        return false;

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
        return false;

    CreatureFamilyEntry const* cFamily =
        sCreatureFamilyStore.LookupEntry(cInfo->family);
    if (!cFamily)
        return false;

    uint32 diet = cFamily->petFoodMask;
    uint32 FoodMask = 1 << (item->FoodType - 1);
    return diet & FoodMask;
}

uint32 Pet::GetCurrentFoodBenefitLevel(uint32 itemlevel)
{
    // -5 or greater food level
    if (getLevel() <= itemlevel + 5) // possible to feed level 60 pet with level
                                     // 55 level food for full effect
        return 35000;
    // -10..-6
    else if (getLevel() <= itemlevel + 10) // pure guess, but sounds good
        return 17000;
    // -14..-11
    else if (getLevel() <=
             itemlevel +
                 14) // level 55 food gets green on 70, makes sense to me
        return 8000;
    // -15 or less
    else
        return 0; // food too low level
}

void Pet::_LoadSpellCooldowns(const PetDbData* data)
{
    m_CreatureSpellCooldowns.clear();
    m_CreatureCategoryCooldowns.clear();

    time_t curTime = WorldTimer::time_no_syscall();

    WorldPacket packet(SMSG_SPELL_COOLDOWN,
        (8 + 1 + size_t(data->spell_cooldowns.size()) * 8));
    packet << ObjectGuid(GetObjectGuid());
    packet << uint8(0x0); // flags (0x1, 0x2)

    for (auto& cd : data->spell_cooldowns)
    {
        uint32 spell_id = cd.id;
        time_t db_time = cd.time;

        if (!sSpellStore.LookupEntry(spell_id))
        {
            logging.error(
                "Pet %u have unknown spell %u in `pet_spell_cooldown`, "
                "skipping.",
                m_charmInfo->GetPetNumber(), spell_id);
            continue;
        }

        // skip outdated cooldown
        if (db_time <= curTime)
            continue;

        packet << uint32(spell_id);
        packet << uint32(uint32(db_time - curTime) * IN_MILLISECONDS);

        _AddCreatureSpellCooldown(spell_id, db_time);

        LOG_DEBUG(logging,
            "Pet (Number: %u) spell %u cooldown loaded (%u secs).",
            m_charmInfo->GetPetNumber(), spell_id, uint32(db_time - curTime));
    }

    if (!m_CreatureSpellCooldowns.empty() && GetOwner())
    {
        ((Player*)GetOwner())->GetSession()->send_packet(std::move(packet));
    }
}

void Pet::_SaveSpellCooldowns(PetDbData* data)
{
    time_t curTime = WorldTimer::time_no_syscall();

    // remove oudated and save active
    data->spell_cooldowns.clear();
    for (auto itr = m_CreatureSpellCooldowns.begin();
         itr != m_CreatureSpellCooldowns.end();)
    {
        if (itr->second <= curTime ||
            itr->first == 33395) // Don't save mage's freeze
            m_CreatureSpellCooldowns.erase(itr++);
        else
        {
            PetDbData::SpellCooldown cd;
            cd.id = itr->first;
            cd.time = itr->second;
            data->spell_cooldowns.push_back(std::move(cd));
            ++itr;
        }
    }
}

void Pet::_LoadSpells(const PetDbData* data)
{
    for (const auto& spell : data->spells)
        addSpell(spell.id, spell.active, PETSPELL_UNCHANGED);
}

void Pet::_SaveSpells(PetDbData* data)
{
    data->spells.clear();
    for (auto itr = m_spells.begin(), next = m_spells.begin();
         itr != m_spells.end(); itr = next)
    {
        ++next;

        // prevent saving family passives to DB
        if (itr->second.type == PETSPELL_FAMILY)
            continue;

        switch (itr->second.state)
        {
        case PETSPELL_REMOVED:
            m_spells.erase(itr);
            continue;
        default:
            break;
        }

        itr->second.state = PETSPELL_UNCHANGED;
        PetDbData::Spell spell;
        spell.id = itr->first;
        spell.active = ActiveStates(itr->second.active);
        data->spells.push_back(std::move(spell));
    }
}

void Pet::_LoadAuras(const PetDbData* data, uint32 timediff, bool only_passive)
{
    remove_auras();

    // all aura related fields
    for (int i = UNIT_FIELD_AURA; i <= UNIT_FIELD_AURASTATE; ++i)
        SetUInt32Value(i, 0);

    for (auto& aura : data->auras)
    {
        ObjectGuid casterGuid = ObjectGuid(aura.caster_guid);
        uint32 item_lowguid = aura.item_guid;
        uint32 spellid = aura.spell_id;
        uint32 stackcount = aura.stacks;
        uint32 remaincharges = aura.charges;
        int32 damage[MAX_EFFECT_INDEX];
        uint32 periodicTime[MAX_EFFECT_INDEX];

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            damage[i] = aura.bp[i];
            periodicTime[i] = aura.periodic_time[i];
        }

        int32 maxduration = aura.max_duration;
        int32 remaintime = aura.duration;
        uint32 effIndexMask = aura.eff_mask;

        SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
        if (!spellproto)
        {
            logging.error("Unknown spell (spellid %u), ignore.", spellid);
            continue;
        }

        if (only_passive && !IsPassiveSpell(spellproto))
            continue;

        if (remaintime != -1 && !IsPositiveSpell(spellproto))
        {
            if (remaintime / IN_MILLISECONDS <= int32(timediff))
                continue;

            remaintime -= timediff * IN_MILLISECONDS;
        }

        // prevent wrong values of remaincharges
        if (spellproto->procCharges == 0)
            remaincharges = 0;

        if (!spellproto->StackAmount)
            stackcount = 1;
        else if (spellproto->StackAmount < stackcount)
            stackcount = spellproto->StackAmount;
        else if (!stackcount)
            stackcount = 1;

        AuraHolder* holder = CreateAuraHolder(spellproto, this, nullptr);
        holder->SetLoadedState(casterGuid,
            ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges,
            maxduration, remaintime);

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if ((effIndexMask & (1 << i)) == 0)
                continue;

            Aura* aura = CreateAura(
                spellproto, SpellEffectIndex(i), nullptr, holder, this);
            if (!damage[i])
                damage[i] = aura->GetModifier()->m_amount;

            aura->SetLoadedState(damage[i], periodicTime[i]);
            holder->AddAura(aura, SpellEffectIndex(i));
        }

        if (!holder->IsEmptyHolder())
            AddAuraHolder(holder);
        else
            delete holder;
    }
}

void Pet::_SaveAuras(PetDbData* data)
{
    data->auras.clear();

    std::vector<AuraHolder*> holders;
    loop_auras([&holders](AuraHolder* holder)
        {
            holders.push_back(holder);
            return true;
        });

    if (holders.empty())
        return;

    for (auto holder : holders)
    {
        bool save = true;
        for (int32 j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEntry const* spellInfo = holder->GetSpellProto();
            if (spellInfo->EffectApplyAuraName[j] == SPELL_AURA_MOD_STEALTH ||
                spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_OWNER ||
                spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_PET)
            {
                save = false;
                break;
            }
        }

        if (holder->GetSpellProto()->HasAttribute(
                SPELL_ATTR_CUSTOM_DONT_SAVE_AURA))
            save = false;

        // skip all holders from spells that are passive or channeled
        // do not save single target holders (unless they were cast by the
        // player)
        if (save && !holder->IsPassive() &&
            !IsChanneledSpell(holder->GetSpellProto()))
        {
            int32 damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAura(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (aur->IsAreaAura() &&
                        holder->GetCasterGuid() != GetObjectGuid())
                        continue;

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
                continue;

            PetDbData::Aura aura;

            aura.caster_guid = holder->GetCasterGuid().GetRawValue();
            aura.item_guid = holder->GetCastItemGuid().GetCounter();
            aura.spell_id = holder->GetId();
            aura.stacks = holder->GetStackAmount();
            aura.charges = holder->GetAuraCharges();

            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                aura.bp[i] = damage[i];
                aura.periodic_time[i] = periodicTime[i];
            }

            aura.max_duration = holder->GetAuraMaxDuration();
            aura.duration = holder->GetAuraDuration();
            aura.eff_mask = effIndexMask;

            data->auras.push_back(std::move(aura));
        }
    }
}

bool Pet::addSpell(uint32 spell_id, ActiveStates active /*= ACT_DECIDE*/,
    PetSpellState state /*= PETSPELL_NEW*/,
    PetSpellType type /*= PETSPELL_NORMAL*/)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        // do pet spell book cleanup
        if (state == PETSPELL_UNCHANGED) // spell load case
        {
            logging.error(
                "Pet::addSpell: nonexistent in SpellStore spell #%u request, "
                "deleting for all pets in `pet_spell`.",
                spell_id);
            CharacterDatabase.PExecute(
                "DELETE FROM pet_spell WHERE spell = '%u'", spell_id);
        }
        else
            logging.error(
                "Pet::addSpell: nonexistent in SpellStore spell #%u request.",
                spell_id);

        return false;
    }

    auto itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            m_spells.erase(itr);
            state = PETSPELL_CHANGED;
        }
        else if (state == PETSPELL_UNCHANGED &&
                 itr->second.state != PETSPELL_UNCHANGED)
        {
            // can be in case spell loading but learned at some previous spell
            // loading
            itr->second.state = PETSPELL_UNCHANGED;

            if (active == ACT_ENABLED)
            {
                m_charmInfo->SetSpellAutocast(spell_id, true);
                ToggleAutocast(spell_id, true);
            }
            else if (active == ACT_DISABLED)
            {
                m_charmInfo->SetSpellAutocast(spell_id, false);
                ToggleAutocast(spell_id, false);
            }

            return false;
        }
        else
            return false;
    }

    PetSpell newspell;
    newspell.state = state;
    newspell.type = type;

    // Pasive or non-autocastable spells cannot be auto-casted
    if (spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_AUTOCASTABLE ||
        spellInfo->Attributes & SPELL_ATTR_PASSIVE)
        newspell.active = ACT_PASSIVE;
    // For other spells we use the last state, or if none exist, disable the
    // auto-casting
    else
    {
        if (active == ACT_DECIDE)           // No previous state exists?
            newspell.active = ACT_DISABLED; // Disabled means the spell can be
                                            // auto-casted but currently isn't
        else
            newspell.active = active;
    }

    if (sSpellMgr::Instance()->GetSpellRank(spell_id) != 0)
    {
        for (PetSpellMap::const_iterator itr2 = m_spells.begin();
             itr2 != m_spells.end(); ++itr2)
        {
            if (itr2->second.state == PETSPELL_REMOVED)
                continue;

            if (sSpellMgr::Instance()->IsRankSpellDueToSpell(
                    spellInfo, itr2->first))
            {
                // replace by new high rank
                if (sSpellMgr::Instance()->IsHighRankOfSpell(
                        spell_id, itr2->first))
                {
                    newspell.active = itr2->second.active;

                    if (newspell.active == ACT_ENABLED)
                        ToggleAutocast(itr2->first, false);

                    unlearnSpell(itr2->first, false, false);
                    break;
                }
                // ignore new lesser rank
                else if (sSpellMgr::Instance()->IsHighRankOfSpell(
                             itr2->first, spell_id))
                    return false;
            }
        }
    }

    m_spells[spell_id] = newspell;

    if (spellInfo->Attributes & SPELL_ATTR_PASSIVE)
        CastSpell(this, spell_id, true);
    else
        m_charmInfo->AddSpellToActionBar(
            spell_id, ActiveStates(newspell.active));

    if (newspell.active == ACT_ENABLED)
        ToggleAutocast(spell_id, true);

    return true;
}

bool Pet::learnSpell(uint32 spell_id)
{
    // prevent duplicated entires in spell book
    if (!addSpell(spell_id))
        return false;

    if (!m_loading)
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            ((Player*)owner)->PetSpellInitialize();
    }
    return true;
}

bool Pet::unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    if (removeSpell(spell_id, learn_prev, clear_ab))
        return true;
    return false;
}

bool Pet::removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    auto itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return false;

    if (itr->second.state == PETSPELL_REMOVED)
        return false;

    if (itr->second.state == PETSPELL_NEW)
        m_spells.erase(itr);
    else
        itr->second.state = PETSPELL_REMOVED;

    remove_auras(spell_id);

    if (learn_prev)
    {
        if (uint32 prev_id =
                sSpellMgr::Instance()->GetPrevSpellInChain(spell_id))
            learnSpell(prev_id);
        else
            learn_prev = false;
    }

    // if remove last rank or non-ranked then update action bar at server and
    // client if need
    if (clear_ab && !learn_prev &&
        m_charmInfo->RemoveSpellFromActionBar(spell_id))
    {
        if (!m_loading)
        {
            // need update action bar for last removed rank
            if (Unit* owner = GetOwner())
                if (owner->GetTypeId() == TYPEID_PLAYER)
                    ((Player*)owner)->PetSpellInitialize();
        }
    }

    return true;
}

void Pet::CleanupActionBar()
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
        if (UnitActionBarEntry const* ab = m_charmInfo->GetActionBarEntry(i))
            if (uint32 action = ab->GetAction())
                if (ab->IsActionBarForSpell() && !HasSpell(action))
                    m_charmInfo->SetActionBar(i, 0, ACT_DISABLED);
}

void Pet::InitPetCreateSpells()
{
    m_charmInfo->InitPetActionBar();
    m_spells.clear();

    int32 usedtrainpoints = 0;

    uint32 petspellid;
    PetCreateSpellEntry const* CreateSpells =
        sObjectMgr::Instance()->GetPetCreateSpellEntry(GetEntry());
    if (CreateSpells)
    {
        Unit* owner = GetOwner();
        Player* p_owner = owner && owner->GetTypeId() == TYPEID_PLAYER ?
                              (Player*)owner :
                              nullptr;

        for (uint8 i = 0; i < 4; ++i)
        {
            if (!CreateSpells->spellid[i])
                continue;

            SpellEntry const* learn_spellproto =
                sSpellStore.LookupEntry(CreateSpells->spellid[i]);
            if (!learn_spellproto)
                continue;

            if (learn_spellproto->Effect[0] == SPELL_EFFECT_LEARN_SPELL ||
                learn_spellproto->Effect[0] == SPELL_EFFECT_LEARN_PET_SPELL)
            {
                petspellid = learn_spellproto->EffectTriggerSpell[0];
                if (p_owner && !p_owner->HasSpell(learn_spellproto->Id))
                {
                    if (IsPassiveSpell(petspellid)) // learn passive skills when
                                                    // tamed, not sure if thats
                                                    // right
                        p_owner->learnSpell(learn_spellproto->Id, false);
                    else
                        AddTeachSpell(learn_spellproto->EffectTriggerSpell[0],
                            learn_spellproto->Id);
                }
            }
            else
                petspellid = learn_spellproto->Id;

            addSpell(petspellid, CreateSpells->auto_cast[i] == true ?
                                     ACT_ENABLED :
                                     ACT_DISABLED);

            SkillLineAbilityMapBounds bounds =
                sSpellMgr::Instance()->GetSkillLineAbilityMapBounds(
                    learn_spellproto->EffectTriggerSpell[0]);

            for (auto _spell_idx = bounds.first; _spell_idx != bounds.second;
                 ++_spell_idx)
            {
                usedtrainpoints += _spell_idx->second->reqtrainpoints;
                break;
            }
        }
    }

    LearnPetPassives();

    CastPetAuras(false);

    SetTP(-usedtrainpoints);
}

void Pet::CheckLearning(uint32 spellid)
{
    // charmed case -> prevent crash
    if (GetTypeId() == TYPEID_PLAYER || getPetType() != HUNTER_PET)
        return;

    Unit* owner = GetOwner();

    if (m_teachspells.empty() || !owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    auto itr = m_teachspells.find(spellid);
    if (itr == m_teachspells.end())
        return;

    if (urand(0, 100) < 10)
    {
        ((Player*)owner)->learnSpell(itr->second, false);
        m_teachspells.erase(itr);
    }
}

uint32 Pet::resetTalentsCost() const
{
    uint32 days =
        uint32(WorldTimer::time_no_syscall() - m_resetTalentsTime) / DAY;

    // The first time reset costs 10 silver; after 1 day cost is reset to 10
    // silver
    if (m_resetTalentsCost < 10 * SILVER || days > 0)
        return 10 * SILVER;
    // then 50 silver
    else if (m_resetTalentsCost < 50 * SILVER)
        return 50 * SILVER;
    // then 1 gold
    else if (m_resetTalentsCost < 1 * GOLD)
        return 1 * GOLD;
    // then increasing at a rate of 1 gold; cap 10 gold
    else
        return (m_resetTalentsCost + 1 * GOLD > 10 * GOLD ?
                    10 * GOLD :
                    m_resetTalentsCost + 1 * GOLD);
}

void Pet::ToggleAutocast(uint32 spellid, bool apply)
{
    const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
        return;

    if (spellInfo->Attributes & SPELL_ATTR_PASSIVE ||
        spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_AUTOCASTABLE)
        return;

    auto itr = m_spells.find(spellid);

    uint32 i;

    if (apply)
    {
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i)
            ; // just search

        if (i == m_autospells.size())
        {
            m_autospells.push_back(spellid);

            if (itr->second.active != ACT_ENABLED)
            {
                itr->second.active = ACT_ENABLED;
                if (itr->second.state != PETSPELL_NEW)
                    itr->second.state = PETSPELL_CHANGED;
            }
        }
    }
    else
    {
        auto itr2 = m_autospells.begin();
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid;
             ++i, itr2++)
            ; // just search

        if (i < m_autospells.size())
        {
            m_autospells.erase(itr2);
            if (itr->second.active != ACT_DISABLED)
            {
                itr->second.active = ACT_DISABLED;
                if (itr->second.state != PETSPELL_NEW)
                    itr->second.state = PETSPELL_CHANGED;
            }
        }
    }
}

bool Pet::IsPermanentPetFor(Player* owner) const
{
    switch (getPetType())
    {
    case SUMMON_PET:
        switch (owner->getClass())
        {
        // oddly enough, Mage's Water Elemental is still treated as temporary
        // pet with Glyph of Eternal Water
        // i.e. does not unsummon at mounting, gets dismissed at teleport etc.
        case CLASS_WARLOCK:
            return GetCreatureInfo()->type == CREATURE_TYPE_DEMON;
        default:
            return false;
        }
    case HUNTER_PET:
        return true;
    default:
        return false;
    }
}

bool Pet::Create(uint32 guidlow, CreatureCreatePos& cPos,
    CreatureInfo const* cinfo, uint32 pet_number)
{
    cinfo_ = cinfo;

    SetMap(cPos.GetMap());

    Object::_Create(guidlow, pet_number, HIGHGUID_PET);

    if (getPetType() == MINI_PET)
        pet_template_ = sPetTemplates::Instance()->get_minipet(cinfo->Entry);
    else
        pet_template_ = sPetTemplates::Instance()->get(cinfo->Entry);

    m_originalEntry = cinfo->Entry;

    if (!InitEntry(cinfo->Entry))
        return false;

    cPos.AddBoundingRadius(GetObjectBoundingRadius());

    cPos.SelectFinalPoint(this);

    if (!cPos.Relocate(this))
        return false;

    SetSheath(SHEATH_STATE_MELEE);
    SetByteValue(UNIT_FIELD_BYTES_2, 1,
        UNIT_BYTE2_FLAG_SANCTUARY | UNIT_BYTE2_FLAG_POSITIVE_AURAS);

    if (getPetType() == MINI_PET) // always non-attackable
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

    pet_template_->apply_spell_immunity(this);

    if (pet_template_->pet_flags & PET_FLAGS_SPAWN_AGGRESSIVE)
        m_charmInfo->SetReactState(REACT_AGGRESSIVE);
    else if (pet_template_->pet_flags & PET_FLAGS_SPAWN_PASSIVE)
        m_charmInfo->SetReactState(REACT_PASSIVE);
    else
        m_charmInfo->SetReactState(REACT_DEFENSIVE);

    bool hasLoot = cinfo_->maxgold > 0 || cinfo_->lootid;
    if (hasLoot && pet_template_->ctemplate_flags & PET_CLFAGS_ALLOW_LOOTING)
        m_lootDistributor = new loot_distributor(this, LOOT_CORPSE);

    return true;
}

bool Pet::HasSpell(uint32 spell) const
{
    auto itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PETSPELL_REMOVED);
}

// Get all passive spells in our skill line
void Pet::LearnPetPassives()
{
    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
        return;

    CreatureFamilyEntry const* cFamily =
        sCreatureFamilyStore.LookupEntry(cInfo->family);
    if (!cFamily)
        return;

    PetFamilySpellsStore::const_iterator petStore =
        sPetFamilySpellsStore.find(cFamily->ID);
    if (petStore != sPetFamilySpellsStore.end())
    {
        for (const auto& elem : petStore->second)
            addSpell(elem, ACT_DECIDE, PETSPELL_NEW, PETSPELL_FAMILY);
    }
}

void Pet::CastPetAuras(bool current)
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    if (!IsPermanentPetFor((Player*)owner))
        return;

    for (auto itr = owner->m_petAuras.begin(); itr != owner->m_petAuras.end();)
    {
        PetAura const* pa = *itr;
        ++itr;

        if (!current && pa->IsRemovedOnChangePet())
            owner->RemovePetAura(pa);
        else
            CastPetAura(pa);
    }
}

void Pet::CastPetAura(PetAura const* aura)
{
    uint32 auraId = aura->GetAura(GetEntry());
    if (!auraId)
        return;

    if (auraId == 35696) // Demonic Knowledge
    {
        // FIXME: When putting additional points in this talent, spell power
        // would not update
        //        This is because AreaAura is triggered, and as such will be
        //        overwritten and applied
        //        instantly, so the owner of the pet's AreaAura::Update never
        //        has time to remove the
        //        spell and it won't realize it's been replaced since it's the
        //        exact same id and caster.
        //        We forcefully remove it here to cause correct behavior
        if (Unit* owner = GetOwner())
            owner->remove_auras(auraId);

        int32 basePoints =
            int32(aura->GetDamage() *
                  (GetStat(STAT_STAMINA) + GetStat(STAT_INTELLECT)) / 100);
        CastCustomSpell(this, auraId, &basePoints, nullptr, nullptr, true);
    }
    else
        CastSpell(this, auraId, true);
}

void Pet::SynchronizeLevelWithOwner()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    switch (getPetType())
    {
    // always same level
    case SUMMON_PET:
        GivePetLevel(owner->getLevel());
        break;
    // can't be greater owner level
    case HUNTER_PET:
        if (getLevel() > owner->getLevel())
            GivePetLevel(owner->getLevel());
        break;
    default:
        break;
    }
}

void Pet::ApplyModeFlags(PetModeFlags mode, bool apply)
{
    if (apply)
        m_petModeFlags = PetModeFlags(m_petModeFlags | mode);
    else
        m_petModeFlags = PetModeFlags(m_petModeFlags & ~mode);

    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_MODE, 12);
    data << GetObjectGuid();
    data << uint32(m_petModeFlags);
    ((Player*)owner)->GetSession()->send_packet(std::move(data));
}

bool Pet::AIM_Initialize()
{
    if (m_AI_locked)
        return false;

    delete i_AI;
    i_AI = nullptr;

    // install pet behavior for player owned pets
    if (GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr)
    {
        pet_behavior_ = new pet_behavior(this);

        if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_AI)
        {
            if (strcmp(cinfo_->AIName, "SmartAI") == 0)
            {
                i_AI = new SmartAI(this);
                i_AI->InitializeAI();
            }
            else if (GetScriptId() != 0)
            {
                i_AI = sScriptMgr::Instance()->GetCreatureAI(this);
            }
        }

        if (!i_AI) // Default pet AI
            i_AI = new pet_ai(this);
    }
    else
    {
        i_AI = make_ai_for(this);
        i_AI->InitializeAI();
    }

    movement_gens.reset();

    return true;
}

void Pet::init_pet_template_data()
{
    // Most pets have no ctemplate_flags, so a preemptive return condition is
    // useful
    if (pet_template_->ctemplate_flags == 0)
        return;

    const CreatureInfo* info = GetCreatureInfo();

    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_HEALTH)
    {
        uint32 health = urand(std::min(info->minhealth, info->maxhealth),
            std::max(info->minhealth, info->maxhealth));
        SetCreateHealth(health);
        SetMaxHealth(health);
        SetHealth(health);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_MANA)
    {
        uint32 mana = urand(std::min(info->minmana, info->maxmana),
            std::max(info->minmana, info->maxmana));
        SetCreateMana(mana);
        SetMaxPower(POWER_MANA, mana);
        SetPower(POWER_MANA, mana);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_ARMOR)
    {
        SetArmor(info->armor);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_FACTION)
    {
        setFaction(info->faction_A);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_DAMAGE)
    {
        SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, info->mindmg);
        SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, info->maxdmg);
        // FIXME: Set attack power once we pull Barroth's patch in
        spell_bonus(0);
    }
    if (pet_template_->ctemplate_flags & PET_CLFAGS_USE_DAMAGE_SCHOOL)
    {
        SetMeleeDamageSchool(static_cast<SpellSchools>(info->dmgschool));
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_UNIT_FLAGS)
    {
        RemoveFlag(
            UNIT_FIELD_FLAGS, 0xFFFFFFFF); // Clear out all previous flags
        SetFlag(UNIT_FIELD_FLAGS, info->unit_flags);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_LEVEL)
    {
        uint32 level = urand(std::min(info->minlevel, info->maxlevel),
            std::max(info->minlevel, info->maxlevel));
        SetLevel(level);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_RESISTANCE)
    {
        SetResistance(SPELL_SCHOOL_HOLY, info->resistance1);
        SetResistance(SPELL_SCHOOL_FIRE, info->resistance2);
        SetResistance(SPELL_SCHOOL_NATURE, info->resistance3);
        SetResistance(SPELL_SCHOOL_FROST, info->resistance4);
        SetResistance(SPELL_SCHOOL_SHADOW, info->resistance5);
        SetResistance(SPELL_SCHOOL_ARCANE, info->resistance6);
    }
    if (pet_template_->ctemplate_flags & PET_CFLAGS_USE_ATTACK_SPEED)
    {
        // TODO: Does not support dual wielding atm
        SetAttackTime(BASE_ATTACK, info->baseattacktime);
        SetAttackTime(OFF_ATTACK, info->baseattacktime);
        SetAttackTime(RANGED_ATTACK, info->rangeattacktime);
    }
}

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

#include "GameObject.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"
#include "GossipDef.h"
#include "InstanceData.h"
#include "LootMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PoolManager.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SpecialVisCreature.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SmartAI.h"
#include "TransportMgr.h"
#include "UpdateMask.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "loot_distributor.h"
#include "Database/DatabaseEnv.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "vmap/DynamicTree.h"
#include "vmap/GameObjectModel.h"
#include "maps/checks.h"
#include "maps/visitors.h"

GameObject::GameObject()
  : WorldObject(), m_model(nullptr), m_goInfo(nullptr), m_goValue(),
    m_displayInfo(nullptr), m_AI(nullptr)
{
    m_objectType |= TYPEMASK_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;
    // 2.3.2 - 0x58
    m_updateFlag =
        (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = GAMEOBJECT_END;
    m_respawnTime = 0;
    m_respawnDelayTime = 25;
    m_temporary = false;
    m_lootState = GO_READY;
    m_spawnedByDefault = true;
    m_useTimes = 0;
    m_spellId = 0;
    m_cooldownTime = 0;
    m_ritualStarted = false;
    m_ritualTime = 0;

    m_AI = nullptr;

    m_captureTimer = 0;
    m_captureSlider = 0.0f;
    m_captureState = CAPTURE_STATE_NEUTRAL;
    despawn_time_ = 0;
}

GameObject::~GameObject()
{
    delete m_model;
    delete m_AI;
}

void GameObject::AddToWorld()
{
    ///- Register the gameobject for guid lookup
    if (!IsInWorld())
    {
        Object::AddToWorld();

        if (m_model)
            GetMap()->InsertModel(*m_model);

        UpdateCollision();
    }
}

void GameObject::RemoveFromWorld()
{
    ///- Remove the gameobject from the accessor
    if (IsInWorld())
    {
        // Notify the outdoor pvp script
        UpdateZoneAreaCache();
        if (OutdoorPvP* outdoorPvP =
                sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
            outdoorPvP->HandleGameObjectRemove(this);

        // Remove GO from owner
        if (ObjectGuid owner_guid = GetOwnerGuid())
        {
            if (Unit* owner = ObjectAccessor::GetUnit(*this, owner_guid))
                owner->RemoveGameObject(this, false);
            else
            {
                logging.error(
                    "Delete %s with SpellId %u LinkedGO %u that lost "
                    "references to owner %s GO list. Crash possible later.",
                    GetGuidStr().c_str(), m_spellId,
                    GetGOInfo()->GetLinkedGameObjectEntry(),
                    owner_guid.GetString().c_str());
            }
        }

        if (m_model)
            if (GetMap()->ContainsModel(*m_model))
                GetMap()->EraseModel(*m_model);

        Object::RemoveFromWorld();
    }
}

bool GameObject::Create(uint32 guidlow, uint32 name_id, Map* map, float x,
    float y, float z, float ang, float rotation0, float rotation1,
    float rotation2, float rotation3, uint32 animprogress, GOState go_state)
{
    assert(map);
    Relocate(x, y, z);
    SetOrientation(ang);
    SetMap(map);

    if (!IsPositionValid())
    {
        logging.error(
            "Gameobject (GUID: %u Entry: %u ) not created. Suggested "
            "coordinates are invalid (X: %f Y: %f)",
            guidlow, name_id, x, y);
        return false;
    }

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(name_id);
    if (!goinfo)
    {
        logging.error(
            "Gameobject (GUID: %u) not created: Entry %u does not exist in "
            "`gameobject_template`. Map: %u  (X: %f Y: %f Z: %f) ang: %f "
            "rotation0: %f rotation1: %f rotation2: %f rotation3: %f",
            guidlow, name_id, map->GetId(), x, y, z, ang, rotation0, rotation1,
            rotation2, rotation3);
        return false;
    }

    Object::_Create(guidlow, goinfo->id, HIGHGUID_GAMEOBJECT);

    m_goInfo = goinfo;

    if (goinfo->type >= MAX_GAMEOBJECT_TYPE)
    {
        logging.error(
            "Gameobject (GUID: %u) not created: Entry %u has invalid type %u "
            "in `gameobject_template`. It may crash client if created.",
            guidlow, name_id, goinfo->type);
        return false;
    }

    SetObjectScale(goinfo->size);

    SetFloatValue(GAMEOBJECT_POS_X, x);
    SetFloatValue(GAMEOBJECT_POS_Y, y);
    SetFloatValue(GAMEOBJECT_POS_Z, z);

    SetFloatValue(GAMEOBJECT_ROTATION + 0, rotation0);
    SetFloatValue(GAMEOBJECT_ROTATION + 1, rotation1);

    UpdateRotationFields(
        rotation2, rotation3); // GAMEOBJECT_FACING, GAMEOBJECT_ROTATION+2/3

    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);

    if (goinfo->type == GAMEOBJECT_TYPE_TRANSPORT)
        SetFlag(GAMEOBJECT_FLAGS, (GO_FLAG_TRANSPORT | GO_FLAG_NODESPAWN));

    SetEntry(goinfo->id);
    SetDisplayId(goinfo->displayId);

    if (goinfo->vmap)
        m_model = GameObjectModel::Create(*this);
    SetGoType(GameobjectTypes(goinfo->type));
    SetGoState(go_state);

    SetGoAnimProgress(animprogress);

    // Initialize Traps and Fishingnode delayed in ::Update
    if (GetGoType() == GAMEOBJECT_TYPE_TRAP ||
        GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
        m_lootState = GO_NOT_READY;

    // Update zone/area cache before call to outdoor pvp scripts for correct
    // zone id
    UpdateZoneAreaCache();

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that
    // map.
    // Normally non-players do not teleport to other maps.
    if (InstanceData* iData = map->GetInstanceData())
        iData->OnObjectCreate(this);

    // Notify Outdoor PvP scripts
    if (OutdoorPvP* outdoorPvP =
            sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
        outdoorPvP->HandleGameObjectCreate(this);

    switch (goinfo->type)
    {
    case GAMEOBJECT_TYPE_TRANSPORT:
        SetUInt32Value(GAMEOBJECT_LEVEL, goinfo->transport.pause);
        SetGoState(
            goinfo->transport.startOpen ? GO_STATE_ACTIVE : GO_STATE_READY);
        SetGoAnimProgress(animprogress);
        m_goValue.Transport.PathProgress = 0;
        m_goValue.Transport.AnimationInfo =
            sTransportMgr::Instance()->GetTransportAnimInfo(goinfo->id);
        m_goValue.Transport.CurrentSeg = 0;
        break;
    }

    AIM_Initialize();

    return true;
}

static bool hunter_trap_check(
    const WorldObject* hunter, const WorldObject* trap, float range, Unit* u)
{
    if (hunter->GetTypeId() == TYPEID_PLAYER &&
        (hunter->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) ||
            u->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_PLAYER_ATTACKABLE)))
        return false;

    Player* owner = u->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (owner && owner->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
        return false;

    if (hunter->GetTypeId() == TYPEID_PLAYER && owner &&
        static_cast<const Player*>(hunter)->ShouldIgnoreTargetBecauseOfPvpFlag(
            owner))
        return false;

    if (!u->isAlive())
        return false;

    if (u->GetTypeId() == TYPEID_UNIT &&
        u->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
        return false;

    if (u->GetTypeId() == TYPEID_UNIT &&
        u->GetCreatureType() == CREATURE_TYPE_CRITTER)
        return false;

    if (u->GetTypeId() == TYPEID_UNIT && static_cast<Creature*>(u)->IsTotem())
        return false;

    if (!u->isTargetableForAttack(false))
        return false;

    return trap->IsWithinDistInMap(u, range) && !hunter->IsFriendlyTo(u);
}

void GameObject::Update(uint32 update_diff, uint32 p_time)
{
    // Update Queued Actions
    if (unlikely(has_queued_actions()))
        update_queued_actions(update_diff);

    if (!m_AI)
    {
        if (!AIM_Initialize())
            logging.error("Could not initialize GameObjectAI");
    }
    else
        m_AI->UpdateAI(update_diff);

    if (GetObjectGuid().IsMOTransport())
    {
        //((Transport*)this)->Update(p_time);
        return;
    }

    // Game Objects with loot despawn a while after they're used (if loot is
    // left in)
    if (despawn_time_ && WorldTimer::time_no_syscall() > despawn_time_)
    {
        SetLootState(GO_JUST_DEACTIVATED);
        despawn_time_ = 0;
        return;
    }

    switch (m_lootState)
    {
    case GO_NOT_READY:
    {
        switch (GetGoType())
        {
        case GAMEOBJECT_TYPE_TRAP: // Initialized delayed to be able to use
                                   // GetOwner()
            {
                // Arming Time for GAMEOBJECT_TYPE_TRAP (6)
                m_cooldownTime = WorldTimer::time_no_syscall() +
                                 GetGOInfo()->trap.startDelay;
                m_lootState = GO_READY;
                break;
            }
        case GAMEOBJECT_TYPE_FISHINGNODE: // Keep not ready for some delay
        {
            // fishing code (bobber ready)
            if (WorldTimer::time_no_syscall() >
                m_respawnTime - FISHING_BOBBER_READY_TIME)
            {
                m_lootState =
                    GO_READY; // can be successfully open with some chance
                // splash bobber (bobber ready now)
                Unit* caster = GetOwner();
                if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                {
                    SetGoState(GO_STATE_ACTIVE);
                    // SetUInt32Value(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);

                    SendForcedObjectUpdate();

                    SendGameObjectCustomAnim(GetObjectGuid());
                }
            }
            break;
        }
        default:
        {
            // Other game objects don't apply and shouldn't cause warnings
            break;
        }
        }
        break;
    }
    case GO_READY:
    {
        if (m_respawnTime > 0) // timer on
        {
            if (m_respawnTime <= WorldTimer::time_no_syscall()) // timer expired
            {
                m_respawnTime = 0;
                ClearAllUsesData();

                switch (GetGoType())
                {
                case GAMEOBJECT_TYPE_FISHINGNODE: // can't fish now
                {
                    Unit* caster = GetOwner();
                    if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        caster->FinishSpell(CURRENT_CHANNELED_SPELL);

                        WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
                        static_cast<Player*>(caster)->GetSession()->send_packet(
                            std::move(data));
                    }
                    // can be deleted
                    m_lootState = GO_JUST_DEACTIVATED;
                    return;
                }
                case GAMEOBJECT_TYPE_DOOR:
                case GAMEOBJECT_TYPE_BUTTON:
                    // we need to open doors if they are closed (add there
                    // another condition if this code breaks some usage, but it
                    // need to be here for battlegrounds)
                    if (GetGoState() != GO_STATE_READY)
                        ResetDoorOrButton();
                // flags in AB are type_button and we need to add them here so
                // no break!
                default:
                    if (!m_spawnedByDefault) // despawn timer
                    {
                        // can be despawned or destroyed
                        SetLootState(GO_JUST_DEACTIVATED);
                        return;
                    }

                    // respawn timer
                    Refresh();
                    break;
                }
            }
        }

        if (isSpawned())
        {
            GameObjectInfo const* goInfo = GetGOInfo();
            if (goInfo->type == GAMEOBJECT_TYPE_SUMMONING_RITUAL) // Ritual
            {
                Unit* caster = GetOwner();
                // owner is first user for non-wild GO objects, if it offline
                // value already set to current user
                if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                    if (Player* firstUser = GetMap()->GetPlayer(m_firstUser))
                        caster = firstUser;

                if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                    return;
                // check participants
                for (auto itr = m_UniqueUsers.begin();
                     itr != m_UniqueUsers.end();)
                {
                    if (Player* plr = GetMap()->GetPlayer(*itr))
                    {
                        // If they are currently not participating in the
                        // ritual, remove them
                        if (plr != ((Player*)caster))
                        {
                            if (!plr->has_aura(
                                    goInfo->summoningRitual.animSpell))
                            {
                                m_UniqueUsers.erase(itr);
                                itr = m_UniqueUsers.begin();
                                continue;
                            }
                        }
                    }
                    ++itr;
                }
                // not enough participants don't continue
                if (GetUniqueUseCount() <
                        goInfo->summoningRitual.reqParticipants &&
                    !m_ritualStarted)
                    return;
                // we now have enough, start the ritual
                else if (GetUniqueUseCount() >=
                             goInfo->summoningRitual.reqParticipants &&
                         !m_ritualStarted)
                {
                    // ritual has started
                    m_ritualStarted = true;
                    if (SpellEntry const* spellInfo = sSpellStore.LookupEntry(
                            goInfo->summoningRitual.spellId))
                        m_ritualTime = GetSpellCastTime(spellInfo);

                    // No ritual should be shorter than 5 seconds
                    if (m_ritualTime < 5000)
                        m_ritualTime = 5000;
                    return;
                }
                // oops someone interrupted the ritual, reset
                else if (GetUniqueUseCount() <
                             goInfo->summoningRitual.reqParticipants &&
                         m_ritualStarted)
                {
                    m_ritualStarted = false;
                    return;
                }

                // Now that ritual is started, wait until timer is complete to
                // continue
                if (m_ritualTime >= update_diff)
                {
                    m_ritualTime -= update_diff;
                    return;
                }
                // Everything is done end the ritual successfully
                EndRitual(true);
                return;
            }
            else if (goInfo->type == GAMEOBJECT_TYPE_TRAP) // Traps
            {
                if (m_cooldownTime >= WorldTimer::time_no_syscall())
                    return;

                // FIXME: this is activation radius (in different casting radius
                // that must be selected from spell data)
                // TODO: move activated state code (cast itself) to
                // GO_ACTIVATED, in this place only check activating and set
                // state
                float radius = float(goInfo->trap.diameter) / 2.0f;
                if (!radius)
                {
                    if (goInfo->trap.cooldown != 3) // cast in other case (at
                                                    // some triggering/linked
                                                    // go/etc explicit call)
                        return;
                    else
                    {
                        if (m_respawnTime > 0)
                            break;

                        // battlegrounds gameobjects has data2 == 0 && data5 ==
                        // 3
                        radius = float(goInfo->trap.cooldown);
                    }
                }

                Unit* enemy = nullptr; // Pointer to enemy if any was found
                if (Unit* owner = GetOwner()) // hunter trap
                {
                    enemy = maps::visitors::yield_best_match<Unit, Player, Pet,
                        Creature, SpecialVisCreature, TemporarySummon>{}(this,
                        radius, [owner, this, radius](auto&& u)
                        {
                            return hunter_trap_check(owner, this, radius, u);
                        });
                }
                else // environmental trap
                {
                    // Affect only players
                    enemy = maps::visitors::yield_best_match<Player>{}(this,
                        radius, [](Player* p)
                        {
                            return p->isAlive();
                        });

                    // Alterac Valley landmines
                    if (enemy && m_goInfo->trap.spellId == 22498)
                    {
                        if (GetEntry() == 179324 &&
                            static_cast<Player*>(enemy)->GetTeam() != ALLIANCE)
                            enemy = nullptr;
                        else if (GetEntry() == 179325 &&
                                 static_cast<Player*>(enemy)->GetTeam() !=
                                     HORDE)
                            enemy = nullptr;
                    }
                }

                if (enemy)
                    Use(enemy);
            }

            if (uint32 max_charges = goInfo->GetCharges())
            {
                if (m_useTimes >= max_charges)
                {
                    m_useTimes = 0;
                    SetLootState(
                        GO_JUST_DEACTIVATED); // can be despawned or destroyed
                }
            }
        }
        break;
    }
    case GO_ACTIVATED:
    {
        switch (GetGoType())
        {
        case GAMEOBJECT_TYPE_DOOR:
        case GAMEOBJECT_TYPE_BUTTON:
            if (GetGOInfo()->GetAutoCloseTime() &&
                (m_cooldownTime < WorldTimer::time_no_syscall()))
                ResetDoorOrButton();
            break;
        case GAMEOBJECT_TYPE_CHEST:
            if (m_lootDistributor)
                m_lootDistributor->update_rolls(update_diff);
            break;
        case GAMEOBJECT_TYPE_GOOBER:
            if (m_cooldownTime < WorldTimer::time_no_syscall())
            {
                RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);

                SetLootState(GO_JUST_DEACTIVATED);
                m_cooldownTime = 0;
            }
            break;
        case GAMEOBJECT_TYPE_CAPTURE_POINT:
            m_captureTimer += p_time;
            if (m_captureTimer >= 5000)
            {
                TickCapturePoint();
                m_captureTimer -= 5000;
            }
            break;
        default:
            break;
        }
        break;
    }
    case GO_JUST_DEACTIVATED:
    {
        switch (GetGoType())
        {
        // if Gameobject should cast spell, then this, but some GOs (type = 10)
        // should be destroyed
        case GAMEOBJECT_TYPE_GOOBER:
            if (uint32 spellId = GetGOInfo()->goober.spellId)
            {
                for (const auto& elem : m_UniqueUsers)
                {
                    if (Player* owner = GetMap()->GetPlayer(elem))
                        owner->CastSpell(owner, spellId, false, nullptr,
                            nullptr, GetObjectGuid());
                }

                ClearAllUsesData();
            }
            SetGoState(GO_STATE_READY);
            // any return here in case battleground traps
            break;
        case GAMEOBJECT_TYPE_CAPTURE_POINT:
            // remove capturing players because slider wont be displayed if
            // capture point is being locked
            for (const auto& elem : m_UniqueUsers)
            {
                if (Player* owner = GetMap()->GetPlayer(elem))
                    owner->SendUpdateWorldState(
                        GetGOInfo()->capturePoint.worldState1,
                        WORLD_STATE_REMOVE);
            }

            m_UniqueUsers.clear();
            SetLootState(GO_READY);
            return; // SetLootState and return because go is treated as "burning
                    // flag" due to GetGoAnimProgress() being 100 and would be
                    // removed on the client
        default:
            break;
        }

        if (GetOwnerGuid())
        {
            if (Unit* owner = GetOwner())
                owner->RemoveGameObject(this, false);

            SetRespawnTime(0);
            Delete();
            return;
        }

        // burning flags in some battlegrounds, if you find better condition,
        // just add it
        if (GetGOInfo()->IsDespawnAtAction() || GetGoAnimProgress() > 0)
        {
            SendObjectDeSpawnAnim(GetObjectGuid());
            // reset flags
            if (GetMap()->Instanceable())
            {
                // In Instances GO_FLAG_LOCKED or GO_FLAG_NO_INTERACT are not
                // changed
                uint32 currentLockOrInteractFlags =
                    GetUInt32Value(GAMEOBJECT_FLAGS) &
                    (GO_FLAG_LOCKED | GO_FLAG_NO_INTERACT);
                SetUInt32Value(GAMEOBJECT_FLAGS,
                    (GetGOInfo()->flags &
                        ~(GO_FLAG_LOCKED | GO_FLAG_NO_INTERACT)) |
                        currentLockOrInteractFlags);
            }
            else
                SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);
        }

        if (m_temporary)
        {
            Delete();
            return;
        }

        SetLootState(GO_READY);
        DeleteLootDistributor();

        if (!m_respawnDelayTime)
            return;

        // since pool system can fail to roll unspawned object, this one can
        // remain spawned, so must set respawn nevertheless
        m_respawnTime = m_spawnedByDefault ?
                            WorldTimer::time_no_syscall() + m_respawnDelayTime :
                            0;

        SaveRespawnTime();

        // if part of pool, let pool system schedule new spawn instead of just
        // scheduling respawn
        if (auto poolid =
                sPoolMgr::Instance()->IsPartOfAPool<GameObject>(GetGUIDLow()))
        {
            if (GetMap()->GetPersistentState())
                sPoolMgr::Instance()->UpdatePool<GameObject>(
                    *GetMap()->GetPersistentState(), poolid, GetGUIDLow());
        }

        // can be not in world at pool despawn
        if (IsInWorld())
            UpdateObjectVisibility();

        break;
    }
    }
}

void GameObject::Refresh()
{
    // not refresh despawned not casted GO (despawned casted GO destroyed in all
    // cases anyway)
    if (m_respawnTime > 0 && m_spawnedByDefault)
        return;

    if (isSpawned() && IsInWorld())
        GetMap()->UpdateObjectVisibility(this);
}

void GameObject::AddUniqueUse(Player* player)
{
    AddUse();

    if (!m_firstUser)
        m_firstUser = player->GetObjectGuid();

    m_UniqueUsers.insert(player->GetObjectGuid());
}

void GameObject::Delete()
{
    SendObjectDeSpawnAnim(GetObjectGuid());

    SetGoState(GO_STATE_READY);
    SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);

    if (auto poolid =
            sPoolMgr::Instance()->IsPartOfAPool<GameObject>(GetGUIDLow()))
    {
        if (GetMap()->GetPersistentState())
            sPoolMgr::Instance()->UpdatePool<GameObject>(
                *GetMap()->GetPersistentState(), poolid, GetGUIDLow());
    }
    // Queue delete again for when added to map
    else if (!IsInWorld())
    {
        queue_action(0, [this]()
            {
                AddObjectToRemoveList();
            });
    }
    // Delete
    else
    {
        AddObjectToRemoveList();
    }
}

void GameObject::SaveToDB()
{
    // this should only be used when the gameobject has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    GameObjectData const* data =
        sObjectMgr::Instance()->GetGOData(GetGUIDLow());
    if (!data)
    {
        logging.error(
            "GameObject::SaveToDB failed, cannot get gameobject data!");
        return;
    }

    SaveToDB(GetMapId(), data->spawnMask);
}

void GameObject::SaveToDB(uint32 mapid, uint8 spawnMask)
{
    const GameObjectInfo* goI = GetGOInfo();

    if (!goI)
        return;

    // update in loaded data (changing data only in this place)
    GameObjectData& data = sObjectMgr::Instance()->NewGOData(GetGUIDLow());

    // data->guid = guid don't must be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.posX = GetFloatValue(GAMEOBJECT_POS_X);
    data.posY = GetFloatValue(GAMEOBJECT_POS_Y);
    data.posZ = GetFloatValue(GAMEOBJECT_POS_Z);
    data.orientation = GetFloatValue(GAMEOBJECT_FACING);
    data.rotation0 = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    data.rotation1 = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    data.rotation2 = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    data.rotation3 = GetFloatValue(GAMEOBJECT_ROTATION + 3);
    data.spawntimesecs = m_spawnedByDefault ? (int32)m_respawnDelayTime :
                                              -(int32)m_respawnDelayTime;
    data.animprogress = GetGoAnimProgress();
    data.go_state = GetGoState();
    data.spawnMask = spawnMask;

    // updated in DB
    std::ostringstream ss;
    ss << "INSERT INTO gameobject VALUES ( " << GetGUIDLow() << ", "
       << GetEntry() << ", " << mapid << ", " << uint32(spawnMask)
       << "," // cast to prevent save as symbol
       << GetFloatValue(GAMEOBJECT_POS_X) << ", "
       << GetFloatValue(GAMEOBJECT_POS_Y) << ", "
       << GetFloatValue(GAMEOBJECT_POS_Z) << ", "
       << GetFloatValue(GAMEOBJECT_FACING) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 1) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 2) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 3) << ", " << m_respawnDelayTime
       << ", " << uint32(GetGoAnimProgress()) << ", " << uint32(GetGoState())
       << ")";

    WorldDatabase.BeginTransaction();
    WorldDatabase.PExecuteLog(
        "DELETE FROM gameobject WHERE guid = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("%s", ss.str().c_str());
    WorldDatabase.CommitTransaction();
}

bool GameObject::LoadFromDB(uint32 guid, Map* map)
{
    GameObjectData const* data = sObjectMgr::Instance()->GetGOData(guid);

    if (!data)
    {
        logging.error(
            "Gameobject (GUID: %u) not found in table `gameobject`, can't "
            "load. ",
            guid);
        return false;
    }

    uint32 entry = data->id;
    // uint32 map_id = data->mapid;                          // already used
    // before call
    float x = data->posX;
    float y = data->posY;
    float z = data->posZ;
    float ang = data->orientation;

    float rotation0 = data->rotation0;
    float rotation1 = data->rotation1;
    float rotation2 = data->rotation2;
    float rotation3 = data->rotation3;

    uint32 animprogress = data->animprogress;
    GOState go_state = data->go_state;

    if (!Create(guid, entry, map, x, y, z, ang, rotation0, rotation1, rotation2,
            rotation3, animprogress, go_state))
        return false;

    if (!GetGOInfo()->GetDespawnPossibility() &&
        !GetGOInfo()->IsDespawnAtAction() && data->spawntimesecs >= 0)
    {
        SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);
        m_spawnedByDefault = true;
        m_respawnDelayTime = 0;
        m_respawnTime = 0;
    }
    else
    {
        if (data->spawntimesecs >= 0)
        {
            m_spawnedByDefault = true;
            m_respawnDelayTime = data->spawntimesecs;

            m_respawnTime =
                map->GetPersistentState() ?
                    map->GetPersistentState()->GetGORespawnTime(GetGUIDLow()) :
                    0;

            // ready to respawn
            if (m_respawnTime && m_respawnTime <= WorldTimer::time_no_syscall())
            {
                m_respawnTime = 0;
                if (map->GetPersistentState())
                    map->GetPersistentState()->SaveGORespawnTime(
                        GetGUIDLow(), 0);
            }
        }
        else
        {
            m_spawnedByDefault = false;
            m_respawnDelayTime = -data->spawntimesecs;
            m_respawnTime = 0;
        }
    }

    return true;
}

struct GameObjectRespawnDeleteWorker
{
    explicit GameObjectRespawnDeleteWorker(uint32 guid) : i_guid(guid) {}

    void operator()(MapPersistentState* state)
    {
        state->SaveGORespawnTime(i_guid, 0);
    }

    uint32 i_guid;
};

void GameObject::DeleteFromDB()
{
    if (!HasStaticDBSpawnData())
    {
        LOG_DEBUG(logging, "Trying to delete not saved gameobject!");
        return;
    }

    GameObjectRespawnDeleteWorker worker(GetGUIDLow());
    sMapPersistentStateMgr::Instance()->DoForAllStatesWithMapId(
        GetMapId(), worker);

    sObjectMgr::Instance()->DeleteGOData(GetGUIDLow());
    WorldDatabase.PExecuteLog(
        "DELETE FROM gameobject WHERE guid = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog(
        "DELETE FROM game_event_gameobject WHERE guid = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog(
        "DELETE FROM gameobject_battleground WHERE guid = '%u'", GetGUIDLow());
}

void GameObject::SetRitualTargetGuid(ObjectGuid target)
{
    ritual_target_ = target;
}

GameObjectInfo const* GameObject::GetGOInfo() const
{
    return m_goInfo;
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/
bool GameObject::HasQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds =
        sObjectMgr::Instance()->GetGOQuestRelationsMapBounds(GetEntry());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
            return true;
    }
    return false;
}

bool GameObject::HasInvolvedQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds =
        sObjectMgr::Instance()->GetGOQuestInvolvedRelationsMapBounds(
            GetEntry());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
            return true;
    }
    return false;
}

bool GameObject::IsTransport() const
{
    // If something is marked as a transport, don't transmit an out of range
    // packet for it.
    GameObjectInfo const* gInfo = GetGOInfo();
    if (!gInfo)
        return false;
    return gInfo->type == GAMEOBJECT_TYPE_TRANSPORT ||
           gInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT;
}

Unit* GameObject::GetOwner() const
{
    return ObjectAccessor::GetUnit(*this, GetOwnerGuid());
}

void GameObject::SaveRespawnTime()
{
    if (m_respawnTime > WorldTimer::time_no_syscall() && m_spawnedByDefault &&
        GetMap()->GetPersistentState())
        GetMap()->GetPersistentState()->SaveGORespawnTime(
            GetGUIDLow(), m_respawnTime);
}

bool GameObject::isVisibleForInState(
    Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    // Not in world
    if (!IsInWorld() || !u->IsInWorld())
        return false;

    // Transport always visible at this step implementation
    if (IsTransport() && IsInMap(u))
        return true;

    // quick check visibility false cases for non-GM-mode
    if (!u->isGameMaster())
    {
        // despawned and then not visible for non-GM in GM-mode
        if (!isSpawned())
            return false;

        // Trap visibility
        if (GetGOInfo()->type == GAMEOBJECT_TYPE_TRAP)
            return IsTrapVisibleToPlayer(u, viewPoint);

        // Smuggled Mana Cell required 10 invisibility type detection/state
        if (GetEntry() == 187039 &&
            ((u->m_detectInvisibilityMask | u->m_invisibilityMask) &
                (1 << 10)) == 0)
            return false;
    }

    // check distance
    return IsWithinDistInMap(viewPoint,
        GetMap()->GetVisibilityDistance() +
            (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f),
        false);
}

bool GameObject::IsTrapVisibleToPlayer(
    const Player* p, const WorldObject* viewPoint) const
{
    if (GetGOInfo()->type != GAMEOBJECT_TYPE_TRAP ||
        !(GetGOInfo()->trap.stealthed || GetGOInfo()->trap.invisible))
        return true;

    Unit* owner = GetOwner();
    if (!owner)
        return true;

    // Traps are always visible to their owner
    if (p == owner)
        return true;

    // Traps are also visible to players in owner's group
    if (owner->GetTypeId() == TYPEID_PLAYER)
    {
        const Group* group = ((Player*)owner)->GetGroup();
        if (group && group->IsMember(p->GetObjectGuid()))
            return true;
    }

    // Detect traps (2836) makes you able to detect traps
    float stealthDetection = 0, invisDetection = 0;
    const Unit::Auras& stealth =
        p->GetAurasByType(SPELL_AURA_MOD_STEALTH_DETECT);
    const Unit::Auras& invis =
        p->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
    for (const auto& elem : stealth)
        if ((elem)->GetMiscValue() == 1) // Trap detection
        {
            stealthDetection = (elem)->GetBasePoints();
            break;
        }
    for (const auto& invi : invis)
        if ((invi)->GetMiscValue() == 3) // Trap detection
        {
            invisDetection = (invi)->GetBasePoints();
            break;
        }

    // FIXME: Patch 1.10 change trap detection to be like stealth. Essentially
    // this code isn't correct
    // and needs a rewrite. But this should work good enough in the meantime.
    if (stealthDetection == 0 && invisDetection == 0)
    {
        // ~7 yards
        stealthDetection = 30;
        invisDetection = 90;
    }

    // This formula is completely made up by me (FIXME: find real formula) --
    // equals to 20 yards with detect traps (rogue)
    float coefficient = 5.0f;
    float dist =
        (2 * stealthDetection / 70. + 2 * invisDetection / 300.) * coefficient;
    return IsWithinDistInMap(viewPoint, dist, false);
}

void GameObject::Respawn()
{
    if (m_spawnedByDefault && m_respawnTime > 0 &&
        GetMap()->GetPersistentState())
    {
        m_respawnTime = WorldTimer::time_no_syscall();
        GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), 0);
    }
}

bool GameObject::ActivateToQuest(Player* pTarget) const
{
    // if GO is ReqCreatureOrGoN for quest
    if (pTarget->HasQuestForGO(GetEntry()))
        return true;

    if (!sObjectMgr::Instance()->IsGameObjectForQuests(GetEntry()))
        return false;

    switch (GetGoType())
    {
    case GAMEOBJECT_TYPE_QUESTGIVER:
    {
        // Not fully clear when GO's can activate/deactivate
        // For cases where GO has additional (except quest itself),
        // these conditions are not sufficient/will fail.
        // Never expect flags|4 for these GO's? (NF-note: It doesn't appear it's
        // expected)

        QuestRelationsMapBounds bounds =
            sObjectMgr::Instance()->GetGOQuestRelationsMapBounds(GetEntry());

        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            const Quest* qInfo =
                sObjectMgr::Instance()->GetQuestTemplate(itr->second);

            // FIXME: remove const_cast
            if (pTarget->CanTakeQuest(
                    qInfo, false, const_cast<GameObject*>(this)))
                return true;
        }

        bounds = sObjectMgr::Instance()->GetGOQuestInvolvedRelationsMapBounds(
            GetEntry());

        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            if ((pTarget->GetQuestStatus(itr->second) ==
                        QUEST_STATUS_INCOMPLETE ||
                    pTarget->GetQuestStatus(itr->second) ==
                        QUEST_STATUS_COMPLETE) &&
                !pTarget->GetQuestRewardStatus(itr->second))
                return true;
        }

        break;
    }
    // scan GO chest with loot including quest items
    case GAMEOBJECT_TYPE_CHEST:
    {
        if (pTarget->GetQuestStatus(GetGOInfo()->chest.questId) ==
            QUEST_STATUS_INCOMPLETE)
            return true;

        if (LootTemplates_Gameobject.HaveQuestLootForPlayer(
                GetGOInfo()->GetLootId(), pTarget))
        {
            // look for battlegroundAV for some objects which are only activated
            // after mine gots captured by own team
            if (GetEntry() == BG_AV_OBJECTID_MINE_N ||
                GetEntry() == BG_AV_OBJECTID_MINE_S)
                if (BattleGround* bg = pTarget->GetBattleGround())
                    if (bg->GetTypeID() == BATTLEGROUND_AV &&
                        !(((BattleGroundAV*)bg)
                                ->PlayerCanDoMineQuest(
                                    GetEntry(), pTarget->GetTeam())))
                        return false;
            return true;
        }
        break;
    }
    case GAMEOBJECT_TYPE_GENERIC:
    {
        if (pTarget->GetQuestStatus(GetGOInfo()->_generic.questID) ==
            QUEST_STATUS_INCOMPLETE)
            return true;
        break;
    }
    case GAMEOBJECT_TYPE_SPELL_FOCUS:
    {
        if (pTarget->GetQuestStatus(GetGOInfo()->spellFocus.questID) ==
            QUEST_STATUS_INCOMPLETE)
            return true;
        break;
    }
    case GAMEOBJECT_TYPE_GOOBER:
    {
        if (pTarget->GetQuestStatus(GetGOInfo()->goober.questId) ==
            QUEST_STATUS_INCOMPLETE)
            return true;
        break;
    }
    default:
        break;
    }

    return false;
}

void GameObject::SummonLinkedTrapIfAny()
{
    uint32 linkedEntry = GetGOInfo()->GetLinkedGameObjectEntry();
    if (!linkedEntry)
        return;

    auto linkedGO = new GameObject;
    if (!linkedGO->Create(GetMap()->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT),
            linkedEntry, GetMap(), GetX(), GetY(), GetZ(), GetO(), 0.0f, 0.0f,
            0.0f, 0.0f, GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
    {
        delete linkedGO;
        return;
    }

    linkedGO->SetRespawnTime(GetRespawnDelay());
    if (GetSpellId())
        linkedGO->SetSpellId(GetSpellId());

    if (GetOwnerGuid())
    {
        if (linkedGO->GetGoType() !=
            GAMEOBJECT_TYPE_TRAP) // TODO: This might be incorrect, but for now
                                  // GOs with owners assume hostile targets,
                                  // which will break for Basic Campfire, e.g.
            linkedGO->SetOwnerGuid(GetOwnerGuid());
        linkedGO->SetUInt32Value(
            GAMEOBJECT_LEVEL, GetUInt32Value(GAMEOBJECT_LEVEL));
    }

    if (!GetMap()->insert(linkedGO))
        delete linkedGO;
}

void GameObject::TriggerLinkedGameObject(Unit* target)
{
    uint32 trapEntry = GetGOInfo()->GetLinkedGameObjectEntry();

    if (!trapEntry)
        return;

    GameObjectInfo const* trapInfo =
        sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
    if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
        return;

    SpellEntry const* trapSpell =
        sSpellStore.LookupEntry(trapInfo->trap.spellId);

    // The range to search for linked trap is weird. We set 0.5 as default. Most
    // (all?)
    // traps are probably expected to be pretty much at the same location as the
    // used GO,
    // so it appears that using range from spell is obsolete.
    float range = 0.5f;

    if (trapSpell) // checked at load already
        range = GetSpellMaxRange(
            sSpellRangeStore.LookupEntry(trapSpell->rangeIndex));

    // search nearest linked GO
    GameObject* trapGO = nullptr;

    {
        // search closest with base of used GO, using max range of trap spell as
        // search radius (why? See above)
        trapGO = maps::visitors::yield_best_match<GameObject>{}(
            this, range, maps::checks::entry_guid{trapEntry, 0});
    }

    // found correct GO
    if (trapGO)
        trapGO->Use(target);
}

GameObject* GameObject::LookupFishingHoleAround(float range)
{
    auto ok = maps::visitors::yield_single<GameObject>{}(this, range,
        [this, range](GameObject* go)
        {
            if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_FISHINGHOLE &&
                go->isSpawned() && IsWithinDistInMap(go, range) &&
                IsWithinDistInMap(
                    go, (float)go->GetGOInfo()->fishinghole.radius))
            {
                return true;
            }
            return false;
        });

    return ok;
}

void GameObject::ResetDoorOrButton()
{
    if (m_lootState == GO_READY || m_lootState == GO_JUST_DEACTIVATED)
        return;

    SwitchDoorOrButton(false);
    SetLootState(GO_JUST_DEACTIVATED);
    m_cooldownTime = 0;
}

void GameObject::UseDoorOrButton(uint32 time_to_restore,
    bool alternative /* = false */, Unit* user /* = NULL */)
{
    if (m_lootState != GO_READY)
        return;

    if (!time_to_restore)
        time_to_restore = GetGOInfo()->GetAutoCloseTime();

    SwitchDoorOrButton(true, alternative);
    SetLootState(GO_ACTIVATED, user);

    m_cooldownTime = WorldTimer::time_no_syscall() + time_to_restore;
}

void GameObject::SwitchDoorOrButton(
    bool activate, bool alternative /* = false */)
{
    if (activate)
        SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
    else
        RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);

    if (GetGoState() == GO_STATE_READY) // if closed -> open
        SetGoState(alternative ? GO_STATE_ACTIVE_ALTERNATIVE : GO_STATE_ACTIVE);
    else // if open -> close
        SetGoState(GO_STATE_READY);
}

void GameObject::Use(
    Unit* user, bool bypass_no_interact, uint32 spell_misc_value)
{
    // user must be provided
    assert(user || PrintEntryError("GameObject::Use (without user)"));

    // NOTE: Use bar of GO might've started before GO_FLAG_NO_INTERACT was
    // added, doesn't mean cheating attempt by default
    if (!bypass_no_interact && HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
        return;

    // by default spell caster is user
    Unit* spellCaster = user;
    uint32 spellId = 0;
    bool triggered = false;

    // test only for exist cooldown data (cooldown timer used for door/buttons
    // reset that not have use cooldown)
    if (uint32 cooldown = GetGOInfo()->GetCooldown())
    {
        if (m_cooldownTime > WorldTimer::time_no_syscall())
            return;

        m_cooldownTime = WorldTimer::time_no_syscall() + cooldown;
    }

    bool scriptReturnValue =
        user->GetTypeId() == TYPEID_PLAYER &&
        sScriptMgr::Instance()->OnGameObjectUse((Player*)user, this);

    switch (GetGoType())
    {
    case GAMEOBJECT_TYPE_DOOR: // 0
    {
        // doors never really despawn, only reset to default state/flags
        UseDoorOrButton(0, false, user);

        // activate script
        if (!scriptReturnValue)
            GetMap()->ScriptsStart(
                sGameObjectScripts, GetGUIDLow(), spellCaster, this);
        return;
    }
    case GAMEOBJECT_TYPE_BUTTON: // 1
    {
        // buttons never really despawn, only reset to default state/flags
        UseDoorOrButton(0, false, user);

        TriggerLinkedGameObject(user);

        // activate script
        if (!scriptReturnValue)
            GetMap()->ScriptsStart(
                sGameObjectScripts, GetGUIDLow(), spellCaster, this);

        return;
    }
    case GAMEOBJECT_TYPE_QUESTGIVER: // 2
    {
        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        if (!sScriptMgr::Instance()->OnGossipHello(player, this))
        {
            player->PrepareGossipMenu(this, GetGOInfo()->questgiver.gossipID);

            // If gossip id is 0 and no quests were added to the menu, don't
            // send it!
            if (GetGOInfo()->questgiver.gossipID == 0)
            {
                if (player->PlayerTalkClass->GetQuestMenu().Empty() &&
                    player->PlayerTalkClass->GetGossipMenu().Empty())
                {
                    player->PlayerTalkClass->ClearMenus();
                    return;
                }
            }

            player->SendPreparedGossip(this);
        }

        return;
    }
    case GAMEOBJECT_TYPE_CHEST: // 3
    {
        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        TriggerLinkedGameObject(user);

        // TODO: possible must be moved to loot release (in different from
        // linked triggering)
        if (GetGOInfo()->chest.eventId)
        {
            LOG_DEBUG(logging, "Chest ScriptStart id %u for GO %u",
                GetGOInfo()->chest.eventId, GetGUIDLow());

            if (!sScriptMgr::Instance()->OnProcessEvent(
                    GetGOInfo()->chest.eventId, user, this, true))
                GetMap()->ScriptsStart(
                    sEventScripts, GetGOInfo()->chest.eventId, user, this);
        }

        return;
    }
    case GAMEOBJECT_TYPE_GENERIC: // 5
    {
        if (scriptReturnValue)
            return;

        // No known way to exclude some - only different approach is to select
        // despawnable GOs by Entry
        SetLootState(GO_JUST_DEACTIVATED);
        return;
    }
    case GAMEOBJECT_TYPE_TRAP: // 6
    {
        if (scriptReturnValue)
            return;

        float radius = float(m_goInfo->trap.diameter) / 2.0f;
        bool IsBattleGroundTrap =
            !radius && m_goInfo->trap.cooldown == 3 && m_respawnTime == 0;

        // FIXME: when GO casting will be implemented trap must cast spell to
        // target
        if (m_goInfo->trap.spellId)
            CastSpell(user, m_goInfo->trap.spellId);
        // use template cooldown if provided
        m_cooldownTime =
            WorldTimer::time_no_syscall() +
            (m_goInfo->trap.cooldown ? m_goInfo->trap.cooldown : uint32(4));

        // count charges
        if (m_goInfo->trap.charges > 0)
            AddUse();

        if (IsBattleGroundTrap && user->GetTypeId() == TYPEID_PLAYER)
        {
            // BattleGround gameobjects case
            if (BattleGround* bg = ((Player*)user)->GetBattleGround())
                bg->HandleTriggerBuff(GetObjectGuid());
        }

        // TODO: all traps can be activated, also those without spell.
        // Some may have have animation and/or are expected to despawn.

        // TODO: Improve this when more information is available, currently
        // these traps are known that must send the anim (Onyxia/ Heigan
        // Fissures/ Trap in DireMaul)
        if (GetDisplayId() == 4392 || GetDisplayId() == 4472 ||
            GetDisplayId() == 6785 || GetDisplayId() == 3073 ||
            GetDisplayId() == 4491)
            SendGameObjectCustomAnim(GetObjectGuid());

        // TODO: Despawning of traps? (Also related to code in ::Update)
        return;
    }
    case GAMEOBJECT_TYPE_CHAIR: // 7 Sitting: Wooden bench, chairs
    {
        GameObjectInfo const* info = GetGOInfo();
        if (!info)
            return;

        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        // a chair may have n slots. we have to calculate their positions and
        // teleport the player to the nearest one

        // check if the db is sane
        if (info->chair.slots > 0)
        {
            float lowestDist = DEFAULT_VISIBILITY_DISTANCE;

            float x_lowest = GetX();
            float y_lowest = GetY();

            // the object orientation + 1/2 pi
            // every slot will be on that straight line
            float orthogonalOrientation = GetO() + M_PI_F * 0.5f;
            // find nearest slot
            for (uint32 i = 0; i < info->chair.slots; ++i)
            {
                // the distance between this slot and the center of the go -
                // imagine a 1D space
                float relativeDistance =
                    (info->size * i) -
                    (info->size * (info->chair.slots - 1) / 2.0f);

                float x_i =
                    GetX() + relativeDistance * cos(orthogonalOrientation);
                float y_i =
                    GetY() + relativeDistance * sin(orthogonalOrientation);

                // calculate the distance between the player and this slot
                float thisDistance = player->GetDistance2d(x_i, y_i);

                /* debug code. It will spawn a npc on each slot to visualize
                them.
                Creature* helper = player->SummonCreature(14496, x_i, y_i,
                GetZ(), GetO(),
                TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 10000);
                std::ostringstream output;
                output << i << ": thisDist: " << thisDistance;
                helper->MonsterSay(output.str().c_str(), LANG_UNIVERSAL);
                */

                if (thisDistance <= lowestDist)
                {
                    lowestDist = thisDistance;
                    x_lowest = x_i;
                    y_lowest = y_i;
                }
            }
            player->TeleportTo(GetMapId(), x_lowest, y_lowest, GetZ(), GetO(),
                TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT |
                    TELE_TO_NOT_UNSUMMON_PET);
        }
        else
        {
            // fallback, will always work
            player->TeleportTo(GetMapId(), GetX(), GetY(), GetZ(), GetO(),
                TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT |
                    TELE_TO_NOT_UNSUMMON_PET);
        }
        player->SetStandState(
            UNIT_STAND_STATE_SIT_LOW_CHAIR + info->chair.height);
        return;
    }
    case GAMEOBJECT_TYPE_SPELL_FOCUS: // 8
    {
        TriggerLinkedGameObject(user);

        // some may be activated in addition? Conditions for this? (ex: entry
        // 181616)
        return;
    }
    case GAMEOBJECT_TYPE_GOOBER: // 10
    {
        // Handle OutdoorPvP use cases
        // Note: this may be also handled by DB spell scripts in the future,
        // when the world state manager is implemented
        if (user->GetTypeId() == TYPEID_PLAYER)
        {
            Player* player = (Player*)user;
            if (OutdoorPvP* outdoorPvP =
                    sOutdoorPvPMgr::Instance()->GetScript(player->GetZoneId()))
                outdoorPvP->HandleGameObjectUse(player, this);
        }

        GameObjectInfo const* info = GetGOInfo();

        TriggerLinkedGameObject(user);

        SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
        SetLootState(GO_ACTIVATED, user);

        // this appear to be ok, however others exist in addition to this that
        // should have custom (ex: 190510, 188692, 187389)
        if (info->goober.customAnim)
            SendGameObjectCustomAnim(GetObjectGuid(), spell_misc_value);
        else
            SetGoState(GO_STATE_ACTIVE);

        m_cooldownTime =
            WorldTimer::time_no_syscall() + info->GetAutoCloseTime();

        if (user->GetTypeId() == TYPEID_PLAYER)
        {
            Player* player = (Player*)user;

            if (info->goober.pageId) // show page...
            {
                WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
                data << ObjectGuid(GetObjectGuid());
                player->GetSession()->send_packet(std::move(data));
            }
            else if (info->goober
                         .gossipID) // ...or gossip, if page does not exist
            {
                if (!sScriptMgr::Instance()->OnGossipHello(player, this))
                {
                    player->PrepareGossipMenu(this, info->goober.gossipID);
                    player->SendPreparedGossip(this);
                }
            }

            if (info->goober.eventId)
            {
                LOG_DEBUG(logging,
                    "Goober ScriptStart id %u for GO entry %u (GUID %u).",
                    info->goober.eventId, GetEntry(), GetGUIDLow());

                if (!sScriptMgr::Instance()->OnProcessEvent(
                        info->goober.eventId, player, this, true))
                    GetMap()->ScriptsStart(
                        sEventScripts, info->goober.eventId, player, this);
            }

            // possible quest objective for active quests
            if (info->goober.questId &&
                sObjectMgr::Instance()->GetQuestTemplate(info->goober.questId))
            {
                // Quest require to be active for GO using
                if (player->GetQuestStatus(info->goober.questId) !=
                    QUEST_STATUS_INCOMPLETE)
                    break;
            }

            player->RewardPlayerAndGroupAtCast(this);
        }

        if (scriptReturnValue)
            return;

        // cast this spell later if provided
        spellId = info->goober.spellId;

        break;
    }
    case GAMEOBJECT_TYPE_CAMERA: // 13
    {
        GameObjectInfo const* info = GetGOInfo();
        if (!info)
            return;

        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        if (info->camera.cinematicId)
            player->SendCinematicStart(info->camera.cinematicId);

        if (info->camera.eventID)
        {
            if (!sScriptMgr::Instance()->OnProcessEvent(
                    info->camera.eventID, player, this, true))
                GetMap()->ScriptsStart(
                    sEventScripts, info->camera.eventID, player, this);
        }

        return;
    }
    case GAMEOBJECT_TYPE_FISHINGNODE: // 17 fishing bobber
    {
        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        if (player->GetObjectGuid() != GetOwnerGuid())
            return;

        switch (getLootState())
        {
        case GO_READY: // ready for loot
        {
            // 1) skill must be >= base_zone_skill
            // 2) if skill == base_zone_skill => 5% chance
            // 3) chance is linear dependence from (base_zone_skill-skill)

            uint32 zone, subzone;
            GetZoneAndAreaId(zone, subzone);

            int32 zone_skill =
                sObjectMgr::Instance()->GetFishingBaseSkillLevel(subzone);
            if (!zone_skill)
                zone_skill =
                    sObjectMgr::Instance()->GetFishingBaseSkillLevel(zone);

            // provide error, no fishable zone or area should be 0
            if (!zone_skill)
                logging.error(
                    "Fishable areaId %u are not properly defined in "
                    "`skill_fishing_base_level`.",
                    subzone);

            int32 skill = player->GetSkillValue(SKILL_FISHING);
            int32 chance = skill - zone_skill + 5;
            int32 roll = irand(1, 100);

            LOG_DEBUG(logging,
                "Fishing check (skill: %i zone min skill: %i chance %i roll: "
                "%i",
                skill, zone_skill, chance, roll);

            // normal chance
            bool success = skill >= zone_skill && chance >= roll;
            GameObject* fishingHole = nullptr;

            // overwrite fail in case fishhole if allowed (after 3.3.0)
            if (!success)
            {
                if (!sWorld::Instance()->getConfig(
                        CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL))
                {
                    // TODO: find reasonable value for fishing hole search
                    fishingHole =
                        LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
                    if (fishingHole)
                        success = true;
                }
            }
            // just search fishhole for success case
            else
                // TODO: find reasonable value for fishing hole search
                fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);

            if (success ||
                sWorld::Instance()->getConfig(
                    CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING))
                player->UpdateFishingSkill();

            // fish catch or fail and junk allowed (after 3.1.0)
            if (success ||
                sWorld::Instance()->getConfig(
                    CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING))
            {
                // prevent removing GO at spell cancel
                player->RemoveGameObject(this, false);
                SetOwnerGuid(player->GetObjectGuid());

                if (fishingHole) // will set at success only
                {
                    fishingHole->Use(player);
                    SetLootState(GO_JUST_DEACTIVATED);
                }
                else
                    player->SendLoot(GetObjectGuid(),
                        success ? LOOT_FISHING : LOOT_FISHING_FAIL);
            }
            else
            {
                // fish escaped, can be deleted now
                SetLootState(GO_JUST_DEACTIVATED);

                WorldPacket data(SMSG_FISH_ESCAPED, 0);
                player->GetSession()->send_packet(std::move(data));
            }
            break;
        }
        case GO_JUST_DEACTIVATED: // nothing to do, will be deleted at next
                                  // update
            break;
        default:
        {
            SetLootState(GO_JUST_DEACTIVATED);

            WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
            player->GetSession()->send_packet(std::move(data));
            break;
        }
        }

        player->FinishSpell(CURRENT_CHANNELED_SPELL);
        return;
    }
    case GAMEOBJECT_TYPE_SUMMONING_RITUAL: // 18
    {
        // If the ritual has started, no one else can use the object
        if (m_ritualStarted)
            return;

        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        Unit* owner = GetOwner();

        GameObjectInfo const* info = GetGOInfo();

        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
        {
            // accept only use by player from same group as owner, excluding
            // owner itself (unique use already added in spell effect)
            if (player == (Player*)owner ||
                (info->summoningRitual.castersGrouped &&
                    !player->IsInSameRaidWith(((Player*)owner))))
                return;

            // expect owner to already be channeling, so if not...
            if (!owner->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                return;

            // in case summoning ritual caster is GO creator
            spellCaster = owner;
        }
        else
        {
            owner = nullptr;

            if (m_firstUser && player->GetObjectGuid() != m_firstUser &&
                info->summoningRitual.castersGrouped)
            {
                if (Group* group = player->GetGroup())
                {
                    if (!group->IsMember(m_firstUser))
                        return;
                }
                else
                    return;
            }

            spellCaster = player;
        }

        AddUniqueUse(player);

        if (info->summoningRitual.animSpell)
        {
            player->CastSpell(player, info->summoningRitual.animSpell, false);
        }

        /*
        // full amount unique participants including original summoner, need
        more
        if (GetUniqueUseCount() < info->summoningRitual.reqParticipants)
            return;

        // owner is first user for non-wild GO objects, if it offline value
        already set to current user
        if (!GetOwnerGuid())
            if (Player* firstUser = GetMap()->GetPlayer(m_firstUser))
                spellCaster = firstUser;

        spellId = info->summoningRitual.spellId;

        // spell have reagent and mana cost but it not expected use its
        // it triggered spell in fact casted at currently channeled GO
        triggered = true;

        // finish owners spell
        if (owner)
            owner->FinishSpell(CURRENT_CHANNELED_SPELL);

        // can be deleted now, if
        if (!info->summoningRitual.ritualPersistent)
            SetLootState(GO_JUST_DEACTIVATED);
        // reset ritual for this GO
        else
            ClearAllUsesData();

        // go to end function to spell casting
        */
        break;
    }
    case GAMEOBJECT_TYPE_SPELLCASTER: // 22
    {
        GameObjectInfo const* info = GetGOInfo();
        if (!info)
            return;
        // These gameobjects are handled via script
        if (info->id == 181621 || info->id == 186812)
            return;

        if (info->spellcaster.partyOnly)
        {
            Unit* caster = GetOwner();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                return;

            if (user->GetTypeId() != TYPEID_PLAYER ||
                !((Player*)user)->IsInSameRaidWith((Player*)caster))
                return;
        }

        spellId = info->spellcaster.spellId;

        AddUse();
        break;
    }
    case GAMEOBJECT_TYPE_MEETINGSTONE: // 23
    {
        GameObjectInfo const* info = GetGOInfo();

        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        Player* targetPlayer =
            ObjectAccessor::FindPlayer(player->GetSelectionGuid());

        // Target must be grouped with caster
        if (!targetPlayer || !targetPlayer->IsInSameGroupWith(player))
            return;

        // required lvl checks!
        uint8 level = player->getLevel();
        if (level < info->meetingstone.minLevel ||
            level > info->meetingstone.maxLevel)
            return;

        level = targetPlayer->getLevel();
        if (level < info->meetingstone.minLevel ||
            level > info->meetingstone.maxLevel)
            return;

        spellId = 23598;

        break;
    }
    case GAMEOBJECT_TYPE_FLAGSTAND: // 24
    {
        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        if (player->CanUseBattleGroundObject())
        {
            // in battleground check
            BattleGround* bg = player->GetBattleGround();
            if (!bg)
                return;
            // BG flag click
            // AB:
            // 15001
            // 15002
            // 15003
            // 15004
            // 15005
            player->remove_auras(SPELL_AURA_MOD_STEALTH, Unit::aura_no_op_true,
                AURA_REMOVE_BY_CANCEL);
            player->remove_auras(SPELL_AURA_MOD_INVISIBILITY,
                Unit::aura_no_op_true, AURA_REMOVE_BY_CANCEL);
            bg->EventPlayerClickedOnFlag(player, this);
            return; // we don't need to delete flag ... it is despawned!
        }
        break;
    }
    case GAMEOBJECT_TYPE_FISHINGHOLE: // 25
    {
        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        player->SendLoot(GetObjectGuid(), LOOT_FISHINGHOLE);
        return;
    }
    case GAMEOBJECT_TYPE_FLAGDROP: // 26
    {
        if (user->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = (Player*)user;

        if (player->CanUseBattleGroundObject())
        {
            // in battleground check
            BattleGround* bg = player->GetBattleGround();
            if (!bg)
                return;
            // BG flag dropped
            // WS:
            // 179785 - Silverwing Flag
            // 179786 - Warsong Flag
            // EotS:
            // 184142 - Netherstorm Flag
            player->remove_auras(SPELL_AURA_MOD_STEALTH, Unit::aura_no_op_true,
                AURA_REMOVE_BY_CANCEL);
            player->remove_auras(SPELL_AURA_MOD_INVISIBILITY,
                Unit::aura_no_op_true, AURA_REMOVE_BY_CANCEL);
            GameObjectInfo const* info = GetGOInfo();
            if (info)
            {
                switch (info->id)
                {
                case 179785: // Silverwing Flag
                case 179786: // Warsong Flag
                    // check if it's correct bg
                    if (bg->GetTypeID() == BATTLEGROUND_WS)
                        bg->EventPlayerClickedOnFlag(player, this);
                    break;
                case 184142: // Netherstorm Flag
                    if (bg->GetTypeID() == BATTLEGROUND_EY)
                        bg->EventPlayerClickedOnFlag(player, this);
                    break;
                }
            }
            // this cause to call return, all flags must be deleted here!!
            spellId = 0;
            Delete();
        }
        break;
    }
    default:
        logging.error(
            "GameObject::Use unhandled GameObject type %u (entry %u).",
            GetGoType(), GetEntry());
        return;
    }

    if (!spellId)
        return;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        logging.error(
            "WORLD: unknown spell id %u at use action for gameobject (Entry: "
            "%u GoType: %u )",
            spellId, GetEntry(), GetGoType());
        return;
    }

    auto spell = new Spell(spellCaster, spellInfo, triggered, GetObjectGuid());

    // spell target is user of GO
    SpellCastTargets targets;
    targets.setUnitTarget(user);

    spell->prepare(&targets);
}

// overwrite WorldObject function for proper name localization
const char* GameObject::GetNameForLocaleIdx(int32 loc_idx) const
{
    if (loc_idx >= 0)
    {
        GameObjectLocale const* cl =
            sObjectMgr::Instance()->GetGameObjectLocale(GetEntry());
        if (cl)
        {
            if (cl->Name.size() > (size_t)loc_idx && !cl->Name[loc_idx].empty())
                return cl->Name[loc_idx].c_str();
        }
    }

    return GetName();
}

void GameObject::UpdateRotationFields(
    float rotation2 /*=0.0f*/, float rotation3 /*=0.0f*/)
{
    SetFloatValue(GAMEOBJECT_FACING, GetO());

    if (rotation2 == 0.0f && rotation3 == 0.0f)
    {
        rotation2 = sin(GetO() / 2);
        rotation3 = cos(GetO() / 2);
    }

    SetFloatValue(GAMEOBJECT_ROTATION + 2, rotation2);
    SetFloatValue(GAMEOBJECT_ROTATION + 3, rotation3);
}

bool GameObject::IsHostileTo(Unit const* unit) const
{
    // always non-hostile to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER &&
        ((Player const*)unit)->isGameMaster())
        return false;

    // test owner instead if have
    if (Unit const* owner = GetOwner())
        return owner->IsHostileTo(unit);

    if (Unit const* targetOwner = unit->GetCharmerOrOwner())
        return IsHostileTo(targetOwner);

    // for not set faction case (wild object) use hostile case
    if (!GetGOInfo()->faction)
        return true;

    // faction base cases
    FactionTemplateEntry const* tester_faction =
        sFactionTemplateStore.LookupEntry(GetGOInfo()->faction);
    FactionTemplateEntry const* target_faction =
        unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
        return false;

    // GvP forced reaction and reputation case
    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force =
                    ((Player*)unit)
                        ->GetReputationMgr()
                        .GetForcedRankIfAny(tester_faction))
                return *force <= REP_HOSTILE;

            // apply reputation state
            FactionEntry const* raw_tester_faction =
                sFactionStore.LookupEntry(tester_faction->faction);
            if (raw_tester_faction && raw_tester_faction->reputationListID >= 0)
                return ((Player const*)unit)
                           ->GetReputationMgr()
                           .GetRank(raw_tester_faction) <= REP_HOSTILE;
        }
    }

    // common faction based case (GvC,GvP)
    return tester_faction->IsHostileTo(*target_faction);
}

bool GameObject::IsFriendlyTo(Unit const* unit) const
{
    // always friendly to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER &&
        ((Player const*)unit)->isGameMaster())
        return true;

    // test owner instead if have
    if (Unit const* owner = GetOwner())
        return owner->IsFriendlyTo(unit);

    if (Unit const* targetOwner = unit->GetCharmerOrOwner())
        return IsFriendlyTo(targetOwner);

    // for not set faction case (wild object) use hostile case
    if (!GetGOInfo()->faction)
        return false;

    // faction base cases
    FactionTemplateEntry const* tester_faction =
        sFactionTemplateStore.LookupEntry(GetGOInfo()->faction);
    FactionTemplateEntry const* target_faction =
        unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
        return false;

    // GvP forced reaction and reputation case
    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force =
                    ((Player*)unit)
                        ->GetReputationMgr()
                        .GetForcedRankIfAny(tester_faction))
                return *force >= REP_FRIENDLY;

            // apply reputation state
            if (FactionEntry const* raw_tester_faction =
                    sFactionStore.LookupEntry(tester_faction->faction))
                if (raw_tester_faction->reputationListID >= 0)
                    return ((Player const*)unit)
                               ->GetReputationMgr()
                               .GetRank(raw_tester_faction) >= REP_FRIENDLY;
        }
    }

    // common faction based case (GvC,GvP)
    return tester_faction->IsFriendlyTo(*target_faction);
}

void GameObject::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(GAMEOBJECT_DISPLAYID, modelId);
    m_displayInfo = sGameObjectDisplayInfoStore.LookupEntry(modelId);
    UpdateModel();
}

float GameObject::GetObjectBoundingRadius() const
{
    // FIXME:
    // 1. This is clearly hack way because GameObjectDisplayInfoEntry have 6
    // floats related to GO sizes, but better that use DEFAULT_BOUNDING_RADIUS
    // 2. In some cases this must be only interactive size, not GO size, current
    // way can affect creature target point auto-selection in strange ways for
    // big underground/virtual GOs
    if (m_displayInfo)
        return fabs(m_displayInfo->maxX * m_displayInfo->maxY) *
               GetObjectScale();

    return DEFAULT_BOUNDING_RADIUS;
}

bool GameObject::IsInSkillupList(Player* player) const
{
    return m_SkillupSet.find(player->GetObjectGuid()) != m_SkillupSet.end();
}

void GameObject::AddToSkillupList(Player* player)
{
    m_SkillupSet.insert(player->GetObjectGuid());
}

struct AddGameObjectToRemoveListInMapsWorker
{
    AddGameObjectToRemoveListInMapsWorker(ObjectGuid guid)
      : i_guid(std::move(guid))
    {
    }

    void operator()(Map* map)
    {
        if (GameObject* pGameobject = map->GetGameObject(i_guid))
            pGameobject->AddObjectToRemoveList();
    }

    ObjectGuid i_guid;
};

void GameObject::AddToRemoveListInMaps(
    uint32 db_guid, GameObjectData const* data)
{
    AddGameObjectToRemoveListInMapsWorker worker(
        ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, db_guid));
    sMapMgr::Instance()->DoForAllMapsWithMapId(data->mapid, worker);
}

struct SpawnGameObjectInMapsWorker
{
    SpawnGameObjectInMapsWorker(uint32 guid, GameObjectData const* data)
      : i_guid(guid), i_data(data)
    {
    }

    void operator()(Map* map)
    {
        // Spawn if necessary (loaded grids only)
        auto pGameobject = new GameObject;
        // LOG_DEBUG(logging,"Spawning gameobject %u", *itr);
        if (!pGameobject->LoadFromDB(i_guid, map))
        {
            delete pGameobject;
        }
        else
        {
            if (pGameobject->isSpawnedByDefault())
                map->insert(pGameobject);
        }
    }

    uint32 i_guid;
    GameObjectData const* i_data;
};

void GameObject::SpawnInMaps(uint32 db_guid, GameObjectData const* data)
{
    SpawnGameObjectInMapsWorker worker(db_guid, data);
    sMapMgr::Instance()->DoForAllMapsWithMapId(data->mapid, worker);
}

bool GameObject::HasStaticDBSpawnData() const
{
    return sObjectMgr::Instance()->GetGOData(GetGUIDLow()) != nullptr;
}

void GameObject::EndRitual(bool success)
{
    // Ritual ended, clean up
    GameObjectInfo const* info = GetGOInfo();
    // Remove channeled spells
    for (const auto& elem : m_UniqueUsers)
    {
        if (Player* plr = GetMap()->GetPlayer(elem))
        {
            if (plr->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                plr->FinishSpell(CURRENT_CHANNELED_SPELL);
        }
    }

    // Ritual of Doom: kill a random participant (including the warlock)
    if (success && !m_UniqueUsers.empty() &&
        info->summoningRitual.spellId == 18541)
    {
        auto itr = m_UniqueUsers.begin();
        std::advance(itr, urand(0, m_UniqueUsers.size() - 1));
        auto guid = *itr;
        if (Player* p = GetMap()->GetPlayer(guid))
            p->CastSpell(p, 20625, true); // Ritual of Doom Sacrifice

        // Everyone else get marked with a dummy aura for having assisted
        // This dummy aura is used to know whom the doomguard should attack
        for (auto& guid : m_UniqueUsers)
            if (Player* p = GetMap()->GetPlayer(guid))
                p->CastSpell(p, 150057, true);
    }

    // can be deleted now, if
    if (!info->summoningRitual.ritualPersistent)
        SetLootState(GO_JUST_DEACTIVATED);
    // reset ritual for this GO
    else
        ClearAllUsesData();

    Unit* owner = GetOwner();
    if (!owner)
        return;

    if (success)
    {
        uint32 spellId = info->summoningRitual.spellId;
        if (!spellId)
            return;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
        if (!spellInfo)
        {
            logging.error(
                "WORLD: unknown spell id %u at use action for gameobject "
                "(Entry: %u GoType: %u )",
                spellId, GetEntry(), GetGoType());
            return;
        }

        auto spell = new Spell(owner, spellInfo,
            uint32(TRIGGER_TYPE_TRIGGERED | TRIGGER_TYPE_NO_CASTING_TIME),
            GetObjectGuid());

        SpellCastTargets targets;
        Player* ritual_target;
        if (spellInfo->HasEffect(SPELL_EFFECT_SUMMON_PLAYER) &&
            (ritual_target = ObjectAccessor::FindPlayer(ritual_target_)) !=
                nullptr)
            targets.setUnitTarget(ritual_target);
        else
            targets.setUnitTarget(owner);

        spell->prepare(&targets);
    }
}

void GameObject::SetLootState(LootState s, Unit* user /* = NULL */)
{
    m_lootState = s;
    if (AI())
        AI()->OnStateChanged(s, user);

    // Update collision on loot state change, except for doors (doors are
    // handled in SetGoState)
    if (m_model && GetGoType() != GAMEOBJECT_TYPE_DOOR)
    {
        // startOpen determines whether we are going to add or remove the LoS on
        // activation
        bool startOpen = (GetGoType() == GAMEOBJECT_TYPE_DOOR ||
                                  GetGoType() == GAMEOBJECT_TYPE_BUTTON ?
                              GetGOInfo()->door.startOpen :
                              false);

        if (const GameObjectData* data =
                sObjectMgr::Instance()->GetGOData(GetGUIDLow()))
            if (data->go_state == GO_STATE_ACTIVE)
                startOpen = !startOpen;

        if (s == GO_ACTIVATED || s == GO_JUST_DEACTIVATED)
            EnableCollision(startOpen);
        else if (s == GO_READY)
            EnableCollision(!startOpen);
    }

    if (s == GO_JUST_DEACTIVATED)
        despawn_time_ = 0;
}

void GameObject::SetGoState(GOState state)
{
    SetUInt32Value(GAMEOBJECT_STATE, state);
    if (m_model)
    {
        if (!IsInWorld())
            return;

        UpdateCollision();
    }
}

void GameObject::EnableCollision(bool enable)
{
    if (!m_model)
        return;

    if (enable)
        m_model->enable();
    else
        m_model->disable();
}

void GameObject::UpdateModel()
{
    if (!IsInWorld() || !m_goInfo->vmap)
        return;
    if (m_model)
        if (GetMap()->ContainsModel(*m_model))
            GetMap()->EraseModel(*m_model);
    delete m_model;
    m_model = GameObjectModel::Create(*this);
    if (m_model)
        GetMap()->InsertModel(*m_model);
}

void GameObject::UpdateCollision()
{
    if (!m_model || !IsInWorld())
        return;

    bool enable;
    switch (GetGoType())
    {
    case GAMEOBJECT_TYPE_DOOR:
    case GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING:
        // Ready is closed; active and active_alternative are open
        enable = GetGoState() == GO_STATE_READY;
        break;
    default:
        enable = true;
        break;
    }

    EnableCollision(enable);
}

void GameObject::CastSpell(Unit* target, uint32 spellId)
{
    const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
        return;

    if (spellInfo->HasTargetType(TARGET_SELF))
    {
        target->CastSpell(target, spellInfo, true);
        return;
    }

    // FIXME: This would actually need some form of Game Object casting to work
    // properly.
    // The main problem is that mangos spell system does not at all support that
    // (and
    // trinity's hacky solution is beyond horrible and not an option)
    Unit* owner = GetOwner();
    if (owner)
        owner->CastSpell(target, spellInfo, true, nullptr, nullptr,
            GetObjectGuid()); // set GO as real caster
    else
        target->CastSpell(target, spellInfo, true);
}

std::string GameObject::GetAIName() const
{
    return sObjectMgr::Instance()->GetGameObjectInfo(GetEntry())->AIName;
}

bool GameObject::AIM_Initialize()
{
    delete m_AI;
    m_AI = nullptr;

    if (GetAIName().compare("SmartGameObjectAI") == 0)
        m_AI = new SmartGameObjectAI(this);
    else
        m_AI = new NullGameObjectAI(this);

    if (!m_AI)
        return false;

    m_AI->InitializeAI();
    return true;
}

void GameObject::OnLootOpen(LootType lootType, Player* looter)
{
    if (m_lootDistributor)
        return;

    if (!isSpawned() && !looter->isGameMaster())
        return;

    if (getLootState() != GO_READY)
        return;

    switch (GetGoType())
    {
    case GAMEOBJECT_TYPE_CHEST:
    case GAMEOBJECT_TYPE_FISHINGHOLE:
    case GAMEOBJECT_TYPE_FISHINGNODE:
        m_lootDistributor = new loot_distributor(this, lootType);

        if (GetGoType() == GAMEOBJECT_TYPE_CHEST &&
            m_goInfo->chest.groupLootRules)
        {
            m_lootDistributor->recipient_mgr()->attempt_add_tap(looter);
            if (looter->GetMap()->IsDungeon()) // Dungeon or raid
            {
                // Add all people inside the dungeon/raid to the recipients
                // list, and mark loot as dungeon loot
                m_lootDistributor->set_dungeon_loot(true);
                if (Group* group = looter->GetGroup())
                {
                    for (auto member : group->members(true))
                    {
                        if (member->GetMap() == looter->GetMap())
                            m_lootDistributor->recipient_mgr()->attempt_add_tap(
                                member);
                    }
                }
            }
        }

        m_lootDistributor->generate_loot(looter);

        if (GetGoType() == GAMEOBJECT_TYPE_CHEST &&
            m_goInfo->chest.groupLootRules)
            m_lootDistributor->start_loot_session();
        break;
    default:
        return;
    }

    if (GetGoType() == GAMEOBJECT_TYPE_CHEST && GetGOInfo()->GetLootId())
    {
        if (GetGOInfo()->chest.groupLootRules)
            despawn_time_ = WorldTimer::time_no_syscall() + 5 * 60;
        else
            despawn_time_ = WorldTimer::time_no_syscall() + 2 * 60;
    }
}

void GameObject::SetCapturePointSlider(int8 value)
{
    GameObjectInfo const* info = GetGOInfo();

    // only activate non-locked capture point
    if (value >= 0)
    {
        m_captureSlider = value;
        SetLootState(GO_ACTIVATED);
    }
    else
        m_captureSlider = -value;

    // set the state of the capture point based on the slider value
    if ((int)m_captureSlider == CAPTURE_SLIDER_ALLIANCE)
        m_captureState = CAPTURE_STATE_WIN_ALLIANCE;
    else if ((int)m_captureSlider == CAPTURE_SLIDER_HORDE)
        m_captureState = CAPTURE_STATE_WIN_HORDE;
    else if (m_captureSlider >
             CAPTURE_SLIDER_MIDDLE + info->capturePoint.neutralPercent * 0.5f)
        m_captureState = CAPTURE_STATE_PROGRESS_ALLIANCE;
    else if (m_captureSlider <
             CAPTURE_SLIDER_MIDDLE - info->capturePoint.neutralPercent * 0.5f)
        m_captureState = CAPTURE_STATE_PROGRESS_HORDE;
    else
        m_captureState = CAPTURE_STATE_NEUTRAL;
}

void GameObject::TickCapturePoint()
{
    // TODO: On retail: Ticks every 5.2 seconds. slider value increase when new
    // player enters on tick

    GameObjectInfo const* info = GetGOInfo();
    float radius = info->capturePoint.radius;

    // search for players in radius
    auto capturingPlayers =
        maps::visitors::yield_set<Player>{}(this, radius, [](Player* p)
            {
                return p->CanUseCapturePoint();
            });

    GuidsSet tempUsers(m_UniqueUsers);
    uint32 neutralPercent = info->capturePoint.neutralPercent;
    int oldValue = m_captureSlider;
    int rangePlayers = 0;

    for (auto& capturingPlayer : capturingPlayers)
    {
        if ((capturingPlayer)->GetTeam() == ALLIANCE)
            ++rangePlayers;
        else
            --rangePlayers;

        ObjectGuid guid = (capturingPlayer)->GetObjectGuid();
        if (!tempUsers.erase(guid))
        {
            // new player entered capture point zone
            m_UniqueUsers.insert(guid);

            // send capture point enter packets
            (capturingPlayer)
                ->SendUpdateWorldState(
                    info->capturePoint.worldState3, neutralPercent);
            (capturingPlayer)
                ->SendUpdateWorldState(
                    info->capturePoint.worldState2, oldValue);
            (capturingPlayer)
                ->SendUpdateWorldState(
                    info->capturePoint.worldState1, WORLD_STATE_ADD);
            (capturingPlayer)
                ->SendUpdateWorldState(info->capturePoint.worldState2,
                    oldValue); // also redundantly sent on retail to prevent
            // displaying the initial capture direction on client
            // capture slider incorrectly
        }
    }

    for (const auto& tempUser : tempUsers)
    {
        // send capture point leave packet
        if (Player* owner = GetMap()->GetPlayer(tempUser))
            owner->SendUpdateWorldState(
                info->capturePoint.worldState1, WORLD_STATE_REMOVE);

        // player left capture point zone
        m_UniqueUsers.erase(tempUser);
    }

    // return if there are not enough players capturing the point (works because
    // minSuperiority is always 1)
    if (rangePlayers == 0)
    {
        // set to inactive if all players left capture point zone
        if (m_UniqueUsers.empty())
            SetActiveObjectState(false);
        return;
    }

    // prevents unloading gameobject before all players left capture point zone
    // (to prevent m_UniqueUsers not being cleared if grid is set to idle)
    SetActiveObjectState(true);

    // cap speed
    int maxSuperiority = info->capturePoint.maxSuperiority;
    if (rangePlayers > maxSuperiority)
        rangePlayers = maxSuperiority;
    else if (rangePlayers < -maxSuperiority)
        rangePlayers = -maxSuperiority;

    // time to capture from 0% to 100% is maxTime for minSuperiority amount of
    // players and minTime for maxSuperiority amount of players (linear
    // function: y = dy/dx*x+d)
    float deltaSlider = info->capturePoint.minTime;

    if (int deltaSuperiority =
            maxSuperiority - info->capturePoint.minSuperiority)
        deltaSlider +=
            (float)(maxSuperiority - abs(rangePlayers)) / deltaSuperiority *
            (info->capturePoint.maxTime - info->capturePoint.minTime);

    // calculate changed slider value for a duration of 5 seconds (5 * 100%)
    deltaSlider = 500.0f / deltaSlider;

    Team progressFaction;
    if (rangePlayers > 0)
    {
        progressFaction = ALLIANCE;
        m_captureSlider += deltaSlider;
        if (m_captureSlider > CAPTURE_SLIDER_ALLIANCE)
            m_captureSlider = CAPTURE_SLIDER_ALLIANCE;
    }
    else
    {
        progressFaction = HORDE;
        m_captureSlider -= deltaSlider;
        if (m_captureSlider < CAPTURE_SLIDER_HORDE)
            m_captureSlider = CAPTURE_SLIDER_HORDE;
    }

    // return if slider did not move a whole percent
    if ((int)m_captureSlider == oldValue)
        return;

    // on retail this is also sent to newly added players even though they
    // already received a slider value
    for (auto& capturingPlayer : capturingPlayers)
        (capturingPlayer)
            ->SendUpdateWorldState(
                info->capturePoint.worldState2, (uint32)m_captureSlider);

    // send capture point events
    uint32 eventId = 0;

    /* WIN EVENTS */
    // alliance wins tower with max points
    if (m_captureState != CAPTURE_STATE_WIN_ALLIANCE &&
        (int)m_captureSlider == CAPTURE_SLIDER_ALLIANCE)
    {
        eventId = info->capturePoint.winEventID1;
        m_captureState = CAPTURE_STATE_WIN_ALLIANCE;
    }
    // horde wins tower with max points
    else if (m_captureState != CAPTURE_STATE_WIN_HORDE &&
             (int)m_captureSlider == CAPTURE_SLIDER_HORDE)
    {
        eventId = info->capturePoint.winEventID2;
        m_captureState = CAPTURE_STATE_WIN_HORDE;
    }

    /* PROGRESS EVENTS */
    // alliance takes the tower from neutral, contested or horde (if there is no
    // neutral area) to alliance
    else if (m_captureState != CAPTURE_STATE_PROGRESS_ALLIANCE &&
             m_captureSlider > CAPTURE_SLIDER_MIDDLE + neutralPercent * 0.5f &&
             progressFaction == ALLIANCE)
    {
        eventId = info->capturePoint.progressEventID1;

        // handle objective complete
        if (m_captureState == CAPTURE_STATE_NEUTRAL)
            if (OutdoorPvP* outdoorPvP =
                    sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
            {
                std::list<Player*> l(
                    capturingPlayers.begin(), capturingPlayers.end());
                outdoorPvP->HandleObjectiveComplete(
                    eventId, l, progressFaction);
            }

        // set capture state to alliance
        m_captureState = CAPTURE_STATE_PROGRESS_ALLIANCE;
    }
    // horde takes the tower from neutral, contested or alliance (if there is no
    // neutral area) to horde
    else if (m_captureState != CAPTURE_STATE_PROGRESS_HORDE &&
             m_captureSlider < CAPTURE_SLIDER_MIDDLE - neutralPercent * 0.5f &&
             progressFaction == HORDE)
    {
        eventId = info->capturePoint.progressEventID2;

        // handle objective complete
        if (m_captureState == CAPTURE_STATE_NEUTRAL)
            if (OutdoorPvP* outdoorPvP =
                    sOutdoorPvPMgr::Instance()->GetScript(GetZoneId()))
            {
                std::list<Player*> l(
                    capturingPlayers.begin(), capturingPlayers.end());
                outdoorPvP->HandleObjectiveComplete(
                    eventId, l, progressFaction);
            }

        // set capture state to horde
        m_captureState = CAPTURE_STATE_PROGRESS_HORDE;
    }

    /* NEUTRAL EVENTS */
    // alliance takes the tower from horde to neutral
    else if (m_captureState != CAPTURE_STATE_NEUTRAL &&
             m_captureSlider >= CAPTURE_SLIDER_MIDDLE - neutralPercent * 0.5f &&
             m_captureSlider <= CAPTURE_SLIDER_MIDDLE + neutralPercent * 0.5f &&
             progressFaction == ALLIANCE)
    {
        eventId = info->capturePoint.neutralEventID1;
        m_captureState = CAPTURE_STATE_NEUTRAL;
    }
    // horde takes the tower from alliance to neutral
    else if (m_captureState != CAPTURE_STATE_NEUTRAL &&
             m_captureSlider >= CAPTURE_SLIDER_MIDDLE - neutralPercent * 0.5f &&
             m_captureSlider <= CAPTURE_SLIDER_MIDDLE + neutralPercent * 0.5f &&
             progressFaction == HORDE)
    {
        eventId = info->capturePoint.neutralEventID2;
        m_captureState = CAPTURE_STATE_NEUTRAL;
    }

    /* CONTESTED EVENTS */
    // alliance attacks tower which is in control or progress by horde (except
    // if alliance also gains control in that case)
    else if ((m_captureState == CAPTURE_STATE_WIN_HORDE ||
                 m_captureState == CAPTURE_STATE_PROGRESS_HORDE) &&
             progressFaction == ALLIANCE)
    {
        eventId = info->capturePoint.contestedEventID1;
        m_captureState = CAPTURE_STATE_CONTEST_HORDE;
    }
    // horde attacks tower which is in control or progress by alliance (except
    // if horde also gains control in that case)
    else if ((m_captureState == CAPTURE_STATE_WIN_ALLIANCE ||
                 m_captureState == CAPTURE_STATE_PROGRESS_ALLIANCE) &&
             progressFaction == HORDE)
    {
        eventId = info->capturePoint.contestedEventID2;
        m_captureState = CAPTURE_STATE_CONTEST_ALLIANCE;
    }

    if (eventId)
        StartEvents_Event(
            GetMap(), eventId, this, this, true, *capturingPlayers.begin());
}

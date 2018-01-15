/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "precompiled.h"
#include "GameEventMgr.h"
#include "zulgurub.h"

instance_zulgurub::instance_zulgurub(Map* map)
  : ScriptedInstance(map), update_hakkar_(false)
{
    Initialize();
}

void instance_zulgurub::Initialize()
{
    memset(&encounters_, 0, sizeof(encounters_));
    memset(&data_, 0, sizeof(data_));
}

bool instance_zulgurub::IsEncounterInProgress() const
{
    for (auto& encounter : encounters_)
    {
        if (encounter == IN_PROGRESS)
            return true;
    }

    return false;
}

void instance_zulgurub::SetData(uint32 type, uint32 data)
{
    if (type < NUM_ENCOUNTERS)
    {
        encounters_[type] = data;
        if (data == DONE)
        {
            OUT_SAVE_INST_DATA;

            std::ostringstream stream;
            for (int i = 0; i < NUM_ENCOUNTERS; ++i)
                stream << encounters_[i] << " ";

            save_to_db_ = stream.str();
            SaveToDB();

            OUT_SAVE_INST_DATA_COMPLETE;
        }

        if (type == TYPE_ARLOKK)
        {
            if (data == FAIL)
            {
                if (auto go =
                        GetSingleGameObjectFromStorage(GO_GONG_OF_BETHEKK))
                    go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            }
            else if (data == IN_PROGRESS)
            {
                SetData(TYPE_ARLOKK_TP_PLACE, 1);
                SetData(TYPE_ARLOKK_PANTHERS_COUNT, 0);
            }

            if (data < SPECIAL)
                DoUseDoorOrButton(GO_FORCEFIELD);
        }

        // Update hakkar power
        if (type < TYPE_HAKKAR && data == DONE)
            update_hakkar_ = true;

        if (type == TYPE_MANDOKIR)
            chained_spirits_.clear();
    }
    else if (type < NUM_ENCOUNTERS + NUM_DATA)
    {
        if (type == TYPE_ARLOKK_TP_PLACE && data == 0)
        {
            if (auto c = GetSingleCreatureFromStorage(NPC_ARLOKK))
            {
                if (GetData(type) == 1)
                    c->NearTeleportTo(-11517.1f, -1650.4f, 41.3f, 0.0f);
                else
                    c->NearTeleportTo(-11518.6, -1606.9f, 41.3f, 0.0f);
            }

            if (GetData(type) == 1)
                data = 2;
            else
                data = 1;
        }

        data_[type - NUM_ENCOUNTERS] = data;
    }
}

uint32 instance_zulgurub::GetData(uint32 type)
{
    if (type < NUM_ENCOUNTERS)
        return encounters_[type];
    else if (type < NUM_ENCOUNTERS + NUM_DATA)
        return data_[type - NUM_ENCOUNTERS];
    return 0;
}

void instance_zulgurub::OnObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
    case GO_GONG_OF_BETHEKK:
        if (GetData(TYPE_ARLOKK) == DONE)
            go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        break;
    case GO_FORCEFIELD:
        break;
    default:
        return;
    }

    m_mGoEntryGuidStore[go->GetEntry()] = go->GetObjectGuid();
}

void instance_zulgurub::OnCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
    case NPC_ARLOKK:
    case NPC_JINDO:
        break;
    case NPC_ZULIAN_PROWLER:
        SetData(TYPE_ARLOKK_PANTHERS_COUNT,
            GetData(TYPE_ARLOKK_PANTHERS_COUNT) + 1);
        return;
    case NPC_HAKKAR:
        update_hakkar_ = true;
        break;
    case NPC_CHAINED_SPIRIT:
        chained_spirits_.push_back(creature->GetObjectGuid());
        return;
    default:
        return;
    }

    m_mNpcEntryGuidStore[creature->GetEntry()] = creature->GetObjectGuid();
}

void instance_zulgurub::OnCreatureDeath(Creature* creature)
{
    if (creature->GetEntry() == NPC_ZULIAN_PROWLER)
    {
        if (GetData(TYPE_ARLOKK_PANTHERS_COUNT) > 0)
            SetData(TYPE_ARLOKK_PANTHERS_COUNT,
                GetData(TYPE_ARLOKK_PANTHERS_COUNT) - 1);
    }
}

void instance_zulgurub::OnCreatureEvade(Creature* creature)
{
    if (creature->GetEntry() == NPC_HAKKAR)
        update_hakkar_ = true;
}

void instance_zulgurub::OnPlayerDeath(Player* player)
{
    // Only ress players inside Mandokirs room
    if (!player->IsWithinDist3d(-12196, -1948, 130, 90))
        return;
    if (!chained_spirits_.empty())
    {
        auto index = urand(0, chained_spirits_.size() - 1);
        auto guid = chained_spirits_[index];
        chained_spirits_.erase(chained_spirits_.begin() + index);
        if (auto spirit = instance->GetCreature(guid))
        {
            // Stop and wait for despawn after revive
            spirit->movement_gens.push(
                new movement::StoppedMovementGenerator());
            // Use mmaps to get to corpse
            spirit->movement_gens.push(new movement::PointMovementGenerator(0,
                player->GetX(), player->GetY(), player->GetZ(), true, false));
            // Put a point with id 1 after the mmaps movement that will make
            // sure we're on the exact X, Y, Z of the player
            spirit->movement_gens.push(
                new movement::PointMovementGenerator(1, player->GetX(),
                    player->GetY(), player->GetZ(), false, false),
                0, movement::get_default_priority(movement::gen::point) - 1);
        }
    }
}

void instance_zulgurub::Update(uint32 /*diff*/)
{
    if (update_hakkar_)
    {
        if (auto hakkar = GetSingleCreatureFromStorage(NPC_HAKKAR))
        {
            update_hakkar_power(hakkar);
            update_hakkar_ = false;
        }
    }
}

void instance_zulgurub::Load(const char* data)
{
    if (!data)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(data);

    std::istringstream stream(data);
    for (int i = 0; i < NUM_ENCOUNTERS; ++i)
        stream >> encounters_[i];

    for (auto& encounter : encounters_)
    {
        if (encounter == IN_PROGRESS)
            encounter = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

void instance_zulgurub::update_hakkar_power(Creature* hakkar)
{
    hakkar->remove_auras(24692);
    for (int i = TYPE_JEKLIK; i < TYPE_HAKKAR; ++i)
        if (GetData(i) != DONE)
            hakkar->CastSpell(hakkar, 24692, true);
}

InstanceData* GetInstData_ZG(Map* map)
{
    return new instance_zulgurub(map);
};

bool GOUse_gong_of_bethekk(Player* /*player*/, GameObject* go)
{
    if (go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
        return true;

    if (auto map = go->GetMap())
    {
        if (auto inst = map->GetInstanceData())
        {
            if (inst->GetData(TYPE_ARLOKK) != DONE &&
                inst->GetData(TYPE_ARLOKK) != IN_PROGRESS)
            {
                go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                go->SummonCreature(NPC_ARLOKK, -11523.2f, -1627.8f, 41.3f, 3.1f,
                    TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    sWorld::Instance()->getConfig(
                        CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                        1000);
                // Arlokk script plays gong sound effect
            }
        }
    }

    return true;
}

bool GOUse_go_brazier_of_madness(Player* /*player*/, GameObject* go)
{
    if (go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
        return true;

    if (auto map = go->GetMap())
    {
        if (auto inst = map->GetInstanceData())
        {
            if (inst->GetData(TYPE_EDGE_OF_MADNESS) != DONE &&
                inst->GetData(TYPE_EDGE_OF_MADNESS) != IN_PROGRESS &&
                inst->GetData(TYPE_EDGE_OF_MADNESS) != SPECIAL)
            {
                /* go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                inst->SetData(TYPE_EDGE_OF_MADNESS, SPECIAL); */

                uint32 entry = 0;
                if (sGameEventMgr::Instance()->IsActiveEvent(29))
                    entry = 15082; // Gri'lek
                else if (sGameEventMgr::Instance()->IsActiveEvent(30))
                    entry = 15083; // Hazza'rah
                else if (sGameEventMgr::Instance()->IsActiveEvent(31))
                    entry = 15084; // Renataki
                else if (sGameEventMgr::Instance()->IsActiveEvent(32))
                    entry = 15085; // Wushoolay
                else
                    return true;

                // Notify lightning
                auto lightnings =
                    GetGameObjectListWithEntryInGrid(go, 180252, 100.0f);
                if (!lightnings.empty())
                {
                    auto animate = [lightnings](uint32 delay, bool all)
                    {
                        if (all)
                        {
                            for (auto lightning : lightnings)
                                lightning->queue_action(delay, [lightning]
                                    {
                                        lightning->SendGameObjectCustomAnim(
                                            lightning->GetObjectGuid());
                                    });
                        }
                        else
                        {
                            auto lightning =
                                lightnings[urand(0, lightnings.size() - 1)];
                            lightning->queue_action(delay, [lightning]
                                {
                                    lightning->SendGameObjectCustomAnim(
                                        lightning->GetObjectGuid());
                                });
                        }
                    };

                    animate(7000, true);
                    animate(14000, true);
                    for (int i = 0; i < 15000; i += urand(300, 700))
                        animate(i, false);
                }

                // TODO: This cloud portals are not the right ones I think
                go->SummonGameObject(
                    180253, -11901.6f, -1906.8f, 65.4f, 0, 0, 0, 0, 0, 12);
                go->SummonGameObject(
                    180254, -11901.6f, -1906.8f, 65.4f, 0, 0, 0, 0, 0, 12);

                go->queue_action(10000, [go, entry]()
                    {
                        go->SummonCreature(entry, -11899.1f, -1903.8f, 65.15f,
                            0.9f, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                            sWorld::Instance()->getConfig(
                                CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS) *
                                1000);
                    });
            }
        }
    }

    return true;
}

void AddSC_instance_zulgurub()
{
    auto script = new Script;
    script->Name = "instance_zulgurub";
    script->GetInstanceData = GetInstData_ZG;
    script->RegisterSelf();

    script = new Script;
    script->Name = "go_gong_of_bethekk";
    script->pGOUse = GOUse_gong_of_bethekk;
    script->RegisterSelf();

    script = new Script;
    script->Name = "go_brazier_of_madness";
    script->pGOUse = GOUse_go_brazier_of_madness;
    script->RegisterSelf();
}

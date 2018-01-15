/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "precompiled.h"
#include "aq40.h"
#include <G3D/Vector4.h>

instance_aq40::instance_aq40(Map* map)
  : ScriptedInstance(map), eject_cooldown_(0), spawned_eye_(false)
{
    Initialize();
}

void instance_aq40::Initialize()
{
    memset(&encounters_, 0, sizeof(encounters_));
    memset(&data_, 0, sizeof(data_));
}

bool instance_aq40::IsEncounterInProgress() const
{
    for (auto& encounter : encounters_)
    {
        if (encounter == IN_PROGRESS)
            return true;
    }

    return false;
}

void instance_aq40::SetData(uint32 type, uint32 data)
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
    }

    switch (type)
    {
    case TYPE_SKERAM:
        if (data == DONE)
            DoUseDoorOrButton(GO_SKERAM_DOOR);
        break;
    case TYPE_TWIN_EMPERORS:
        DoUseDoorOrButton(GO_TWINS_ENTRY_DOOR);
        if (data == DONE)
            DoUseDoorOrButton(GO_TWINS_EXIT_DOOR);
        break;
    case DATA_TRIGGER_SKERAM_SPLIT:
    {
        skeram_split();
        break;
    }
    case DATA_TRIGGER_VISCIDUS_SPAWN_GLOBS:
    {
        viscidus_globs();
        break;
    }
    case DATA_TRIGGER_TWIN_SHARE_INFO:
    {
        twins_share_info();
        break;
    }
    case DATA_TRIGGER_TWIN_TELEPORT:
    {
        twins_teleport();
        break;
    }
    }
}

uint32 instance_aq40::GetData(uint32 type)
{
    if (type < NUM_ENCOUNTERS)
        return encounters_[type];
    return 0;
}

void instance_aq40::OnObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
    case GO_SKERAM_DOOR:
        if (GetData(TYPE_SKERAM) == DONE)
            go->UseDoorOrButton(0, false);
        break;
    case GO_TWINS_ENTRY_DOOR:
        break;
    case GO_TWINS_EXIT_DOOR:
        if (GetData(TYPE_TWIN_EMPERORS) == DONE)
            go->UseDoorOrButton(0, false);
        break;
    default:
        return;
    }

    m_mGoEntryGuidStore[go->GetEntry()] = go->GetObjectGuid();
}

void instance_aq40::OnCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
    case NPC_OURO_SPAWNER:
        if (GetData(TYPE_OURO) != DONE && creature->isDead())
            creature->Respawn();
        return;
    case NPC_SKERAM:
        if (creature->IsTemporarySummon())
            return;
        break;
    case NPC_SARTURA:
    case NPC_VISCIDUS:
    case NPC_MASTERS_EYE:
    case NPC_EMPEROR_VEKNILASH:
    case NPC_EMPEROR_VEKLOR:
    case NPC_CTHUN:
        break;
    default:
        return;
    }

    m_mNpcEntryGuidStore[creature->GetEntry()] = creature->GetObjectGuid();
}

void instance_aq40::Update(uint32 diff)
{
    if (!spawned_eye_ && GetData(TYPE_TWIN_EMPERORS) != DONE)
    {
        instance->SummonCreature(NPC_MASTERS_EYE, -8954, 1234, -100, 5.2f,
            TEMPSUMMON_MANUAL_DESPAWN, 0);
        spawned_eye_ = true;
    }
}

void instance_aq40::Load(const char* data)
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

void instance_aq40::handle_area_trigger(
    Player* player, const AreaTriggerEntry* at)
{
    switch (at->id)
    {
    case 4033:
        if (eject_cooldown_ < WorldTimer::time_no_syscall())
        {
            eject_cooldown_ = WorldTimer::time_no_syscall() + 5;
            player->SummonCreature(15800, -8546.2f, 1987.4f, -96.5f, 0,
                TEMPSUMMON_TIMED_DESPAWN, 10000);
        }
        break;
    case 4034:
    {
        player->NearTeleportTo(-8578, 1986, 101, player->GetO());
        player->queue_action(0, [player]()
            {
                int angle = (WorldTimer::getMSTime() / 10) % 360;
                player->KnockBack(angle * (M_PI_F / 180.0f), 50.0f, 15.0f);
                player->remove_auras(26476); // Remove digestive acid
            });
        break;
    }
    case 4047: // Twin Emperors Masters Eye RP event
        if (auto masters_eye = GetSingleCreatureFromStorage(NPC_MASTERS_EYE))
            masters_eye->AI()->Notify(1);
        break;
    case 4052:
        if (auto sartura = GetSingleCreatureFromStorage(NPC_SARTURA))
            if (sartura->isAlive() && sartura->AI() && !sartura->isInCombat())
                sartura->AI()->AttackStart(player);
        break;
    }
}

void instance_aq40::skeram_split()
{
    auto skeram = GetSingleCreatureFromStorage(NPC_SKERAM);
    if (!skeram)
        return;

    std::vector<G3D::Vector4> spots = {
        G3D::Vector4(-8308, 2061, 133.1, 0.8),
        G3D::Vector4(-8333, 2122, 133.1, 6.0),
        G3D::Vector4(-8344, 2082, 125.7, 0.4),
    };

    auto& tl = skeram->getThreatManager().getThreatList();
    for (auto& ref : tl)
    {
        auto unit = ref->getTarget();
        if (unit && unit->GetTypeId() == TYPEID_PLAYER)
            if (auto group = static_cast<Player*>(unit)->GetGroup())
                group->ClearTargetIcon(skeram->GetObjectGuid());
    }

    auto idx = urand(0, 2);
    skeram->SetVisibility(VISIBILITY_OFF);
    skeram->queue_action(0, [skeram]()
        {
            skeram->SetVisibility(VISIBILITY_ON);
        });
    skeram->NearTeleportTo(
        spots[idx].x, spots[idx].y, spots[idx].z, spots[idx].w);

    for (int i = 0; i < 2; ++i)
    {
        spots.erase(spots.begin() + idx);
        idx = urand(0, spots.size() - 1);
        if (auto c = skeram->SummonCreature(NPC_SKERAM, spots[idx].x,
                spots[idx].y, spots[idx].z, spots[idx].w,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000, SUMMON_OPT_NO_LOOT))
        {
            c->SetMaxHealth(skeram->GetMaxHealth() * 0.1f);
            c->SetHealth(skeram->GetHealth() * 0.1f);
            c->SetPower(POWER_MANA, skeram->GetPower(POWER_MANA));
        }
    }
}

void instance_aq40::viscidus_globs()
{
    auto viscidus = GetSingleCreatureFromStorage(NPC_VISCIDUS);
    if (!viscidus || viscidus->GetMaxHealth() == 0)
        return;

    int max =
        (viscidus->GetHealth() / (float)viscidus->GetMaxHealth() + 0.05f) /
        0.05f;
    if (max > 20)
        max = 20;
    else if (max < 1)
        max = 1;

    std::vector<uint32> available;
    for (int i = 0; i < 20; ++i)
        available.push_back(25865 + i);

    for (int i = 0; i < max; ++i)
    {
        auto index = urand(0, available.size() - 1);
        viscidus->CastSpell(viscidus, available[index], true);
        available.erase(available.begin() + index);
    }

    for (int i = 0; i < 20 - max; ++i)
        viscidus->AI()->Notify(1);
}

void instance_aq40::twins_share_info()
{
    auto veknilash = GetSingleCreatureFromStorage(NPC_EMPEROR_VEKNILASH);
    auto veklor = GetSingleCreatureFromStorage(NPC_EMPEROR_VEKLOR);
    if (!veknilash || !veklor || veknilash->isDead() || veklor->isDead() ||
        veknilash->GetMaxHealth() == 0 || veklor->GetMaxHealth() == 0)
        return;

    float veknilash_pct =
        veknilash->GetHealth() / (float)veknilash->GetMaxHealth();
    float veklor_pct = veklor->GetHealth() / (float)veklor->GetMaxHealth();

    if (veknilash_pct > veklor_pct)
    {
        int newhp = veknilash->GetMaxHealth() * veklor_pct;
        if (newhp < 1)
            newhp = 1;
        veknilash->SetHealth(newhp);
    }

    if (veklor_pct > veknilash_pct)
    {
        int newhp = veklor->GetMaxHealth() * veknilash_pct;
        if (newhp < 1)
            newhp = 1;
        veklor->SetHealth(newhp);
    }
}

void instance_aq40::twins_teleport()
{
    auto veknilash = GetSingleCreatureFromStorage(NPC_EMPEROR_VEKNILASH);
    auto veklor = GetSingleCreatureFromStorage(NPC_EMPEROR_VEKLOR);
    if (!veknilash || !veklor || veknilash->isDead() || veklor->isDead())
        return;

    float x, y, z, o, x_, y_, z_, o_;
    veklor->GetPosition(x, y, z);
    o = veklor->GetO();
    veknilash->GetPosition(x_, y_, z_);
    o_ = veknilash->GetO();

    veklor->NearTeleportTo(x_, y_, z_, o_);
    veknilash->NearTeleportTo(x, y, z, o);
}

InstanceData* GetInstData_AQ40(Map* map)
{
    return new instance_aq40(map);
};

bool at_aq40(Player* player, const AreaTriggerEntry* at)
{
    if (player->isGameMaster())
        return false;
    if (auto map = player->GetMap())
    {
        if (auto instdata = map->GetInstanceData())
            if (auto aq40 = dynamic_cast<instance_aq40*>(instdata))
                aq40->handle_area_trigger(player, at);
    }
    return true;
}

void AddSC_instance_aq40()
{
    auto script = new Script;
    script->Name = "instance_aq40";
    script->GetInstanceData = GetInstData_AQ40;
    script->RegisterSelf();

    script = new Script;
    script->Name = "at_aq40";
    script->pAreaTrigger = at_aq40;
    script->RegisterSelf();
}

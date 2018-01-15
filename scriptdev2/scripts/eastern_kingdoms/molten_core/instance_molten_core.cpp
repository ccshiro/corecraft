/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "precompiled.h"
#include "molten_core.h"

instance_molten_core::instance_molten_core(Map* map)
  : ScriptedInstance(map), update_runes_(false)
{
    Initialize();
}

void instance_molten_core::Initialize()
{
    memset(&encounters_, 0, sizeof(encounters_));
}

bool instance_molten_core::IsEncounterInProgress() const
{
    for (auto& encounter : encounters_)
    {
        if (encounter == IN_PROGRESS)
            return true;
    }

    return false;
}

void instance_molten_core::SetData(uint32 type, uint32 data)
{
    if (type < NUM_ENCOUNTERS)
    {
        encounters_[type] = data;
        if (data == DONE ||
            (type > TYPE_LUCIFRON && type < TYPE_MAJORDOMO && data == SPECIAL))
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

    if (type > TYPE_LUCIFRON && type < TYPE_MAJORDOMO)
        update_majordomo_runes();
}

uint32 instance_molten_core::GetData(uint32 type)
{
    if (type < NUM_ENCOUNTERS)
        return encounters_[type];
    return 0;
}

void instance_molten_core::OnObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
    case GO_RUNE_OF_KRESS:
    case GO_RUNE_OF_MOHN:
    case GO_RUNE_OF_BLAZ:
    case GO_RUNE_OF_MAZJ:
    case GO_RUNE_OF_ZETH:
    case GO_RUNE_OF_THERI:
    case GO_RUNE_OF_KORO:
        break;
    default:
        return;
    }

    m_mGoEntryGuidStore[go->GetEntry()] = go->GetObjectGuid();
    update_runes_ = true;
}

void instance_molten_core::OnCreatureCreate(Creature* creature)
{
    if (creature->GetEntry() == NPC_MAJORDOMO)
        m_mNpcEntryGuidStore[creature->GetEntry()] = creature->GetObjectGuid();
}

void instance_molten_core::Update(uint32 /*diff*/)
{
    if (update_runes_)
    {
        update_runes_ = false;
        update_majordomo_runes();
    }

    if (GetData(TYPE_MAJORDOMO) == DONE)
    {
        // Summon Majordomo at ragnaros
        SetData(TYPE_MAJORDOMO, MAJORDOMO_SPECIAL2);
        instance->SummonCreature(NPC_MAJORDOMO, 847.103f, -816.153f, -229.755f,
            4.344f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
    }
}

void instance_molten_core::Load(const char* data)
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

    if (encounters_[TYPE_RAGNAROS] != DONE &&
        (encounters_[TYPE_MAJORDOMO] == SPECIAL ||
            encounters_[TYPE_MAJORDOMO] == MAJORDOMO_SPECIAL2))
        encounters_[TYPE_MAJORDOMO] = DONE;

    OUT_LOAD_INST_DATA_COMPLETE;
}

void instance_molten_core::update_majordomo_runes()
{
    if (GetData(TYPE_MAJORDOMO) == DONE || GetData(TYPE_MAJORDOMO) == SPECIAL)
        return;

    if (GetSingleCreatureFromStorage(NPC_MAJORDOMO) != nullptr)
        return;

    bool doused_all = true;
    auto do_rune = [this, &doused_all](uint32 type, uint32 entry)
    {
        if (auto go = GetSingleGameObjectFromStorage(entry))
        {
            if (GetData(type) == DONE)
                go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            else
                go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
        }

        if (GetData(type) != SPECIAL)
            doused_all = false;
    };

    do_rune(TYPE_MAGMADAR, GO_RUNE_OF_KRESS);
    do_rune(TYPE_GEHENNAS, GO_RUNE_OF_MOHN);
    do_rune(TYPE_GARR, GO_RUNE_OF_BLAZ);
    do_rune(TYPE_SHAZZRAH, GO_RUNE_OF_MAZJ);
    do_rune(TYPE_BARON_GEDDON, GO_RUNE_OF_ZETH);
    do_rune(TYPE_GOLEMAGG, GO_RUNE_OF_THERI);
    do_rune(TYPE_SULFURON, GO_RUNE_OF_KORO);

    // Spawn Majordomo if all runes are doused
    if (doused_all)
    {
        instance->SummonCreature(NPC_MAJORDOMO, 758.089f, -1176.71f, -118.64f,
            3.12144f, TEMPSUMMON_MANUAL_DESPAWN, 0);
        SetData(
            TYPE_MAJORDOMO, SPECIAL); // Prevent possiblity of multiple spawns
    }
}

InstanceData* GetInstData_MC(Map* map)
{
    return new instance_molten_core(map);
};

bool GOUse_rune_of_warding(Player* /*player*/, GameObject* go)
{
    if (go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
        return true;

    if (auto map = go->GetMap())
    {
        uint32 type;
        switch (go->GetEntry())
        {
        case GO_RUNE_OF_KRESS:
            type = TYPE_MAGMADAR;
            break;
        case GO_RUNE_OF_MOHN:
            type = TYPE_GEHENNAS;
            break;
        case GO_RUNE_OF_BLAZ:
            type = TYPE_GARR;
            break;
        case GO_RUNE_OF_MAZJ:
            type = TYPE_SHAZZRAH;
            break;
        case GO_RUNE_OF_ZETH:
            type = TYPE_BARON_GEDDON;
            break;
        case GO_RUNE_OF_THERI:
            type = TYPE_GOLEMAGG;
            break;
        case GO_RUNE_OF_KORO:
            type = TYPE_SULFURON;
            break;
        default:
            return true;
        }
        if (auto inst = map->GetInstanceData())
            inst->SetData(type, SPECIAL);
    }

    return true;
}

void AddSC_instance_molten_core()
{
    auto script = new Script;
    script->Name = "instance_molten_core";
    script->GetInstanceData = GetInstData_MC;
    script->RegisterSelf();

    script = new Script;
    script->Name = "go_rune_of_warding";
    script->pGOUse = GOUse_rune_of_warding;
    script->RegisterSelf();
}

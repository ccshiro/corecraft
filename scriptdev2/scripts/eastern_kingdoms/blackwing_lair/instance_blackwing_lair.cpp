/* Copyright (C) 2015 Corecraft <https://www.worldofcorecraft.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "precompiled.h"
#include "blackwing_lair.h"

static ObjectGuid spawn_spots[8] = {
    // South
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007559),
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007560),
    // East
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007561),
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007562),
    // North
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007563),
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007564),
    // West
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007565),
    ObjectGuid(HIGHGUID_UNIT, 100184, 1007566),
};

instance_blackwing_lair::instance_blackwing_lair(Map* map)
  : ScriptedInstance(map), chromaggus_colors_(0), nefarian_colors_(0),
    razorgore_add_timer_(0), wave_(0), spawned_vael_rp_trigger_(false)
{
    Initialize();
}

void instance_blackwing_lair::Initialize()
{
    memset(&encounters_, 0, sizeof(encounters_));
    memset(&data_, 0, sizeof(data_));
}

bool instance_blackwing_lair::IsEncounterInProgress() const
{
    for (auto& encounter : encounters_)
    {
        if (encounter == IN_PROGRESS)
            return true;
    }

    return false;
}

void instance_blackwing_lair::SetData(uint32 type, uint32 data)
{
    if (type < NUM_ENCOUNTERS)
    {
        bool save_colors = false;
        if (chromaggus_colors_ == 0)
        {
            // Colors are: 0-4
            uint32 color_one = urand(0, 4), color_two = urand(0, 4);
            while (color_one == color_two)
                color_two = urand(0, 4);
            chromaggus_colors_ = color_one | (color_two << 8);
            save_colors = true;
        }
        if (nefarian_colors_ == 0)
        {
            // Colors are: 0-4
            uint32 color_one = urand(0, 4), color_two = urand(0, 4);
            while (color_one == color_two)
                color_two = urand(0, 4);
            nefarian_colors_ = color_one | (color_two << 8);
            save_colors = true;
        }

        encounters_[type] = data;
        if (data == DONE || save_colors)
        {
            OUT_SAVE_INST_DATA;

            std::ostringstream stream;
            for (int i = 0; i < NUM_ENCOUNTERS; ++i)
            {
                // Pack color with corresonding encounter in DB
                if (i == TYPE_CHROMAGGUS)
                    stream << (encounters_[i] | (chromaggus_colors_ << 8))
                           << " ";
                else if (i == TYPE_NEFARIAN)
                    stream << (encounters_[i] | (nefarian_colors_ << 8)) << " ";
                else
                    stream << encounters_[i] << " ";
            }

            save_to_db_ = stream.str();
            SaveToDB();

            OUT_SAVE_INST_DATA_COMPLETE;
        }
    }
    else if (type < NUM_ENCOUNTERS + NUM_DATA)
    {
        if (type == DATA_DESTROYED_EGGS_COUNT)
        {
            if (data == 1)
                data = GetData(type) + 1;
            if (data == RAZORGORE_EGGS_COUNT)
            {
                if (auto razorgore =
                        GetSingleCreatureFromStorage(NPC_RAZORGORE))
                {
                    razorgore->remove_auras(SPELL_POSSESS);
                    razorgore->AI()->Notify(1);
                }
            }
        }
        if (type == DATA_KILLED_DRAKONIDS)
        {
            if (data == 1)
                data = GetData(type) + 1;
            if (data == NEFARIAN_DRAKONID_COUNT)
            {
                if (auto nefarius = GetSingleCreatureFromStorage(NPC_NEFARIUS))
                    nefarius->AI()->Notify(1);
            }
        }

        data_[type - NUM_ENCOUNTERS] = data;
    }

    switch (type)
    {
    case TYPE_RAZORGORE:
    {
        DoUseDoorOrButton(GO_RAZORGORE_DOOR);
        if (data == DONE)
            DoUseDoorOrButton(GO_VAELASTRASZ_DOOR);
        razorgore_add_timer_ = 45000;
        wave_ = 0;
        SetData(DATA_DESTROYED_EGGS_COUNT, 0);
        break;
    }
    case TYPE_VAELASTRASZ:
    {
        DoUseDoorOrButton(GO_VAELASTRASZ_DOOR);
        if (data == DONE)
            DoUseDoorOrButton(GO_HALLS_OF_STRIFE_DOOR);
        break;
    }
    case TYPE_BROODLORD:
    {
        if (data == DONE)
            DoUseDoorOrButton(GO_BROODLORD_DOOR);
        break;
    }
    case TYPE_CHROMAGGUS:
    {
        if (data == DONE)
            DoUseDoorOrButton(GO_CHROMAGGUS_GUARDING_DOOR);
        break;
    }
    case TYPE_NEFARIAN:
    {
        DoUseDoorOrButton(GO_NEFARIAN_DOOR);
        if (data == FAIL)
        {
            if (auto nefarius = GetSingleCreatureFromStorage(NPC_NEFARIUS))
                nefarius->SetRespawnTime(900);
        }
        SetData(DATA_KILLED_DRAKONIDS, 0);
        break;
    }
    case DATA_TRIGGER_BREATH_ONE:
    case DATA_TRIGGER_BREATH_TWO:
    {
        uint32 color = (type == DATA_TRIGGER_BREATH_ONE) ?
                           (chromaggus_colors_ & 0xFF) :
                           (chromaggus_colors_ >> 8);
        if (color > 4)
            color = 0;
        auto chromaggus = GetSingleCreatureFromStorage(NPC_CHROMAGGUS);
        if (!chromaggus)
            break;
        switch (color)
        {
        case 0: // Red: Incinerate
            chromaggus->CastSpell(chromaggus, 23308, false);
            break;
        case 1: // Green: Corrosive Acid
            chromaggus->CastSpell(chromaggus, 23313, false);
            break;
        case 2: // Blue: Frost Burn
            chromaggus->CastSpell(chromaggus, 23187, false);
            break;
        case 3: // Black: Ignite Flesh
            chromaggus->CastSpell(chromaggus, 23315, false);
            break;
        case 4: // Bronze: Time Lapse
            chromaggus->CastSpell(chromaggus, 23310, false);
            break;
        }
        break;
    }
    case DATA_TRIGGER_NEFARIAN_DRAKONIDS:
    {
        uint32 color_one = nefarian_colors_ & 0xFF;
        uint32 color_two = nefarian_colors_ >> 8;
        if (color_one > 4)
            color_one = 0;
        if (color_two > 4)
            color_two = 0;
        auto nefarius = GetSingleCreatureFromStorage(NPC_NEFARIUS);
        if (!nefarius)
            break;
        static const uint32 npc_ids[5] = {14264, 14262, 14261, 14265, 14263};
        nefarius->SummonCreature(npc_ids[color_one], -7526.0f, -1135.0f,
            473.35f, 5.3f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000);
        nefarius->SummonCreature(npc_ids[color_two], -7601.6f, -1188.7f,
            475.46f, 5.3f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 4000);
        break;
    }
    case DATA_TRIGGER_NEFARIAN_CLASS_CALL:
    {
        class_call();
        break;
    }
    }
}

uint32 instance_blackwing_lair::GetData(uint32 type)
{
    if (type < NUM_ENCOUNTERS)
        return encounters_[type];
    else if (type < NUM_ENCOUNTERS + NUM_DATA)
        return data_[type - NUM_ENCOUNTERS];
    return 0;
}

void instance_blackwing_lair::OnObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
    case GO_VAELASTRASZ_DOOR:
        if (GetData(TYPE_RAZORGORE) == DONE)
            go->UseDoorOrButton(0, false);
        break;
    case GO_HALLS_OF_STRIFE_DOOR:
        if (GetData(TYPE_VAELASTRASZ) == DONE)
            go->UseDoorOrButton(0, false);
        break;
    case GO_BROODLORD_DOOR:
        if (GetData(TYPE_BROODLORD) == DONE)
            go->UseDoorOrButton(0, false);
        break;
    case GO_CHROMAGGUS_GUARDING_DOOR:
        if (GetData(TYPE_CHROMAGGUS) == DONE)
            go->UseDoorOrButton(0, false);
        break;
    case GO_BLACK_DRAGON_EGG:
        if (!go->isSpawned() && GetData(TYPE_RAZORGORE) != DONE)
            go->Respawn();
        return;
    case GO_RAZORGORE_DOOR:
    case GO_ORB_OF_DOMINATION:
    case GO_NEFARIAN_DOOR:
        break;
    case GO_SUPPRESSION_DEVICE:
        if (GetData(TYPE_BROODLORD) == DONE)
            go->Delete();
        return;
    default:
        return;
    }

    m_mGoEntryGuidStore[go->GetEntry()] = go->GetObjectGuid();
}

void instance_blackwing_lair::OnCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
    case NPC_RAZORGORE:
    case NPC_GRETHOK_THE_CONTROLLER:
    case NPC_BLACKWING_GUARSMAN:
        if (creature->isDead() && GetData(TYPE_RAZORGORE) != DONE)
            creature->Respawn();
        break; // Save razorgore
    case NPC_NEFARIUS:
        if (GetData(TYPE_NEFARIAN) != DONE && creature->isDead())
            creature->Respawn();
        break;
    case NPC_CHROMAGGUS:
    case NPC_NEFARIAN:
        break;
    default:
        return;
    }

    m_mNpcEntryGuidStore[creature->GetEntry()] = creature->GetObjectGuid();
}

void instance_blackwing_lair::Update(uint32 diff)
{
    if (!spawned_vael_rp_trigger_)
    {
        instance->SummonCreature(NPC_VAEL_RP_TRIGGER, -7494, -1022, 409, 0,
            TEMPSUMMON_DEAD_DESPAWN, 0);
        spawned_vael_rp_trigger_ = true;
    }

    if (GetData(TYPE_RAZORGORE) == IN_PROGRESS &&
        GetData(DATA_DESTROYED_EGGS_COUNT) < RAZORGORE_EGGS_COUNT)
    {
        if (razorgore_add_timer_ <= diff)
        {
            razorgore_add_timer_ = 15000;
            ++wave_;

            // First 3 waves have set count:
            int count = 0;
            if (wave_ == 1 || wave_ == 2)
                count = 4;
            else if (wave_ == 3)
                count = 8;
            else
                count = urand(4, 8);

            // 0x1 & 0x2 is one corner, 0x4 and 0x8 is next, etc We pick random
            // order of corners then random spot in corner, then move on to next
            // corner and repeat. With 4 mobs all corners will be used.
            uint32 used_mask = 0;
            while (count--)
            {
                size_t index = 0;

                // Select corner
                int corner = urand(0, 3);
                for (int i = 0; i < 4; ++i)
                {
                    int current_cnt = bool((1 << corner * 2) & used_mask) +
                                      bool((1 << (corner * 2 + 1)) & used_mask);
                    int other_cnt = bool((1 << i * 2) & used_mask) +
                                    bool((1 << (i * 2 + 1)) & used_mask);
                    if (current_cnt > other_cnt)
                        corner = i;
                }

                // Select spot in corner
                if (((1 << (corner * 2 + 1)) & used_mask || urand(0, 1)) &&
                    ((1 << corner * 2) & used_mask) == 0)
                    index = corner * 2;
                else
                    index = corner * 2 + 1;

                used_mask |= 1 << index;

                if (auto spawner = instance->GetCreature(spawn_spots[index]))
                    spawner->AI()->Notify(1);
            }
        }
        else
            razorgore_add_timer_ -= diff;
    }
}

void instance_blackwing_lair::class_call()
{
    auto nefarian = GetSingleCreatureFromStorage(NPC_NEFARIAN);
    if (!nefarian)
        return;

    std::set<uint8> available_classes;
    const ThreatList& tl = nefarian->getThreatManager().getThreatList();
    for (auto& ref : tl)
    {
        auto player = ref->getTarget();
        if (player->GetTypeId() == TYPEID_PLAYER)
            available_classes.insert(player->getClass());
    }

    if (available_classes.empty())
        return;

    auto itr = available_classes.begin();
    std::advance(itr, urand(0, available_classes.size() - 1));
    uint8 rand_class = *itr;

    nefarian->AI()->Notify(rand_class);
}

void instance_blackwing_lair::Load(const char* data)
{
    if (!data)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(data);

    std::istringstream stream(data);
    for (int i = 0; i < NUM_ENCOUNTERS; ++i)
    {
        // Pack color with corresonding encounter in DB
        if (i == TYPE_CHROMAGGUS)
        {
            uint32 tmp;
            stream >> tmp;
            chromaggus_colors_ = tmp >> 8;
            encounters_[i] = tmp & 0xFF;
        }
        else if (i == TYPE_NEFARIAN)
        {
            uint32 tmp;
            stream >> tmp;
            nefarian_colors_ = tmp >> 8;
            encounters_[i] = tmp & 0xFF;
        }
        else
            stream >> encounters_[i];
    }

    for (auto& encounter : encounters_)
    {
        if (encounter == IN_PROGRESS)
            encounter = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstData_BWL(Map* map)
{
    return new instance_blackwing_lair(map);
};

bool GOUse_go_orb_of_domination(Player* player, GameObject* go)
{
    if (go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT))
        return true;

    if (auto map = go->GetMap())
    {
        if (auto inst = dynamic_cast<ScriptedInstance*>(map->GetInstanceData()))
        {
            if (inst->GetData(TYPE_RAZORGORE) == IN_PROGRESS)
            {
                if (!player->has_aura(SPELL_MIND_EXHAUSTION))
                {
                    if (auto razorgore =
                            inst->GetSingleCreatureFromStorage(NPC_RAZORGORE))
                    {
                        if (!razorgore->has_aura(SPELL_POSSESS))
                        {
                            player->CastSpell(player, SPELL_MIND_EXHAUSTION,
                                TRIGGER_TYPE_TRIGGERED |
                                    TRIGGER_TYPE_BYPASS_SPELL_QUEUE);
                            player->CastSpell(razorgore, SPELL_POSSESS,
                                TRIGGER_TYPE_TRIGGERED |
                                    TRIGGER_TYPE_BYPASS_SPELL_QUEUE);
                        }
                    }
                }
            }
        }
    }

    return true;
}

void AddSC_instance_blackwing_lair()
{
    auto script = new Script;
    script->Name = "instance_blackwing_lair";
    script->GetInstanceData = GetInstData_BWL;
    script->RegisterSelf();

    script = new Script;
    script->Name = "go_orb_of_domination";
    script->pGOUse = GOUse_go_orb_of_domination;
    script->RegisterSelf();
}

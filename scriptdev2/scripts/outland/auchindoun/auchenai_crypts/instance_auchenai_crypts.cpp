#include "auchenai_crypts.h"

instance_auchenai_crypts::instance_auchenai_crypts(Map* pMap)
  : ScriptedInstance(pMap)
{
    Initialize();
}
void instance_auchenai_crypts::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_auchenai_crypts::SetData(uint32 uiType, uint32 uiData)
{
    if (uiType >= MAX_ENCOUNTER)
        return;

    m_auiEncounter[uiType] = uiData;

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_auchenai_crypts::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_auchenai_crypts::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2];

    for (auto& elem : m_auiEncounter)
    {
        if (elem == IN_PROGRESS)
            elem = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

InstanceData* GetInstanceData_instance_auchenai_crypts(Map* pMap)
{
    return new instance_auchenai_crypts(pMap);
}

void AddSC_instance_auchenai_crypts()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_auchenai_crypts";
    pNewScript->GetInstanceData = &GetInstanceData_instance_auchenai_crypts;
    pNewScript->RegisterSelf();
}

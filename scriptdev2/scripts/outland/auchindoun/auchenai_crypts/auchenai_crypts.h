#ifndef DEF_SETHEKK_HALLS_H
#define DEF_SETHEKK_HALLS_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 3,

    TYPE_SHIRRAK = 0,
    TYPE_EXARCH_MALADAAR = 1,
    TYPE_AVATAR = 2, // If done we do not awad more loot from him
};

class MANGOS_DLL_DECL instance_auchenai_crypts : public ScriptedInstance
{
public:
    instance_auchenai_crypts(Map* pMap);
    ~instance_auchenai_crypts() {}

    void Initialize() override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
};

#endif

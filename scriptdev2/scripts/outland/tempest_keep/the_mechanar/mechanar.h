/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_MECHANAR_H
#define DEF_MECHANAR_H

#include "precompiled.h"

enum
{
    MAX_ENCOUNTER = 4,

    TYPE_SEPETHREA = 1,
    TYPE_GATEWATCHER_IRONHAND = 2,
    TYPE_GATEWATCHER_GYROKILL = 3,
    TYPE_PATHALEON_THE_CALCULATOR = 4,

    GO_MOARG_1_DOOR = 184632,
    GO_MOARG_2_DOOR = 184322,
    GO_NETHERMANCER_ENCOUNTER_DOOR = 184449,
};

class MANGOS_DLL_DECL instance_mechanar : public ScriptedInstance
{
public:
    instance_mechanar(Map* pMap);

    void Initialize() override;

    void SetData(uint32 uiType, uint32 uiData) override;
    uint32 GetData(uint32 uiType) override;

    const char* Save() override { return m_strInstData.c_str(); }
    void Load(const char* chrIn) override;

    void OnObjectCreate(GameObject* pGo) override;

private:
    uint32 m_auiEncounter[MAX_ENCOUNTER];
    std::string m_strInstData;
};

#endif

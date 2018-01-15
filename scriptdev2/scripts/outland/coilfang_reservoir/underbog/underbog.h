/* Copyright (C) 2012 Corecraft
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_UNDERBOG_H
#define DEF_UNDERBOG_H

enum
{
    NPC_CLAW = 17827,
    NPC_WINDCALLER_CLAW = 17894,
    NPC_SWAMPLORD_MUSELEK = 17826,
};

class MANGOS_DLL_DECL instance_underbog : public ScriptedInstance
{
public:
    instance_underbog(Map* pMap);

    void Initialize() override;

    void OnCreatureCreate(Creature* pCreature) override;
    void OnObjectCreate(GameObject* pGo) override;

    void SetData(uint32 /*uiType*/, uint32 /*uiData*/) override {}
    uint32 GetData(uint32 /*uiType*/) override { return 0; }

    // No need to save and load this instance (only one encounter needs special
    // handling, no doors used)
};

#endif

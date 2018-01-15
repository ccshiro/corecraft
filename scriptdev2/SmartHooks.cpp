#include "SmartScript.h"
#include "precompiled.h"

bool AreaTrigger_smart_trigger(Player* player, const AreaTriggerEntry* trigger)
{
    if (player->isGameMaster())
        return false;

    SmartScript script;
    script.OnInitialize(nullptr, trigger);
    script.ProcessEventsFor(
        SMART_EVENT_AREATRIGGER_ONTRIGGER, player, trigger->id);

    return true;
}

void AddSC_SmartTrigger()
{
    auto script = new Script;
    script->Name = "SmartTrigger";
    script->pAreaTrigger = &AreaTrigger_smart_trigger;
    script->RegisterSelf();
}

/* Copyright (C) 2006 - 2012 ScriptDev2 <http://www.scriptdev2.com/>
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_PRECOMPILED_H
#define SC_PRECOMPILED_H

#include "BattleGround.h"
#include "BattleGroundAV.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureGroup.h"
#include "CreatureGroupMgr.h"
#include "GameObject.h"
#include "Object.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "sc_creature.h"
#include "sc_gossip.h"
#include "sc_grid_searchers.h"
#include "sc_instance.h"
#include "../ScriptMgr.h"
#include "movement/generators.h"
#include "movement/HomeMovementGenerator.h"
#include "movement/IdleMovementGenerator.h"
#include "movement/PointMovementGenerator.h"
#include "movement/RandomMovementGenerator.h"
#include "movement/TargetedMovementGenerator.h"
#include "movement/WaypointMovementGenerator.h"
#include <G3D/Vector3.h>

#ifdef WIN32
#include <windows.h> // sort off
BOOL APIENTRY DllMain(
    HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return true;
}
#endif

#endif

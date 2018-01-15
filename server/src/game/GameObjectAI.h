/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2013 Corecraft <https://www.worldofcorecraft.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_GAMEOBJECTAI_H
#define TRINITY_GAMEOBJECTAI_H

#include "CreatureAI.h"
#include "SharedDefines.h"

class GameObject;
class Player;
class Quest;

class GameObjectAI
{
public:
    explicit GameObjectAI(GameObject* g) : go(g) {}
    virtual ~GameObjectAI() {}

    virtual void UpdateAI(uint32 /*diff*/) {}
    virtual void InitializeAI() { Reset(); }
    virtual void Reset() {}

    // Pass parameters between AI
    virtual void DoAction(int32 /*param*/ = 0) {}
    virtual void SetGUID(ObjectGuid /*guid*/, int32 /*id*/ = 0) {}
    virtual uint64 GetGUID(int32 /*id*/ = 0) const { return 0; }

    virtual uint32 GetData(uint32 /*id*/) const { return 0; }
    virtual void SetData64(uint32 /*id*/, uint64 /*value*/) {}
    virtual uint64 GetData64(uint32 /*id*/) const { return 0; }
    virtual void SetData(uint32 /*id*/, uint32 /*value*/) {}

    virtual uint32 GetDialogStatus(Player* /*player*/) { return 100; }

    /* CALLBACKS */

    /**
     * Callback for when @player gossips with gameobject.
     * @Return: true if you wish to override default behavior, false if you wish
     * to fall back on it.
     */
    virtual bool OnGossipHello(Player* /*player*/) { return false; }

    /**
     * Callback for when @player selects a gossip option from gameobject.
     * @Return: true if you wish to override default behavior, false if you
     * still wish to make use of it.
     */
    virtual bool OnGossipSelect(Player* /*player*/, uint32 /*sender*/,
        uint32 /*action*/, uint32 /*menuId*/ = 0,
        const char* /*code*/ = nullptr)
    {
        return false;
    }

    /**
     * Callback when @quest accepted by @player.
     */
    virtual void OnQuestAccept(Player* /*player*/, const Quest* /*quest*/) {}

    /**
     * Callback when @quest rewarded by @player.
     */
    virtual void OnQuestReward(Player* /*player*/, const Quest* /*quest*/) {}

    /**
     * Callback when event with id @eventId eithers starts or finishes (with
     * gameobject as source), as indicated by @start.
     */
    virtual void OnGameEvent(bool /*start*/, uint32 /*eventId*/) {}

    /**
     * Callback when GO LootState (@state) changes due to @unit interacting with
     * it.
     */
    virtual void OnStateChanged(uint32 /*state*/, Unit* /*unit*/) {}

protected:
    GameObject* go;
};

class NullGameObjectAI : public GameObjectAI
{
public:
    explicit NullGameObjectAI(GameObject* g);

    void UpdateAI(uint32 /*diff*/) override {}
};

#endif

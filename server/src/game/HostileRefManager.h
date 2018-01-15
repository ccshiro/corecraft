/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _HOSTILEREFMANAGER
#define _HOSTILEREFMANAGER

#include "Common.h"
#include "ObjectGuid.h"
#include "Utilities/LinkedReference/RefManager.h"

class HostileReference;
struct SpellEntry;
class ThreatManager;
class Unit;

//=================================================

class HostileRefManager : public RefManager<Unit, ThreatManager>
{
public:
    explicit HostileRefManager(Unit* pOwner);
    ~HostileRefManager();

    Unit* getOwner() { return iOwner; }

    // send threat to all my hateres for the pVictim
    // The pVictim is hated than by them as well
    // use for buffs and healing threat functionality
    void threatAssist(Unit* pVictim, float threat,
        SpellEntry const* threatSpell = nullptr, bool pSingleTarget = false);

    // Modifies threat for all targets in hostile list
    // This function is used for Auras of type
    // 103 (SPELL_AURA_MOD_TOTAL_THREAT). See Aura::HandleAuraModTotalThreat
    void modThreat(Unit* pVictim, float threat, bool apply);

    void addThreatPercent(int32 pValue);

    // The references are not needed anymore
    // tell the source to remove them from the list and free the mem
    void deleteReferences();

    // Invoke callback for all references; reference is deleted if function
    // returns true
    void deleteReferencesCallback(std::function<bool(Unit* target)> func);

    // Remove specific faction references
    void deleteReferencesForFaction(uint32 faction);

    HostileReference* getFirst()
    {
        return ((HostileReference*)RefManager<Unit, ThreatManager>::getFirst());
    }

    void updateThreatTables();

    void setOnlineOfflineState(bool pIsOnline);

    // set state for one reference, defined by Unit
    void setOnlineOfflineState(Unit* pCreature, bool pIsOnline);

    void setIgnoredState(Unit* target, bool ignored);

    // delete one reference, defined by Unit
    void deleteReference(Unit* pCreature);

    // redirection threat data
    void SetThreatRedirection(ObjectGuid guid)
    {
        m_redirectionTargetGuid = guid;
    }

    void ResetThreatRedirection() { m_redirectionTargetGuid.Clear(); }

    Unit* GetThreatRedirectionTarget() const;

private:
    Unit* iOwner; // owner of manager variable, back ref. to it, always exist

    ObjectGuid m_redirectionTargetGuid; // in 2.x redirected only full threat
};
//=================================================
#endif

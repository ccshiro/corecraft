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

#ifndef _THREATMANAGER
#define _THREATMANAGER

#include "Common.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "UnitEvents.h"
#include "Utilities/LinkedReference/Reference.h"
#include <list>

class Creature;
struct SpellEntry;
class ThreatManager;
class Unit;
//==============================================================

//==============================================================
// Class to calculate the real threat based

class ThreatCalcHelper
{
public:
    static float CalcThreat(Unit* pHatedUnit, Unit* pHatingUnit, float threat,
        bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell);
};

//==============================================================
class MANGOS_DLL_SPEC HostileReference : public Reference<Unit, ThreatManager>
{
public:
    HostileReference(Unit* pUnit, ThreatManager* pThreatManager, float pThreat);

    //=================================================
    void addThreat(float pMod);

    void addUntauntableThreat(float threat) { iUntauntableThreat += threat; }

    void setThreat(float pThreat) { addThreat(pThreat - getThreat()); }

    void addThreatPercent(int32 pPercent)
    {
        // for special -100 case avoid rounding
        addThreat(pPercent == -100 ? -iThreat : iThreat * pPercent / 100.0f);
    }

    float getThreat() const { return iThreat; }

    float getUntauntableThreat() const { return iUntauntableThreat; }

    bool isOnline() const { return iOnline; }

    bool isIgnored() const { return iIgnored; }

    // The Unit might be in water and the creature can not enter the water, but
    // has range attack
    // in this case online = true, but accessable = false
    bool isAccessable() const { return iAccessible; }

    // used for temporary setting a threat and reducting it later again.
    // the threat modification is stored
    void addTempThreat(float pThreat)
    {
        if (pThreat != 0.0f)
        {
            addThreat(pThreat);
            iTempThreatModifier += pThreat;
        }
    }
    void removeAllTempThreat()
    {
        if (iTempThreatModifier != 0.0f)
        {
            addThreat(-iTempThreatModifier);
            iTempThreatModifier = 0.0f;
        }
    }
    float getTempThreatModifier() { return iTempThreatModifier; }

    //=================================================
    // check, if source can reach target and set the status
    void updateOnlineStatus();

    void setOnlineOfflineState(bool pIsOnline);

    void setAccessibleState(bool pIsAccessible);

    void setIgnoredState(bool ignored);
    //=================================================

    bool operator==(const HostileReference& pHostileReference) const
    {
        return pHostileReference.getUnitGuid() == getUnitGuid();
    }

    //=================================================

    ObjectGuid const& getUnitGuid() const { return iUnitGuid; }

    //=================================================
    // reference is not needed anymore. realy delete it !

    void removeReference();

    //=================================================

    HostileReference* next()
    {
        return ((HostileReference*)Reference<Unit, ThreatManager>::next());
    }

    //=================================================

    // Tell our refTo (target) object that we have a link
    void targetObjectBuildLink() override;

    // Tell our refTo (taget) object, that the link is cut
    void targetObjectDestroyLink() override;

    // Tell our refFrom (source) object, that the link is cut (Target destroyed)
    void sourceObjectDestroyLink() override;

private:
    // Inform the source, that the status of that reference was changed
    void fireStatusChanged(
        ThreatRefStatusChangeEvent& pThreatRefStatusChangeEvent);

    Unit* getSourceUnit();

private:
    float iThreat;
    float iUntauntableThreat;
    float iTempThreatModifier; // used for temporary threat
    ObjectGuid iUnitGuid;
    bool iOnline;
    bool iAccessible;
    bool iIgnored; // the target must remain in combat with us, but is
                   // completely ignored from being attacked
};

//==============================================================

typedef std::list<HostileReference*> ThreatList;

class MANGOS_DLL_SPEC ThreatContainer
{
private:
    HostileReference* iTauntedBy;
    HostileReference* iFocusTarget;
    // Blacklisted targets are those we cannot attack until they perform
    // another direct hostile action
    std::set<ObjectGuid> blacklisted_;

    ThreatList iThreatList;
    bool iDirty;

protected:
    friend class ThreatManager;

    void remove(HostileReference* pRef)
    {
        if (iTauntedBy == pRef)
            iTauntedBy = nullptr;
        if (iFocusTarget == pRef)
            iFocusTarget = nullptr;
        iThreatList.remove(pRef);
    }
    void addReference(HostileReference* pHostileReference)
    {
        iThreatList.push_back(pHostileReference);
    }
    void clearReferences();
    // Sort the list if necessary
    void update();

    bool blacklisted(Unit* target, bool force = false);
    void remove_blacklisted(Unit* target);

public:
    ThreatContainer()
    {
        iDirty = false;
        iTauntedBy = nullptr;
        iFocusTarget = nullptr;
    }
    ~ThreatContainer() { clearReferences(); }

    HostileReference* addThreat(
        Unit* pVictim, float threat, bool unauntable_threat = false);

    void modifyThreatPercent(Unit* pVictim, int32 percent);

    HostileReference* selectNextVictim(
        Creature* pAttacker, HostileReference* pCurrentVictim);

    void setDirty(bool pDirty) { iDirty = pDirty; }

    bool isDirty() const { return iDirty; }

    bool empty() const { return (iThreatList.empty()); }

    HostileReference* getMostHated()
    {
        return iThreatList.empty() ? nullptr : iThreatList.front();
    }

    HostileReference* getReferenceByTarget(Unit* pVictim);

    ThreatList const& getThreatList() const { return iThreatList; }

    void setTauntedBy(HostileReference* ref) { iTauntedBy = ref; }
    HostileReference* getTauntedBy() { return iTauntedBy; }
    void setFocusTarget(HostileReference* ref) { iFocusTarget = ref; }
};

//=================================================

class MANGOS_DLL_SPEC ThreatManager
{
public:
    friend class HostileReference;

    explicit ThreatManager(Unit* pOwner);

    ~ThreatManager() { clearReferences(); }

    void clearReferences();

    void addThreat(Unit* pVictim, float threat, bool crit,
        SpellSchoolMask schoolMask, SpellEntry const* threatSpell,
        bool unauntable_threat);
    void addThreat(Unit* pVictim, float threat)
    {
        addThreat(
            pVictim, threat, false, SPELL_SCHOOL_MASK_NONE, nullptr, false);
    }
    // If a target is not already on the threat-list, it will be added as
    // black-listed, otherwise addCharmer does nothing
    void addCharmer(Unit* victim);
    void removeCharmer(Unit* victim);

    // add threat as raw value (ignore redirections and expection all mods
    // applied already to it
    void addThreatDirectly(
        Unit* pVictim, float threat, bool unauntable_threat = false);

    void modifyThreatPercent(Unit* pVictim, int32 pPercent);

    float getThreat(Unit* pVictim, bool pAlsoSearchOfflineList = false);
    float getUntauntableThreat(Unit* pVictim);

    // Note: target can be on list with 0 threat; use this to determine if on
    // list instead
    bool hasTarget(Unit* victim, bool search_offline = false);

    bool isThreatListEmpty() const { return iThreatContainer.empty(); }
    // returns true if no target exists in the threat list that can be attacked
    bool isThreatListEmptyOrUseless() const;

    void processThreatEvent(
        ThreatRefStatusChangeEvent* threatRefStatusChangeEvent);

    HostileReference* getCurrentVictim() { return iCurrentVictim; }

    Unit* getOwner() const { return iOwner; }

    Unit* getHostileTarget();

    void tauntApply(Unit* pTaunter);
    void tauntFadeOut(Unit* pTaunter);
    void tauntTransferAggro(Unit* pTaunter);
    void setFocusTarget(Unit* pUnit);

    void setCurrentVictim(HostileReference* pHostileReference);

    void setDirty(bool pDirty) { iThreatContainer.setDirty(pDirty); }

    // Don't must be used for explicit modify threat values in iterator return
    // pointers
    ThreatList const& getThreatList() const
    {
        return iThreatContainer.getThreatList();
    }

private:
    HostileReference* iCurrentVictim;
    Unit* iOwner;
    ThreatContainer iThreatContainer;
    ThreatContainer iThreatOfflineContainer;
    ThreatContainer iThreatIgnoredContainer;
};

//=================================================
#endif

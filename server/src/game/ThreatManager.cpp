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

#include "ThreatManager.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Unit.h"
#include "UnitEvents.h"

//==============================================================
//================= ThreatCalcHelper ===========================
//==============================================================

// The pHatingUnit is not used yet
float ThreatCalcHelper::CalcThreat(Unit* pHatedUnit, Unit* /*pHatingUnit*/,
    float threat, bool crit, SpellSchoolMask schoolMask,
    SpellEntry const* pThreatSpell)
{
    // all flat mods applied early
    if (!threat)
        return 0.0f;

    // Threat reduction is not modded by your threat spell mods
    if (threat < 0)
        return threat;

    if (pThreatSpell)
    {
        if (pThreatSpell->HasAttribute(SPELL_ATTR_EX_NO_THREAT))
            return 0.0f;

        if (Player* modOwner = pHatedUnit->GetSpellModOwner())
            modOwner->ApplySpellMod(pThreatSpell->Id, SPELLMOD_THREAT, threat);

        if (crit)
            threat *= pHatedUnit->GetTotalAuraMultiplierByMiscMask(
                SPELL_AURA_MOD_CRITICAL_THREAT, schoolMask);
    }

    threat = pHatedUnit->ApplyTotalThreatModifier(threat, schoolMask);
    return threat;
}

//============================================================
//================= HostileReference ==========================
//============================================================

HostileReference::HostileReference(
    Unit* pUnit, ThreatManager* pThreatManager, float pThreat)
{
    iThreat = pThreat;
    iUntauntableThreat = false;
    iTempThreatModifier = 0.0f;
    link(pUnit, pThreatManager);
    iUnitGuid = pUnit->GetObjectGuid();
    iOnline = true;
    iAccessible = true;
    iIgnored = false;
}

//============================================================
// Tell our refTo (target) object that we have a link
void HostileReference::targetObjectBuildLink()
{
    getTarget()->addHatedBy(this);
}

//============================================================
// Tell our refTo (taget) object, that the link is cut
void HostileReference::targetObjectDestroyLink()
{
    getTarget()->removeHatedBy(this);
}

//============================================================
// Tell our refFrom (source) object, that the link is cut (Target destroyed)

void HostileReference::sourceObjectDestroyLink()
{
    setOnlineOfflineState(false);
}

//============================================================
// Inform the source, that the status of the reference changed

void HostileReference::fireStatusChanged(
    ThreatRefStatusChangeEvent& pThreatRefStatusChangeEvent)
{
    if (getSource())
        getSource()->processThreatEvent(&pThreatRefStatusChangeEvent);
}

//============================================================

void HostileReference::addThreat(float pMod)
{
    iThreat += pMod;
    // the threat is changed. Source and target unit have to be availabe
    // if the link was cut before relink it again
    if (!isOnline())
        updateOnlineStatus();
    if (pMod != 0.0f)
    {
        ThreatRefStatusChangeEvent event(
            UEV_THREAT_REF_THREAT_CHANGE, this, pMod);
        fireStatusChanged(event);
    }

    if (isValid() && pMod >= 0)
    {
        Unit* victim_owner = getTarget()->GetOwner();
        if (victim_owner && victim_owner->isAlive())
            getSource()->addThreat(victim_owner, 0.0f); // create a threat to
                                                        // the owner of a pet,
                                                        // if the pet attacks
    }
}

//============================================================
// check, if source can reach target and set the status

void HostileReference::updateOnlineStatus()
{
    bool online = false;
    bool accessible = false;

    if (!isValid())
    {
        if (Unit* target =
                ObjectAccessor::GetUnit(*getSourceUnit(), getUnitGuid()))
            link(target, getSource());
    }
    // only check for online status if
    // ref is valid
    // target is no player or not gamemaster
    // target is not in flight
    if (isValid() && ((getTarget()->GetTypeId() != TYPEID_PLAYER ||
                          !((Player*)getTarget())->isGameMaster()) ||
                         !getTarget()->IsTaxiFlying()))
    {
        Creature* creature = static_cast<Creature*>(getSourceUnit());
        accessible = getTarget()->isInAccessablePlaceFor(creature);
        online = true;
    }
    setAccessibleState(accessible);
    setOnlineOfflineState(online);
}

//============================================================
// set the status and fire the event on status change

void HostileReference::setOnlineOfflineState(bool pIsOnline)
{
    if (iOnline != pIsOnline)
    {
        iOnline = pIsOnline;
        if (!iOnline)
            setAccessibleState(
                false); // if not online that not accessable as well

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ONLINE_STATUS, this);
        fireStatusChanged(event);
    }
}

//============================================================

void HostileReference::setAccessibleState(bool pIsAccessible)
{
    if (iAccessible != pIsAccessible)
    {
        iAccessible = pIsAccessible;

        ThreatRefStatusChangeEvent event(
            UEV_THREAT_REF_ASSECCIBLE_STATUS, this);
        fireStatusChanged(event);
    }
}

void HostileReference::setIgnoredState(bool ignored)
{
    if (iIgnored != ignored)
    {
        iIgnored = ignored;

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_IGNORED_STATUS, this);
        fireStatusChanged(event);
    }
}

//============================================================
// prepare the reference for deleting
// this is called be the target

void HostileReference::removeReference()
{
    invalidate();

    ThreatRefStatusChangeEvent event(UEV_THREAT_REF_REMOVE_FROM_LIST, this);
    fireStatusChanged(event);
}

//============================================================

Unit* HostileReference::getSourceUnit()
{
    return (getSource()->getOwner());
}

//============================================================
//================ ThreatContainer ===========================
//============================================================

void ThreatContainer::clearReferences()
{
    for (ThreatList::const_iterator i = iThreatList.begin();
         i != iThreatList.end(); ++i)
    {
        (*i)->unlink();
        delete (*i);
    }
    blacklisted_.clear();
    iThreatList.clear();
    iTauntedBy = nullptr;
    iFocusTarget = nullptr;
}

//============================================================
// Return the HostileReference of NULL, if not found
HostileReference* ThreatContainer::getReferenceByTarget(Unit* pVictim)
{
    HostileReference* result = nullptr;
    ObjectGuid guid = pVictim->GetObjectGuid();
    for (ThreatList::const_iterator i = iThreatList.begin();
         i != iThreatList.end(); ++i)
    {
        if ((*i)->getUnitGuid() == guid)
        {
            result = (*i);
            break;
        }
    }

    return result;
}

//============================================================
// Add the threat, if we find the reference

HostileReference* ThreatContainer::addThreat(
    Unit* pVictim, float threat, bool untauntable_threat)
{
    HostileReference* ref = getReferenceByTarget(pVictim);
    if (ref)
    {
        ref->addThreat(threat);
        if (untauntable_threat)
            ref->addUntauntableThreat(threat);
        if (threat > 0)
            blacklisted_.erase(pVictim->GetObjectGuid());
    }
    return ref;
}

//============================================================

void ThreatContainer::modifyThreatPercent(Unit* pVictim, int32 pPercent)
{
    if (HostileReference* ref = getReferenceByTarget(pVictim))
    {
        if (pPercent < -100)
        {
            ref->removeReference();
            delete ref;
        }
        else
            ref->addThreatPercent(pPercent);
    }
}

//============================================================

bool HostileReferenceSortPredicate(
    const HostileReference* lhs, const HostileReference* rhs)
{
    // std::list::sort ordering predicate must be: (Pred(x,y)&&Pred(y,x))==false
    return lhs->getThreat() > rhs->getThreat(); // reverse sorting
}

//============================================================
// Check if the list is dirty and sort if necessary

void ThreatContainer::update()
{
    if (iDirty && iThreatList.size() > 1)
    {
        iThreatList.sort(HostileReferenceSortPredicate);
    }
    iDirty = false;
}

//============================================================
// return the next best victim
// could be the current victim

HostileReference* ThreatContainer::selectNextVictim(
    Creature* pAttacker, HostileReference* pCurrentVictim)
{
    HostileReference* pCurrentRef = nullptr;
    bool found = false;
    bool onlySecondChoiceTargetsFound = false;
    bool checkedCurrentVictim = false;

    // Return focus target if we have one (used for scripting purposes)
    if (iFocusTarget)
    {
        if (iFocusTarget->isValid() && iFocusTarget->isOnline())
        {
            if (Unit* target = iFocusTarget->getTarget())
                if (target->isAlive() && !pAttacker->IsOutOfThreatArea(target))
                    return iFocusTarget;
        }
        iFocusTarget = nullptr;
    }

    // Return the target taunting us if we're under a taunt effect
    if (iTauntedBy)
    {
        if (iTauntedBy->isValid() && iTauntedBy->isOnline())
        {
            if (Unit* target = iTauntedBy->getTarget())
                if (target->isAlive() && !pAttacker->IsOutOfThreatArea(target))
                    return iTauntedBy;
        }
        iTauntedBy = nullptr;
    }

    // If we're rooted we take the first attackable target that is highest on
    // threat
    if (pAttacker->isInRoots())
    {
        for (ThreatList::const_iterator itr = iThreatList.begin();
             itr != iThreatList.end(); ++itr)
        {
            if (!(*itr)->isValid() || !(*itr)->isOnline())
                continue;

            Unit* pTarget = (*itr)->getTarget();
            assert(pTarget); // if the ref has status online the target must be
                             // there!

            if ((pAttacker->CanReachWithMeleeAttack(pTarget) ||
                    ((Creature*)pAttacker)->ReachWithSpellAttack(pTarget)) &&
                pAttacker->IsWithinWmoLOSInMap(pTarget))
                return *itr;
        }
        // No attackable targets, return first found valid aggro target:
        for (ThreatList::const_iterator itr = iThreatList.begin();
             itr != iThreatList.end(); ++itr)
            if ((*itr)->isValid() && (*itr)->isOnline() && !(*itr)->isIgnored())
                return *itr;
    }

    ThreatList::const_iterator lastRef = iThreatList.end();
    --lastRef;

    for (ThreatList::const_iterator iter = iThreatList.begin();
         iter != iThreatList.end() && !found;)
    {
        pCurrentRef = (*iter);

        Unit* pTarget = pCurrentRef->getTarget();
        assert(
            pTarget); // if the ref has status online the target must be there!

        // some units are prefered in comparison to others
        // if (checkThreatArea) consider IsOutOfThreatArea - expected to be only
        // set for pCurrentVictim
        //     This prevents dropping valid targets due to 1.1 or 1.3 threat
        //     rule vs invalid current target
        bool black =
            blacklisted(pTarget); // Must always be checked, to update status
        if (!onlySecondChoiceTargetsFound &&
            (pAttacker->IsSecondChoiceTarget(
                 pTarget, pCurrentRef == pCurrentVictim) ||
                black))
        {
            if (iter != lastRef)
                ++iter;
            else
            {
                // if we reached to this point, everyone in the threatlist is a
                // second choice target. In such a situation the target with the
                // highest threat should be attacked.
                onlySecondChoiceTargetsFound = true;
                iter = iThreatList.begin();
            }

            // current victim is a second choice target, so don't compare threat
            // with it below
            if (pCurrentRef == pCurrentVictim)
                pCurrentVictim = nullptr;

            // second choice targets are only handled threat dependend if we
            // have only have second choice targets
            continue;
        }

        if (!pAttacker->IsOutOfThreatArea(
                pTarget)) // skip non attackable currently targets
        {
            if (pCurrentVictim) // select 1.3/1.1 better target in comparison
                                // current target
            {
                // normal case: pCurrentRef is still valid and most hated
                if (pCurrentVictim == pCurrentRef)
                {
                    found = true;
                    break;
                }

                // we found a valid target, but only compare its threat if the
                // currect victim is also a valid target
                // Additional check to prevent unneeded comparision in case of
                // valid current victim
                if (!checkedCurrentVictim)
                {
                    Unit* pCurrentTarget = pCurrentVictim->getTarget();
                    assert(pCurrentTarget);
                    if (pAttacker->IsSecondChoiceTarget(pCurrentTarget, true))
                    {
                        // CurrentVictim is invalid, so return CurrentRef
                        found = true;
                        break;
                    }
                    checkedCurrentVictim = true;
                }

                // list sorted and and we check current target, then this is
                // best case
                if (pCurrentRef->getThreat() <=
                    1.1f * pCurrentVictim->getThreat())
                {
                    pCurrentRef = pCurrentVictim;
                    found = true;
                    break;
                }

                if (pCurrentRef->getThreat() >
                        1.3f * pCurrentVictim->getThreat() ||
                    (pCurrentRef->getThreat() >
                            1.1f * pCurrentVictim->getThreat() &&
                        pAttacker->CanReachWithMeleeAttack(pTarget)))
                { // implement 110% threat rule for targets in melee range
                    found =
                        true; // and 130% rule for targets in ranged distances
                    break;    // for selecting alive targets
                }
            }
            else // select any
            {
                found = true;
                break;
            }
        }
        ++iter;
    }
    if (!found)
        pCurrentRef = nullptr;

    return pCurrentRef;
}

bool ThreatContainer::blacklisted(Unit* target, bool force)
{
    if (blacklisted_.find(target->GetObjectGuid()) != blacklisted_.end())
        return true;

    bool found = false;

    if (!force)
    {
        if (target->HasAuraType(SPELL_AURA_MOD_FEAR) ||
            target->HasAuraType(SPELL_AURA_MOD_CONFUSE))
            found = true;
        if (!found)
        {
            target->loop_auras([&found](AuraHolder* holder)
                {
                    if ((!holder->IsPositive() &&
                            holder->GetSpellProto()->AuraInterruptFlags &
                                AURA_INTERRUPT_FLAG_DAMAGE) ||
                        holder->GetSpellProto()->HasAttribute(
                            SPELL_ATTR_CUSTOM_AURA_CAUSES_BLACKLIST))
                        found = true;
                    return !found; // break when found is true
                });
        }
    }
    else
    {
        found = true; // force
    }

    if (found)
    {
        blacklisted_.insert(target->GetObjectGuid());
        return true;
    }

    return false;
}

void ThreatContainer::remove_blacklisted(Unit* target)
{
    auto itr = blacklisted_.find(target->GetObjectGuid());
    if (itr != blacklisted_.end())
        blacklisted_.erase(itr);
}

//============================================================
//=================== ThreatManager ==========================
//============================================================

ThreatManager::ThreatManager(Unit* owner)
  : iCurrentVictim(nullptr), iOwner(owner)
{
}

//============================================================

void ThreatManager::clearReferences()
{
    iThreatContainer.clearReferences();
    iThreatOfflineContainer.clearReferences();
    iThreatIgnoredContainer.clearReferences();
    iCurrentVictim = nullptr;
}

//============================================================

void ThreatManager::addThreat(Unit* pVictim, float pThreat, bool crit,
    SpellSchoolMask schoolMask, SpellEntry const* pThreatSpell,
    bool untauntable_threat)
{
    // function deals with adding threat and adding players and pets into
    // ThreatList
    // mobs, NPCs, guards have ThreatList and HateOfflineList
    // players and pets have only InHateListOf
    // HateOfflineList is used co contain unattackable victims (in-flight,
    // in-water, GM etc.)

    // not to self
    if (pVictim == getOwner())
        return;

    // not to GM
    if (!pVictim || (pVictim->GetTypeId() == TYPEID_PLAYER &&
                        ((Player*)pVictim)->isGameMaster()))
        return;

    // not to dead and not for dead
    if (!pVictim->isAlive() || !getOwner()->isAlive())
        return;

    // You cannot add threat to someone in feign death
    if (pVictim->HasAuraType(SPELL_AURA_FEIGN_DEATH))
        return;

    assert(getOwner()->GetTypeId() == TYPEID_UNIT);

    float threat = ThreatCalcHelper::CalcThreat(
        pVictim, iOwner, pThreat, crit, schoolMask, pThreatSpell);

    if (threat > 0.0f)
    {
        if (Unit* redirectedTarget =
                pVictim->getHostileRefManager().GetThreatRedirectionTarget())
        {
            if (redirectedTarget != getOwner() && redirectedTarget->isAlive())
            {
                if (AuraHolder* holder = pVictim->get_aura(34477))
                {
                    if (holder->DropAuraCharge())
                    {
                        holder->SetAuraDuration(100);
                        if (AuraHolder* redirect =
                                redirectedTarget->get_aura(35079))
                            redirect->SetAuraDuration(100);
                    }
                }
                addThreatDirectly(redirectedTarget, threat, untauntable_threat);
                threat = 0; // but still need add to threat list
            }
        }
    }

    addThreatDirectly(pVictim, threat, untauntable_threat);
}

void ThreatManager::addCharmer(Unit* victim)
{
    if (hasTarget(victim))
        return;
    // Add 0 threat and blacklist target
    addThreat(victim, 0.0f);
    iThreatContainer.blacklisted(victim);
}

void ThreatManager::removeCharmer(Unit* victim)
{
    iThreatContainer.remove_blacklisted(victim);
}

void ThreatManager::addThreatDirectly(
    Unit* pVictim, float threat, bool untauntable_threat)
{
    HostileReference* ref =
        iThreatContainer.addThreat(pVictim, threat, untauntable_threat);
    // Ref is not in the online refs, search the offline refs next
    if (!ref)
        ref = iThreatOfflineContainer.addThreat(pVictim, threat);

    if (!ref) // there was no ref => create a new one
    {
        // threat has to be 0 here
        auto hostileReference = new HostileReference(pVictim, this, 0);
        iThreatContainer.addReference(hostileReference);
        hostileReference->addThreat(threat); // now we add the real threat
        if (untauntable_threat)
            hostileReference->addUntauntableThreat(threat);
        if (pVictim->GetTypeId() == TYPEID_PLAYER &&
            ((Player*)pVictim)->isGameMaster())
            hostileReference->setOnlineOfflineState(
                false); // GM is always offline

        // Certain mobs make other mobs aggro on that target as well
        auto vec =
            sObjectMgr::Instance()->get_defenders(getOwner()->GetObjectGuid());
        if (vec)
        {
            for (auto guid : *vec)
            {
                Creature* c = getOwner()->GetMap()->GetCreature(guid);
                if (!c || !c->isAlive())
                    continue;
                if (c->isInCombat())
                    c->AddThreat(pVictim);
                else if (c->AI())
                    c->AI()->AttackStart(pVictim);
            }
        }
    }
}

//============================================================

void ThreatManager::modifyThreatPercent(Unit* pVictim, int32 pPercent)
{
    iThreatContainer.modifyThreatPercent(pVictim, pPercent);
}

//============================================================

Unit* ThreatManager::getHostileTarget()
{
    iThreatContainer.update();

    // Do not calculate next victim if we're under a threat ignoring CC effect
    if (getOwner()->IsAffectedByThreatIgnoringCC())
        return nullptr;

    iCurrentVictim = iThreatContainer.selectNextVictim(
        (Creature*)getOwner(), iCurrentVictim);
    return iCurrentVictim != nullptr ? iCurrentVictim->getTarget() : nullptr;
}

//============================================================

float ThreatManager::getThreat(Unit* pVictim, bool pAlsoSearchOfflineList)
{
    float threat = 0.0f;
    HostileReference* ref = iThreatContainer.getReferenceByTarget(pVictim);
    if (!ref && pAlsoSearchOfflineList)
        ref = iThreatOfflineContainer.getReferenceByTarget(pVictim);
    if (ref)
        threat = ref->getThreat();
    return threat;
}

//============================================================
float ThreatManager::getUntauntableThreat(Unit* pVictim)
{
    float threat = 0.0f;
    HostileReference* ref = iThreatContainer.getReferenceByTarget(pVictim);
    if (ref)
        threat = ref->getUntauntableThreat();
    return threat;
}

//============================================================
bool ThreatManager::hasTarget(Unit* victim, bool search_offline)
{
    if (iThreatContainer.getReferenceByTarget(victim) != nullptr)
        return true;
    if (search_offline &&
        iThreatOfflineContainer.getReferenceByTarget(victim) != nullptr)
        return true;
    return false;
}

//============================================================
bool ThreatManager::isThreatListEmptyOrUseless() const
{
    if (isThreatListEmpty())
        return true;

    auto owner = static_cast<const Creature*>(getOwner());

    const auto& tl = iThreatContainer.getThreatList();
    for (const auto& ref : tl)
    {
        auto target = ref->getTarget();
        if (!owner->IsOutOfThreatArea(target))
            return false;
    }

    return true;
}

//============================================================

void ThreatManager::tauntApply(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        iThreatContainer.setTauntedBy(ref);
    }
}

//============================================================

void ThreatManager::tauntFadeOut(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        // Clear target if we are still last taunters.
        // If GetTauntedBy() == ref fails it would mean someone else taunted
        // before our taunt ran out, and it's now they who should clear
        // TauntedBy
        if (iThreatContainer.getTauntedBy() == ref)
        {
            iThreatContainer.setTauntedBy(nullptr);
        }
    }
}

//============================================================

void ThreatManager::tauntTransferAggro(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        if (iCurrentVictim && (ref->getThreat() <= iCurrentVictim->getThreat()))
        {
            // Update threat and change victim. By changing victim
            // we assure that to over-take the taunter melee dps need to
            // do 10% more threat than the taunters current and ranged 30% more.
            float threat = (iCurrentVictim->getThreat() -
                               iCurrentVictim->getUntauntableThreat()) -
                           ref->getThreat();
            iCurrentVictim = ref;
            if (threat > 0)
                ref->addThreat(threat);
        }
    }
}

//============================================================

void ThreatManager::setFocusTarget(Unit* pUnit)
{
    if (!pUnit)
        iThreatContainer.setFocusTarget(nullptr);
    else if (HostileReference* ref =
                 iThreatContainer.getReferenceByTarget(pUnit))
        iThreatContainer.setFocusTarget(ref);
}

//============================================================

void ThreatManager::setCurrentVictim(HostileReference* pHostileReference)
{
    iCurrentVictim = pHostileReference;
}

//============================================================
// The hated unit is gone, dead or deleted
// return true, if the event is consumed

void ThreatManager::processThreatEvent(
    ThreatRefStatusChangeEvent* threatRefStatusChangeEvent)
{
    threatRefStatusChangeEvent->setThreatManager(
        this); // now we can set the threat manager

    HostileReference* hostileReference =
        threatRefStatusChangeEvent->getReference();

    switch (threatRefStatusChangeEvent->getType())
    {
    case UEV_THREAT_REF_THREAT_CHANGE:
        if ((getCurrentVictim() == hostileReference &&
                threatRefStatusChangeEvent->getFValue() < 0.0f) ||
            (getCurrentVictim() != hostileReference &&
                threatRefStatusChangeEvent->getFValue() > 0.0f))
            setDirty(true); // the order in the threat list might have changed
        break;
    case UEV_THREAT_REF_ONLINE_STATUS:
        if (!hostileReference->isOnline())
        {
            if (hostileReference == getCurrentVictim())
            {
                setCurrentVictim(nullptr);
                setDirty(true);
            }
            iThreatContainer.remove(hostileReference);
            iThreatOfflineContainer.addReference(hostileReference);
        }
        else
        {
            if (getCurrentVictim() &&
                hostileReference->getThreat() >
                    (1.1f * getCurrentVictim()->getThreat()))
                setDirty(true);
            iThreatContainer.addReference(hostileReference);
            iThreatOfflineContainer.remove(hostileReference);
        }
        break;
    case UEV_THREAT_REF_REMOVE_FROM_LIST:
        if (hostileReference == getCurrentVictim())
        {
            setCurrentVictim(nullptr);
            setDirty(true);
        }
        if (hostileReference->isOnline())
            iThreatContainer.remove(hostileReference);
        else
            iThreatOfflineContainer.remove(hostileReference);
        if (getOwner()->GetTypeId() == TYPEID_UNIT &&
            hostileReference->getUnitGuid().IsPlayer())
            static_cast<Creature*>(getOwner())
                ->abandon_taps(hostileReference->getUnitGuid());
        break;
    case UEV_THREAT_REF_IGNORED_STATUS:
        if (hostileReference->isIgnored())
        {
            if (hostileReference == getCurrentVictim())
            {
                setCurrentVictim(nullptr);
                setDirty(true);
            }
            iThreatContainer.remove(hostileReference);
            iThreatIgnoredContainer.addReference(hostileReference);
        }
        else
        {
            if (getCurrentVictim() &&
                hostileReference->getThreat() >
                    (1.1f * getCurrentVictim()->getThreat()))
                setDirty(true);
            iThreatContainer.addReference(hostileReference);
            iThreatIgnoredContainer.remove(hostileReference);
        }
        break;
    }
}

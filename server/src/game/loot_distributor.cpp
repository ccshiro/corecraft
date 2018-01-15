#include "loot_distributor.h"
#include "BattleGroundAV.h"
#include "GameObject.h"
#include "Group.h"
#include "logging.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "World.h"
#include "loot_selection.h"
#include "Platform/Define.h"
#include <new>

static auto& logger = logging.get_logger("loot.distribution");

loot_distributor::loot_distributor(Object* lootee, LootType loot_type)
  : loot_method_(FREE_FOR_ALL), loot_threshold_(ITEM_QUALITY_POOR),
    lootee_(lootee), lootee_guid_(lootee->GetObjectGuid()), loot_(nullptr),
    loot_type_(loot_type), loot_checked_by_chosen_one_(false),
    recipient_mgr_(lootee), rolls_started_(false), has_rollable_items_(false),
    roll_timer_(0), dungeon_loot_(false)
{
    /* Empty */
}

loot_distributor::~loot_distributor()
{
    cancel_loot_session(); // Must call before deleting loot
    delete loot_;

    assert(pending_rolls_.empty());
}

void loot_distributor::start_loot_session()
{
    if (recipient_mgr_.taps()->empty() || !loot_)
        return;

    if (Group* loot_grp = recipient_mgr_.group())
    {
        // Only build next group looter for corpse looting
        if (lootee_guid_.IsCreatureOrPet() && loot_type_ == LOOT_CORPSE)
        {
            if (loot_grp->GetLootMethod() != FREE_FOR_ALL)
                chosen_group_looter_ =
                    loot_grp->GetNextGroupLooter(*recipient_mgr_.taps());
        }

        loot_method_ = loot_grp->GetLootMethod();
        loot_threshold_ = loot_grp->GetLootThreshold();
        if (loot_method_ == MASTER_LOOT)
            master_looter_ = loot_grp->GetLooterGuid();
    }
    else
    {
        // Pick an owner if it's not a group that receives the loot
        if (Player* owner = recipient_mgr_.first_valid_player())
            loot_owner_ = owner->GetObjectGuid();
    }

    select_and_notify_looters();
}

void loot_distributor::select_and_notify_looters()
{
    assert(loot_);

    // Do not notify anyone if this is not a on-the-ground creature loot
    if (!lootee_guid_.IsCreatureOrPet() || loot_type_ != LOOT_CORPSE)
        return;

    if (recipient_mgr_.party_loot())
    {
        if (chosen_group_looter_.IsEmpty() && loot_method_ != FREE_FOR_ALL)
            return;

        Player* main_looter = nullptr;
        if (loot_method_ != FREE_FOR_ALL)
        {
            main_looter =
                sObjectAccessor::Instance()->FindPlayer(chosen_group_looter_);
            if (!main_looter)
                return;
            acceptable_loot_viewers_.insert(main_looter->GetObjectGuid());
        }

        // Should everyone see this loot?
        bool show_everyone = false;
        if (loot_method_ == FREE_FOR_ALL)
            show_everyone = true;
        else
        {
            for (auto& elem : loot_->items_)
            {
                if (elem.one_per_player) // Items that have one copy for
                                         // eachplayer makes everyone see the
                                         // loot (we still need to loop on to
                                         // see if we have rollable items)
                    show_everyone = true;

                if (elem.item_quality >= loot_threshold_ &&
                    loot_method_ !=
                        ROUND_ROBIN) // Round Robin is always only one person
                {
                    has_rollable_items_ = true;
                    show_everyone = true;
                    break;
                }
            }
        }

        if (!show_everyone && !main_looter) // show everyone false at this point
                                            // means looting is not FFA (aka we
                                            // need a picked looter)
            return;

        // Consider if any quest items could change the fact we're not showing
        // anyone but the picked looter
        bool only_quest_items = false;
        std::vector<LootItem*> ffa_quest_items;
        ffa_quest_items.reserve(loot_->quest_items_.size());
        if (!show_everyone)
        {
            for (auto& elem : loot_->quest_items_)
            {
                // Add quest items that dropped but the main looter cannot loot,
                // so anyone else can see them
                // Or quest items that are one per player
                if (elem.one_per_player ||
                    !loot_->can_loot_item(&elem, main_looter->GetObjectGuid()))
                {
                    show_everyone = true;
                    only_quest_items = true;
                    ffa_quest_items.push_back(&elem);
                    ffa_items_.insert(&elem);
                }
            }
        }

        // Add everyone who should see the loot
        if (show_everyone)
        {
            for (const auto& elem : *recipient_mgr_.taps())
            {
                if (only_quest_items)
                {
                    for (auto& ffa_quest_item : ffa_quest_items)
                        if (loot_->can_loot_item(ffa_quest_item, elem))
                            acceptable_loot_viewers_.insert(elem);
                }
                else
                    acceptable_loot_viewers_.insert(elem);
            }
        }
        else // Only main looter can loot
            acceptable_loot_viewers_.insert(main_looter->GetObjectGuid());
    }
    else
    {
        // If we got here there should only be one tapper
        acceptable_loot_viewers_.insert(*recipient_mgr_.taps()->begin());
    }
}

void loot_distributor::start_rolls()
{
    assert(loot_);

    if (rolls_started_)
        return;

    bool anyone = anyone_can_view_loot();
    if (!recipient_mgr_.party_loot() || !rollable_loot_type() ||
        (!anyone && !has_rollable_items_ && !dungeon_loot_) ||
        (recipient_mgr_.taps()->empty() && !anyone))
        return;

    if (loot_method_ == FREE_FOR_ALL || loot_method_ == MASTER_LOOT ||
        loot_method_ == ROUND_ROBIN)
        return;

    // We need to save everyone who passes due to greed and send out those
    // results after all rollers have been added
    // (or not everyone will see the pass that player did) - Also passes for
    // unique items which the player already has
    typedef std::vector<std::pair<Roll*, ObjectGuid>> auto_pass;
    auto_pass auto_passers;
    auto_passers.reserve(loot_->items_.size() * recipient_mgr_.taps()->size());

    // Start roll for each item & player
    for (auto itr = loot_->items_.begin(); itr != loot_->items_.end(); ++itr)
    {
        if (itr->item_quality < loot_threshold_ ||
            itr->one_per_player) // Do not roll for FFA items
            continue;

        // Create a roll and save it
        itr->is_blocked = true;
        auto roll = new Roll(itr - loot_->items_.begin(), &*itr);
        pending_rolls_.push_back(roll);

        // Get roll packet
        WorldPacket data = build_roll_start_packet(*roll);

        // Add and notify all rollers
        for (const auto& elem : *recipient_mgr_.taps())
        {
            if (!loot_->can_loot_item(&*itr, elem))
            {
                auto_passers.push_back(std::make_pair(roll, elem));
                roll->add_auto_passer(elem);
                continue;
            }

            if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem))
            {
                // Skip people not in the same map as us. TODO: This was the
                // case for long in WoW. But was it still in 2.4.3? (If not,
                // global lookup for rolls will be needed)
                if (!static_cast<WorldObject*>(lootee_)->IsInMap(
                        plr)) // We're a world object if we get here
                {
                    // When this was still the case in WoW we didn't get a pass
                    // message, so skipping adding to auto_passers
                    roll->add_auto_passer(elem);
                    continue;
                }

                if (!uniqueness_check(plr, &*itr))
                {
                    // If the item is unique and we already have it
                    auto_passers.push_back(
                        std::make_pair(roll, plr->GetObjectGuid()));
                    roll->add_auto_passer(elem);
                    continue;
                }

                if (loot_method_ == NEED_BEFORE_GREED)
                {
                    const ItemPrototype* item =
                        ObjectMgr::GetItemPrototype(itr->itemid);
                    if (item && plr->can_use_item(item) == EQUIP_ERR_OK)
                        roll->add_roller(plr->GetObjectGuid());
                    else
                    {
                        roll->add_auto_passer(elem);
                        auto_passers.push_back(
                            std::make_pair(roll, plr->GetObjectGuid()));
                        continue;
                    }
                }
                else
                    roll->add_roller(plr->GetObjectGuid());

                if (plr->GetSession())
                    plr->GetSession()->send_packet(&data);
            }
        }
    }

    // Notify need before greed passes after all rollers are added (or not
    // everyone will see the pass that player did)
    for (auto& auto_passer : auto_passers)
        send_player_roll(*auto_passer.first, auto_passer.second, true);

    // End any rolls that has no rollers
    for (auto roll : pending_rolls_)
    {
        if (roll->all_rolls_placed() && !roll->finished())
            complete_roll(roll);

        // NOTE: complete_roll can clear pending_rolls_
        if (pending_rolls_.empty())
            break;
    }

    roll_timer_ = LOOT_ROLL_TIMEOUT;
    rolls_started_ = true;
}

void loot_distributor::finish_rolls()
{
    assert(loot_);

    // End any unfinished rolls and distribute their loot
    for (auto roll : pending_rolls_)
    {
        if (!roll->finished())
            complete_roll(roll);

        // NOTE: complete_roll can clear pending_rolls_
        if (pending_rolls_.empty())
            break;
    }
}

void loot_distributor::complete_roll(Roll* roll)
{
    assert(loot_);
    assert(roll);

    roll->end_roll();

    // Send out the relevant roll results
    for (auto itr = roll->send_to().begin(); itr != roll->send_to().end();
         ++itr)
        send_player_roll(*roll, *itr);

    if (roll->winner())
    {
        send_roll_won(*roll);
        LootItem* item = &loot_->items_[roll->loot_slot()];
        item->is_blocked = false;

        if (Player* winner =
                sObjectAccessor::Instance()->FindPlayer(roll->winner()))
        {
            if (winner->GetSession())
            {
                inventory::transaction trans(
                    true, inventory::transaction::send_party);
                trans.add(item->itemid, item->count, item->randomPropertyId);
                if (!winner->storage().verify(trans))
                {
                    winner->SendEquipError(
                        static_cast<InventoryResult>(trans.error()), nullptr,
                        nullptr, item->itemid);
                }
                else
                {
                    item->is_looted = true;
                    loot_->notify_item_removed(roll->loot_slot());
                    --loot_->unlooted_count_;
                    loot_->on_loot_item(item, winner->GetObjectGuid());
                    winner->storage().finalize(trans);
                }
            }
        }
    }
    else
    {
        // Everyone passed
        send_roll_all_passed(*roll);
        loot_->items_[roll->loot_slot()].is_blocked = false;
        ffa_items_.insert(
            &loot_->items_[roll->loot_slot()]); // Now FFA lootable
    }

    // The following code is about the sparkling of loot, and is not needed for
    // any non-creature corpse loot
    if (!lootee_guid_.IsCreatureOrPet() || loot_type_ != LOOT_CORPSE)
        return;

    // Remove the lootability if the last item was just removed
    if (loot_->looted())
    {
        acceptable_loot_viewers_.clear();
        lootee_->RemoveFlag(UNIT_DYNAMIC_FLAGS,
            UNIT_DYNFLAG_LOOTABLE); // Causes an update of the corpse's
                                    // sparkling
        end_loot_session();
    }
    else if (!loot_->gold_)
    {
        // We have to reconsider everyone individually if gold has already been
        // removed (with gold in the corpse everyone would still be able to see
        // it)
        for (auto itr = acceptable_loot_viewers_.begin();
             itr != acceptable_loot_viewers_.end();)
        {
            auto currItr =
                itr++; // Take a step forward before we begin playing with the
                       // object

            Player* player = sObjectAccessor::Instance()->FindPlayer(*currItr);
            if (!player)
                continue;

            bool remove_current = true;
            // Check if we can loot any remaining item
            for (size_t i = 0; i < loot_->size(); ++i)
            {
                LootItem* item = loot_->get_slot_item(i);
                if (!item)
                    continue;
                if (players_loot_slot(player, item, item->needs_quest) !=
                    LOOT_SLOT_NONE)
                {
                    remove_current = false;
                    break;
                }
            }
            if (!remove_current)
                continue; // Yes, we can still loot.

            // Nothing left for this person to loot
            acceptable_loot_viewers_.erase(*currItr);
            if (lootee_guid_.IsCreatureOrPet() && loot_type_ == LOOT_CORPSE)
            {
                // We need to mark this player for a force update of his dynamic
                // flags
                lootee_->ForceUpdateDynflagForPlayer(player);
            }
        }
    }
}

void loot_distributor::_update_rolls(const uint32 diff)
{
    if (roll_timer_ <= diff)
    {
        finish_rolls();
    }
    else
        roll_timer_ -= diff;
}

Player* loot_distributor::loot_owner() const
{
    if (!loot_owner_)
        return nullptr;
    return sObjectAccessor::Instance()->FindPlayer(loot_owner_);
}

Roll* loot_distributor::get_roll_for_item(const LootItem* item) const
{
    for (const auto& elem : pending_rolls_)
        if ((elem)->item() == item)
            return elem;
    return nullptr;
}

void loot_distributor::end_loot_session()
{
    for (auto& elem : pending_rolls_)
        delete elem;
    pending_rolls_.clear();
    roll_timer_ = 0;
    has_rollable_items_ = false;
}

void loot_distributor::cancel_loot_session()
{
    if (!loot_)
        return;

    finish_rolls();
    end_loot_session();
}

bool loot_distributor::can_view_loot(const Player* looter) const
{
    if (!loot_)
        return false;

    if (lootee_->GetTypeId() == TYPEID_UNIT &&
        static_cast<Creature*>(lootee_)->IsTemporarySummon() &&
        !static_cast<TemporarySummon*>(lootee_)->CanDropLoot())
        return false;

    if (anyone_can_view_loot())
        return true;

    if (owner_only_loot())
        return looter->GetObjectGuid() == loot_owner_;

    if (loot_type_ == LOOT_SKINNING)
        return recipient_mgr_.has_tap(looter);

    // If main looter has checked out the loot and it still contains gold,
    // everyone can check it out (we must have a tap though)
    if (loot_checked_by_chosen_one_ && loot_->gold_ &&
        recipient_mgr_.has_tap(looter))
        return true;

    // If there exists FFA items you can loot you can always display the loot
    // window
    if (!ffa_items_.empty())
    {
        // Must of course have tap, though:
        if (!recipient_mgr_.has_tap(looter))
            return false;

        for (const auto& elem : ffa_items_)
            if (!(elem)->is_looted &&
                loot_->can_loot_item(elem, looter->GetObjectGuid()))
                return true;
    }

    return acceptable_loot_viewers_.find(looter->GetObjectGuid()) !=
           acceptable_loot_viewers_.end();
}

void loot_distributor::generate_loot(Player* loot_owner)
{
    LOG_DEBUG(logger,
        "loot_distributor::generate_loot: Loot owner: %s Lootee: %s",
        loot_owner ? loot_owner->GetGuidStr().c_str() : "None",
        lootee_guid_.GetString().c_str());

    if (!loot_owner && needs_loot_owner())
        return;

    if (lootee_guid_.IsCreatureOrPet() &&
        static_cast<Creature*>(lootee_)->IsTemporarySummon() &&
        !static_cast<TemporarySummon*>(lootee_)->CanDropLoot())
        return;

    cancel_loot_session(); // In case an on-going session exists
    delete loot_;
    loot_ = new (std::nothrow) Loot;
    if (!loot_)
        return;

    loot_owner_.Clear();
    if (loot_owner)
        loot_owner_ = loot_owner->GetObjectGuid();

    switch (lootee_guid_.GetHigh())
    {
    case HIGHGUID_GAMEOBJECT:
        generate_GO_loot(loot_owner);
        break;
    case HIGHGUID_ITEM:
        generate_item_loot(loot_owner);
        break;
    case HIGHGUID_CORPSE:
        generate_corpse_loot(loot_owner);
        break;
    case HIGHGUID_UNIT:
        generate_creature_loot(loot_owner);
        break;
    case HIGHGUID_PET:
        generate_creature_loot(loot_owner);
        break;
    default:
        break;
    }
}

bool loot_distributor::display_loot(Player* player)
{
    LOG_DEBUG(logger, "loot_distributor::DisplayLoot. Looter: %s Lootee: %s",
        player->GetGuidStr().c_str(), lootee_guid_.GetString().c_str());

    // Release currently looted target
    if (ObjectGuid lootGuid = player->GetLootGuid())
        player->GetSession()->DoLootRelease(lootGuid);

    if (!loot_)
    {
        logger.error(
            "Attempted looting object without any generated loot in it "
            "(Object: %s Looting player: %s)",
            lootee_guid_.GetString().c_str(), player->GetGuidStr().c_str());
        return false;
    }

    if (!can_view_loot(player))
    {
        logger.error(
            "Attempted looting object without being able to view loot (Object: "
            "%s Looting player: %s)",
            lootee_guid_.GetString().c_str(), player->GetGuidStr().c_str());
        return false;
    }

    bool result = false;
    switch (lootee_guid_.GetHigh())
    {
    case HIGHGUID_GAMEOBJECT:
        result = display_GO_loot(player);
        break;
    case HIGHGUID_ITEM:
        result = display_item_loot(player);
        break;
    case HIGHGUID_CORPSE:
        result = display_corpse_loot(player);
        break;
    case HIGHGUID_UNIT:
        result = display_creature_loot(player);
        break;
    case HIGHGUID_PET:
        result = display_creature_loot(player);
        break;
    default:
        break;
    }
    if (!result)
        return false;

    player->SetLootGuid(lootee_guid_);

    // Some loot types are server side only; for those we send another one
    LootType client_type = loot_type_;
    switch (loot_type_)
    {
    case LOOT_SKINNING:
        client_type = LOOT_PICKPOCKETING;
        break;
    case LOOT_PROSPECTING:
        client_type = LOOT_PICKPOCKETING;
        break;
    case LOOT_INSIGNIA:
        client_type = LOOT_CORPSE;
        break;
    case LOOT_NORMAL_ITEM:
        client_type = LOOT_PICKPOCKETING;
        break;
    case LOOT_FISHING_FAIL:
        client_type = LOOT_FISHING;
        break;
    case LOOT_FISHINGHOLE:
        client_type = LOOT_FISHING;
        break;
    case LOOT_LOCKPICKING:
        client_type = LOOT_SKINNING;
        break;
    default:
        break;
    }

    // Starts rolls if necessary
    if (!rolls_started_)
        start_rolls();

    // Note: We check so we can display the loot above in this function
    uint32 itemPackSize = (1 + 4 + 1 + 4 + 4 + 4 + 4 + 4 + 1) *
                          (loot_->items_.size() + loot_->quest_items_.size());
    WorldPacket data(SMSG_LOOT_RESPONSE, ((8 + 1 + 4 + 1) + itemPackSize));
    data << ObjectGuid(lootee_guid_);
    data << uint8(client_type);
    std::set<uint8> available_slots;
    build_loot_view(player, data, available_slots);
    player->SendDirectMessage(std::move(data));
    loot_->add_looter(player->GetObjectGuid(), available_slots);

    if ((lootee_guid_.IsCreatureOrPet() && loot_type_ != LOOT_PICKPOCKETING) ||
        (lootee_guid_.IsGameObject() &&
            static_cast<GameObject*>(lootee_)->GetGoType() ==
                GAMEOBJECT_TYPE_CHEST))
        player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);

    if (loot_method_ == MASTER_LOOT &&
        player->GetObjectGuid() == master_looter_)
        send_master_looting_list();

    return true;
}

void loot_distributor::close_loot_display(Player* player)
{
    // Note: we can't remove_looter at the top; we need it to check
    // can_see_item_slot

    // All loot release related code is done in LootHandler.cpp
    if (!loot_)
        return;

    // No need to modify lootable status if anyone can view this loot
    if (anyone_can_view_loot())
    {
        loot_->remove_looter(player->GetObjectGuid());
        return;
    }

    // If loot is empty erase everyone from looting (the removal of sparkling is
    // handled by WorldSession::DoLootRelease())
    if (loot_->looted())
    {
        acceptable_loot_viewers_.clear();
        loot_->remove_looter(player->GetObjectGuid());
        return;
    }

    // If player is the chosen group looter, his releasing the corpse means
    // everyone else can now look at the loot as well
    if (player->GetObjectGuid() == chosen_group_looter_)
    {
        loot_checked_by_chosen_one_ = true;

        for (auto& elem : loot_->items_)
            if (!elem.is_looted && !get_roll_for_item(&elem)) // We can never
                                                              // make a rolled
                                                              // for item FFA at
                                                              // this point
                ffa_items_.insert(&elem);
        for (auto& elem : loot_->quest_items_)
            if (!elem.is_looted)
                ffa_items_.insert(&elem);

        lootee_->ForceUpdateDynflag();
    }

    // Does player have any remaining loot?
    if (loot_->gold_)
    {
        loot_->remove_looter(player->GetObjectGuid());
        return;
    }
    if (loot_->unlooted_count_)
    {
        uint8 slot = 0;
        for (auto itr = loot_->items_.begin(); itr != loot_->items_.end();
             ++itr, ++slot)
        {
            if (!itr->is_looted &&
                loot_->can_loot_item(&*itr, player->GetObjectGuid()) &&
                loot_->can_see_item_slot(player->GetObjectGuid(), slot))
            {
                loot_->remove_looter(player->GetObjectGuid());
                return;
            }
        }
        for (auto itr = loot_->quest_items_.begin();
             itr != loot_->quest_items_.end(); ++itr, ++slot)
        {
            if (!itr->is_looted &&
                loot_->can_loot_item(&*itr, player->GetObjectGuid()) &&
                loot_->can_see_item_slot(player->GetObjectGuid(), slot))
            {
                loot_->remove_looter(player->GetObjectGuid());
                return;
            }
        }
    }

    loot_->remove_looter(player->GetObjectGuid());

    // Don't do stuff below this point for game objects
    if (lootee_guid_.IsGameObject())
        return;

    // No remaining loot for player, remove him as looter
    acceptable_loot_viewers_.erase(player->GetObjectGuid());
    // Clear the loot if we have no one whom can loot our loot any longer
    if (acceptable_loot_viewers_.empty())
        loot_->clear();

    // Stop sparkling for this player (if target is a dead creature)
    if (lootee_guid_.IsCreatureOrPet() &&
        (loot_type_ == LOOT_CORPSE || loot_type_ == LOOT_SKINNING))
    {
        // (PS: Only this player should be updated, since outside of him there's
        // still loot to be had (otherwise we wouldn't get here))
        lootee_->ForceUpdateDynflagForPlayer(player);
    }
}

void loot_distributor::apply_loot_selection(loot_selection_type type)
{
    auto item_ids = sLootSelection::Instance()->select_loot(type, lootee_);
    for (auto& item_id : item_ids)
    {
        if (ObjectMgr::GetItemPrototype(item_id.first))
        {
            loot_->unlooted_count_ += item_id.second;
            loot_->items_.push_back(LootItem(item_id.first, item_id.second));
        }
    }
}

void loot_distributor::generate_GO_loot(Player* loot_owner)
{
    assert(loot_);

    GameObject* go = loot_owner->GetMap()->GetGameObject(lootee_guid_);
    if (!go)
        return;

    if (loot_type_ == LOOT_FISHING)
    {
        uint32 zoneid, areaid;
        go->GetZoneAndAreaId(zoneid, areaid);
        // Use subzone loot if such exists, if not use zone loot
        if (!loot_->fill_loot(areaid, LootTemplates_Fishing, &recipient_mgr_,
                true, (areaid != zoneid)) &&
            areaid != zoneid)
            loot_->fill_loot(
                zoneid, LootTemplates_Fishing, &recipient_mgr_, true);
    }
    else if (loot_type_ == LOOT_FISHING_FAIL) // Junk fishes, etc
        loot_->fill_loot(0, LootTemplates_Fishing, &recipient_mgr_, true);
    else
    {
        auto goinfo = go->GetGOInfo();
        if (goinfo->GetLootId())
            loot_->fill_loot(go->GetGOInfo()->GetLootId(),
                LootTemplates_Gameobject, &recipient_mgr_, true);

        if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST)
            apply_loot_selection(LOOT_SELECTION_TYPE_CHEST);
    }
}

void loot_distributor::generate_item_loot(Player* loot_owner)
{
    assert(loot_);

    Item* item = loot_owner->GetItemByGuid(lootee_guid_);
    if (!item)
        return;

    if (!recipient_mgr_.has_tap(loot_owner))
        recipient_mgr_.add_solo_tap(loot_owner);

    if (loot_type_ == LOOT_DISENCHANTING)
        loot_->fill_loot(item->GetProto()->DisenchantID,
            LootTemplates_Disenchant, &recipient_mgr_, true);
    else if (loot_type_ == LOOT_PROSPECTING)
        loot_->fill_loot(
            item->GetEntry(), LootTemplates_Prospecting, &recipient_mgr_, true);
    else
    {
        auto proto = item->GetProto();
        loot_->fill_loot(item->GetEntry(), LootTemplates_Item, &recipient_mgr_,
            true, proto->MaxMoneyLoot == 0);
        loot_->generate_money_loot(proto->MinMoneyLoot, proto->MaxMoneyLoot);

        // TODO: When more selection types that are Items are added we need to
        //       mark lockboxes somehow
        if (proto->Flags & ITEM_FLAG_LOOTABLE)
            apply_loot_selection(LOOT_SELECTION_TYPE_LOCKBOX);
    }
}

void loot_distributor::generate_corpse_loot(Player* looter)
{
    assert(loot_);

    Corpse* corpse = static_cast<Corpse*>(lootee_);

    // Alterac Valley
    if (corpse->GetMap()->GetId() == 30)
    {
        // 0: alliance loot, 0xFFFFFF (3 byte integer): horde loot
        if (looter->GetTeam() == HORDE) // looter is opposite team of lootee
            loot_->fill_loot(0, LootTemplates_Creature, &recipient_mgr_, false);
        else
            loot_->fill_loot(
                0xFFFFFF, LootTemplates_Creature, &recipient_mgr_, false);
    }

    // FIXME: This formula needs redoing
    uint32 level = corpse->GetOwnerLevel();
    // This may need a better formula (Now it works like this: lvl10: ~6copper,
    // lvl70: ~9silver)
    loot_->gold_ =
        (uint32)(urand(50, 150) * 0.016f * pow(((float)level) / 5.76f, 2.5f) *
                 sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
}

void loot_distributor::generate_creature_loot(Player*)
{
    assert(loot_);

    Creature* creature = static_cast<Creature*>(lootee_);

    if (loot_type_ == LOOT_PICKPOCKETING)
    {
        if (uint32 lootid = creature->GetCreatureInfo()->pickpocketLootId)
            loot_->fill_loot(
                lootid, LootTemplates_Pickpocketing, &recipient_mgr_, false);

        // Generate extra money for pick pocket loot
        const uint32 a = urand(0, creature->getLevel() / 2);
        // const uint32 b = urand(0, lootOwner->getLevel()/2); FIXME: Is this
        // really correct? Leaving it out and using a for b in the meantime
        const uint32 b = urand(0, creature->getLevel() / 2);
        loot_->gold_ =
            uint32(10 * (a + b) *
                   sWorld::Instance()->getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
    }
    else if (loot_type_ == LOOT_SKINNING)
    {
        loot_->fill_loot(creature->GetCreatureInfo()->SkinLootId,
            LootTemplates_Skinning, &recipient_mgr_, false);
    }
    else // On Death Loot
    {
        // Don't generate loot if no recipients
        if (recipient_mgr()->empty())
            return;

        if (uint32 lootid = creature->GetCreatureInfo()->lootid)
            loot_->fill_loot(
                lootid, LootTemplates_Creature, &recipient_mgr_, false);

        if ((static_cast<Creature*>(lootee_)->GetCreatureInfo()->flags_extra &
                CREATURE_FLAG_NO_LOOT_SELECTION) == 0)
        {
            if (static_cast<Creature*>(lootee_)->IsRare())
                apply_loot_selection(LOOT_SELECTION_TYPE_RARE_CREATURE);
            else
                apply_loot_selection(LOOT_SELECTION_TYPE_CREATURE);
        }

        loot_->generate_money_loot(creature->GetCreatureInfo()->mingold,
            creature->GetCreatureInfo()->maxgold);
    }
}

bool loot_distributor::display_GO_loot(Player* player)
{
    assert(loot_);

    GameObject* go = static_cast<GameObject*>(lootee_);

    if (!go->isSpawned())
        return false;

    if (go->getLootState() == GO_READY)
    {
        if ((go->GetEntry() == BG_AV_OBJECTID_MINE_N ||
                go->GetEntry() == BG_AV_OBJECTID_MINE_S))
            if (BattleGround* bg = player->GetBattleGround())
                if (bg->GetTypeID() == BATTLEGROUND_AV)
                    if (!(((BattleGroundAV*)bg)
                                ->PlayerCanDoMineQuest(
                                    go->GetEntry(), player->GetTeam())))
                        return false;
    }

    go->SetLootState(GO_ACTIVATED);
    return true;
}

bool loot_distributor::display_item_loot(Player* player)
{
    assert(loot_);

    Item* item = static_cast<Item*>(lootee_);

    // NOTE: loot state set and loot generated in Item::OnLootOpen()
    if (!item->HasGeneratedLoot())
    {
        logger.error(
            "loot_distributor::display_item_loot called for %s with loot type: "
            "%u, but item %u has not had loot generated yet!",
            player->GetObjectGuid().GetString().c_str(), loot_type_,
            item->GetEntry());
        return false;
    }

    return true;
}

bool loot_distributor::display_corpse_loot(Player* player)
{
    assert(loot_);

    Corpse* corpse = player->GetMap()->GetCorpse(lootee_guid_);
    if (!corpse ||
        !((loot_type_ == LOOT_CORPSE) || (loot_type_ == LOOT_INSIGNIA)) ||
        (corpse->GetType() != CORPSE_BONES))
        return false;

    return true;
}

bool loot_distributor::display_creature_loot(Player* player)
{
    assert(loot_);

    Creature* creature;
    if (lootee_guid_.IsPet())
        creature = player->GetMap()->GetPet(lootee_guid_);
    else
        creature = player->GetMap()->GetCreature(lootee_guid_);

    if (!creature ||
        creature->isAlive() != (loot_type_ == LOOT_PICKPOCKETING) ||
        !creature->IsWithinDistInMap(player, INTERACTION_DISTANCE) ||
        (loot_type_ == LOOT_PICKPOCKETING && player->IsFriendlyTo(creature)))
        return false;

    // Pick-pocketing
    if (loot_type_ == LOOT_PICKPOCKETING)
        return true;
    else if (loot_type_ == LOOT_SKINNING)
    {
        // Set unit as lootable if something was skinned
        if (!loot_->looted())
            creature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
        return true;
    }

    // On-Death loot
    return true;
}

bool loot_distributor::within_loot_dist(Player* looter) const
{
    // Must have item in bags to loot it (TODO: Check that it's not in the bank,
    // if we're not close to a bank)
    if (lootee_guid_.IsItem())
        return looter->GetItemByGuid(lootee_guid_);

    // Owned game objects (fishing bobber) can be looted no matter the distance
    if (lootee_guid_.IsGameObject() &&
        static_cast<GameObject*>(lootee_)->GetOwnerGuid() ==
            looter->GetObjectGuid())
        return true;

    // Fish schools can be looted within fishing distance (which is 20 yards;
    // adding some extra leniancy)
    if (lootee_guid_.IsGameObject() &&
        static_cast<GameObject*>(lootee_)->GetGoType() ==
            GAMEOBJECT_TYPE_FISHINGHOLE &&
        looter->IsWithinDistInMap(static_cast<WorldObject*>(lootee_), 30.0f))
        return true;

    return looter->IsWithinDistInMap(
        static_cast<WorldObject*>(lootee_), INTERACTION_DISTANCE);
}

LootSlotType loot_distributor::players_loot_slot(
    Player* looter, LootItem* item, bool quest_item) const
{
    // Note: LOOT_SLOT_NONE is a Server side indicator of lacking permission

    if (!loot_ || item->is_looted)
        return LOOT_SLOT_NONE;

    // can_loot_item returns false if this player has looted a FFA item (an item
    // which each player has their own copy of)
    // Master looting lootability needs to be checked further below (since
    // master looter can handout recipes he can't see normally)
    if (!anyone_can_view_loot() && loot_method_ != MASTER_LOOT &&
        !loot_->can_loot_item(item, looter->GetObjectGuid()))
        return LOOT_SLOT_NONE;

    if (anyone_can_view_loot())
    {
        // Must check master looting for chests (since it does not lock the item
        // like rolling does)
        if (loot_method_ == MASTER_LOOT &&
            item->item_quality >= loot_threshold_)
        {
            if (looter->GetObjectGuid() == master_looter_)
                return LOOT_SLOT_MASTER;
            else
                return LOOT_SLOT_VIEW;
        }

        // Check quest and conditions
        if (!loot_->can_loot_item(item, looter->GetObjectGuid(), true))
            return LOOT_SLOT_NONE;

        return LOOT_SLOT_NORMAL;
    }

    // Skinning just depends on if you're a recipient or not
    if (lootee_guid_.IsCreatureOrPet() &&
        loot_type_ ==
            LOOT_SKINNING) // note: skinning used for locked GOs as well
        return recipient_mgr_.has_tap(looter) ? LOOT_SLOT_OWNER :
                                                LOOT_SLOT_NONE;

    if (recipient_mgr_.party_loot())
    {
        if (!recipient_mgr_.has_tap(looter))
            return LOOT_SLOT_NONE;

        if (item->one_per_player)
            return LOOT_SLOT_NORMAL;

        // You can never see quest items you do not have the quest for
        if (quest_item && !looter->HasQuestForItem(item->itemid))
            return LOOT_SLOT_NONE;

        // Handle the different looting methods
        return _players_loot_slot_different_modes(looter, item, quest_item);
    }
    // Solo looting means you can loot everything as long as you're the owner
    else if (loot_owner_ == looter->GetObjectGuid())
        return LOOT_SLOT_OWNER;

    return LOOT_SLOT_NONE;
}

// Internal helper to split up above function
LootSlotType loot_distributor::_players_loot_slot_different_modes(
    Player* looter, LootItem* item, bool /*quest_item*/) const
{
    assert(loot_);

    // Note: quest items that can be looted by anyone are in ffaItems as well,
    // so no extra check for them needed
    switch (loot_method_)
    {
    case FREE_FOR_ALL:
        return LOOT_SLOT_NORMAL;

    case ROUND_ROBIN:
        if (chosen_group_looter_ == looter->GetObjectGuid() ||
            ffa_items_.find(item) != ffa_items_.end())
            return LOOT_SLOT_NORMAL;
        return LOOT_SLOT_NONE;

    case MASTER_LOOT:
        if (item->item_quality >= loot_threshold_)
        {
            if (looter->GetObjectGuid() == master_looter_)
            {
                if (item->one_per_player)
                {
                    // Item has a copy per player, which means we won't master
                    // loot it even though it's above threshold
                    if (!loot_->can_loot_item(item, looter->GetObjectGuid()) ||
                        !uniqueness_check(looter, item)) // Master looting
                                                         // skipped these checks
                                                         // earlier
                        return LOOT_SLOT_NONE;
                    else
                        return LOOT_SLOT_NORMAL;
                }
                else
                {
                    // We skip the condition id for this item, since master
                    // looter can still see it and hand it out (in the case of
                    // profession-only recipes)
                    return LOOT_SLOT_MASTER;
                }
            }
            else
            {
                return LOOT_SLOT_VIEW;
            }
        }
        else
        {
            // Items below threshold (for this item we are considered the same
            // as any normal looter)
            if (!loot_->can_loot_item(item, looter->GetObjectGuid()) ||
                !uniqueness_check(looter,
                    item)) // Master looting skipped these checks earlier
                return LOOT_SLOT_NONE;
            if (chosen_group_looter_ == looter->GetObjectGuid() ||
                ffa_items_.find(item) != ffa_items_.end())
                return LOOT_SLOT_NORMAL;
            return LOOT_SLOT_NONE;
        }

    case GROUP_LOOT:
    case NEED_BEFORE_GREED:
        if (item->item_quality >= loot_threshold_)
        {
            // If part of ffa items this item is now lootable
            if (ffa_items_.find(item) != ffa_items_.end())
                return LOOT_SLOT_NORMAL;

            // If roll is already done this item becomes lootable by the winner
            if (Roll* roll = get_roll_for_item(item))
                if (roll->finished())
                    return roll->winner() == looter->GetObjectGuid() ?
                               LOOT_SLOT_NORMAL :
                               LOOT_SLOT_NONE;

            return LOOT_SLOT_VIEW;
        }
        if (chosen_group_looter_ == looter->GetObjectGuid() ||
            ffa_items_.find(item) != ffa_items_.end())
            return LOOT_SLOT_NORMAL;
        return LOOT_SLOT_NONE;

    default:
        break;
    }

    return LOOT_SLOT_NONE;
}

/*
 * LOOTING NETWORK CODE
 */
void loot_distributor::build_loot_view(
    Player* looter, WorldPacket& packet, std::set<uint8>& available_indices)
{
    assert(loot_);

    packet << uint32(loot_->gold_); // gold

    uint8 item_count = 0;
    size_t item_count_pos = packet.wpos();
    packet << uint8(0); // item count

    uint8 slot_count = 0;

    // Normal drops
    for (auto itr = loot_->items_.begin(); itr != loot_->items_.end();
         ++itr, ++slot_count)
    {
        LootSlotType slot = players_loot_slot(looter, &*itr, false);
        if (slot == LOOT_SLOT_NONE ||
            slot >= MAX_LOOT_SLOT_TYPE) // LOOT_SLOT_NONE: Server side
                                        // indication to skip sending loot
            continue;

        packet << uint8(slot_count);   // index
        packet << uint32(itr->itemid); // item id
        packet << uint32(itr->count);  // item count
        packet << uint32(ObjectMgr::GetItemPrototype(itr->itemid)
                             ->DisplayInfoID);   // display id
        packet << uint32(itr->randomSuffix);     // Rand suffix
        packet << uint32(itr->randomPropertyId); // prop id
        packet << uint8(slot);                   // slot type

        ++item_count;
        available_indices.insert(slot_count);
    }

    // Quest drops
    for (auto itr = loot_->quest_items_.begin();
         itr != loot_->quest_items_.end(); ++itr, ++slot_count)
    {
        LootSlotType slot = players_loot_slot(looter, &*itr, true);
        if (slot == LOOT_SLOT_NONE ||
            slot >= MAX_LOOT_SLOT_TYPE) // LOOT_SLOT_NONE: Server side
                                        // indication to skip sending loot
            continue;

        packet << uint8(slot_count);   // index
        packet << uint32(itr->itemid); // item id
        packet << uint32(itr->count);  // item count
        packet << uint32(ObjectMgr::GetItemPrototype(itr->itemid)
                             ->DisplayInfoID);   // display id
        packet << uint32(itr->randomSuffix);     // Rand suffix
        packet << uint32(itr->randomPropertyId); // prop id
        packet << uint8(slot);                   // slot type

        ++item_count;
        available_indices.insert(slot_count);
    }

    packet.put<uint8>(item_count_pos, item_count);
}

WorldPacket loot_distributor::build_roll_start_packet(const Roll& r) const
{
    assert(loot_);

    LootItem& li = loot_->items_[r.loot_slot()];
    WorldPacket data(SMSG_LOOT_START_ROLL, (8 + 4 + 4 + 4 + 4 + 4 + 1));
    data << lootee_guid_;          // object guid of what we're looting
    data << uint32(r.loot_slot()); // item slot in loot
    data << uint32(
        li.itemid); // the itemEntryId for the item that shall be rolled for
    data << uint32(li.randomSuffix);     // randomSuffix
    data << uint32(li.randomPropertyId); // item random property ID
    data << uint32(
        LOOT_ROLL_TIMEOUT); // the countdown time to choose "need" or "greed"

    return data;
}

void loot_distributor::send_master_looting_list()
{
    if (!master_looter_)
        return;
    Player* masterLooter =
        sObjectAccessor::Instance()->FindPlayer(master_looter_);
    if (!masterLooter || !masterLooter->GetSession())
        return;

    WorldPacket data(
        SMSG_LOOT_MASTER_LIST, 1 + recipient_mgr_.taps()->size() * 8);
    data << uint8(recipient_mgr_.taps()->size());

    // Add all tappers
    for (const auto& elem : *recipient_mgr_.taps())
        data << elem;

    // We only need to send the list to the master looter
    masterLooter->GetSession()->send_packet(std::move(data));
}

void loot_distributor::send_roll_all_passed(const Roll& r) const
{
    assert(loot_);

    LootItem& li = loot_->items_[r.loot_slot()];
    WorldPacket data(SMSG_LOOT_ALL_PASSED, (8 + 4 + 4 + 4 + 4));
    data << lootee_guid_;          // object guid of what we're looting
    data << uint32(r.loot_slot()); // item slot in loot
    data << uint32(
        li.itemid); // the itemEntryId for the item that shall be rolled for
    data << uint32(li.randomSuffix);     // randomSuffix
    data << uint32(li.randomPropertyId); // item random property ID

    for (const auto& elem : r)
        if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem.first))
            if (plr->GetSession())
                plr->GetSession()->send_packet(&data);
}

void loot_distributor::send_roll_won(const Roll& r) const
{
    assert(loot_);

    LootItem& li = loot_->items_[r.loot_slot()];
    WorldPacket data(SMSG_LOOT_ROLL_WON, (8 + 4 + 4 + 4 + 4 + 8 + 1 + 1));
    data << lootee_guid_;          // object guid of what we're looting
    data << uint32(r.loot_slot()); // item slot in loot
    data << uint32(
        li.itemid); // the itemEntryId for the item that shall be rolled for
    data << uint32(li.randomSuffix);           // randomSuffix
    data << uint32(li.randomPropertyId);       // item random property ID
    data << r.winner();                        // guid of the player who won
    data << uint8(r.players_roll(r.winner())); // 1-100 roll result
    data << uint8(r.players_roll_type(r.winner())); // Roll::RollType

    for (const auto& elem : r)
        if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem.first))
            if (plr->GetSession())
                plr->GetSession()->send_packet(&data);
}

void loot_distributor::send_player_roll(
    const Roll& r, ObjectGuid roller, bool auto_pass) const
{
    assert(loot_);

    if (!r.has_roller(roller))
        return;

    Roll::roll_type rt;
    uint8 rollNumber;
    rt = r.players_roll_type(roller);
    if (rt == Roll::ROLL_PENDING)
        return;
    rollNumber = r.finished() ? r.players_roll(roller) :
                                (rt == Roll::ROLL_NEED) ? 0 : 128;
    if (rt == Roll::ROLL_PASS)
        rollNumber = 128; // Pass is always 128
    if (rt == Roll::ROLL_NEED && !r.finished())
        rt = static_cast<Roll::roll_type>(0); // This does not mean pass in this
                                              // case; it means need for
                                              // unfinished roll

    LootItem& li = loot_->items_[r.loot_slot()];
    WorldPacket data(SMSG_LOOT_ROLL, (8 + 4 + 8 + 4 + 4 + 4 + 1 + 1 + 1));
    data << lootee_guid_;          // object guid of what we're looting
    data << uint32(r.loot_slot()); // item slot in loot
    data << uint64(roller);
    data << uint32(
        li.itemid); // the itemEntryId for the item that shall be rolled for
    data << uint32(li.randomSuffix);     // randomSuffix
    data << uint32(li.randomPropertyId); // item random property ID
    data << uint8(rollNumber); // How much you rolled. If roll is not done: 0
                               // for need (see next value's remarks) or 128 for
                               // any other option
    data << uint8(rt); // 0 for need (if rollnumb is 0) or pass (if roll numb is
                       // 128), 1 for need if the roll is finished and 2 for
                       // greed
    data << uint8(auto_pass ? 1 : 0); // 1: "You automatically passed on: %s
                                      // because you cannot loot that item."

    for (const auto& elem : r)
        if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem.first))
            if (plr->GetSession())
                plr->GetSession()->send_packet(&data);
}

void loot_recipient_mgr::attempt_add_tap(Player* player)
{
    if (empty())
    {
        add_first_tap(player);
    }
    else if (!tapping_group_id_ && player->GetGroup())
    {
        // There can only be one player in our tap list at this point
        Player* first = first_valid_player();
        if (first && first->GetGroup() &&
            first->GetGroup()->GetId() == player->GetGroup()->GetId())
            add_tap(player);
    }
    else if (player->GetGroup() &&
             player->GetGroup()->GetId() == tapping_group_id_)
    {
        add_tap(player);
    }
}

void loot_recipient_mgr::add_tap(Player* player)
{
    assert(player);

    // If a group has formed since first tap was created
    if (!empty() && !tapping_group_id_ && player->GetGroup())
    {
        // The entire group is not added at this point
        tapping_group_id_ = player->GetGroup()->GetId();
    }

    // Limit amount of taps depending on group type
    if (!empty() && player->GetGroup())
    {
        if (player->GetGroup()->isRaidGroup())
        {
            if (loot_recipients_.size() >= 40)
                return;
        }
        else
        {
            if (loot_recipients_.size() >= 5)
                return;
        }
    }

    if (tapping_group_id_)
    {
        auto find = loot_recipients_.find(player->GetObjectGuid());
        if (find == loot_recipients_.end())
        {
            loot_recipients_.insert(
                player->GetObjectGuid()); // Must insert before force call
            // We need to force a dynamic update, so that tag status will be
            // properly displayed
            owner_->ForceUpdateDynflagForPlayer(player);
        }
    }
    else
        loot_recipients_.insert(player->GetObjectGuid());
}

void loot_recipient_mgr::add_first_tap(Player* player)
{
    if (auto group = player->GetGroup())
    {
        tapping_group_id_ = player->GetGroup()->GetId();
        for (auto other : group->members(true))
        {
            if (player == other)
                loot_recipients_.insert(other->GetObjectGuid());
            else if ((player->GetMap()->IsDungeon() &&
                         (player->GetInstanceId() == other->GetInstanceId())) ||
                     (player->IsWithinDistInMap(
                         other, sWorld::Instance()->getConfig(
                                    CONFIG_FLOAT_GROUP_XP_DISTANCE))))
            {
                // You're eligable if you're either in the same instance as
                // the tagging player, or in XP distance
                loot_recipients_.insert(other->GetObjectGuid());
            }
        }
    }
    else
        add_tap(player);

    if (owner_->GetTypeId() == TYPEID_UNIT)
        owner_->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED);
}

Group* loot_recipient_mgr::group() const
{
    if (!tapping_group_id_)
        return nullptr;
    return sObjectMgr::Instance()->GetGroupById(tapping_group_id_);
}

void loot_recipient_mgr::reset()
{
    if (owner_->GetTypeId() == TYPEID_UNIT)
        owner_->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED);
    loot_recipients_.clear();
    tapping_group_id_ = 0;
}

Player* loot_recipient_mgr::first_valid_player() const
{
    Player* found = nullptr;
    for (const auto& elem : loot_recipients_)
        if ((found = sObjectAccessor::Instance()->FindPlayer(elem)) != nullptr)
            break;
    return found;
}

void Roll::add_roller(ObjectGuid player)
{
    individual_roll_data rd;
    if (rolls_.find(player) == rolls_.end())
        ++pending_roll_count_;
    rolls_[player] = rd;
}

void Roll::add_auto_passer(ObjectGuid player)
{
    individual_roll_data rd;
    rd.auto_passing_ = true;
    rd.roll_type_ = ROLL_PASS;
    rd.roll_result_ = 0;
    rolls_[player] = rd;
}

void Roll::end_roll()
{
    auto winner = rolls_.end();

    std::vector<ObjectGuid> needs;
    needs.reserve(rolls_.size());
    std::vector<ObjectGuid> greeds;
    greeds.reserve(rolls_.size());

    for (auto itr = rolls_.begin(); itr != rolls_.end(); ++itr)
    {
        // Put any pending rolls as passed
        if (itr->second.roll_type_ == ROLL_PENDING)
        {
            itr->second.roll_type_ = ROLL_PASS;
            send_to_.push_back(itr->first);
        }

        // Shouldn't ever get run, but just in case we make sure we can need the
        // item: ((not needed in TBC I found out, leaving it here in case we
        // implement wotlk))
        /*if (itr->second.roll_type_ == ROLL_NEED && !itr->second.canNeed)
        {
            itr->second.roll_type_ = ROLL_PASS;
            send_to_.push_back(itr->first);
        }*/

        // A passer can never be a winner (PS: life lesson included for free)
        if (itr->second.roll_type_ == ROLL_PASS)
            continue;
        // Skip greeders if needers are already found
        if (itr->second.roll_type_ == ROLL_GREED && !needs.empty())
            continue;

        itr->second.roll_result_ = urand(1, 100);

        if (winner == rolls_.end())
            winner = itr;
        else
        {
            if (winner->second.roll_type_ == ROLL_GREED &&
                itr->second.roll_type_ == ROLL_NEED) // Need trumps Greed
                winner = itr;
            else if (itr->second.roll_result_ >
                     winner->second.roll_result_) // Bigger roll
                winner = itr;
            else if (winner->second.roll_result_ ==
                     itr->second.roll_result_) // Equal roll
            {
                // Do a further roll to decide who gets the loot. 0 winner
                // stays, 1 he goes
                // TODO: This is mathematically unfair if more than 2 people
                // have equal rolls.
                if (urand(0, 1) == 1)
                    winner = itr;
            }
        }

        if (itr->second.roll_type_ == ROLL_NEED)
            needs.push_back(itr->first);
        else if (itr->second.roll_type_ == ROLL_GREED &&
                 winner->second.roll_type_ != ROLL_NEED)
            greeds.push_back(itr->first); // Don't push back greeders if we
                                          // already got a needer
    }

    if (!needs.empty())
    {
        send_to_.reserve(needs.size());
        for (auto& need : needs)
            send_to_.push_back(need);
    }
    else if (!greeds.empty())
    {
        send_to_.reserve(greeds.size());
        for (auto& greed : greeds)
            send_to_.push_back(greed);
    }

    if (winner == rolls_.end())
        winner_.Clear();
    else
        winner_ = winner->first;

    finished_ = true;
}

uint32 Roll::players_roll(ObjectGuid looter) const
{
    auto find = rolls_.find(looter);
    if (find != rolls_.end())
        return find->second.roll_result_;
    return 0;
}

Roll::roll_type Roll::players_roll_type(ObjectGuid looter) const
{
    auto find = rolls_.find(looter);
    if (find != rolls_.end())
        return find->second.roll_type_;
    return ROLL_PENDING;
}

bool Roll::place_players_roll(ObjectGuid looter, roll_type roll)
{
    if (!pending_roll_count_)
        return false;

    auto find = rolls_.find(looter);
    if (find != rolls_.end())
    {
        if (find->second.roll_type_ == ROLL_PENDING)
        {
            find->second.roll_type_ = roll;
            --pending_roll_count_;
            return true;
        }
    }

    return false;
}

/*
 * PLAYER ACTION RESPONSES
 */

void loot_distributor::attempt_master_loot_handout(
    Player* master_looter, Player* target, uint8 loot_slot)
{
    LOG_DEBUG(logger,
        "loot_distributor::AttemptMasterLootHandout: Lootee: %s MasterLooter: "
        "%s Target: %s LootSlot: %i",
        lootee_->GetGuidStr().c_str(), master_looter->GetGuidStr().c_str(),
        target->GetGuidStr().c_str(), loot_slot);

    if (!loot_)
        return;
    if (master_looter->GetLootGuid() != lootee_guid_)
        return;
    if (loot_method_ != MASTER_LOOT ||
        master_looter->GetObjectGuid() != master_looter_)
        return;
    // Loot we can master loot is also loot that is rollable
    if (!rollable_loot_type())
        return;
    if (!within_loot_dist(master_looter))
        return;
    if (!can_view_loot(master_looter))
        return;

    // Make sure looter can see the item
    if (!loot_->can_see_item_slot(master_looter->GetObjectGuid(), loot_slot))
        return;
    LootItem* item = loot_->get_slot_item(loot_slot);
    if (!item)
        return;

    if (!uniqueness_check(target, item))
    {
        master_looter->SendEquipError(
            EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, nullptr);
        return;
    }

    if (item->is_looted)
    {
        master_looter->SendEquipError(
            EQUIP_ERR_ALREADY_LOOTED, nullptr, nullptr);
        return;
    }

    // Check conditions for the target
    if (!loot_->can_loot_item(item, target->GetObjectGuid()) ||
        !uniqueness_check(target, item))
    {
        master_looter->SendEquipError(
            EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW, nullptr, nullptr);
        return;
    }

    // Only allow items that are supposed to be master looted to be master
    // looted
    LootSlotType slotType = players_loot_slot(master_looter, item, false);
    if (slotType != LOOT_SLOT_MASTER)
        return;

    inventory::transaction trans;
    trans.add(item->itemid, item->count, item->randomPropertyId);
    if (!target->storage().verify(trans))
    {
        target->SendEquipError(static_cast<InventoryResult>(trans.error()),
            nullptr, nullptr, item->itemid);

        // Send duplicate of error massage to master looter
        if (master_looter != target)
            master_looter->SendEquipError(
                static_cast<InventoryResult>(trans.error()), nullptr, nullptr,
                item->itemid);
        return;
    }
    target->storage().finalize(trans);
    // XXX notify players... (winner->SendNewItem(stored_item, item->count,
    // false, false, true);)

    // mark as looted
    loot_->on_loot_item(item, target->GetObjectGuid());
    item->is_looted = true;

    loot_->notify_item_removed(loot_slot);
    --loot_->unlooted_count_;
}

void loot_distributor::attempt_loot_money(Player* looter)
{
    LOG_DEBUG(logger,
        "loot_distributor::AttemptLootMoney. Lootee: %s Looter: %s",
        lootee_->GetGuidStr().c_str(), looter->GetGuidStr().c_str());

    if (!loot_ || !loot_->gold_)
        return;
    if (looter->GetLootGuid() != lootee_guid_)
        return;
    if (!within_loot_dist(looter))
        return;
    if (!can_view_loot(looter))
        return;

    if (recipient_mgr_.party_loot() && !lootee_guid_.IsItem())
    {
        if (recipient_mgr_.taps()->empty())
            return;

        uint32 eligible_players = 0;
        for (const auto& elem : *recipient_mgr_.taps())
        {
            Player* plr = sObjectAccessor::Instance()->FindPlayer(elem);
            if (plr && plr->GetSession() &&
                plr->IsWithinDistInMap(static_cast<WorldObject*>(lootee_),
                    sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_GROUP_XP_DISTANCE)))
                ++eligible_players;
        }

        uint32 money_split = loot_->gold_;
        uint32 money_rest = 0;
        if (eligible_players > 1)
        {
            money_split /= eligible_players;
            money_rest = loot_->gold_ % eligible_players;
        }

        for (const auto& elem : *recipient_mgr_.taps())
        {
            Player* plr = sObjectAccessor::Instance()->FindPlayer(elem);
            if (plr && plr->GetSession() &&
                plr->IsWithinDistInMap(static_cast<WorldObject*>(lootee_),
                    sWorld::Instance()->getConfig(
                        CONFIG_FLOAT_GROUP_XP_DISTANCE)))
            {
                uint32 money = money_split;
                // Give the rest to the chosen group looter (if one exists)
                if (plr->GetObjectGuid() == chosen_group_looter_)
                {
                    money += money_rest;
                    money_rest = 0;
                }

                // Skip looter for now (for sake of rest)
                if (plr->GetObjectGuid() == looter->GetObjectGuid())
                    continue;

                WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4);
                data << uint32(money);
                plr->GetSession()->send_packet(std::move(data));
                // XXX (spill()?)
                inventory::transaction trans;
                trans.add(money);
                plr->storage().finalize(trans);
            }
        }

        WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4);
        data << uint32(money_split + money_rest);
        looter->GetSession()->send_packet(std::move(data));
        // XXX (spill()?)
        inventory::transaction trans;
        trans.add(money_split + money_rest);
        looter->storage().finalize(trans);
    }
    else
    {
        // XXX (spill()?)
        inventory::transaction trans;
        trans.add(loot_->gold_);
        looter->storage().finalize(trans);
    }

    // Remove the money and notify anyone who has the loot window open
    loot_->notify_money_removed();
    loot_->gold_ = 0;

    if (lootee_guid_.IsItem())
        static_cast<Item*>(lootee_)->SetLootState(ITEM_LOOT_CHANGED);
}

void loot_distributor::attempt_loot_item(Player* looter, uint8 loot_slot)
{
    LOG_DEBUG(logger,
        "loot_distributor::AttemptLootItem. Lootee: %s Looter: %s LootSlot: %i",
        lootee_->GetGuidStr().c_str(), looter->GetGuidStr().c_str(), loot_slot);

    if (!loot_)
        return;
    if (looter->GetLootGuid() != lootee_guid_)
        return;
    if (!within_loot_dist(looter))
        return;
    if (!can_view_loot(looter))
        return;

    // Make sure looter can see the item
    if (!loot_->can_see_item_slot(looter->GetObjectGuid(), loot_slot))
        return;
    LootItem* item = loot_->get_slot_item(loot_slot);
    if (!item)
        return;

    if (!uniqueness_check(looter, item))
    {
        looter->SendEquipError(EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, nullptr);
        return;
    }

    if (item->is_looted)
    {
        looter->SendEquipError(EQUIP_ERR_ALREADY_LOOTED, nullptr, nullptr);
        return;
    }

    if (item->is_blocked)
    {
        looter->SendEquipError(EQUIP_ERR_OBJECT_IS_BUSY, nullptr, nullptr);
        return;
    }

    // Check conditions and quest status
    if (!loot_->can_loot_item(
            item, looter->GetObjectGuid(), anyone_can_view_loot()))
        return;

    // Make sure we have permission to loot this item (aka slot is normal or
    // owner)
    LootSlotType slotType = players_loot_slot(looter, item, item->needs_quest);

    if (recipient_mgr_.party_loot() && !lootee_guid_.IsItem())
    {
        if (slotType != LOOT_SLOT_NORMAL)
            return;
    }
    else
    {
        if (slotType != LOOT_SLOT_OWNER && slotType != LOOT_SLOT_NORMAL)
            return;
    }

    inventory::transaction trans(true, inventory::transaction::send_party);
    trans.add(item->itemid, item->count, item->randomPropertyId);
    if (!looter->storage().verify(trans))
    {
        looter->SendEquipError(static_cast<InventoryResult>(trans.error()),
            nullptr, nullptr, item->itemid);
    }
    else
    {
        looter->storage().finalize(trans);

        // One per player means everyone can loot their own copy
        if (item->one_per_player)
        {
            looter->SendNotifyLootItemRemoved(loot_slot);
        }
        else if (item->needs_quest)
        {
            loot_->notify_quest_item_removed(loot_slot);
            item->is_looted = true;
        }
        else
        {
            loot_->notify_item_removed(loot_slot);
            item->is_looted = true;
        }
        --loot_->unlooted_count_;
        loot_->on_loot_item(item, looter->GetObjectGuid());
    }

    if (lootee_guid_.IsItem())
        static_cast<Item*>(lootee_)->SetLootState(ITEM_LOOT_CHANGED);
}

void loot_distributor::auto_store_all_loot(Player* player)
{
    if (!loot_)
        return;

    if (!can_view_loot(player))
        return;

    for (size_t slot = 0; slot < loot_->size(); ++slot)
    {
        LootItem* item = loot_->get_slot_item(slot);
        if (!item)
            continue;

        if (!uniqueness_check(player, item))
            continue;

        if (item->is_looted || item->is_blocked)
            continue;

        if (!loot_->can_loot_item(item, player->GetObjectGuid()))
            continue;

        LootSlotType slotType =
            players_loot_slot(player, item, item->needs_quest);
        if (slotType != LOOT_SLOT_NORMAL && slotType != LOOT_SLOT_OWNER)
            continue;

        inventory::transaction trans;
        trans.add(item->itemid, item->count, item->randomPropertyId);
        if (!player->storage().verify(trans))
        {
            player->SendEquipError(static_cast<InventoryResult>(trans.error()),
                nullptr, nullptr, item->itemid);
            continue;
        }

        player->storage().finalize(trans);
        // XXX notify players... (winner->SendNewItem(stored_item, item->count,
        // false, false, true);)
        loot_->on_loot_item(item, player->GetObjectGuid());
        item->is_looted = true;
        --loot_->unlooted_count_;
    }
}

void loot_distributor::attempt_place_roll(
    Player* roller, uint8 loot_slot, uint8 roll_type)
{
    if (!loot_)
        return;

    LootItem* item = loot_->get_slot_item(loot_slot);
    if (!item)
        return;

    Roll* roll = get_roll_for_item(item);
    if (!roll)
        return;

    if (!roll->finished() && roll->has_roller(roller->GetObjectGuid()))
    {
        // You can only place it once (the roll class makes sure of this)
        if (roll->place_players_roll(
                roller->GetObjectGuid(),
                static_cast<Roll::roll_type>(
                    roll_type))) // Updates pending roll count
            send_player_roll(*roll, roller->GetObjectGuid());

        // Finish the rolls if this was the last pending one
        if (roll->all_rolls_placed())
            complete_roll(roll);
    }
}

void loot_distributor::force_gold(uint32 gold)
{
    if (!loot_)
    {
        loot_ = new (std::nothrow) Loot;
        if (!loot_)
            return;
    }

    loot_->gold_ = gold;
}

void loot_distributor::force_add_item(const LootItem& item)
{
    if (!loot_)
    {
        loot_ = new (std::nothrow) Loot;
        if (!loot_)
            return;
    }

    // We need to invalidate all current looters in order to insert an item
    for (const auto& elem : loot_->looting_players_)
    {
        if (Player* plr = sObjectAccessor::Instance()->FindPlayer(elem))
        {
            // Force the releasing of loot
            plr->SendLootRelease(lootee_guid_);
            plr->SetLootGuid(ObjectGuid());
            if (lootee_guid_.IsGameObject())
                static_cast<GameObject*>(lootee_)->SetLootState(GO_READY);
            // Remove looter
            loot_->remove_looter(plr->GetObjectGuid());
        }
    }

    loot_->unlooted_count_ += 1;
    loot_->items_.push_back(item);
}

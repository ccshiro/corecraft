#include "inventory/slot.h"
#include "ItemPrototype.h"

bool inventory::slot::can_hold(const ItemPrototype* prototype) const
{
    // TODO: Incorporate all these, and also verify them because russians
    //       made these decisions.
    //
    // Player::CanStoreItem
    // Player::CanEquipItem
    // Player::CanBankItem

    // those functions are full of dicks and they deal with too many things
    // for example, a slot can never know if the unique item can be added here
    // only the storage has that data

    // things a slot CAN check:
    // - Correct item type for this slot
    // - If the player can equip that item
    //   however... should the slot care?
    //   isn't it the responsability of a player to say
    //   that he can use an item?

    // CONCLUSION:
    // - The slot should say if the item can be held in this slot (e.g. bag in
    // bagslots, ring in ringslots, ...)
    // - the player should say if he can equip an item with certain properties
    // (armor class, level requirement, profession requirement, ...)
    // - the storage should say if an item passes the unique or unique-equipped
    // constraints

    // Bagslots can hold only hold bags
    if (bagslot() || bank_bagslot())
        return prototype->InventoryType == INVTYPE_BAG;

    // Equipment slots can only hold a few types of items
    if (equipment())
    {
        switch (index_)
        {
        case head:
            return prototype->InventoryType == INVTYPE_HEAD;
        case neck:
            return prototype->InventoryType == INVTYPE_NECK;
        case shoulders:
            return prototype->InventoryType == INVTYPE_SHOULDERS;
        case shirt:
            return prototype->InventoryType == INVTYPE_BODY;
        case chest:
            return prototype->InventoryType == INVTYPE_CHEST ||
                   prototype->InventoryType == INVTYPE_ROBE;
        case waist:
            return prototype->InventoryType == INVTYPE_WAIST;
        case legs:
            return prototype->InventoryType == INVTYPE_LEGS;
        case feet:
            return prototype->InventoryType == INVTYPE_FEET;
        case wrists:
            return prototype->InventoryType == INVTYPE_WRISTS;
        case hands:
            return prototype->InventoryType == INVTYPE_HANDS;
        case finger1: // nobreak;
        case finger2:
            return prototype->InventoryType == INVTYPE_FINGER;
        case trinket1: // nobreak;
        case trinket2:
            return prototype->InventoryType == INVTYPE_TRINKET;
        case back:
            return prototype->InventoryType == INVTYPE_CLOAK;
        case main_hand_e:
            return prototype->InventoryType == INVTYPE_WEAPON ||
                   prototype->InventoryType == INVTYPE_2HWEAPON ||
                   prototype->InventoryType == INVTYPE_WEAPONMAINHAND;
        case off_hand_e:
            return prototype->InventoryType == INVTYPE_WEAPON ||
                   prototype->InventoryType == INVTYPE_SHIELD ||
                   prototype->InventoryType == INVTYPE_WEAPONOFFHAND ||
                   prototype->InventoryType == INVTYPE_HOLDABLE;
        case ranged_e:
            return prototype->InventoryType == INVTYPE_RANGED ||
                   prototype->InventoryType == INVTYPE_THROWN ||
                   prototype->InventoryType == INVTYPE_RANGEDRIGHT ||
                   prototype->InventoryType == INVTYPE_RELIC;
        case tabard:
            return prototype->InventoryType == INVTYPE_TABARD;
        }
    }

    // Keyring can only hold keys
    if (keyring())
        return prototype->Class == ITEM_CLASS_KEY;

    // Keys can only be in the keyring
    if (prototype->Class == ITEM_CLASS_KEY)
        return keyring();

    // Any other slot can hold any type of item
    return true;
}

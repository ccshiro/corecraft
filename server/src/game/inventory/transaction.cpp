#include "inventory/transaction.h"
#include "Item.h"
#include "inventory/storage.h"

inventory::transaction& inventory::transaction::add(Item* item)
{
    operand op;
    op.type = operand::by_item;
    op.item = item;
    actions_.push_back(action(op_add, op));
    return *this;
}

inventory::transaction& inventory::transaction::add(
    uint32 id, uint32 count, uint32 rand_prop)
{
    operand op;
    op.type = operand::by_id;
    op.id = id;
    op.rand_prop = rand_prop;
    op.count = count;
    actions_.push_back(action(op_add, op));
    return *this;
}

inventory::transaction& inventory::transaction::add(copper c)
{
    operand op;
    op.type = operand::by_gold;
    op.gold = c.get();
    actions_.push_back(action(op_add, op));
    return *this;
}

inventory::transaction& inventory::transaction::remove(Item* item)
{
    operand op;
    op.type = operand::by_item;
    op.item = item;
    actions_.push_back(action(op_remove, op));
    return *this;
}

inventory::transaction& inventory::transaction::remove(uint32 id, uint32 count)
{
    operand op;
    op.type = operand::by_id;
    op.id = id;
    op.count = count;
    actions_.push_back(action(op_remove, op));
    return *this;
}

inventory::transaction& inventory::transaction::remove(copper c)
{
    operand op;
    op.type = operand::by_gold;
    op.gold = c.get();
    actions_.push_back(action(op_remove, op));
    return *this;
}

inventory::transaction& inventory::transaction::destroy(Item* item)
{
    operand op;
    op.type = operand::by_item;
    op.item = item;
    actions_.push_back(action(op_destroy, op));
    return *this;
}

inventory::transaction& inventory::transaction::destroy(uint32 id, uint32 count)
{
    operand op;
    op.type = operand::by_id;
    op.id = id;
    op.count = count;
    actions_.push_back(action(op_destroy, op));
    return *this;
}

bool inventory::transaction::has(Item* item, int* op) const
{
    for (const auto& elem : actions_)
    {
        if (elem.second.type != operand::by_item)
            continue;
        if (elem.second.item == item)
        {
            if (op)
                *op = elem.first;
            return true;
        }
    }
    return false;
}

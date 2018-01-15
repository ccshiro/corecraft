#ifndef GAME__BATTLEFIELD_CONTAINER_H
#define GAME__BATTLEFIELD_CONTAINER_H

#include "BattleGround.h"
#include "battlefield_specification.h"
#include <algorithm>
#include <map>
#include <vector>

namespace battlefield
{
inline bool battlefield_sort(BattleGround* a, BattleGround* b)
{
    return a->GetClientInstanceID() < b->GetClientInstanceID();
}

class container
{
public:
    typedef std::vector<BattleGround*>
        battlegrounds; // Sorted by client instance id

    void insert(BattleGround* bg)
    {
        battlefield::bracket bracket = bg->get_bracket();
        int i = index_for_field_type(bg->get_specification().get_type());
        if (i < 0)
            return;
        battlefields_[i][bracket.min()].push_back(bg);
        std::sort(battlefields_[i][bracket.min()].begin(),
            battlefields_[i][bracket.min()].end(), battlefield_sort);

        battlefields_vector_.push_back(bg);
    }

    void remove(BattleGround* bg)
    {
        battlefield::bracket bracket = bg->get_bracket();
        int i = index_for_field_type(bg->get_specification().get_type());
        if (i < 0)
            return;
        auto itr = std::find(battlefields_[i][bracket.min()].begin(),
            battlefields_[i][bracket.min()].end(), bg);
        if (itr != battlefields_[i][bracket.min()].end())
            battlefields_[i][bracket.min()].erase(itr);
        // No need to resort as the order of those that remain stays the same

        itr = std::find(
            battlefields_vector_.begin(), battlefields_vector_.end(), bg);
        if (itr != battlefields_vector_.end())
            battlefields_vector_.erase(itr);
    }

    // Returned vector is sorted in accordance to client instance ID
    const battlegrounds* find(const specification& spec) const
    {
        int i = index_for_field_type(spec.get_type());
        if (i < 0)
            return nullptr;

        auto itr = battlefields_[i].find(spec.get_bracket().min());
        if (itr == battlefields_[i].end())
            return nullptr;

        return &itr->second;
    }

    BattleGround* get_battleground(uint32 instance_id)
    {
        for (auto& elem : battlefields_vector_)
            if ((elem)->GetInstanceID() == instance_id)
                return elem;
        return nullptr;
    }

    battlegrounds::iterator begin() { return battlefields_vector_.begin(); }
    battlegrounds::iterator end() { return battlefields_vector_.end(); }

    void clear()
    {
        for (auto& elem : battlefields_)
            elem.clear();
        battlefields_vector_.clear();
    }
    battlegrounds::iterator erase(battlegrounds::iterator itr)
    {
        BattleGround* bg = *itr;
        battlefield::bracket bracket = bg->get_bracket();
        int i = index_for_field_type(bg->get_specification().get_type());
        if (i >= 0)
        {
            auto itr = std::find(battlefields_[i][bracket.min()].begin(),
                battlefields_[i][bracket.min()].end(), bg);
            if (itr != battlefields_[i][bracket.min()].end())
                battlefields_[i][bracket.min()].erase(itr);
            // No need to resort as the order of those that remain stays the
            // same
        }
        return battlefields_vector_.erase(itr);
    }

private:
    typedef std::map<int, battlegrounds> bracket_map;
    static const int battlefield_count = 5; // Arena as one

    // Duplicated data: Both sorted and non-sorted
    battlegrounds battlefields_vector_;
    bracket_map battlefields_[battlefield_count];

    int index_for_field_type(type t) const
    {
        switch (t)
        {
        case alterac_valley:
            return 0;
        case warsong_gulch:
            return 1;
        case arathi_basin:
            return 2;
        case eye_of_the_storm:
            return 3;
        case rated_2v2:
        case rated_3v3:
        case rated_5v5:
        case skirmish_2v2:
        case skirmish_3v3:
        case skirmish_5v5:
            return 4;
        default:
            break;
        }
        return -1;
    }
};

} // namespace battlefield

#endif

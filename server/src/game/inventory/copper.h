#ifndef MANGOS__GAME__INVENTORY__COPPER_H
#define MANGOS__GAME__INVENTORY__COPPER_H

#include "Common.h"

namespace inventory
{
// We use a hard limit of 200 000 gold, this seems reasonable for TBC even if
// it's
// not really the same limit blizzard had. No one should reach that limit.
const uint32 copper_limit = 2000000000;
const uint32 silver_limit = copper_limit / 100;
const uint32 gold_limit = silver_limit / 100;

class copper
{
public:
    copper(uint32 c) : value_(c), spill_(0)
    {
        if (value_ > copper_limit)
            value_ = copper_limit;
    }

    copper& operator+=(const copper& rhs)
    {
        spill_ = 0;
        // Even if the operands are at the copper limit, they do not
        // add up to something that can overflow the uint32.
        value_ += rhs.value_;
        if (value_ > copper_limit)
        {
            spill_ = value_ - copper_limit;
            value_ = copper_limit;
        }
        return *this;
    }

    copper& operator-=(const copper& rhs)
    {
        spill_ = 0;
        if (rhs.value_ > value_)
        {
            spill_ = rhs.value_ - value_;
            value_ = 0;
        }
        else
        {
            value_ -= rhs.value_;
        }
        return *this;
    }

    uint32 spill() const { return spill_; }
    uint32 get() const { return value_; }
    std::string str()
    {
        // Examples of returned strings:
        // "12g 43s 12c (spill: 4g 12s)"
        // "12s"

        std::ostringstream ss;

        uint32 g = value_ / 10000;
        uint32 s = (value_ - 10000 * g) / 100;
        uint32 c = (value_ - 10000 * g - 100 * s);

        if (g)
            ss << g << (s || c || spill_ ? "g " : "g");
        if (s)
            ss << s << (c || spill_ ? "s " : "s");
        if (c)
            ss << c << "c";
        if (spill_)
            ss << " (spill: " << copper(spill_).str() << ")";

        return ss.str();
    }

private:
    uint32 value_;
    uint32 spill_;
};

inline copper operator+(copper lhs, const copper& rhs)
{
    lhs += rhs;
    return lhs;
}

inline copper operator-(copper lhs, const copper& rhs)
{
    lhs -= rhs;
    return lhs;
}

// Convenience functions for conversions to copper
inline copper gold(uint32 gold)
{
    if (gold > gold_limit)
        gold = gold_limit;
    return copper(gold * 10000);
}
inline copper silver(uint32 silver)
{
    if (silver > silver_limit)
        silver = silver_limit;
    return copper(silver * 100);
}

} // namespace inventory

#endif // MANGOS__GAME__INVENTORY__COPPER_H

/*
 * Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com/>
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

#ifndef MANGOS__REF_COUNTER_H
#define MANGOS__REF_COUNTER_H

#include <atomic>
#include <utility>

namespace MaNGOS
{
// purpose: thread-safe reference counter
class ref_counter
{
public:
    ref_counter() : ref_{new std::atomic<int>{1}} {}

    ~ref_counter()
    {
        if (--*ref_ == 0)
            delete ref_;
    }

    ref_counter(const ref_counter& rc)
    {
        ref_ = rc.ref_;
        *ref_ += 1;
    }

    ref_counter(ref_counter&& rc) : ref_{new std::atomic<int>{1}}
    {
        std::swap(ref_, rc.ref_);
    }

    ref_counter& operator=(const ref_counter& rc)
    {
        ref_ = rc.ref_;
        *ref_ += 1;
        return *this;
    }

    ref_counter& operator=(ref_counter&& rc)
    {
        std::swap(ref_, rc.ref_);
        return *this;
    }

    // returns: true if this is the only object still referencing this,
    // i.e. no copies still in existance
    bool empty() const { return *ref_ == 1; }

private:
    std::atomic<int>* ref_;
};

} // namespace MaNGOS

#endif // MANGOS__REF_COUNTER_H

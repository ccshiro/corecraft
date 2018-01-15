/*
 * Copyright (C) 2009-2012 MaNGOS <http://getmangos.com/>
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

#ifndef LOCKEDQUEUE_H
#define LOCKEDQUEUE_H

#include "Common.h"
#include <boost/thread/locks.hpp>
#include <mutex>
#include <queue>

namespace MaNGOS
{
template <class T>
class locked_queue
{
public:
    void push(const T& item)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        queue_.push(item);
    }

    bool pop(T& result)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        if (queue_.empty())
            return false;

        result = queue_.front();
        queue_.pop();

        return true;
    }

    template <class Checker>
    bool pop(T& result, Checker& check)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        if (queue_.empty())
            return false;

        result = queue_.front();
        if (!check.Process(result))
            return false;

        queue_.pop();
        return true;
    }

    bool empty()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::recursive_mutex mutex_;
    std::queue<T> queue_;
};
}
#endif

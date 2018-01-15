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

#ifndef MANGOS_SINGLETON_H
#define MANGOS_SINGLETON_H

#include "Platform/Define.h"
#include <boost/noncopyable.hpp>
#include <memory>
#include <mutex>

namespace MaNGOS
{
template <class T>
class SingletonGuard
{
    std::shared_ptr<std::lock_guard<std::recursive_mutex>> guard_;
    T* inst_;

public:
    SingletonGuard(T* inst, std::recursive_mutex& m)
      : guard_(new std::lock_guard<std::recursive_mutex>(m)), inst_(inst)
    {
    }
    // guard_ deleted when SingletonGuard goes out of scope

    T& operator*() const { return *inst_; }
    T* operator->() const { return inst_; }
};

template <class T>
class MANGOS_DLL_DECL Singleton : private boost::noncopyable
{
public:
    typedef SingletonGuard<T> instance;

    static SingletonGuard<T> Instance()
    {
        if (!inst_)
            CreateInst();

        return SingletonGuard<T>(inst_, mutex_);
    }

private:
    static T* inst_;
    static std::recursive_mutex mutex_;

    static void CreateInst()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!inst_)
            inst_ = new T;
    }
};

template <class T>
T* Singleton<T>::inst_ = nullptr;
template <class T>
std::recursive_mutex Singleton<T>::mutex_;

template <class T>
class MANGOS_DLL_DECL UnlockedSingleton : private boost::noncopyable
{
public:
    typedef T* instance;

    static instance Instance()
    {
        if (!inst_)
            CreateInst();

        return inst_;
    }

private:
    static instance inst_;
    static std::recursive_mutex mutex_;

    static void CreateInst()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!inst_)
            inst_ = new T;
    }
};

template <class T>
T* UnlockedSingleton<T>::inst_ = nullptr;
template <class T>
std::recursive_mutex UnlockedSingleton<T>::mutex_;
}

#endif

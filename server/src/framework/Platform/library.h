/*
 * Copyright (C) 2013 CoreCraft <https://www.worldofcorecraft.com/>
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

#ifndef MANGOS__FRAMEWORK__LIBRARY_H
#define MANGOS__FRAMEWORK__LIBRARY_H

#include <stdexcept>
#include <string>

#ifdef WIN32
#include <windows.h> // sort off
#else
#include <dlfcn.h> // sort off
#endif

namespace MaNGOS
{
class library
{
public:
    library(const std::string& path);
    ~library();

    template <typename T>
    T symbol(const std::string& name) const;

private:
#ifdef WIN32
    typedef HMODULE handle_type;
    typedef FARPROC symbol_type;
#else
    typedef void* handle_type;
    typedef void* symbol_type;
#endif

    handle_type handle_;
};

template <typename T>
T library::symbol(const std::string& name) const
{
#ifdef WIN32
    symbol_type sym = GetProcAddress(handle_, name.c_str());
#else
    symbol_type sym = dlsym(handle_, name.c_str());
#endif
    if (!sym)
        throw std::runtime_error("invalid symbol");
    return reinterpret_cast<T>(sym);
}
}

#endif // MANGOS_LIBRARY_H_

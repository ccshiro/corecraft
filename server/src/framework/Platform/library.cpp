/*
 * Copyright (C) 2014 CoreCraft <https://www.worldofcorecraft.com/>
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

#include "library.h"

MaNGOS::library::library(const std::string& path)
{
#ifdef WIN32
    handle_ = LoadLibraryA(path.c_str());
    if (!handle_)
    {
        LPSTR msg_err = nullptr;
        auto err = GetLastError();
        auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                       FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&msg_err, 0, nullptr);
        auto str = std::string(msg_err, size);
        LocalFree(msg_err);
        throw std::runtime_error(
            std::string("library not loaded. err: ") + str);
    }
#else
    handle_ = dlopen(path.c_str(), RTLD_NOW);
    if (!handle_)
        throw std::runtime_error(
            std::string("library not loaded. err: ") + dlerror());
#endif
}

MaNGOS::library::~library()
{
#ifdef WIN32
    FreeLibrary(handle_);
#else
    dlclose(handle_);
#endif
}

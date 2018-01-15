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

#ifndef _MMAP_COMMON_H
#define _MMAP_COMMON_H

#include "Platform/Define.h"
#include <errno.h>
#include <string>
#include <vector>

#ifndef WIN32
#include <stddef.h> // sort off
#include <dirent.h> // sort off
#endif

#define GRID_SIZE 533.33333f

// these are WORLD UNIT based metrics
// this are basic unit dimentions
// value have to divide GRID_SIZE(533.3333f) ( aka: 0.5333, 0.2666, 0.3333,
// 0.1333, etc )
#define BASE_UNIT_DIM 0.2666666f
// All are in UNIT metrics!
#define VERTEX_PER_MAP int(GRID_SIZE / BASE_UNIT_DIM + 0.5f)
#define VERTEX_PER_TILE 80 // must divide VERTEX_PER_MAP
#define TILES_PER_MAP (VERTEX_PER_MAP / VERTEX_PER_TILE)

using namespace std;

namespace MMAP
{
inline bool matchWildcardFilter(const char* filter, const char* str)
{
    if (!filter || !str)
        return false;

    // end on null character
    while (*filter && *str)
    {
        if (*filter == '*')
        {
            if (*++filter ==
                '\0') // wildcard at end of filter means all remaing chars match
                return true;

            while (true)
            {
                if (*filter == *str)
                    break;
                if (*str == '\0')
                    return false; // reached end of string without matching next
                                  // filter character
                str++;
            }
        }
        else if (*filter != *str)
            return false; // mismatch

        filter++;
        str++;
    }

    return ((*filter == '\0' || (*filter == '*' && *++filter == '\0')) &&
            *str == '\0');
}

enum ListFilesResult
{
    LISTFILE_DIRECTORY_NOT_FOUND = 0,
    LISTFILE_OK = 1
};

inline ListFilesResult getDirContents(
    vector<string>& fileList, string dirpath = ".", string filter = "*")
{
#ifdef WIN32
    HANDLE hFind;
    WIN32_FIND_DATA findFileInfo;
    string directory;

    directory = dirpath + "/" + filter;

    hFind = FindFirstFile(directory.c_str(), &findFileInfo);

    if (hFind == INVALID_HANDLE_VALUE)
        return LISTFILE_DIRECTORY_NOT_FOUND;

    do
    {
        if ((findFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            fileList.push_back(string(findFileInfo.cFileName));
    } while (FindNextFile(hFind, &findFileInfo));

    FindClose(hFind);

#else
    const char* p = dirpath.c_str();
    DIR* dirp = opendir(p);
    struct dirent* dp;

    while (dirp)
    {
        errno = 0;
        if ((dp = readdir(dirp)) != nullptr)
        {
            if (matchWildcardFilter(filter.c_str(), dp->d_name))
                fileList.push_back(string(dp->d_name));
        }
        else
            break;
    }

    if (dirp)
        closedir(dirp);
    else
        return LISTFILE_DIRECTORY_NOT_FOUND;
#endif

    return LISTFILE_OK;
}
}

#endif

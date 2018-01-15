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

#ifndef MANGOSSERVER_COMMON_H
#define MANGOSSERVER_COMMON_H

#include "LockedQueue.h"
#include "Platform/Define.h"
#include "estd/algorithm.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <initializer_list>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__sun__)
#include <ieeefp.h> // sort off
#endif

#if PLATFORM == PLATFORM_WINDOWS
#if !defined(FD_SETSIZE)
#define FD_SETSIZE 4096
#endif
#include <ws2tcpip.h> // sort off
#else
#include <sys/types.h>  // sort off
#include <sys/ioctl.h>  // sort off
#include <sys/socket.h> // sort off
#include <netinet/in.h> // sort off
#include <unistd.h>     // sort off
#include <netdb.h>      // sort off
#endif

#if COMPILER == COMPILER_MICROSOFT

#include <float.h> // sort off
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define finite(X) _finite(X)

#else

#define stricmp strcasecmp
#define strnicmp strncasecmp

#endif

// These next two macros are extremely retarded, but for some reason ACE had
// them and mangos used them
#define UI64LIT(n) (n)
#define SI64LIT(n) (n)

inline float finiteAlways(float f)
{
    return finite(f) ? f : 0.0f;
}

#define atol(a) strtoul(a, NULL, 10)

#define STRINGIZE(a) #a

// used for creating values for respawn for example
#define MAKE_PAIR64(l, h) uint64(uint32(l) | (uint64(h) << 32))
#define PAIR64_HIPART(x) \
    (uint32)((uint64(x) >> 32) & UI64LIT(0x00000000FFFFFFFF))
#define PAIR64_LOPART(x) (uint32)(uint64(x) & UI64LIT(0x00000000FFFFFFFF))

#define MAKE_PAIR32(l, h) uint32(uint16(l) | (uint32(h) << 16))
#define PAIR32_HIPART(x) (uint16)((uint32(x) >> 16) & 0x0000FFFF)
#define PAIR32_LOPART(x) (uint16)(uint32(x) & 0x0000FFFF)

enum TimeConstants
{
    MINUTE = 60,
    HOUR = MINUTE * 60,
    DAY = HOUR * 24,
    WEEK = DAY * 7,
    MONTH = DAY * 30,
    YEAR = MONTH * 12,
    IN_MILLISECONDS = 1000
};

enum AccountTypes
{
    SEC_PLAYER = 0,    // a player
    SEC_TICKET_GM = 1, // a GM with no powers except answering tickets
    SEC_POWER_GM =
        2, // a GM with limited powers for assisting the server and its players
    SEC_FULL_GM = 3, // a GM with all available powers (note: powers can still
                     // be made out of reach by making them SEC_CONSOLE)
    SEC_CONSOLE = 4  // an account cannot have this security level
};

// Used in mangosd/realmd
enum RealmFlags
{
    REALM_FLAG_NONE = 0x00,
    REALM_FLAG_INVALID = 0x01,
    REALM_FLAG_OFFLINE = 0x02,
    REALM_FLAG_SPECIFYBUILD = 0x04, // client will show realm version in
                                    // RealmList screen in form "RealmName
                                    // (major.minor.revision.build)"
    REALM_FLAG_UNK1 = 0x08,
    REALM_FLAG_UNK2 = 0x10,
    REALM_FLAG_NEW_PLAYERS = 0x20,
    REALM_FLAG_RECOMMENDED = 0x40,
    REALM_FLAG_FULL = 0x80
};

enum LocaleConstant
{
    LOCALE_enUS = 0, // also enGB
    LOCALE_koKR = 1,
    LOCALE_frFR = 2,
    LOCALE_deDE = 3,
    LOCALE_zhCN = 4,
    LOCALE_zhTW = 5,
    LOCALE_esES = 6,
    LOCALE_esMX = 7,
    LOCALE_ruRU = 8
};

#define MAX_LOCALE 9
#define DEFAULT_LOCALE LOCALE_enUS

LocaleConstant GetLocaleByName(const std::string& name);

extern char const* localeNames[MAX_LOCALE];

struct LocaleNameStr
{
    char const* name;
    LocaleConstant locale;
};

// used for iterate all names including alternative
extern LocaleNameStr const fullLocaleNameList[];

// operator new[] based version of strdup() function! Release memory by using
// operator delete[] !
inline char* mangos_strdup(const char* source)
{
    auto dest = new char[strlen(source) + 1];
    strcpy(dest, source);
    return dest;
}

// we always use stdlibc++ std::max/std::min, undefine some not C++ standard
// defines (Win API and some pother platforms)
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_F
#define M_PI_F float(M_PI)
#endif

#ifndef countof
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

#endif

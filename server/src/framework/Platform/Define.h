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

#ifndef MANGOS_DEFINE_H
#define MANGOS_DEFINE_H

#include "Platform/CompilerDefs.h"
#include <boost/detail/endian.hpp>
#include <sys/types.h>
#include <cinttypes>
#include <cstdint>

typedef std::int64_t int64;
typedef std::int32_t int32;
typedef std::int16_t int16;
typedef std::int8_t int8;
typedef std::uint64_t uint64;
typedef std::uint32_t uint32;
typedef std::uint16_t uint16;
typedef std::uint8_t uint8;

#define I32FMT "%08X"
#define SIZEFMTD "%" PRIuPTR
#define I64FMT "%016" PRIX64
#define UI64FMTD "%" PRIu64
#define SI64FMTD "%" PRId64

#define MANGOS_LITTLEENDIAN 0
#define MANGOS_BIGENDIAN 1

#if !defined(MANGOS_ENDIAN)
#if defined(BOOST_BIG_ENDIAN)
#define MANGOS_ENDIAN MANGOS_BIGENDIAN
#else // BOOST_BIG_ENDIAN
#define MANGOS_ENDIAN MANGOS_LITTLEENDIAN
#endif // BOOST_BIG_ENDIAN
#endif // MANGOS_ENDIAN

#ifdef WIN32
#define MANGOS_SCRIPT_NAME "mangosscript.dll"
#else
#define MANGOS_SCRIPT_NAME "libmangosscript.so"
#endif // WIN32

#if PLATFORM == PLATFORM_WINDOWS
#define MANGOS_EXPORT __declspec(dllexport)
#define MANGOS_IMPORT __cdecl
#else // PLATFORM != PLATFORM_WINDOWS
#define MANGOS_EXPORT export
#if defined(__APPLE_CC__) && defined(BIG_ENDIAN)
#define MANGOS_IMPORT __attribute__((longcall))
#elif defined(__x86_64__)
#define MANGOS_IMPORT
#else
#define MANGOS_IMPORT __attribute__((cdecl))
#endif //__APPLE_CC__ && BIG_ENDIAN
#endif // PLATFORM

#if PLATFORM == PLATFORM_WINDOWS
#ifdef MANGOS_WIN32_DLL_IMPORT
#define MANGOS_DLL_DECL __declspec(dllimport)
#else //!MANGOS_WIN32_DLL_IMPORT
#ifdef MANGOS_WIND_DLL_EXPORT
#define MANGOS_DLL_DECL __declspec(dllexport)
#else //!MANGOS_WIND_DLL_EXPORT
#define MANGOS_DLL_DECL
#endif // MANGOS_WIND_DLL_EXPORT
#endif // MANGOS_WIN32_DLL_IMPORT
#else  // PLATFORM != PLATFORM_WINDOWS
#define MANGOS_DLL_DECL
#endif // PLATFORM

#if PLATFORM == PLATFORM_WINDOWS
#define MANGOS_DLL_SPEC __declspec(dllexport)
#ifndef DECLSPEC_NORETURN
#define DECLSPEC_NORETURN __declspec(noreturn)
#endif // DECLSPEC_NORETURN
#else  // PLATFORM != PLATFORM_WINDOWS
#define MANGOS_DLL_SPEC
#define DECLSPEC_NORETURN
#endif // PLATFORM

#if COMPILER == COMPILER_GNU
#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_PRINTF(F, V) __attribute__((format(printf, F, V)))
#else // COMPILER != COMPILER_GNU
#define ATTR_NORETURN
#define ATTR_PRINTF(F, V)
#endif // COMPILER == COMPILER_GNU

#if COMPILER != COMPILER_MICROSOFT
typedef uint16 WORD;
typedef uint32 DWORD;
#endif // COMPILER

#if COMPILER == COMPILER_GNU
#if !defined(__GXX_EXPERIMENTAL_CXX0X__) || (__GNUC__ < 4) || \
    (__GNUC__ == 4) && (__GNUC_MINOR__ < 7)
#define override
#endif
#endif

typedef uint64 OBJECT_HANDLE;

// This is a workaround for va_copy in Visual Studio 2010 and below.
// Idea for solution was obtained at:
// http://stackoverflow.com/questions/558223/va-copy-porting-to-visual-c
#include <cstdarg> // sort off
#if COMPILER == COMPILER_MICROSOFT && _MSC_VER <= 1600
#define va_copy(dest, src) ((dest) = (src))
#endif

#endif // MANGOS_DEFINE_H

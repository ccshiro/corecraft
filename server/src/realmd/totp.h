/*
 * Copyright (C) 2013      Corecraft   <http://www.worldofcorecraft.com>
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TOTP_H
#define TOTP_H

#include "openssl/evp.h"
#include "openssl/hmac.h"
#include <stdint.h>

#define SHA1_DIGEST_SIZE 20

// START: libOATH: base32.h
/* base32.h -- Encode binary data using printable characters.
   Copyright (C) 2004-2006, 2009-2013 Free Software Foundation, Inc.
   Adapted from Simon Josefsson's base64 code by Gijs van Tulder.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

/* This uses that the expression (n+(k-1))/k means the smallest
   integer >= n/k, i.e., the ceiling of n/k.  */
#define BASE32_LENGTH(inlen) ((((inlen) + 4) / 5) * 8)

struct base32_decode_context
{
    unsigned int i;
    char buf[8];
};

bool isbase32(char ch);

bool base32_decode_ctx(struct base32_decode_context* ctx, const char* in,
    size_t inlen, char* out, size_t* outlen);

// END: base32.h code

namespace TOTP
{
unsigned int GenerateToken(const char* b32key, uint64_t timestamp = time(NULL));
}

#endif

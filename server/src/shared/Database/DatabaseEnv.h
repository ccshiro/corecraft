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

#if !defined(DATABASEENV_H)
#define DATABASEENV_H

#include "Common.h"
#include "logging.h"
#include "Database/Field.h"
#include "Database/QueryResult.h"

#ifdef DO_POSTGRESQL
#include "Database/QueryResultPostgre.h" // sort off
#include "Database/Database.h"           // sort off
#include "Database/DatabasePostgre.h"    // sort off
typedef DatabasePostgre DatabaseType;
#define _LIKE_ "ILIKE"
#define _TABLE_SIM_ "\""
#define _CONCAT3_(A, B, C) "( " A " || " B " || " C " )"
#define _OFFSET_ "LIMIT 1 OFFSET %d"
#else
#include "Database/QueryResultMysql.h" // sort off
#include "Database/Database.h"         // sort off
#include "Database/DatabaseMysql.h" // sort off
typedef DatabaseMysql DatabaseType;
#define _LIKE_ "LIKE"
#define _TABLE_SIM_ "`"
#define _CONCAT3_(A, B, C) "CONCAT( " A " , " B " , " C " )"
#define _OFFSET_ "LIMIT %d,1"
#endif

extern DatabaseType WorldDatabase;
extern DatabaseType CharacterDatabase;
extern DatabaseType LoginDatabase;

#endif

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

#include "VMapFactory.h"
#include <sys/types.h>

using namespace G3D;

namespace VMAP
{
VMapManager2* gVMapManager = nullptr;

//===============================================
// just return the instance
VMapManager2* VMapFactory::createOrGetVMapManager()
{
    if (gVMapManager == nullptr)
        gVMapManager = new VMapManager2(); // should be taken from config ...
                                           // Please change if you like :-)
    return gVMapManager;
}

//===============================================
// delete all internal data structures
void VMapFactory::clear()
{
    if (gVMapManager)
    {
        delete gVMapManager;
        gVMapManager = nullptr;
    }
}
}

/* Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com>
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

#ifndef SCRIPTS__OUTLAND_H
#define SCRIPTS__OUTLAND_H

#include "hellfire_dark_portal_event.h"
#include "precompiled.h"

class outland_instance_data : public InstanceData
{
public:
    outland_instance_data(Map* map) : InstanceData(map), dp_event(this) {}

    void Update(uint32 diff) override;

private:
    dark_portal_event dp_event;
};

#endif // SCRIPTS__OUTLAND_H

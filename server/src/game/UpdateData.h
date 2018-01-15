/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Coypright (C) 2014 Corecraft <https://www.worldofcorecraft.com/>
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

#ifndef __UPDATEDATA_H
#define __UPDATEDATA_H

#include "ByteBuffer.h"
#include "ObjectGuid.h"
#include <vector>

class WorldPacket;
class WorldSession;

enum ObjectUpdateType
{
    UPDATETYPE_VALUES = 0,
    UPDATETYPE_MOVEMENT = 1,
    UPDATETYPE_CREATE_OBJECT = 2,
    UPDATETYPE_CREATE_OBJECT2 = 3,
    UPDATETYPE_OUT_OF_RANGE_OBJECTS = 4,
    UPDATETYPE_NEAR_OBJECTS = 5
};

enum ObjectUpdateFlags
{
    UPDATEFLAG_NONE = 0x0000,
    UPDATEFLAG_SELF = 0x0001,
    UPDATEFLAG_TRANSPORT = 0x0002,
    UPDATEFLAG_HAS_ATTACKING_TARGET = 0x0004,
    UPDATEFLAG_LOWGUID = 0x0008,
    UPDATEFLAG_HIGHGUID = 0x0010,
    UPDATEFLAG_LIVING = 0x0020,
    UPDATEFLAG_HAS_POSITION = 0x0040,
};

class UpdateData
{
public:
    UpdateData();

    void AddOutOfRangeGUID(ObjectGuidSet& guids);
    void AddOutOfRangeGUID(ObjectGuid const& guid);
    void AddUpdateBlock(const ByteBuffer& block);

    // If hasTransport is true, then the transport data MUST COME FIRST, in
    // other words
    // call AddUpdateBlock with the transport data as the FIRST call
    void SendPacket(WorldSession* session, bool hasTransport = false);

    // begin to end should be WorldSession pointers, same restrictions as normal
    // SendPacket apply
    template <typename iterator>
    void SendPacket(iterator begin, iterator end, bool hasTransport = false);

    bool HasData() { return !data_blob_empty_ || !m_outOfRangeGUIDs.empty(); }
    void Clear();

    ObjectGuidSet const& GetOutOfRangeGUIDs() const
    {
        return m_outOfRangeGUIDs;
    }

protected:
    struct data_blob
    {
        data_blob() : block_count(0) {}

        ByteBuffer blob;
        uint32 block_count;
    };
    bool data_blob_empty_;
    std::vector<data_blob> data_blobs_; // If the update data grows too big we
                                        // need to send it as multiple packets
    size_t current_blob_; // Current data_blob index we're writing to
    bool has_transport_;
    ObjectGuidSet m_outOfRangeGUIDs;

    void Compress(void* dst, uint32* dst_size, void* src, int src_size);
    bool next_packet(WorldPacket& packet);
};

template <typename iterator>
void UpdateData::SendPacket(iterator begin, iterator end, bool hasTransport)
{
    for (iterator itr = begin; itr != end; ++itr)
        SendPacket(*itr, hasTransport);
}

#endif

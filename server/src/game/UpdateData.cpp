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

#include "UpdateData.h"
#include "ByteBuffer.h"
#include "Common.h"
#include "logging.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "World.h"
#include "WorldPacket.h"
#include <zlib.h>

#define DATA_SIZE_LIMIT \
    60000 // Actual limit is sizeof(int16_t), but compression is going to reduce
          // output size a lot,
// but we cannot know the compression size while building blocks, so we have to
// guess a limit
// While testing 60k uncompressed has not yet netted me more than 20k
// compressed, even with a lot
// of transport & out of range GUID data, if further testing goes against this
// we need to decrease
// this limit, or add some other condition

UpdateData::UpdateData()
  : data_blob_empty_(true), current_blob_(0), has_transport_(false)
{
    // Add an empty, first, data_blob
    data_blobs_.push_back(data_blob());
}

void UpdateData::AddOutOfRangeGUID(ObjectGuidSet& guids)
{
    m_outOfRangeGUIDs.insert(guids.begin(), guids.end());
}

void UpdateData::AddOutOfRangeGUID(ObjectGuid const& guid)
{
    m_outOfRangeGUIDs.insert(guid);
}

void UpdateData::AddUpdateBlock(const ByteBuffer& block)
{
    if (block.size() > DATA_SIZE_LIMIT)
        throw std::runtime_error(
            "UpdateData::AddUpdateBlock: block bigger than DATA_SIZE_LIMIT");

    // Add a new blob if current one has ran out of space
    if (data_blobs_[current_blob_].blob.size() + block.size() > DATA_SIZE_LIMIT)
    {
        ++current_blob_;
        data_blobs_.push_back(data_blob());
    }

    data_blobs_[current_blob_].blob.append(block);
    ++data_blobs_[current_blob_].block_count;

    if (data_blob_empty_)
        data_blob_empty_ = false;
}

void UpdateData::Compress(void* dst, uint32* dst_size, void* src, int src_size)
{
    z_stream c_stream;

    c_stream.zalloc = (alloc_func) nullptr;
    c_stream.zfree = (free_func) nullptr;
    c_stream.opaque = (voidpf) nullptr;

    // default Z_BEST_SPEED (1)
    int z_res = deflateInit(
        &c_stream, sWorld::Instance()->getConfig(CONFIG_UINT32_COMPRESSION));
    if (z_res != Z_OK)
    {
        logging.error(
            "Can't compress update packet (zlib: deflateInit) Error code: %i "
            "(%s)",
            z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    c_stream.next_out = (Bytef*)dst;
    c_stream.avail_out = *dst_size;
    c_stream.next_in = (Bytef*)src;
    c_stream.avail_in = (uInt)src_size;

    z_res = deflate(&c_stream, Z_NO_FLUSH);
    if (z_res != Z_OK)
    {
        logging.error(
            "Can't compress update packet (zlib: deflate) Error code: %i (%s)",
            z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    if (c_stream.avail_in != 0)
    {
        logging.error(
            "Can't compress update packet (zlib: deflate not greedy)");
        *dst_size = 0;
        return;
    }

    z_res = deflate(&c_stream, Z_FINISH);
    if (z_res != Z_STREAM_END)
    {
        logging.error(
            "Can't compress update packet (zlib: deflate should report "
            "Z_STREAM_END instead %i (%s)",
            z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    z_res = deflateEnd(&c_stream);
    if (z_res != Z_OK)
    {
        logging.error(
            "Can't compress update packet (zlib: deflateEnd) Error code: %i "
            "(%s)",
            z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    *dst_size = c_stream.total_out;
}

void UpdateData::SendPacket(WorldSession* session, bool hasTransport)
{
    has_transport_ = hasTransport;

    current_blob_ = 0; // reset blob index to start sending from the beginning

    bool more;
    do
    {
        WorldPacket packet;
        more = next_packet(packet);
        session->send_packet(std::move(packet));
    } while (more);

    current_blob_ =
        data_blobs_.size() -
        1; // restore blob index in case user intends to fill in more data
}

bool UpdateData::next_packet(WorldPacket& packet)
{
    uint32 oor_size = current_blob_ == 0 && !m_outOfRangeGUIDs.empty() ?
                          1 + 4 + 9 * m_outOfRangeGUIDs.size() :
                          0;

    packet.initialize(SMSG_UPDATE_OBJECT,
        4 + 1 + oor_size + data_blobs_[current_blob_].blob.size());
    size_t buf_start_pos = packet.wpos();

    packet << uint32(!m_outOfRangeGUIDs.empty() && current_blob_ == 0 ?
                         data_blobs_[current_blob_].block_count + 1 :
                         data_blobs_[current_blob_].block_count);

    // Only the first packet is permitted to have transport; this is indeed
    // a shitty arbitrary restriction, but one made to keep the refactoring
    // to a bare minimum
    packet << uint8(has_transport_ ? 1 : 0);
    has_transport_ = false;

    // Add out of range GUIDs
    if (current_blob_ == 0 && !m_outOfRangeGUIDs.empty())
    {
        packet << uint8(UPDATETYPE_OUT_OF_RANGE_OBJECTS);
        packet << uint32(m_outOfRangeGUIDs.size());
        for (auto& set : m_outOfRangeGUIDs)
            packet << set.WriteAsPacked();
    }

    packet.append(data_blobs_[current_blob_].blob);

    size_t size = packet.size() - 4; // -4 for header
    if (size > 100)
    {
        uint32 dst_size = compressBound(size);
        WorldPacket compressed(
            SMSG_COMPRESSED_UPDATE_OBJECT, dst_size + 4); // +4 for compr. size
        size_t packet_start = compressed.wpos();

        compressed.resize(
            dst_size + 4 + 4); // + 4 for compr. size + 4 for header

        compressed.put<uint32>(packet_start, size);

        Compress(compressed.contents() + packet_start + 4, &dst_size,
            (void*)(packet.contents() + buf_start_pos), size);
        if (dst_size == 0)
        {
            logging.error("UpdateData::next_packet Compress() returned size 0");
            return false;
        }

        compressed.resize(
            dst_size + 4 + 4); // + 4 for compr. size + 4 for header

        if (dst_size + 4 + 4 >= 32767) // + 4 for compr. size + 4 for header
            logging.error(
                "UpdateData::next_packet wrote too big of a packet, causing "
                "the client to show everything as Unknown. Read comment at "
                "#define DATA_SIZE_LIMIT and adjust accordingly.");

        packet = std::move(compressed);
    }

    ++current_blob_;

    return current_blob_ < data_blobs_.size(); // finished sending it all?
}

void UpdateData::Clear()
{
    data_blobs_.clear();
    m_outOfRangeGUIDs.clear();

    // Add an empty, first, data_blob
    data_blobs_.push_back(data_blob());
}

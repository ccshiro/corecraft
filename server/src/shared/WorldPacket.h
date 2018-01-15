/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2014 corecraft <https://www.worldofcorecraft.com/>
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

#ifndef MANGOSSERVER_WORLDPACKET_H
#define MANGOSSERVER_WORLDPACKET_H

#include "ByteBuffer.h"
#include "Common.h"

// NOTE: m_opcode and size stored in platfom dependent format
//       ignore endianess until send, and converted at receive
// NOTE2: for send packets either the opcode constructor needs
//        to be used, or Initialize() invoked. It's invalid to
//        try to send a packet that has not been initialized.
class WorldPacket : public ByteBuffer
{
public:
    WorldPacket() : ByteBuffer{0}, initialized_{false}, opcode_{0} {}

    explicit WorldPacket(uint16 opcode, size_t reserve = 32 - server_header_sz)
      : ByteBuffer(0), initialized_(true), opcode_(opcode)
    {
        reserve_header_bytes();
        _storage.reserve(reserve + server_header_sz);
    }

    // copy
    WorldPacket(const WorldPacket& packet)
      : ByteBuffer(packet), initialized_(packet.initialized_),
        opcode_(packet.opcode_)
    {
    }

    WorldPacket& operator=(const WorldPacket& packet)
    {
        ByteBuffer::operator=(packet);
        initialized_ = packet.initialized_;
        opcode_ = packet.opcode_;
        return *this;
    }

    // move
    WorldPacket(WorldPacket&& packet) noexcept : ByteBuffer{std::move(packet)}
    {
        std::swap(opcode_, packet.opcode_);
        std::swap(initialized_, packet.initialized_);
    }

    WorldPacket& operator=(WorldPacket&& packet) noexcept
    {
        ByteBuffer::operator=(std::move(packet));
        std::swap(opcode_, packet.opcode_);
        std::swap(initialized_, packet.initialized_);
        return *this;
    }

    bool initialized() const { return initialized_; }

    void clear()
    {
        ByteBuffer::clear();
        opcode_ = 0;
        initialized_ = false;
    }

    void initialize(uint16 opcode, size_t reserve = 32 - server_header_sz)
    {
        ByteBuffer::clear();
        reserve_header_bytes();
        _storage.reserve(reserve + server_header_sz);
        opcode_ = opcode;
        initialized_ = true;
    }

    uint16 opcode() const { return opcode_; }
    void opcode(uint16 opcode) { opcode_ = opcode; }

protected:
    static const size_t server_header_sz = 4;
    bool initialized_;
    uint16 opcode_;

    // To avoid unnecessary copying later on we make place for the header
    // in the buffer. This only applies for outgoing packets.
    void reserve_header_bytes()
    {
        _storage.resize(server_header_sz);
        _rpos = server_header_sz;
        _wpos = server_header_sz;
    }
};

#endif

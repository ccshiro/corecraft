#include "netstat.h"
#include "Opcodes.h"
#include "WorldConnection.h"
#include "WorldPacket.h"
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <fstream>
#include <iomanip>

void netstat::send(const WorldPacket& p)
{
    std::lock_guard<std::mutex> guard(mutex_);

    // NOTE: output header included in p.size()

    send_[p.opcode()].opcode = p.opcode();
    ++send_[p.opcode()].count;
    send_[p.opcode()].bytes += p.size();

    ++send_summary_.packets;
    send_summary_.bytes += p.size();
}

void netstat::recv(const WorldPacket& p)
{
    std::lock_guard<std::mutex> guard(mutex_);

    recv_[p.opcode()].opcode = p.opcode();
    ++recv_[p.opcode()].count;
    recv_[p.opcode()].bytes += p.size() + sizeof(input_header);
}

void netstat::on_write()
{
    std::lock_guard<std::mutex> guard(mutex_);

    ++send_summary_.writes;
}

// Only does something if the log interval timer has expired
void netstat::log()
{
    if (timer_.Passed())
    {
        timer_.Reset();

        write_log(send_, "netstat_send.log");
        write_log(recv_, "netstat_recv.log");
    }
}

template <typename T>
static void table_print(std::ofstream& o, T t)
{
    o << std::left << std::setw(50) << std::setfill(' ') << t;
}

void netstat::write_log(
    std::map<uint32 /*opcode*/, stat>& map, const std::string& filename)
{
    std::vector<stat> v;

    {
        std::lock_guard<std::mutex> guard(mutex_);
        boost::copy(map | boost::adaptors::map_values, std::back_inserter(v));
    }

    std::sort(v.begin(), v.end(), [](const stat& a, const stat& b)
        {
            return a.bytes > b.bytes;
        });

    std::ofstream out(filename);
    if (!out.is_open())
        return;

    // extra send logging
    if (filename.compare("netstat_send.log") == 0)
    {
        out << "Send summary:\n";
        out << "Total bytes (ignoring protocol overhead): "
            << send_summary_.bytes;
        out << "\nTotal logical packets: " << send_summary_.packets;
        out << "\nTotal writes: " << send_summary_.writes;
        out << std::endl
            << std::endl;
    }

    table_print(out, "Opcode");
    table_print(out, "Data Amount (bytes)");
    table_print(out, "Packet Count");
    out << std::endl;

    for (auto& s : v)
    {
        table_print(out, LookupOpcodeName(s.opcode));
        table_print(out, s.bytes);
        table_print(out, s.count);
        out << std::endl;
    }
}

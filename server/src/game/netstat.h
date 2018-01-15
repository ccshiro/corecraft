#ifndef GAME__NETSTAT_H
#define GAME__NETSTAT_H

#include "Common.h"
#include "Timer.h"
#include <boost/thread.hpp>

class WorldPacket;

class netstat
{
    netstat() { timer_.SetInterval(60); }

public:
    netstat(const netstat&) = delete;

    static inline netstat& instance();

    void send(const WorldPacket& p);
    void recv(const WorldPacket& p);
    void on_write();

    // Only does something if the log interval timer has expired
    void log();

private:
    struct stat
    {
        uint32 opcode = 0; // opcode
        uint32 count = 0;  // # of packets
        uint64 bytes = 0;  // # of bytes
    };
    struct send_summary
    {
        uint64 bytes =
            0; // # of total bytes sent (not including protocol overhead)
        uint32 packets = 0; // # of total logical packets
        uint32 writes = 0;  // # of asio write calls
    };

    void write_log(
        std::map<uint32 /*opcode*/, stat>& map, const std::string& filename);

    std::mutex mutex_;
    std::map<uint32 /*opcode*/, stat> send_;
    std::map<uint32 /*opcode*/, stat> recv_;
    send_summary send_summary_;

    SystemIntervalTimer timer_;
};

netstat& netstat::instance()
{
    static netstat inst;
    return inst;
}

#ifdef SERVER_LOG_NETSTAT
#define NETSTAT_LOG_SEND(p) netstat::instance().send(p)
#define NETSTAT_ON_WRITE() netstat::instance().on_write()
#define NETSTAT_LOG_RECV(p) netstat::instance().recv(p)
#define NETSTAT_WRITE_LOGS() netstat::instance().log()
#else
#define NETSTAT_LOG_SEND(p) ((void)0)
#define NETSTAT_ON_WRITE() ((void)0)
#define NETSTAT_LOG_RECV(p) ((void)0)
#define NETSTAT_WRITE_LOGS() ((void)0)
#endif

#endif

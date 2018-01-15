#ifndef __WORLDCONNECTION_H
#define __WORLDCONNECTION_H

#include "DBCStores.h"
#include "WorldPacket.h"
#include "Auth/AuthCrypt.h"
#include <boost/asio.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <queue>

class WorldSession;

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

struct output_header
{
    uint16 size;
    uint16 opcode;
};

struct input_header
{
    uint16 size;
    uint32 opcode;
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif
// All members marked with //* can be accessed from multiple threads (i.e. you
// must lock mutex_ before using them)
class world_connection : private boost::noncopyable,
                         public boost::enable_shared_from_this<world_connection>
{
public:
    world_connection(boost::asio::io_service& io);
    ~world_connection();

    boost::asio::ip::tcp::socket& socket() { return socket_; }
    void start();
    void stop(bool hard = false);
    void queue_packet(WorldPacket&& packet);
    bool is_closed();
    std::string remote_address() const;

    // Interface functions copied from the WorldSocket to make WorldSession
    // happy, these should eventually go away
    std::string GetRemoteAddress() { return remote_address(); }
    bool IsClosed() { return is_closed(); }
    void CloseSocket() { stop(); }

    void null_session();

private:
    void handle_read_header(const boost::system::error_code& e);
    void handle_read_data(const boost::system::error_code& e);
    void handle_send_timer(const boost::system::error_code& e);
    void handle_write(const boost::system::error_code& e);
    void handle_ping(WorldPacket& packet);
    void handle_auth_session(WorldPacket& recvPacket);
    void hard_stop();

    // YOU MUST HOLD THE LOCK ON THE QUEUE MUTEX TO CALL THIS
    void flush_queue();

    AuthCrypt auth_crypter_; //*
    input_header header_;
    boost::asio::ip::tcp::socket socket_;
    WorldPacket packet_;
    WorldSession* session_;
    uint32 seed_;
    bool is_closing_; //*
    std::mutex mutex_;

    // We've disabled nagle's algorithm, and in an attempt to reduce TCP and IP
    // overhead
    // we instead start a 10 ms timer when a send is asked for, in the hopes to
    // get more
    // packets during that period. After the 10 ms have passed we flush the
    // queue.
    // The theory behind the 10 ms is as follows: most of your packets are
    // assumed to be
    // originating from your own character or characters in cells around you. It
    // then further
    // assumes that those cells will have been updated in the 10 ms that pass,
    // and therefore
    // most packets the connection will receive have been queued by the timer
    // expiry.
    // The theory currently lacks practical basis, and needs experiments.
    // See the following article for more information about nagle
    // http://boundary.com/blog/2012/05/02/know-a-delay-nagles-algorithm-and-you/
    //
    // How our queue works:
    // 1. A packet is marked for wanting to be sent with queue_packet. If
    // send_in_progress_ is
    //    false, a 10 millisecond timer is started (in an attempt to reduce
    //    protocol overhead).
    // 2. When the timer expires the queue is flushed.
    // 3. After the pending data has been sent, the handle_write function
    // receives a callback call,
    //    and if more queued data exists, that will be flushed, otherwise it
    //    will do nothing.
    // 4. There are 3 points where we can decide to send a packet:
    //    3.1 When queue_packet's 10 ms timer has expired (handle_send_timer)
    //    3.2 When a write handler returns and queue_ is not empty
    //    (handle_write)
    //    3.3 When a ping packet is received, and we're currently not sending,
    //    then queue_packet
    //        will cause an immediate flush that cancels the 10 ms timer
    //        (handle_ping)
    typedef std::vector<std::vector<uint8_t>> queue_type;
    boost::asio::high_resolution_timer timer_;
    queue_type queue_;             //*
    queue_type currently_sending_; //*
    bool send_in_progress_;        //*
    bool timer_started_;           //*

    time_t last_ping_;
    uint32 spam_ping_count_;
};

#endif

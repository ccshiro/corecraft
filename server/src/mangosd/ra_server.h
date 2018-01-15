#ifndef MANGOSD__RA_SERVER_H
#define MANGOSD__RA_SERVER_H

#include "Common.h"
#include "ra_connection.h"
#include "Policies/Singleton.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
#include <vector>

class ra_server
{
public:
    friend class MaNGOS::UnlockedSingleton<ra_server>; // Needs to invoke our
                                                       // constructor

    bool start(const std::string& address, const std::string& port);
    void stop();
    void join();

private:
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;
    typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef boost::shared_ptr<ra_connection> connection_ptr;
    typedef std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;
    typedef boost::shared_ptr<boost::thread> thread_ptr;

    ra_server(); // Only UnlockedSingleton can create us

    void initialize_io_service();
    void initialize_listener(
        const std::string& address, const std::string& port);
    void start_accept();
    void run();
    void handle_accept(const boost::system::error_code& e);

    io_service_ptr io_service_;
    work_ptr work_;
    acceptor_ptr acceptor_;
    connection_ptr connection_;
    thread_ptr thread_;
};

#define sRAServer MaNGOS::UnlockedSingleton<ra_server>

#endif

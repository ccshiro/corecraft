#ifndef __WORLDSERVER_H
#define __WORLDSERVER_H

#include "WorldConnection.h"
#include "Policies/Singleton.h"
#include <boost/asio.hpp>
#include <string>
#include <vector>

class world_server
{
public:
    friend class MaNGOS::UnlockedSingleton<world_server>; // Needs to invoke our
                                                          // constructor

    bool start(const std::string& address, const std::string& port);
    void stop();

private:
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;
    typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef boost::shared_ptr<world_connection> connection_ptr;
    typedef std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;

    world_server(); // Only UnlockedSingleton can create us

    bool initialize_io_services();
    void initialize_listener(
        const std::string& address, const std::string& port);
    void start_accept();
    void run();
    void handle_accept(const boost::system::error_code& e);
    boost::asio::io_service& get_io_service();

    bool no_delay_;
    size_t next_io_service_;
    std::vector<io_service_ptr> io_services_;
    std::vector<work_ptr> work_;
    acceptor_ptr acceptor_;
    connection_ptr connection_;
};

#define sWorldServer MaNGOS::UnlockedSingleton<world_server>

#endif

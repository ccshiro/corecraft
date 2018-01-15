#ifndef MANGOSD__RA_SERVER_H
#define MANGOSD__RA_SERVER_H

#include "Common.h"
#include "connection.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
#include <vector>

class server
{
public:
    server(boost::asio::io_service& io_service) : io_service_(io_service) {}

    bool start(const std::string& address, const std::string& port);

private:
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;
    typedef boost::shared_ptr<connection> connection_ptr;
    typedef std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;
    ;

    void initialize_listener(
        const std::string& address, const std::string& port);
    void start_accept();
    void handle_accept(const boost::system::error_code& e);

    boost::asio::io_service& io_service_;
    work_ptr work_;
    acceptor_ptr acceptor_;
    connection_ptr connection_;
};

#endif

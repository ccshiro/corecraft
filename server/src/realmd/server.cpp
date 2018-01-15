#include "server.h"
#include "logging.h"
#include "Config/Config.h"
#include <exception>

bool server::start(const std::string& address, const std::string& port)
{
    try
    {
        work_.reset(new boost::asio::io_service::work(io_service_));
        initialize_listener(address, port);
    }
    catch (std::exception& e)
    {
        logging.error("%s", e.what());
        return false;
    }

    return true;
}

void server::initialize_listener(
    const std::string& address, const std::string& port)
{
    LOG_DEBUG(logging, "Starting Realm listener");
    boost::asio::ip::tcp::resolver resolver(io_service_);
    boost::asio::ip::tcp::resolver::query query(address, port);
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

    acceptor_.reset(new boost::asio::ip::tcp::acceptor(io_service_));
    acceptor_->open(endpoint.protocol());
    acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen();
    start_accept();
}

void server::start_accept()
{
    LOG_DEBUG(logging, "RealmdServer: Accepting connections");
    connection_.reset(new connection(io_service_));
    acceptor_->async_accept(
        connection_->socket(), boost::bind(&server::handle_accept, this,
                                   boost::asio::placeholders::error));
}

void server::handle_accept(const boost::system::error_code& e)
{
    LOG_DEBUG(logging, "RealmdServer: Started listener");
    if (!e)
    {
        connection_->start();
    }
    start_accept();
}

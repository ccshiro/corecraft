#include "ra_server.h"
#include "logging.h"
#include "Config/Config.h"
#include <exception>

ra_server::ra_server()
{
}

bool ra_server::start(const std::string& address, const std::string& port)
{
    try
    {
        initialize_io_service();
        initialize_listener(address, port);
        run();
    }
    catch (std::exception& e)
    {
        logging.error("%s", e.what());
        return false;
    }

    return true;
}

void ra_server::run()
{
    thread_.reset(new boost::thread(
        boost::bind(&boost::asio::io_service::run, io_service_)));
}

void ra_server::stop()
{
    io_service_->stop();
}

void ra_server::join()
{
    thread_->join();
}

void ra_server::initialize_io_service()
{
    io_service_.reset(new boost::asio::io_service);
    work_.reset(new boost::asio::io_service::work(*io_service_));
}

void ra_server::initialize_listener(
    const std::string& address, const std::string& port)
{
    LOG_DEBUG(logging, "Starting RA listener");
    boost::asio::ip::tcp::resolver resolver(*io_service_);
    boost::asio::ip::tcp::resolver::query query(address, port);
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

    acceptor_.reset(new boost::asio::ip::tcp::acceptor(*io_service_));
    acceptor_->open(endpoint.protocol());
    acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen();
    start_accept();
}

void ra_server::start_accept()
{
    LOG_DEBUG(logging, "RA: Accepting connections");
    connection_.reset(new ra_connection(*io_service_));
    acceptor_->async_accept(
        connection_->socket(), boost::bind(&ra_server::handle_accept, this,
                                   boost::asio::placeholders::error));
}

void ra_server::handle_accept(const boost::system::error_code& e)
{
    LOG_DEBUG(logging, "RA: Started listener");
    if (!e)
    {
        connection_->start();
    }
    start_accept();
}

#include "WorldServer.h"
#include "logging.h"
#include "Config/Config.h"
#include <boost/thread.hpp>
#include <exception>

world_server::world_server() : next_io_service_(0)
{
}

bool world_server::start(const std::string& address, const std::string& port)
{
    try
    {
        if (!initialize_io_services())
            return false;
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

void world_server::run()
{
    LOG_DEBUG(logging, "start network threads");
    std::vector<boost::shared_ptr<boost::thread>> threads;
    for (std::size_t i = 0; i < io_services_.size(); ++i)
    {
        LOG_DEBUG(logging, "network thread %lu", i);
        boost::shared_ptr<boost::thread> thread(new boost::thread(
            boost::bind(&boost::asio::io_service::run, io_services_[i])));
        threads.push_back(thread);
    }

    LOG_DEBUG(logging, "Waiting for network threads to finish");
    for (auto& thread : threads)
        thread->join();
}

void world_server::stop()
{
    for (auto& elem : io_services_)
        elem->stop();
}

bool world_server::initialize_io_services()
{
    size_t num_threads = static_cast<size_t>(
        sConfig::Instance()->GetIntDefault("Concurrency.NetworkThreads", 1));
    if (num_threads <= 0)
    {
        logging.error(
            "Concurrency.NetworkThreads is wrong in your config file");
        return false;
    }

    for (size_t i = 0; i < num_threads; ++i)
    {
        io_service_ptr io(new boost::asio::io_service);
        work_ptr work(new boost::asio::io_service::work(*io));
        io_services_.push_back(io);
        work_.push_back(work);
    }
    return true;
}

void world_server::initialize_listener(
    const std::string& address, const std::string& port)
{
    LOG_DEBUG(logging, "Starting listener");
    no_delay_ = sConfig::Instance()->GetBoolDefault("Network.TcpNodelay", true);
    boost::asio::ip::tcp::resolver resolver(get_io_service());
    boost::asio::ip::tcp::resolver::query query(address, port);
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

    acceptor_.reset(new boost::asio::ip::tcp::acceptor(get_io_service()));
    acceptor_->open(endpoint.protocol());
    acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen();
    start_accept();
}

void world_server::start_accept()
{
    LOG_DEBUG(logging, "Accepting connections");
    connection_.reset(new world_connection(get_io_service()));
    acceptor_->async_accept(
        connection_->socket(), boost::bind(&world_server::handle_accept, this,
                                   boost::asio::placeholders::error));
}

void world_server::handle_accept(const boost::system::error_code& e)
{
    LOG_DEBUG(logging, "Started listener");
    if (!e)
    {
        connection_->socket().set_option(
            boost::asio::ip::tcp::no_delay(no_delay_));
        connection_->start();
    }
    start_accept();
}

boost::asio::io_service& world_server::get_io_service()
{
    boost::asio::io_service& io_service = *io_services_[next_io_service_];
    ++next_io_service_;
    if (next_io_service_ == io_services_.size())
        next_io_service_ = 0;
    return io_service;
}

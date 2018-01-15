#include "ra_connection.h"
#include "AccountMgr.h"
#include "logging.h"
#include "ObjectMgr.h"
#include "Util.h"
#include "World.h"
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <algorithm>
#include <cstring>

static auto& logger = logging.get_logger("remoteadmin");

ra_connection::ra_connection(boost::asio::io_service& io)
  : socket_(io), account_id_(0), stage_(stage::username)
{
    /* empty */
}

void ra_connection::start()
{
    LOG_DEBUG(logging, "RA: Start connection for %s", remote_address().c_str());

    send("Username: ");

    boost::asio::async_read_until(socket_, streambuf_, '\n',
        boost::bind(&ra_connection::handle_read, shared_from_this(),
                                      boost::asio::placeholders::error));
}

void ra_connection::stop()
{
    if (is_closed())
    {
        LOG_DEBUG(logging, "Trying to close an already closed connection");
        return;
    }

    LOG_DEBUG(logging, "Stop connection for %s", remote_address().c_str());
    boost::system::error_code ignored_ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
    socket_.close();
}

std::string ra_connection::remote_address() const
{
    if (socket_.is_open())
    {
        boost::system::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (ec)
        {
            logging.error("ra_connection::remote_address() error: %s",
                ec.message().c_str());
            return "<unable to get address>";
        }
        return endpoint.address().to_string();
    }
    else
    {
        return "<socket not connected>";
    }
}

void ra_connection::send(const std::string& str)
{
    if (is_closed())
        return;

    shared_const_buffer buffer(str);
    boost::asio::async_write(socket_, buffer,
        boost::bind(&ra_connection::handle_write, shared_from_this(),
                                 boost::asio::placeholders::error));
}

void ra_connection::handle_read(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }

    std::istream is(&streambuf_);
    std::string line;
    is >> line;

    switch (stage_)
    {
    case stage::username:
        handle_stage_username(line);
        break;
    case stage::password:
        handle_stage_password(line);
        break;
    case stage::authenticated:
        handle_stage_authenticated(line);
        break;
    }

    if (is_closed())
        return;

    boost::asio::async_read_until(socket_, streambuf_, '\n',
        boost::bind(&ra_connection::handle_read, shared_from_this(),
                                      boost::asio::placeholders::error));
}

void ra_connection::handle_write(const boost::system::error_code& e)
{
    if (e)
    {
        stop();
        return;
    }
}

void ra_connection::handle_stage_username(const std::string& line)
{
    uint32 acc_id = sAccountMgr::Instance()->GetId(line.c_str());
    if (!acc_id)
    {
        logger.info("%s tried to login with an invalid username: %s.",
            remote_address().c_str(), line.c_str());
        send("That username does not exist.\nUsername: ");
        return;
    }

    AccountTypes access = sAccountMgr::Instance()->GetSecurity(acc_id);
    if (access < SEC_FULL_GM)
    {
        logger.info(
            "%s tried to login with a username that has insufficient "
            "privileges: %s.",
            remote_address().c_str(), line.c_str());
        send(
            "That user does not have the needed privileges to use Remote "
            "Administration.\n");
        return;
    }

    send("\nPassword: ");
    stage_ = stage::password;
    account_id_ = acc_id;
    username_ = line;
}

void ra_connection::handle_stage_password(const std::string& line)
{
    if (sAccountMgr::Instance()->CheckPassword(account_id_, line))
    {
        logger.info(remote_address().c_str(), username_.c_str());
        send("You were successfully logged in.\n");
        stage_ = stage::authenticated;
    }
    else
    {
        send("Invalid password.\nUsername: ");
        stage_ = stage::username;
    }
}

void ra_connection::handle_stage_authenticated(const std::string& line)
{
    sWorld::Instance()->queue_cli_command(
        make_cli_command(account_id_, SEC_CONSOLE, line,
            boost::bind(&ra_connection::handle_command_completed,
                             shared_from_this(), _1)));
}

void ra_connection::handle_command_completed(const std::string& completion_text)
{
    send(completion_text);
}

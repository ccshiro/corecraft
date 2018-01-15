#ifndef MANGOSD__RA_CONNECTION_H
#define MANGOSD__RA_CONNECTION_H

#include "Common.h"
#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <string>

class ra_connection : private boost::noncopyable,
                      public boost::enable_shared_from_this<ra_connection>
{
public:
    ra_connection(boost::asio::io_service& io);

    boost::asio::ip::tcp::socket& socket() { return socket_; }
    void start();
    void stop();
    void send(const std::string& str);
    bool is_closed() const { return !socket_.is_open(); }
    std::string remote_address() const;

private:
    void handle_read(const boost::system::error_code& e);
    void handle_write(const boost::system::error_code& e);

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf streambuf_;

    std::string username_;
    uint32 account_id_;

    class stage
    {
    public:
        enum type
        {
            username,
            password,
            authenticated
        };
    };
    stage::type stage_;

    void handle_stage_username(const std::string& line);
    void handle_stage_password(const std::string& line);
    void handle_stage_authenticated(const std::string& line);
    void handle_command_completed(const std::string& completion_text);

    // Code From Boost Start
    // Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot
    // com)
    //
    // Distributed under the Boost Software License, Version 1.0. (See
    // accompanying
    // file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
    //
    // A reference-counted non-modifiable buffer class.
    class shared_const_buffer
    {
    public:
        // Construct from a std::string.
        explicit shared_const_buffer(const std::string& data)
          : data_(new std::vector<char>(data.begin(), data.end())),
            buffer_(boost::asio::buffer(*data_))
        {
        }

        // Implement the ConstBufferSequence requirements.
        typedef boost::asio::const_buffer value_type;
        typedef const boost::asio::const_buffer* const_iterator;
        const boost::asio::const_buffer* begin() const { return &buffer_; }
        const boost::asio::const_buffer* end() const { return &buffer_ + 1; }

    private:
        boost::shared_ptr<std::vector<char>> data_;
        boost::asio::const_buffer buffer_;
    };
    // Code From Boost End
};

#endif

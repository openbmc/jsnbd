#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/process/async_pipe.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace utils
{

class PipeReader
{
    using Buffer =
        boost::asio::dynamic_string_buffer<char, std::char_traits<char>,
                                           std::allocator<char>>;

  public:
    explicit PipeReader(boost::asio::io_context& ioc);

    size_t asyncRead(Buffer& buf, boost::asio::yield_context& yield,
                     boost::system::error_code& error);

    boost::process::async_pipe& getPipe()
    {
        return pipe;
    }

  private:
    boost::process::async_pipe pipe;
};

} // namespace utils

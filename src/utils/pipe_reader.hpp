#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/process/async_pipe.hpp>

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
    PipeReader(boost::asio::io_context& ioc);

    size_t asyncRead(Buffer&& buf, boost::asio::yield_context&& yield);

    boost::process::async_pipe& getPipe()
    {
        return pipe;
    }

  private:
    boost::asio::io_context& ioc;
    boost::process::async_pipe pipe;
};

} // namespace utils

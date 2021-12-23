#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>

namespace utils
{

class StreamDescriptor
{
  public:
    StreamDescriptor(boost::asio::io_context& ioc, int fd);

    void asyncWait(boost::asio::posix::descriptor_base::wait_type type,
                   boost::asio::yield_context&& yield);

  private:
    boost::asio::io_context& ioc;
    boost::asio::posix::stream_descriptor sd;
};

} // namespace utils

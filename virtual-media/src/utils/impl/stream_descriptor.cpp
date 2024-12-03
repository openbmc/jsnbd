#include "utils/stream_descriptor.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

namespace utils
{

StreamDescriptor::StreamDescriptor(boost::asio::io_context& ioc, int fd) :
    sd(ioc)
{
    sd.assign(fd);
}

void StreamDescriptor::asyncWait(
    boost::asio::posix::descriptor_base::wait_type type,
    boost::asio::yield_context&& yield)
{
    boost::system::error_code ec;
    sd.async_wait(type, yield[ec]);
}

} // namespace utils

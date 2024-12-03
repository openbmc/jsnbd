#include "utils/stream_descriptor.hpp"

#include <boost/asio/steady_timer.hpp>

#include <chrono>

namespace utils
{

StreamDescriptor::StreamDescriptor(boost::asio::io_context& ioc,
                                   [[maybe_unused]] int fd) : sd(ioc)
{}

void StreamDescriptor::asyncWait(
    [[maybe_unused]] boost::asio::posix::descriptor_base::wait_type type,
    boost::asio::yield_context&& yield)
{
    boost::asio::steady_timer tmr{sd.get_executor()};
    boost::system::error_code ec;

    tmr.expires_after(std::chrono::milliseconds(1));
    tmr.async_wait(yield[ec]);
}

} // namespace utils

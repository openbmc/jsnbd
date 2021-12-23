#include "utils/pipe_reader.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/system/error_code.hpp>

namespace utils
{

PipeReader::PipeReader(boost::asio::io_context& ioc) : ioc(ioc), pipe(ioc)
{}

size_t PipeReader::asyncRead(Buffer&& buf, boost::asio::yield_context&& yield)
{
    boost::system::error_code ec;
    return boost::asio::async_read_until(pipe, std::move(buf), '\n', yield[ec]);
}

} // namespace utils

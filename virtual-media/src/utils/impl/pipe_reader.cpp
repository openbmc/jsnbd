#include "utils/pipe_reader.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace utils
{

PipeReader::PipeReader(boost::asio::io_context& ioc) : pipe(ioc) {}

size_t PipeReader::asyncRead(Buffer& buf, boost::asio::yield_context& yield,
                             boost::system::error_code& error)
{
    boost::system::error_code ec;
    size_t bytesTransfered = boost::asio::async_read_until(pipe, buf, '\n',
                                                           yield[ec]);
    error = ec;

    return bytesTransfered;
}

} // namespace utils

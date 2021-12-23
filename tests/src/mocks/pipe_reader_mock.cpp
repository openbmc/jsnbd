#include "utils/pipe_reader.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#include <type_traits>

namespace utils
{

PipeReader::PipeReader(boost::asio::io_context& ioc) : pipe(ioc) {}

size_t
    PipeReader::asyncRead([[maybe_unused]] Buffer&& buf,
                          [[maybe_unused]] boost::asio::yield_context&& yield,
                          boost::system::error_code& error)
{
    error = boost::asio::error::misc_errors::eof;

    return 0;
}

} // namespace utils

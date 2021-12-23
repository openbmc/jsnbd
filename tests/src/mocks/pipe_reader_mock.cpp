#include "utils/pipe_reader.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#include <type_traits>

namespace utils
{

PipeReader::PipeReader(boost::asio::io_context& ioc) : ioc(ioc), pipe(ioc)
{}

size_t PipeReader::asyncRead([[maybe_unused]] Buffer&& buf,
                             boost::asio::yield_context&& yield)
{
    using result_type = typename boost::asio::async_result<
        std::decay_t<boost::asio::yield_context>,
        void(boost::system::error_code, int)>;
    typename result_type::completion_handler_type handler(
        std::forward<boost::asio::yield_context>(yield));

    result_type result(handler);
    handler(boost::asio::error::misc_errors::eof, 2);

    return 0;
}

} // namespace utils

#include "utils/child.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/process/args.hpp>
#include <boost/process/io.hpp>

#include <string>
#include <system_error>
#include <vector>

namespace utils
{

Child::Child(boost::asio::io_context& ioc, const std::string& app,
             const std::vector<std::string>& args, PipeReader& reader)
{
    std::error_code ec;

    child = boost::process::child(
        app, boost::process::args(args),
        (boost::process::std_out & boost::process::std_err) > reader.getPipe(),
        ec, ioc);
    if (ec)
    {
        throw std::system_error(ec);
    }
}

int Child::exitCode()
{
    return child.exit_code();
}

int Child::nativeExitCode()
{
    return child.native_exit_code();
}

bool Child::running()
{
    return child.running();
}

void Child::terminate()
{
    child.terminate();
}

void Child::wait()
{
    child.wait();
}

} // namespace utils

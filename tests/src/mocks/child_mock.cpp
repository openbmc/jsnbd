#include "mocks/child_mock.hpp"

#include "utils/child.hpp"

MockChildEngine* MockChild::engine = nullptr;

namespace utils
{

Child::Child([[maybe_unused]] boost::asio::io_context& ioc,
             [[maybe_unused]] const std::string& app,
             [[maybe_unused]] const std::vector<std::string>& args,
             [[maybe_unused]] PipeReader& reader)
{
    MockChild::create();
}

int Child::exitCode()
{
    return MockChild::exitCode();
}

int Child::nativeExitCode()
{
    return MockChild::nativeExitCode();
}

bool Child::running()
{
    return MockChild::running();
}

void Child::terminate()
{
    MockChild::terminate();
}

void Child::wait()
{
    MockChild::wait();
}

} // namespace utils

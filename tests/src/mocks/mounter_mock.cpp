#include "logger.hpp"
#include "mocks/mounter_test_dirs.hpp"
#include "utils/mounter.hpp"

#include <system_error>

namespace utils
{

int result = -1;

Mounter::Mounter([[maybe_unused]] const char* source, const char* target,
                 [[maybe_unused]] const char* filesystemtype,
                 [[maybe_unused]] unsigned long mountflags, const void* data) :
    target(target)
{
    std::string mountDir(target);
    std::string options(static_cast<const char*>(data));

    if ((target == directories::mountDir1) &&
        (options.find("vers=3.1.1") != std::string::npos))
    {
        throw std::system_error(std::make_error_code(std::errc::bad_address));
    }

    if (target == directories::mountDir2)
    {
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory));
    }
}

Mounter::~Mounter()
{
    std::string mountDir(target);
    result = 0;
    if (target == directories::mountDir3)
    {
        result = 1;
        LogMsg(Logger::Error, "Unable to unmout directory ", target,
               ", ec=", EBUSY);
    }
}

} // namespace utils

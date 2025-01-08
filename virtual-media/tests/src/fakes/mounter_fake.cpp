#include "fakes/mounter_fake.hpp"

#include "fakes/mounter_dirs_fake.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/mounter.hpp"

#include <cerrno>
#include <string>
#include <system_error>

using namespace virtual_media_test;

int MounterHelper::unmountResult = -1;

namespace utils
{

Mounter::Mounter([[maybe_unused]] const std::string& source,
                 const std::string& target,
                 [[maybe_unused]] const std::string& filesystemtype,
                 [[maybe_unused]] unsigned long mountflags, const void* data) :
    target(target)
{
    std::string options(static_cast<const char*>(data));

    if ((target == directories::mountDir0) &&
        (options.find("vers=3.1.1") != std::string::npos))
    {
        throw std::system_error(std::make_error_code(std::errc::bad_address));
    }

    if (target == directories::mountDir1)
    {
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory));
    }
}

Mounter::~Mounter()
{
    MounterHelper::unmountResult = 0;
    if (target == directories::mountDir2)
    {
        MounterHelper::unmountResult = 1;
        LOGGER_ERROR("Unable to unmount directory {}, ec={}", target, EBUSY);
    }
}

} // namespace utils

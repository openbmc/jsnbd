#include "utils/mounter.hpp"

#include "utils/log-wrapper.hpp"

#include <sys/mount.h>

#include <cerrno>
#include <string>
#include <system_error>

namespace utils
{

Mounter::Mounter(const std::string& source, const std::string& target,
                 const std::string& filesystemtype, unsigned long mountflags,
                 const void* data) : target(target)
{
    int ec = ::mount(source.c_str(), target.c_str(), filesystemtype.c_str(),
                     mountflags, data);

    if (ec < 0)
    {
        throw std::system_error(
            std::make_error_code(static_cast<std::errc>(errno)));
    }
}

Mounter::~Mounter()
{
    int result = ::umount(target.c_str());
    if (result < 0)
    {
        LOGGER_ERROR("Unable to unmount directory {}, ec={}", target, errno);
    }
}

} // namespace utils

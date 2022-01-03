#include "utils/mounter.hpp"

#include "utils/log-wrapper.hpp"

#include <sys/mount.h>

#include <system_error>

namespace utils
{

Mounter::Mounter(const char* source, const char* target,
                 const char* filesystemtype, unsigned long mountflags,
                 const void* data) :
    target(target)
{
    int ec = ::mount(source, target, filesystemtype, mountflags, data);

    if (ec < 0)
    {
        throw std::system_error(
            std::make_error_code(static_cast<std::errc>(errno)));
    }
}

Mounter::~Mounter()
{
    int result = ::umount(target);
    if (result < 0)
    {
        LOGGER_ERROR << "Unable to unmout directory " << target
                     << ", ec=" << errno;
    }
}

} // namespace utils

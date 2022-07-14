#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <filesystem>
#include <system_error>

namespace utils
{

class FileObject
{
  public:
    explicit FileObject(int fd) : fd(fd) {}

    explicit FileObject(const char* filename, int flags)
    {
        fd = ::open(filename, flags);
        if (fd < 0)
        {
            throw std::filesystem::filesystem_error(
                "Couldn't open file",
                std::make_error_code(static_cast<std::errc>(errno)));
        }
    }

    FileObject() = delete;
    FileObject(const FileObject&) = delete;
    FileObject& operator=(const FileObject&) = delete;

    int ioctl(unsigned long request)
    {
        return ::ioctl(fd, request);
    }

    ssize_t write(void* data, ssize_t nw)
    {
        return ::write(fd, data, static_cast<size_t>(nw));
    }

    ~FileObject()
    {
        ::close(fd);
    }

  private:
    int fd;
};

} // namespace utils

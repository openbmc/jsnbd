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

    // Ignore C-style vararg usage in open and ioctl functions. Those are system
    // methods and there's no reasonable replacement for them in C++.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
    explicit FileObject(const char* filename, int flags) :
        fd(::open(filename, flags))
    {
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
    FileObject(FileObject&&) = delete;
    FileObject& operator=(FileObject&&) = delete;

    int ioctl(unsigned long request) const
    {
        return ::ioctl(fd, request);
    }
    // NOLINTEND(cppcoreguidelines-pro-type-vararg)

    ssize_t write(void* data, ssize_t nw) const
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

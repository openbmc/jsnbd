#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>

namespace utils
{

class FileObject
{
  public:
    explicit FileObject(int fd) : fd(fd)
    {}

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

class SignalSender
{
  public:
    virtual ~SignalSender() = default;

    virtual void send(std::optional<const std::error_code> status) = 0;
};

class NotificationWrapper
{
  public:
    virtual ~NotificationWrapper() = default;

    virtual void
        start(std::function<void(const boost::system::error_code&)> handler,
              const std::chrono::seconds& duration) = 0;

    virtual void notify(const std::error_code& ec) = 0;
};

} // namespace utils

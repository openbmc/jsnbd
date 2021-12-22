#pragma once

#include "logger.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <compare>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

#define NBD_DISCONNECT _IO(0xab, 8)
#define NBD_CLEAR_SOCK _IO(0xab, 4)

class NBDDevice
{
  public:
    NBDDevice() = default;
    explicit NBDDevice(const std::string_view& nbdName)
    {
        if (!nbdName.empty())
        {
            const auto iter = std::ranges::find(
                std::cbegin(nameMatching), std::cend(nameMatching), nbdName);
            if (iter != std::cend(nameMatching))
            {
                value = *iter;
            }
        }
    }
    NBDDevice(const NBDDevice&) = default;
    NBDDevice(NBDDevice&&) = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) = default;

    auto operator<=>(const NBDDevice& rhs) const = default;

    bool isValid() const
    {
        return (!value.empty());
    }

    explicit operator bool()
    {
        return isValid();
    }

    bool isReady() const
    {
        if (value.empty())
        {
            return false;
        }
        int fd = open(toPath().c_str(), O_EXCL);
        if (fd < 0)
        {
            return false;
        }
        close(fd);
        return true;
    }

    void disconnect() const
    {
        if (value.empty())
        {
            return;
        }

        int fd = open(toPath().c_str(), O_RDWR);

        if (fd < 0)
        {
            LogMsg(Logger::Error, "Couldn't open device ", toPath().c_str());
            return;
        }
        if (ioctl(fd, NBD_DISCONNECT) < 0)
        {
            LogMsg(Logger::Info, "Ioctl failed: \n");
        }
        if (ioctl(fd, NBD_CLEAR_SOCK) < 0)
        {
            LogMsg(Logger::Info, "Ioctl failed: \n");
        }
        close(fd);
    }

    const std::string& toString() const
    {
        return value;
    }

    fs::path toPath() const
    {
        if (value.empty())
        {
            return {};
        }
        return fs::path("/dev") / fs::path(value);
    }

  private:
    std::string value{};

    static constexpr std::array<std::string_view, 16> nameMatching = {
        "nbd0", "nbd1", "nbd2",  "nbd3",  "nbd4",  "nbd5",  "nbd6",  "nbd7",
        "nbd8", "nbd9", "nbd10", "nbd11", "nbd12", "nbd13", "nbd14", "nbd15"};
};

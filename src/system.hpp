#pragma once

#include "logger.hpp"

#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

#define NBD_DISCONNECT _IO(0xab, 8)
#define NBD_CLEAR_SOCK _IO(0xab, 4)

class NBDDevice
{
  public:
    enum Value : uint8_t // This is explicite not a class to avoid naming like
                         // NBDDevice::Device::nbd0
    {
        nbd0 = 0,
        nbd1 = 1,
        nbd2 = 2,
        nbd3 = 3,
        nbd4 = 4,
        nbd5 = 5,
        nbd6 = 6,
        nbd7 = 7,
        nbd8 = 8,
        nbd9 = 9,
        nbd10 = 10,
        unknown = 0xFF
    };

    NBDDevice() = default;
    NBDDevice(Value v) : value(v){};
    explicit NBDDevice(const char* nbdName)
    {
        if (nbdName != nullptr)
        {
            const auto iter =
                std::find(nameMatching.cbegin(), nameMatching.cend(), nbdName);
            if (iter != nameMatching.cend())
            {
                value = static_cast<Value>(
                    std::distance(nameMatching.cbegin(), iter));
            }
        }
    }
    NBDDevice(const NBDDevice&) = default;
    NBDDevice(NBDDevice&&) = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) = default;

    bool operator==(const NBDDevice& rhs) const
    {
        return value == rhs.value;
    }
    bool operator!=(const NBDDevice& rhs) const
    {
        return value != rhs.value;
    }
    bool operator<(const NBDDevice& rhs) const
    {
        return value < rhs.value;
    }
    explicit operator bool()
    {
        return (value != unknown);
    }

    bool isReady() const
    {
        if (value == unknown)
        {
            return false;
        }
        int fd = open(to_path().c_str(), O_EXCL);
        if (fd < 0)
        {
            return false;
        }
        close(fd);
        return true;
    }

    void disconnect() const
    {
        if (value == unknown)
        {
            return;
        }

        int fd = open(to_path().c_str(), O_RDWR);

        if (fd < 0)
        {
            LogMsg(Logger::Error, "Couldn't open device ", to_path().c_str());
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

    std::string to_string() const
    {
        if (value == unknown)
        {
            return "";
        }
        return nameMatching[static_cast<uint8_t>(value)];
    }

    fs::path to_path() const
    {
        if (value == unknown)
        {
            return fs::path();
        }
        return fs::path("/dev") /
               fs::path(nameMatching[static_cast<uint8_t>(value)]);
    }

  private:
    Value value = unknown;

    static const inline std::vector<std::string> nameMatching = {
        "nbd0", "nbd1", "nbd2", "nbd3", "nbd4", "nbd5",
        "nbd6", "nbd7", "nbd8", "nbd9", "nbd10"};
};

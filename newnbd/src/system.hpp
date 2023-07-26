#pragma once

#include <fcntl.h>
#include <linux/nbd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <boost/beast/core/file_posix.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>

class NBDDevice
{
  public:
    explicit NBDDevice(int nbdIndex) : index(nbdIndex) {}
    NBDDevice(const NBDDevice&) = delete;
    NBDDevice(NBDDevice&&) noexcept = default;

    NBDDevice& operator=(const NBDDevice&) = delete;
    NBDDevice& operator=(NBDDevice&&) noexcept = default;

    ~NBDDevice() = default;

    auto operator<=>(const NBDDevice& rhs) const = default;

    bool isReady() const
    {
        if (index < 0)
        {
            return false;
        }

        boost::beast::file_posix file;
        boost::system::error_code ec;
        file.open(toPath().string().c_str(), boost::beast::file_mode::write,
                  ec);

        if (ec)
        {
            std::cout << "Ready check error";
            return false;
        }

        return true;
    }

    bool disconnect() const
    {
        if (index < 0)
        {
            return false;
        }

        boost::beast::file_posix file;
        boost::system::error_code ec;
        file.open(toPath().string().c_str(), boost::beast::file_mode::write,
                  ec);
        if (ec)
        {
            std::cout << "Disconnect error: " << ec.message();
            return false;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (ioctl(file.native_handle(), NBD_DISCONNECT) < 0)
        {
            std::cout << "Disconnect failed: \n";
            return false;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        if (ioctl(file.native_handle(), NBD_CLEAR_SOCK) < 0)
        {
            std::cout << "Clear socket failed: \n";
            return false;
        }

        return true;
    }

    std::string toString() const
    {
        return std::to_string(index);
    }

    std::filesystem::path toPath() const
    {
        if (index < 0)
        {
            return {};
        }
        return std::filesystem::path("/dev") /
               std::filesystem::path(std::to_string(index));
    }

  private:
    int index = -1;
};

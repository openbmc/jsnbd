#pragma once

#include "logger.hpp"
#include "utils/utils.hpp"

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

template <class File = utils::FileObject>
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
    NBDDevice(NBDDevice&&) noexcept = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) noexcept = default;

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

        try
        {
            File file(toPath().c_str(), O_EXCL);
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LogMsg(Logger::Error, "Ready check error: ", e.what());
            return false;
        }

        return true;
    }

    bool disconnect() const
    {
        if (value.empty())
        {
            return false;
        }

        try
        {
            File file(toPath().c_str(), O_RDWR);
            if (file.ioctl(NBD_DISCONNECT) < 0)
            {
                LogMsg(Logger::Info, "Ioctl failed: \n");
            }
            if (file.ioctl(NBD_CLEAR_SOCK) < 0)
            {
                LogMsg(Logger::Info, "Ioctl failed: \n");
            }
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LogMsg(Logger::Error, "Disconnect error: ", e.what());
            return false;
        }

        return true;
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

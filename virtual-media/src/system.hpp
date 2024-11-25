#pragma once

#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <linux/nbd.h>

#include <boost/beast/core/file_base.hpp>

#include <cstddef>
#include <filesystem>
#include <regex>
#include <string>

template <class File = utils::FileObject>
class NBDDevice
{
  public:
    NBDDevice() = default;
    explicit NBDDevice(const std::string& nbdName)
    {
        if (!nbdName.empty())
        {
            std::regex nbdRegex("nbd([0-9]+)");
            std::smatch nbdMatch;

            if (std::regex_search(nbdName, nbdMatch, nbdRegex))
            {
                size_t nbdId = std::stoul(nbdMatch[1]);
                if (nbdId < maxNbdCount)
                {
                    value = nbdMatch[0];
                }
            }
        }
    }
    NBDDevice(const NBDDevice&) = default;
    NBDDevice(NBDDevice&&) noexcept = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) noexcept = default;

    ~NBDDevice() = default;

    auto operator<=>(const NBDDevice& rhs) const = default;

    bool isValid() const
    {
        return (!value.empty());
    }

    bool isReady() const
    {
        if (value.empty())
        {
            return false;
        }

        try
        {
            File file(toPath().c_str(), boost::beast::file_mode::read);
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LOGGER_ERROR("Ready check error: {}", e.what());
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
            File file(toPath().c_str(), boost::beast::file_mode::write);
            if (file.ioctl(NBD_DISCONNECT) < 0)
            {
                LOGGER_INFO("Ioctl NBD_DISCONNECT failed");
            }
            if (file.ioctl(NBD_CLEAR_SOCK) < 0)
            {
                LOGGER_INFO("Ioctl NBD_CLEAR_SOCK failed");
            }
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LOGGER_ERROR("Disconnect error: {}", e.what());
            return false;
        }

        return true;
    }

    const std::string& toString() const
    {
        return value;
    }

    std::filesystem::path toPath() const
    {
        if (value.empty())
        {
            return {};
        }
        return std::filesystem::path("/dev") / std::filesystem::path(value);
    }

  private:
    std::string value;

    static constexpr size_t maxNbdCount = 16;
};

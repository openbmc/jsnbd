#pragma once

#include "system.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <boost/beast/core/file_base.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class Configuration
{
  public:
    enum class Mode
    {
        // Proxy mode - works directly from browser and uses JavaScript/HTML5
        // to communicate over Secure WebSockets directly to HTTPS endpoint
        // hosted by bmcweb on BMC
        proxy = 0,

#ifdef LEGACY_MODE_ENABLED
        // Legacy mode - is initiated from browser using Redfish defined
        // VirtualMedia schemas, then BMC process connects to external
        // CIFS/HTTPS image pointed during initialization.
        legacy = 1,
#endif

        invalid
    };

    struct MountPoint
    {
        static constexpr int defaultTimeout = 30;

        NBDDevice<> nbdDevice;
        std::string unixSocket;
        std::string endPointId;
        uint64_t timeout{defaultTimeout};
        std::optional<int> blocksize;
        Mode mode{Mode::proxy};

        static std::vector<std::string> toArgs(const MountPoint& mp)
        {
            const auto timeout = std::to_string(mp.timeout);
            return {"-t", timeout, "-u", mp.unixSocket, mp.nbdDevice.toPath(),
                    "-nL"};
        }
    };

    std::optional<const MountPoint*>
        getMountPoint(const std::string& name) const
    {
        const auto mp = mountPoints.find(name);
        if (mp != mountPoints.end())
        {
            return &mp->second;
        }

        return std::nullopt;
    }

    std::unordered_map<std::string, MountPoint> mountPoints;
    bool valid{false};

    explicit Configuration(const std::string& file) :
        valid(loadConfiguration(file))
    {}

  private:
    bool loadConfiguration(const std::string& file) noexcept
    {
        try
        {
            utils::FileObject configFile(file, boost::beast::file_mode::read);
            std::optional<std::string> configData = configFile.readContent();
            if (!configData)
            {
                LOGGER_ERROR("Couldn't read contents of '{}'", file);
                return false;
            }

            nlohmann::json data = nlohmann::json::parse(*configData, nullptr,
                                                        false);
            if (data.is_discarded())
            {
                LOGGER_ERROR("Invalid configuration data");
                return false;
            }

            setupVariables(data);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            LOGGER_CRITICAL("Could not open configuration file '{}': {}", file,
                            e.what());
            return false;
        }
        catch (const std::exception& e)
        {
            LOGGER_CRITICAL("Could not set up configuration: {}", e.what());
            return false;
        }

        return true;
    }

    template <typename T>
    bool getConfig(const nlohmann::json& json, std::string_view key,
                   T& value) const
    {
        const auto it = json.find(key);
        if (it == json.cend())
        {
            return false;
        }

        const T* valuePtr = it->get_ptr<const T*>();
        if (!valuePtr)
        {
            return false;
        }
        value = *valuePtr;
        return true;
    }

    bool setupVariables(const nlohmann::json& config)
    {
        for (const auto& item : config.items())
        {
            if (item.key() == "MountPoints")
            {
                for (const auto& mountpoint : item.value().items())
                {
                    MountPoint mp;

                    std::string unixSocket;
                    if (!getConfig(mountpoint.value(), "UnixSocket",
                                   unixSocket))
                    {
                        LOGGER_ERROR("UnixSocket required, not set");
                        continue;
                    }

                    mp.unixSocket = unixSocket;

                    std::string endpointId;
                    if (!getConfig(mountpoint.value(), "EndpointId",
                                   endpointId))
                    {
                        LOGGER_INFO("EndpointId required, not set");
                        continue;
                    }

                    mp.endPointId = endpointId;

                    uint64_t timeout = 0;
                    if (!getConfig(mountpoint.value(), "Timeout", timeout))
                    {
                        LOGGER_INFO("Timeout not set, use default");
                    }
                    else
                    {
                        mp.timeout = timeout;
                    }

                    uint64_t blockSize = 0;
                    if (!getConfig(mountpoint.value(), "BlockSize", blockSize))
                    {
                        LOGGER_INFO("BlockSize not set, use default");
                    }
                    else
                    {
                        mp.blocksize = blockSize;
                    }

                    std::string modeStr;
                    if (!getConfig(mountpoint.value(), "Mode", modeStr))
                    {
                        LOGGER_INFO(
                            "Mode does not exist, skip this mount point");
                        continue;
                    }

                    Mode mode = fromString(modeStr);
                    switch (mode)
                    {
                        case Mode::proxy:
#ifdef LEGACY_MODE_ENABLED
                        case Mode::legacy:
#endif
                        {
                            mp.mode = mode;
                            break;
                        }
                        case Mode::invalid:
                        {
                            LOGGER_ERROR("Mode not set, skip this mount point");
                            continue;
                        }
                    }

                    std::string nbdDevice;
                    if (!getConfig(mountpoint.value(), "NBDDevice", nbdDevice))
                    {
                        LOGGER_ERROR("NBDDevice required, not set");
                        continue;
                    }

                    mp.nbdDevice = NBDDevice(nbdDevice);
                    if (mp.nbdDevice.isValid())
                    {
                        mountPoints.emplace(mountpoint.key(), std::move(mp));
                    }
                }
            }
        }
        return true;
    }

    static Mode fromString(std::string_view str)
    {
        if (str == "proxy")
        {
            return Mode::proxy;
        }
#ifdef LEGACY_MODE_ENABLED
        if (str == "legacy")
        {
            return Mode::legacy;
        }
#endif

        return Mode::invalid;
    }
};

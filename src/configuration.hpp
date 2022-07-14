#pragma once

#include "system.hpp"
#include "utils/log-wrapper.hpp"

#include <sys/types.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
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

        // Legacy mode - is initiated from browser using Redfish defined
        // VirtualMedia schemas, then BMC process connects to external
        // CIFS/HTTPS image pointed during initialization.
        legacy = 1,
    };

    static constexpr mode_t defaultUmask = 077;

    struct MountPoint
    {
        static constexpr int defaultTimeout = 30;

        NBDDevice<> nbdDevice;
        std::string unixSocket;
        std::string endPointId;
        std::optional<int> timeout;
        std::optional<int> blocksize;
        Mode mode{Mode::proxy};

        static std::vector<std::string> toArgs(const MountPoint& mp)
        {
            const auto timeout =
                std::to_string(mp.timeout.value_or(defaultTimeout));
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
        std::ifstream configFile(file);
        if (!configFile.is_open())
        {
            LOGGER_CRITICAL << "Could not open configuration file '" << file
                            << "'";
            return false;
        }
        try
        {
            auto data = nlohmann::json::parse(configFile, nullptr);
            setupVariables(data);
        }
        catch (const std::exception& e)
        {
            LOGGER_CRITICAL << "Error parsing JSON file. Reason: " << e.what();
            return false;
        }
        return true;
    }

    template <typename T>
    bool getConfig(const nlohmann::json& json, std::string_view key,
                   T& value) const
    {
        const auto it = json.find(key);
        if (it != json.cend())
        {
            const T* valuePtr = it->get_ptr<const T*>();
            if (!valuePtr)
            {
                return false;
            }
            value = *valuePtr;
            return true;
        }
        return false;
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
                    std::string nbdDevice;
                    if (!getConfig(mountpoint.value(),
                                   std::string_view("NBDDevice"), nbdDevice))
                    {
                        LOGGER_ERROR << "NBDDevice required, not set";
                        continue;
                    }

                    mp.nbdDevice = NBDDevice(nbdDevice);
                    if (!mp.nbdDevice.isValid())
                    {
                        LOGGER_ERROR << "NBDDevice unrecognized.";
                        continue;
                    }

                    std::string unixSocket;
                    if (!getConfig(mountpoint.value(),
                                   std::string_view("UnixSocket"), unixSocket))
                    {
                        LOGGER_ERROR << "UnixSocket required, not set";
                        continue;
                    }

                    mp.unixSocket = unixSocket;

                    std::string endpointId;
                    if (!getConfig(mountpoint.value(), "EndpointId",
                                   endpointId))
                    {
                        LOGGER_INFO << "EndpointId required, not set";
                        continue;
                    }

                    mp.endPointId = endpointId;

                    uint64_t timeout = 0;
                    if (!getConfig(mountpoint.value(), "Timeout", timeout))
                    {
                        LOGGER_INFO << "Timeout not set, use default";
                    }
                    else
                    {
                        mp.timeout = timeout;
                    }

                    uint64_t blockSize = 0;
                    if (!getConfig(mountpoint.value(), "BlockSize", blockSize))
                    {
                        LOGGER_INFO << "BlockSize not set, use default";
                    }
                    else
                    {
                        mp.blocksize = blockSize;
                    }

                    uint64_t mode = 0;
                    if (!getConfig(mountpoint.value(), "Mode", mode))
                    {
                        LOGGER_INFO
                            << "Mode does not exist, skip this mount point";
                        continue;
                    }

                    switch (mode)
                    {
                        case 0:
                        {
                            mp.mode = Configuration::Mode::proxy;
                            break;
                        }
#ifdef LEGACY_MODE_ENABLED
                        case 1:
                        {
                            mp.mode = Configuration::Mode::legacy;
                            break;
                        }
#endif
                        default:
                        {
                            LOGGER_ERROR
                                << "Mode not set, skip this mount point";
                            continue;
                        }
                    }

                    mountPoints.emplace(mountpoint.key(), std::move(mp));
                }
            }
        }
        return true;
    }
};

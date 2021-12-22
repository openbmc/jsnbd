#pragma once

#include "logger.hpp"
#include "system.hpp"

#include <sys/types.h>

#include <nlohmann/json.hpp>

#include <chrono>
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

        NBDDevice nbdDevice;
        std::string unixSocket;
        std::string endPointId;
        std::optional<int> timeout;
        std::optional<int> blocksize;
        std::chrono::seconds remainingInactivityTimeout;
        Mode mode;

        static std::vector<std::string> toArgs(const MountPoint& mp)
        {
            const auto timeout =
                std::to_string(mp.timeout.value_or(defaultTimeout));
            return {"-t", timeout, "-u", mp.unixSocket, mp.nbdDevice.toPath(),
                    "-n"};
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

    bool valid = false;
    std::unordered_map<std::string, MountPoint> mountPoints;
    static std::chrono::seconds inactivityTimeout;

    Configuration(const std::string& file)
    {
        valid = loadConfiguration(file);
    }

  private:
    bool loadConfiguration(const std::string& file) noexcept
    {
        std::ifstream configFile(file);
        if (!configFile.is_open())
        {
            LogMsg(Logger::Critical, "Could not open configuration file '",
                   file, "'");
            return false;
        }
        try
        {
            auto data = nlohmann::json::parse(configFile, nullptr);
            setupVariables(data);
        }
        catch (const std::exception& e)
        {
            LogMsg(Logger::Critical,
                   "Error parsing JSON file. Reason: ", e.what());
            return false;
        }
        return true;
    }

    template <typename T>
    bool getConfig(const nlohmann::json& json, const std::string_view& key,
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
        inactivityTimeout =
            std::chrono::seconds(config.value("InactivityTimeout", 0));
        if (inactivityTimeout == std::chrono::seconds(0))
        {
            LogMsg(Logger::Error, "InactivityTimeout required, not set");
        }

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
                        LogMsg(Logger::Error, "NBDDevice required, not set");
                        continue;
                    }

                    mp.nbdDevice = NBDDevice(nbdDevice);
                    if (!mp.nbdDevice.isValid())
                    {
                        LogMsg(Logger::Error, "NBDDevice unrecognized.");
                        continue;
                    }

                    std::string unixSocket;
                    if (!getConfig(mountpoint.value(),
                                   std::string_view("UnixSocket"), unixSocket))
                    {
                        LogMsg(Logger::Error, "UnixSocket required, not set");
                        continue;
                    }

                    mp.unixSocket = unixSocket;

                    std::string endpointId;
                    if (!getConfig(mountpoint.value(), "EndpointId",
                                   endpointId))
                    {
                        LogMsg(Logger::Info, "EndpointId required, not set");
                        continue;
                    }

                    mp.endPointId = endpointId;

                    uint64_t timeout;
                    if (!getConfig(mountpoint.value(), "Timeout", timeout))
                    {
                        LogMsg(Logger::Info, "Timeout not set, use default");
                    }
                    else
                    {
                        mp.timeout = timeout;
                    }

                    uint64_t blockSize;
                    if (!getConfig(mountpoint.value(), "BlockSize", blockSize))
                    {
                        LogMsg(Logger::Info, "BlockSize not set, use default");
                    }
                    else
                    {
                        mp.blocksize = blockSize;
                    }

                    uint64_t mode;
                    if (!getConfig(mountpoint.value(), "Mode", mode))
                    {

                        LogMsg(Logger::Error,
                               "Mode does not exist, skip this mount point");
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
                            LogMsg(Logger::Error,
                                   "Mode not set, skip this mount point");
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

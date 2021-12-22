#pragma once

#include "logger.hpp"
#include "system.hpp"

#include <sys/types.h>

#include <boost/chrono.hpp>
#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>
#include <string>
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
            std::vector<std::string> args = {
                "-t", timeout, "-u", mp.unixSocket, mp.nbdDevice.to_path(),
                "-n"};
            return args;
        }
    };

    const MountPoint* getMountPoint(const std::string& name) const
    {
        const auto mp = mountPoints.find(name);
        if (mp != mountPoints.end())
        {
            return &(mp->second);
        }
        else
        {
            return nullptr;
        }
    }

    bool valid = false;
    boost::container::flat_map<std::string, MountPoint> mountPoints;
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
            LogMsg(Logger::Critical, "Could not open configuration file");
            return false;
        }
        try
        {
            auto data = nlohmann::json::parse(configFile, nullptr);
            setupVariables(data);
        }
        catch (nlohmann::json::exception& e)
        {
            LogMsg(Logger::Critical, "Error parsing JSON file");
            return false;
        }
        catch (std::out_of_range& e)
        {
            LogMsg(Logger::Critical, "Error parsing JSON file");
            return false;
        }
        return true;
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
                    const auto nbdDeviceIter =
                        mountpoint.value().find("NBDDevice");
                    if (nbdDeviceIter != mountpoint.value().cend())
                    {
                        const std::string* value =
                            nbdDeviceIter->get_ptr<const std::string*>();
                        if (value)
                        {
                            mp.nbdDevice = NBDDevice(value->c_str());
                            if (!mp.nbdDevice)
                            {
                                LogMsg(Logger::Error,
                                       "NBDDevice unrecognized.");
                                continue;
                            }
                        }
                        else
                        {
                            LogMsg(Logger::Error,
                                   "NBDDevice required, not set");
                            continue;
                        }
                    };
                    const auto unixSocketIter =
                        mountpoint.value().find("UnixSocket");
                    if (unixSocketIter != mountpoint.value().cend())
                    {
                        const std::string* value =
                            unixSocketIter->get_ptr<const std::string*>();
                        if (value)
                        {
                            mp.unixSocket = *value;
                        }
                        else
                        {
                            LogMsg(Logger::Error,
                                   "UnixSocket required, not set");
                            continue;
                        }
                    }
                    const auto endPointIdIter =
                        mountpoint.value().find("EndpointId");
                    if (endPointIdIter != mountpoint.value().cend())
                    {
                        const std::string* value =
                            endPointIdIter->get_ptr<const std::string*>();
                        if (value)
                        {
                            mp.endPointId = *value;
                        }
                        else
                        {
                            LogMsg(Logger::Info,
                                   "EndpointId required, not set");
                            continue;
                        }
                    }
                    const auto timeoutIter = mountpoint.value().find("Timeout");
                    if (timeoutIter != mountpoint.value().cend())
                    {
                        const uint64_t* value =
                            timeoutIter->get_ptr<const uint64_t*>();
                        if (value)
                        {
                            mp.timeout = *value;
                        }
                        else
                        {
                            LogMsg(Logger::Info,
                                   "Timeout not set, use default");
                        }
                    }
                    const auto blocksizeIter =
                        mountpoint.value().find("BlockSize");
                    if (blocksizeIter != mountpoint.value().cend())
                    {
                        const uint64_t* value =
                            blocksizeIter->get_ptr<const uint64_t*>();
                        if (value)
                        {
                            mp.blocksize = *value;
                        }
                        else
                        {
                            LogMsg(Logger::Info,
                                   "BlockSize not set, use default");
                        }
                    }
                    const auto modeIter = mountpoint.value().find("Mode");
                    if (modeIter != mountpoint.value().cend())
                    {
                        const uint64_t* value =
                            modeIter->get_ptr<const uint64_t*>();
                        if (value)
                        {
                            if (*value == 0)
                            {
                                mp.mode = Configuration::Mode::proxy;
                            }
#if LEGACY_MODE_ENABLED
                            else if (*value == 1)
                            {
                                mp.mode = Configuration::Mode::legacy;
                            }
#endif
                            else
                            {
                                LogMsg(Logger::Error,
                                       "Incorrect Mode, skip this mount point");
                                continue;
                            }
                        }
                        else
                        {
                            LogMsg(Logger::Error,
                                   "Mode not set, skip this mount point");
                            continue;
                        }
                    }
                    else
                    {
                        LogMsg(Logger::Error,
                               "Mode does not exist, skip this mount point");
                        continue;
                    }
                    mountPoints[mountpoint.key()] = std::move(mp);
                }
            }
        }
        return true;
    }
};

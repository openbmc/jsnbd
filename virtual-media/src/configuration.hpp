#pragma once

#include "system.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <boost/beast/core/file_base.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
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

        // Standard mode - is initiated from browser using Redfish defined
        // VirtualMedia schemas, then BMC process connects to external
        // CIFS/HTTPS image pointed during initialization.
        standard = 1,

        invalid
    };

    struct MountPoint
    {
        static constexpr uint64_t timeout = 30;

        NBDDevice<> nbdDevice;
        std::string unixSocket;
        std::string endPointId;
        Mode mode{Mode::proxy};

        static std::vector<std::string> toArgs(const MountPoint& mp)
        {
            const auto timeout =
                std::to_string(Configuration::MountPoint::timeout);
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

            return setupVariables(data);
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
    }

    bool setupVariables(const nlohmann::json& config)
    {
        size_t proxyMountPoints = 0;
        size_t standardMountPoints = 0;
        for (const auto& item : config.items())
        {
            if (item.key() == "ProxyMountPoints")
            {
                proxyMountPoints = item.value();
            }
            if (item.key() == "StandardMountPoints")
            {
                standardMountPoints = item.value();
            }
        }

        return createMountPointsMap(proxyMountPoints, standardMountPoints);
    }

    bool createMountPointsMap(const size_t proxyMountPoints,
                              const size_t standardMountPoints)
    {
        constexpr std::string_view slotPrefix = "Slot";
        size_t mountPointCounter = 0;

        for (size_t i = 0; i < proxyMountPoints; i++)
        {
            std::optional<MountPoint> mp = createMountPoint(mountPointCounter,
                                                            Mode::proxy);
            if (!mp)
            {
                return false;
            }
            std::string slotStr = std::format("{}_{}", slotPrefix,
                                              mountPointCounter);
            mountPoints[slotStr] = std::move(*mp);
            mountPointCounter++;
        }

        for (size_t i = 0; i < standardMountPoints; i++)
        {
            std::optional<MountPoint> mp = createMountPoint(mountPointCounter,
                                                            Mode::standard);
            if (!mp)
            {
                return false;
            }
            std::string slotStr = std::format("{}_{}", slotPrefix,
                                              mountPointCounter);
            mountPoints[slotStr] = std::move(*mp);
            mountPointCounter++;
        }

        return true;
    }

    static std::optional<MountPoint> createMountPoint(const size_t index,
                                                      const Mode mode)
    {
        MountPoint mp;
        std::string nbdDevId = std::format("nbd{}", index);
        mp.nbdDevice = NBDDevice(nbdDevId);

        if (!mp.nbdDevice.isValid())
        {
            return {};
        }

        std::string unixSocketStr = std::format("/run/virtual-media/nbd{}.sock",
                                                index);
        mp.unixSocket = unixSocketStr;

        mp.mode = mode;

        if (mode == Mode::proxy)
        {
            std::string endPointIdStr = std::format("/nbd/{}", index);
            mp.endPointId = endPointIdStr;
        }

        return mp;
    }
};

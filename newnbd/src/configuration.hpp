#pragma once

#include "system.hpp"

#include <sys/types.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
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

    struct MountPoint
    {
        MountPoint() = default;
        static constexpr int defaultTimeout = 30;
        std::string name;
        std::optional<NBDDevice> nbdDevice;
        std::string unixSocket;
        std::string endPointId;
        std::optional<int> timeout;
        std::optional<int> blocksize;
        std::chrono::seconds remainingInactivityTimeout{0};
        Mode mode = Mode::proxy;

        static std::vector<std::string> toArgs(const MountPoint& mp)
        {
            const auto timeout =
                std::to_string(mp.timeout.value_or(defaultTimeout));
            return {"-t", timeout, "-u", mp.unixSocket, mp.nbdDevice->toPath(),
                    "-n"};
        }
    };

    const MountPoint* getMountPoint(std::string_view name) const
    {
        const auto mp = std::ranges::find_if(
            mountPoints, [name](auto&& val) { return val.name == name; });
        if (mp == mountPoints.end())
        {
            return nullptr;
        }
        return &*mp;
    }

    bool valid = false;
    std::vector<MountPoint> mountPoints;
    static constexpr std::chrono::seconds inactivityTimeout{1800};

    explicit Configuration()
    {
        for (size_t index = 0; index < 4; index++)
        {
            MountPoint& mp = mountPoints.emplace_back();
            mp.name = "nbd" + std::to_string(index);
            mp.nbdDevice.emplace(index);
            mp.unixSocket = "/run/virtual-media/nbd" + std::to_string(index) +
                            ".sock";
            mp.endPointId = "/nbd/" + std::to_string(index);
            mp.timeout = 90;
            mp.blocksize = 512;
            if (index < 2)
            {
                mp.mode = Configuration::Mode::proxy;
            }
            else
            {
                mp.mode = Configuration::Mode::legacy;
            }
        }
    }
};

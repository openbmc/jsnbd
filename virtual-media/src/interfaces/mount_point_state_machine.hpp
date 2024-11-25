#pragma once

#include "configuration.hpp"
#include "resources.hpp"

#include <boost/asio/io_context.hpp>

#include <memory>
#include <optional>
#include <string>

struct BasicState;

namespace interfaces
{

struct MountPointStateMachine
{
    struct Target
    {
        std::unique_ptr<resource::Mount> mountPoint;
    };

    MountPointStateMachine() = default;

    MountPointStateMachine(const MountPointStateMachine&) = delete;
    MountPointStateMachine(MountPointStateMachine&&) = delete;

    MountPointStateMachine& operator=(const MountPointStateMachine&) = delete;
    MountPointStateMachine& operator=(MountPointStateMachine&&) = delete;

    virtual ~MountPointStateMachine() = default;

    virtual std::string_view getName() const = 0;
    virtual Configuration::MountPoint& getConfig() = 0;
    virtual std::optional<Target>& getTarget() = 0;
    virtual BasicState& getState() = 0;
    virtual int& getExitCode() = 0;
    virtual boost::asio::io_context& getIOC() = 0;
};

} // namespace interfaces

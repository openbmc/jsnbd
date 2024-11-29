#pragma once

#include "configuration.hpp"
#include "resources.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

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

    virtual void emitRegisterDBusEvent(
        std::shared_ptr<sdbusplus::asio::connection> bus,
        std::shared_ptr<sdbusplus::asio::object_server> objServer) = 0;

    virtual void setMountPointInterface(
        std::unique_ptr<sdbusplus::asio::dbus_interface>) = 0;
    virtual void setProcessInterface(
        std::unique_ptr<sdbusplus::asio::dbus_interface>) = 0;
    virtual void setServiceInterface(
        std::unique_ptr<sdbusplus::asio::dbus_interface>) = 0;
};

} // namespace interfaces

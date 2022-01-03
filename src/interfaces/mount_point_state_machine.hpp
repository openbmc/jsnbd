#pragma once

#include "configuration.hpp"
#include "resources.hpp"
#include "system.hpp"
#include "types/dbus_types.hpp"
#include "utils/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <memory>
#include <optional>
#include <string>
#include <system_error>

struct BasicState;

namespace interfaces
{

struct MountPointStateMachine
{
    struct Target
    {
        std::string imgUrl;
        bool rw;
        std::unique_ptr<resource::Mount> mountPoint;
        std::unique_ptr<utils::CredentialsProvider> credentials;
    };

    virtual ~MountPointStateMachine() = default;

    virtual void notify(const std::error_code& ec = {}) = 0;
    virtual void
        notificationInitialize(std::shared_ptr<sdbusplus::asio::connection> con,
                               const std::string& svc, const std::string& iface,
                               const std::string& name) = 0;

    virtual void notificationStart() = 0;

    virtual std::string_view getName() const = 0;
    virtual Configuration::MountPoint& getConfig() = 0;
    virtual std::optional<Target>& getTarget() = 0;
    virtual BasicState& getState() = 0;
    virtual int& getExitCode() = 0;
    virtual boost::asio::io_context& getIOC() = 0;

    virtual void
        emitRegisterDBusEvent(std::shared_ptr<sdbusplus::asio::connection> bus,
                              std::shared_ptr<object_server> objServer) = 0;
    virtual void emitMountEvent(std::optional<Target>) = 0;
    virtual void emitUnmountEvent() = 0;
    virtual void emitSubprocessStoppedEvent() = 0;
    virtual void emitUdevStateChangeEvent(const NBDDevice<>& dev,
                                          StateChange devState) = 0;
};

} // namespace interfaces

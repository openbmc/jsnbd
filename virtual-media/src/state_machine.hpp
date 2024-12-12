#pragma once

#include "configuration.hpp"
#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "state/basic_state.hpp"
#include "state/initial_state.hpp"
#include "system.hpp"
#include "utils/impl/dbus_notify_wrapper.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

struct MountPointStateMachine : public interfaces::MountPointStateMachine
{
    MountPointStateMachine(boost::asio::io_context& ioc,
                           DeviceMonitor& devMonitor, const std::string& name,
                           const Configuration::MountPoint& config) :
        ioc{ioc}, name{name}, config{config}
    {
        devMonitor.addDevice(config.nbdDevice);
    }

    MountPointStateMachine(const MountPointStateMachine&) = delete;
    MountPointStateMachine(MountPointStateMachine&&) = delete;

    MountPointStateMachine& operator=(const MountPointStateMachine&) = delete;
    MountPointStateMachine& operator=(MountPointStateMachine&&) = delete;

    ~MountPointStateMachine() override = default;

    std::string_view getName() const override
    {
        return name;
    }

    Configuration::MountPoint& getConfig() override
    {
        return config;
    }

    std::optional<Target>& getTarget() override
    {
        return target;
    }

    BasicState& getState() override
    {
        return *state;
    }

    int& getExitCode() override
    {
        return exitCode;
    }

    boost::asio::io_context& getIOC() override
    {
        return ioc;
    }

    void changeState(std::unique_ptr<BasicState> newState)
    {
        state = std::move(newState);
        LOGGER_INFO("{} state changed to {}", name, state->getStateName());

        if (auto nextState = state->onEnter())
        {
            changeState(std::move(nextState));
        }
    }

    template <class EventT>
    void emitEvent(EventT&& event)
    {
        LOGGER_INFO("{} received {} while in {}", name, event.eventName,
                    state->getStateName());

        if (auto newState = state->handleEvent(std::forward<EventT>(event)))
        {
            changeState(std::move(newState));
        }
    }

    void emitRegisterDBusEvent(
        std::shared_ptr<sdbusplus::asio::connection> bus,
        std::shared_ptr<sdbusplus::asio::object_server> objServer) override
    {
        emitEvent(RegisterDbusEvent(bus, objServer));
    }

    void emitMountEvent(std::optional<Target> newTarget) override
    {
        emitEvent(MountEvent(std::move(newTarget)));
    }

    void emitUnmountEvent() override
    {
        emitEvent(UnmountEvent());
    }

    void emitSubprocessStoppedEvent() override
    {
        emitEvent(SubprocessStoppedEvent());
    }

    void emitUdevStateChangeEvent(const NBDDevice<>& dev,
                                  StateChange devState) override
    {
        if (config.nbdDevice == dev)
        {
            emitEvent(UdevStateChangeEvent(devState));

            return;
        }

        LOGGER_DEBUG("{} Ignoring request.", name);
    }

    void setMountPointInterface(
        std::unique_ptr<sdbusplus::asio::dbus_interface> iface) override
    {
        mountPointIface = std::move(iface);
    }

    void setProcessInterface(
        std::unique_ptr<sdbusplus::asio::dbus_interface> iface) override
    {
        processIface = std::move(iface);
    }

    void setServiceInterface(
        std::unique_ptr<sdbusplus::asio::dbus_interface> iface) override
    {
        serviceIface = std::move(iface);
    }

    void
        notificationInitialize(std::shared_ptr<sdbusplus::asio::connection> con,
                               const std::string& svc, const std::string& iface,
                               const std::string& name) override
    {
        auto signal = std::make_unique<utils::DbusSignalSender>(
            std::move(con), svc, iface, name);

        auto timer = std::make_unique<boost::asio::steady_timer>(ioc);

        completionNotification =
            std::make_unique<utils::DbusNotificationWrapper>(std::move(signal),
                                                             std::move(timer));
    }

    void notificationStart() override
    {
        auto notificationHandler = [this](const boost::system::error_code& ec) {
            if (ec == boost::system::errc::operation_canceled)
            {
                return;
            }

            LOGGER_ERROR("[App] timedout when waiting for target state");

            this->notify(
                std::make_error_code(std::errc::device_or_resource_busy));
        };

        LOGGER_DEBUG("Started notification");
        completionNotification->start(
            std::move(notificationHandler),
            std::chrono::seconds(Configuration::MountPoint::timeout + 5));
    }

    void notify(const std::error_code& ec = {}) override
    {
        completionNotification->notify(ec);
    }

    boost::asio::io_context& ioc;
    std::string name;
    Configuration::MountPoint config;
    std::unique_ptr<utils::NotificationWrapper> completionNotification;

    std::optional<Target> target;
    std::unique_ptr<BasicState> state = std::make_unique<InitialState>(*this);
    int exitCode = -1;

  private:
    std::unique_ptr<sdbusplus::asio::dbus_interface> mountPointIface;
    std::unique_ptr<sdbusplus::asio::dbus_interface> processIface;
    std::unique_ptr<sdbusplus::asio::dbus_interface> serviceIface;
};

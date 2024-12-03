#pragma once

#include "configuration.hpp"
#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "state/basic_state.hpp"
#include "state/initial_state.hpp"
#include "system.hpp"
#include "utils/log-wrapper.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

    boost::asio::io_context& ioc;
    std::string name;
    Configuration::MountPoint config;

    std::optional<Target> target;
    std::unique_ptr<BasicState> state = std::make_unique<InitialState>(*this);
    int exitCode = -1;

  private:
    std::unique_ptr<sdbusplus::asio::dbus_interface> mountPointIface;
    std::unique_ptr<sdbusplus::asio::dbus_interface> processIface;
    std::unique_ptr<sdbusplus::asio::dbus_interface> serviceIface;
};

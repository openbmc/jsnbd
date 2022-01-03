#pragma once

#include "configuration.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "state/basic_state.hpp"
#include "state/initial_state.hpp"
#include "system.hpp"
#include "types/dbus_types.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

struct MountPointStateMachine : public interfaces::MountPointStateMachine
{
    MountPointStateMachine(boost::asio::io_context& ioc,
                           DeviceMonitor& devMonitor, const std::string& name,
                           const Configuration::MountPoint& config) :
        ioc{ioc},
        name{name}, config{config}
    {
        devMonitor.addDevice(config.nbdDevice);
    }

    MountPointStateMachine& operator=(MountPointStateMachine&&) = delete;

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
        LogMsg(Logger::Info, name, " state changed to ", state->getStateName());
        if ((newState = state->onEnter()))
        {
            changeState(std::move(newState));
        }
    }

    template <class EventT>
    void emitEvent(EventT&& event)
    {
        LogMsg(Logger::Info, name, " received ", event.eventName, " while in ",
               state->getStateName());

        if (auto newState = state->handleEvent(std::move(event)))
        {
            changeState(std::move(newState));
        }
    }

    void
        emitRegisterDBusEvent(std::shared_ptr<sdbusplus::asio::connection> bus,
                              std::shared_ptr<object_server> objServer) override
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
        }
        else
        {
            LogMsg(Logger::Debug, name, " Ignoring request.");
        }
    }

    boost::asio::io_context& ioc;
    std::string name;
    Configuration::MountPoint config;

    std::optional<Target> target;
    std::unique_ptr<BasicState> state = std::make_unique<InitialState>(*this);
    int exitCode = -1;
};

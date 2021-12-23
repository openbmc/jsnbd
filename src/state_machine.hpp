#pragma once
#include "interfaces/mount_point_state_machine.hpp"
#include "state/basic_state.hpp"

#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <functional>
#include <memory>
#include <system_error>

struct MountPointStateMachine : public interfaces::MountPointStateMachine
{
    MountPointStateMachine(boost::asio::io_context& ioc,
                           const std::string& name,
                           const Configuration::MountPoint& config) :
        ioc{ioc},
        name{name}, config{config}
    {}

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
    }

    template <class EventT>
    void emitEvent(EventT&& event)
    {
        LogMsg(Logger::Info, name, " received ", event.eventName, " while in ",
               state->getStateName());
    }

    boost::asio::io_context& ioc;
    std::string name;
    Configuration::MountPoint config;

    std::optional<Target> target;
    std::unique_ptr<BasicState> state;
    int exitCode = -1;
};

#pragma once

#include "interfaces/mount_point_state_machine.hpp"
#include "state/basic_state.hpp"
#include "utils/log-wrapper.hpp"

#include <boost/asio/io_context.hpp>

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

struct MountPointStateMachine : public interfaces::MountPointStateMachine
{
    MountPointStateMachine(boost::asio::io_context& ioc,
                           const std::string& name,
                           const Configuration::MountPoint& config) :
        ioc{ioc}, name{name}, config{config}
    {}

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
    }

    template <class EventT>
    void emitEvent(EventT&& event)
    {
        LOGGER_INFO("{} received {} while in {}", name, event.eventName,
                    state->getStateName());
    }

    boost::asio::io_context& ioc;
    std::string name;
    Configuration::MountPoint config;

    std::optional<Target> target;
    std::unique_ptr<BasicState> state;
    int exitCode = -1;
};

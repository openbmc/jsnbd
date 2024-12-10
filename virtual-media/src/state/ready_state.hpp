#pragma once

#include "activating_state.hpp"
#include "basic_state.hpp"
#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "utils/log-wrapper.hpp"

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

struct ReadyState : public BasicStateT<ReadyState>
{
    static std::string_view stateName()
    {
        return "ReadyState";
    }

    explicit ReadyState(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}

    ReadyState(interfaces::MountPointStateMachine& machine, const std::errc& ec,
               const std::string& message) : BasicStateT(machine)
    {
        LOGGER_ERROR("{} Errno = {} : {}", machine.getName(),
                     static_cast<int>(ec), message);
    }

    std::unique_ptr<BasicState> onEnter() override
    {
        return nullptr;
    }

    std::unique_ptr<BasicState> handleEvent(MountEvent event)
    {
        if (event.target)
        {
            machine.getTarget() = std::move(event.target);
        }

        return std::make_unique<ActivatingState>(machine);
    }

    [[noreturn]] static std::unique_ptr<BasicState>
        handleEvent(UnmountEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);

        throw sdbusplus::exception::SdBusError(
            EPERM, "Operation not permitted in ready state");
    }

    template <class AnyEvent>
    std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);

        return nullptr;
    }
};

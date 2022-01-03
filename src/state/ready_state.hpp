#pragma once

#include "activating_state.hpp"
#include "basic_state.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "utils/log-wrapper.hpp"

#include <sdbusplus/exception.hpp>

#include <memory>
#include <string>
#include <system_error>

struct ReadyState : public BasicStateT<ReadyState>
{
    static std::string_view stateName()
    {
        return "ReadyState";
    }

    ReadyState(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {
        machine.notify();
    }

    ReadyState(interfaces::MountPointStateMachine& machine, const std::errc& ec,
               const std::string& message) :
        BasicStateT(machine)
    {
        LOGGER_ERROR << machine.getName() << " Errno = " << static_cast<int>(ec)
                     << " : " << message;
        machine.notify(std::make_error_code(ec));
    }

    std::unique_ptr<BasicState> onEnter() override
    {
        return nullptr;
    }

    std::unique_ptr<BasicState> handleEvent(MountEvent event)
    {
        machine.notificationStart();
        if (event.target)
        {
            machine.getTarget() = std::move(event.target);
        }

        return std::make_unique<ActivatingState>(machine);
    }

    [[noreturn]] std::unique_ptr<BasicState> handleEvent(UnmountEvent event)
    {
        LOGGER_ERROR << "Invalid event: " << event.eventName;

        throw sdbusplus::exception::SdBusError(
            EPERM, "Operation not permitted in ready state");
    }

    template <class AnyEvent>
    std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR << "Invalid event: " << event.eventName;

        return nullptr;
    }
};

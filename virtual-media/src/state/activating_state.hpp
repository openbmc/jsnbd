#pragma once

#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "resources.hpp"
#include "state/basic_state.hpp"
#include "utils/log-wrapper.hpp"

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <memory>
#include <string_view>

struct ActivatingState : public BasicStateT<ActivatingState>
{
    static std::string_view stateName()
    {
        return "ActivatingState";
    }

    explicit ActivatingState(interfaces::MountPointStateMachine& machine);

    std::unique_ptr<BasicState> onEnter() override;

    std::unique_ptr<BasicState> handleEvent(UdevStateChangeEvent event);
    std::unique_ptr<BasicState> handleEvent(SubprocessStoppedEvent event);

    template <class AnyEvent>
    [[noreturn]] std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);
        throw sdbusplus::exception::SdBusError(EBUSY, "Resource is busy");
    }

  private:
    std::unique_ptr<resource::Process> process;
    std::unique_ptr<resource::Gadget> gadget;
};

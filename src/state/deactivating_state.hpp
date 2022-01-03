#pragma once

#include "basic_state.hpp"
#include "ready_state.hpp"
#include "resources.hpp"

#include <events.hpp>
#include <sdbusplus/exception.hpp>

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>

struct DeactivatingState : public BasicStateT<DeactivatingState>
{
    static std::string_view stateName()
    {
        return "DeactivatingState";
    }

    template <class EventT>
    DeactivatingState(interfaces::MountPointStateMachine& machine,
                      std::unique_ptr<resource::Process> process,
                      std::unique_ptr<resource::Gadget> gadget, EventT event) :
        BasicStateT(machine),
        process(std::move(process)), gadget(std::move(gadget))
    {
        handleEvent(std::move(event));
    }

    DeactivatingState(interfaces::MountPointStateMachine& machine,
                      std::unique_ptr<resource::Process> process,
                      std::unique_ptr<resource::Gadget> gadget) :
        BasicStateT(machine),
        process(std::move(process)), gadget(std::move(gadget))
    {}

    std::unique_ptr<BasicState> onEnter() override
    {
        gadget = nullptr;
        process = nullptr;

        return nullptr;
    }

    std::unique_ptr<BasicState> handleEvent(UdevStateChangeEvent event)
    {
        udevStateChangeEvent = std::move(event);
        return evaluate();
    }

    std::unique_ptr<BasicState> handleEvent(SubprocessStoppedEvent event)
    {
        subprocessStoppedEvent = std::move(event);
        return evaluate();
    }

    template <class AnyEvent>
    [[noreturn]] std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LogMsg(Logger::Error, "Invalid event: ", event.eventName);
        throw sdbusplus::exception::SdBusError(EBUSY, "Resource is busy");
    }

  private:
    std::unique_ptr<BasicState> evaluate()
    {
        if (udevStateChangeEvent && subprocessStoppedEvent)
        {
            if (udevStateChangeEvent->devState == StateChange::removed)
            {
                LogMsg(Logger::Info, machine.getName(),
                       " udev StateChange::removed");
                return std::make_unique<ReadyState>(machine);
            }
            LogMsg(Logger::Error, machine.getName(), " udev StateChange::",
                   static_cast<std::underlying_type_t<StateChange>>(
                       udevStateChangeEvent->devState));
            return std::make_unique<ReadyState>(machine,
                                                std::errc::connection_refused,
                                                "Not expected udev state");
        }
        return nullptr;
    }

    std::unique_ptr<resource::Process> process;
    std::unique_ptr<resource::Gadget> gadget;
    std::optional<UdevStateChangeEvent> udevStateChangeEvent;
    std::optional<SubprocessStoppedEvent> subprocessStoppedEvent;
};

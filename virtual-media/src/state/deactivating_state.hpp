#pragma once

#include "events.hpp"
#include "resources.hpp"
#include "state/basic_state.hpp"
#include "state/ready_state.hpp"
#include "system.hpp"
#include "utils/log-wrapper.hpp"

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

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
        BasicStateT(machine), process(std::move(process)),
        gadget(std::move(gadget))
    {
        handleEvent(std::move(event));
    }

    DeactivatingState(interfaces::MountPointStateMachine& machine,
                      std::unique_ptr<resource::Process> process,
                      std::unique_ptr<resource::Gadget> gadget) :
        BasicStateT(machine), process(std::move(process)),
        gadget(std::move(gadget))
    {}

    std::unique_ptr<BasicState> onEnter() override
    {
        gadget = nullptr;
        process = nullptr;

        return nullptr;
    }

    std::unique_ptr<BasicState> handleEvent(UdevStateChangeEvent event)
    {
        udevStateChangeEvent = event;
        return evaluate();
    }

    std::unique_ptr<BasicState> handleEvent(SubprocessStoppedEvent event)
    {
        subprocessStoppedEvent = event;
        return evaluate();
    }

    template <class AnyEvent>
    [[noreturn]] std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);

        throw sdbusplus::exception::SdBusError(EBUSY, "Resource is busy");
    }

  private:
    std::unique_ptr<BasicState> evaluate()
    {
        if (udevStateChangeEvent && subprocessStoppedEvent)
        {
            if (udevStateChangeEvent->devState == StateChange::removed)
            {
                LOGGER_INFO("{} udev StateCjamge::removed", machine.getName());

                // reset mountPoint manually to stop share from staying
                // mounted after unmounting
                if (machine.getTarget())
                {
                    machine.getTarget()->mountPoint.reset();
                }

                return std::make_unique<ReadyState>(machine);
            }
            LOGGER_ERROR("{} udev StateChange::{}", machine.getName(),
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

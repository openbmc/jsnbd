#pragma once

#include "events.hpp"
#include "resources.hpp"
#include "state/basic_state.hpp"
#include "state/deactivating_state.hpp"
#include "utils/log-wrapper.hpp"

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <memory>
#include <string_view>
#include <utility>

struct ActiveState : public BasicStateT<ActiveState>
{
    static std::string_view stateName()
    {
        return "ActiveState";
    }

    ActiveState(interfaces::MountPointStateMachine& machine,
                std::unique_ptr<resource::Process> process,
                std::unique_ptr<resource::Gadget> gadget) :
        BasicStateT(machine), process(std::move(process)),
        gadget(std::move(gadget))
    {}

    std::unique_ptr<BasicState> onEnter() override
    {
        return nullptr;
    }

    std::unique_ptr<BasicState> handleEvent(UdevStateChangeEvent event)
    {
        return std::make_unique<DeactivatingState>(machine, std::move(process),
                                                   std::move(gadget), event);
    }

    std::unique_ptr<BasicState> handleEvent(SubprocessStoppedEvent event)
    {
        return std::make_unique<DeactivatingState>(machine, std::move(process),
                                                   std::move(gadget), event);
    }

    std::unique_ptr<BasicState> handleEvent([[maybe_unused]] UnmountEvent event)
    {
        return std::make_unique<DeactivatingState>(machine, std::move(process),
                                                   std::move(gadget));
    }

    [[noreturn]] static std::unique_ptr<BasicState>
        handleEvent(MountEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);

        throw sdbusplus::exception::SdBusError(
            EPERM, "Operation not permitted in active state");
    }

    template <class AnyEvent>
    [[noreturn]] std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);

        throw sdbusplus::exception::SdBusError(
            EOPNOTSUPP, "Operation not supported in active state");
    }

  private:
    std::unique_ptr<resource::Process> process;
    std::unique_ptr<resource::Gadget> gadget;
};

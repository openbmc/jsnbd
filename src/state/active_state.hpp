#pragma once

#include "basic_state.hpp"
#include "deactivating_state.hpp"
#include "resources.hpp"

#include <sdbusplus/exception.hpp>

#include <string>
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
        BasicStateT(machine),
        process(std::move(process)), gadget(std::move(gadget))
    {
        machine.notify();
    }

    virtual std::unique_ptr<BasicState> onEnter() override
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
        machine.notificationStart();
        return std::make_unique<DeactivatingState>(machine, std::move(process),
                                                   std::move(gadget));
    }

    [[noreturn]] std::unique_ptr<BasicState> handleEvent(MountEvent event)
    {
        LogMsg(Logger::Error, "Invalid event: ", event.eventName);
        throw sdbusplus::exception::SdBusError(
            EPERM, "Operation not permitted in active state");
    }

    template <class AnyEvent>
    [[noreturn]] std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LogMsg(Logger::Error, "Invalid event: ", event.eventName);
        throw sdbusplus::exception::SdBusError(
            EOPNOTSUPP, "Operation not supported in active state");
    }

  private:
    std::unique_ptr<resource::Process> process;
    std::unique_ptr<resource::Gadget> gadget;
    std::string lastStats;
};

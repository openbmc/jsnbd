#include "activating_state.hpp"

#include "basic_state.hpp"
#include "deactivating_state.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "ready_state.hpp"

#include <memory>
#include <system_error>

ActivatingState::ActivatingState(interfaces::MountPointStateMachine& machine) :
    BasicStateT(machine)
{}

std::unique_ptr<BasicState> ActivatingState::onEnter()
{
    // Reset previous exit code
    machine.getExitCode() = -1;

    return nullptr;
}

std::unique_ptr<BasicState>
    ActivatingState::handleEvent(UdevStateChangeEvent event)
{
    return std::make_unique<DeactivatingState>(machine, std::move(process),
                                               std::move(gadget), event);
}

std::unique_ptr<BasicState>
    ActivatingState::handleEvent([[maybe_unused]] SubprocessStoppedEvent event)
{
    LogMsg(Logger::Error, "Process ended prematurely");
    return std::make_unique<ReadyState>(machine, std::errc::connection_refused,
                                        "Process ended prematurely");
}

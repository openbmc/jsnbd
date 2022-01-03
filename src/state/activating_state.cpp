#include "state/activating_state.hpp"

#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "resources.hpp"
#include "state/active_state.hpp"
#include "state/basic_state.hpp"
#include "state/deactivating_state.hpp"
#include "state/ready_state.hpp"

#include <memory>
#include <system_error>

ActivatingState::ActivatingState(interfaces::MountPointStateMachine& machine) :
    BasicStateT(machine)
{}

std::unique_ptr<BasicState> ActivatingState::onEnter()
{
    // Reset previous exit code
    machine.getExitCode() = -1;

    if (machine.getConfig().mode == Configuration::Mode::proxy)
    {
        return activateProxyMode();
    }
    return nullptr;
}

std::unique_ptr<BasicState>
    ActivatingState::handleEvent(UdevStateChangeEvent event)
{
    if (event.devState == StateChange::inserted)
    {
        gadget = std::make_unique<resource::Gadget>(machine, event.devState);
        return std::make_unique<ActiveState>(machine, std::move(process),
                                             std::move(gadget));
    }

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
std::unique_ptr<BasicState> ActivatingState::activateProxyMode()
{
    process = std::make_unique<resource::Process>(
        machine, std::make_shared<::Process>(
                     machine.getIOC(), machine.getName(),
                     "/usr/sbin/nbd-client", machine.getConfig().nbdDevice));

    if (!process->spawn(Configuration::MountPoint::toArgs(machine.getConfig()),
                        [&machine = machine](int exitCode) {
                            LogMsg(Logger::Info, machine.getName(),
                                   " process ended.");
                            machine.getExitCode() = exitCode;
                            machine.emitSubprocessStoppedEvent();
                        }))
    {
        return std::make_unique<ReadyState>(
            machine, std::errc::operation_canceled, "Failed to spawn process");
    }

    return nullptr;
}


#include "resources.hpp"

#include "interfaces/mount_point_state_machine.hpp"
#include "system.hpp"
#include "utils/log-wrapper.hpp"

#include <boost/asio/post.hpp>

#include <exception>
#include <string>

namespace resource
{

Process::~Process()
{
    if (spawned)
    {
        try
        {
            process->stop([&machine = *machine] {
                boost::asio::post(machine.getIOC(), [&machine]() {
                    machine.emitSubprocessStoppedEvent();
                });
            });
        }
        catch (const std::exception& e)
        {
            LOGGER_ERROR("Problem with stopping nbd process: {}", e.what());
        }
    }
}

Gadget::Gadget(interfaces::MountPointStateMachine& machine,
               StateChange devState) : machine(&machine)
{
    status = UsbGadget::configure(
        std::string(machine.getName()), machine.getConfig().nbdDevice, devState,
        machine.getTarget() ? machine.getTarget()->rw : false);
}

Gadget::~Gadget()
{
    UsbGadget::configure(std::string(machine->getName()),
                         machine->getConfig().nbdDevice, StateChange::removed);
}

} // namespace resource


#include "resources.hpp"

#include "interfaces/mount_point_state_machine.hpp"
#include "system.hpp"

#include <boost/asio/post.hpp>

#include <string>

namespace resource
{

Process::~Process()
{
    if (spawned)
    {
        process->stop([&machine = *machine] {
            boost::asio::post(machine.getIOC(), [&machine]() {
                machine.emitSubprocessStoppedEvent();
            });
        });
    }
}

Gadget::Gadget(interfaces::MountPointStateMachine& machine,
               StateChange devState) :
    machine(&machine)
{
    status = UsbGadget::configure(
        std::string(machine.getName()), machine.getConfig().nbdDevice, devState,
        machine.getTarget() ? machine.getTarget()->rw : false);
}

Gadget::~Gadget()
{
    int32_t ret = UsbGadget::configure(std::string(machine->getName()),
                                       machine->getConfig().nbdDevice,
                                       StateChange::removed);
    if (ret != 0)
    {
        // This shouldn't ever happen, perhaps best is to restart the app
        LogMsg(Logger::Critical, machine->getName(),
               " Some serious failure happened!");

        boost::asio::post(machine->getIOC(), [&machine = *machine]() {
            machine.emitUdevStateChangeEvent(machine.getConfig().nbdDevice,
                                             StateChange::unknown);
        });
    }
}

} // namespace resource

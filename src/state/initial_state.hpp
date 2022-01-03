#pragma once

#include "configuration.hpp"
#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "logger.hpp"
#include "state/active_state.hpp"
#include "state/basic_state.hpp"
#include "state/ready_state.hpp"
#include "system.hpp"

#include <boost/asio/spawn.hpp>
#include <sdbusplus/message.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

struct InitialState : public BasicStateT<InitialState>
{
    static std::string_view stateName()
    {
        return "InitialState";
    }

    InitialState(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}

    std::unique_ptr<BasicState> handleEvent(const RegisterDbusEvent& event)
    {
        const bool isLegacy =
            (machine.getConfig().mode == Configuration::Mode::legacy);

        addMountPointInterface(event);
        addProcessInterface(event);
        addServiceInterface(event, isLegacy);

        UdevGadget::forceUdevChange();

        return std::make_unique<ReadyState>(machine);
    }

    template <class AnyEvent>
    std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LogMsg(Logger::Error, "Invalid event: ", event.eventName);
        return nullptr;
    }

  private:
    static std::string
        getObjectPath(interfaces::MountPointStateMachine& machine)
    {
        LogMsg(Logger::Debug, "getObjectPath entry()");
        if (machine.getConfig().mode == Configuration::Mode::proxy)
        {
            return "/xyz/openbmc_project/VirtualMedia/Proxy/";
        }
        return "/xyz/openbmc_project/VirtualMedia/Legacy/";
    }

    void addProcessInterface(const RegisterDbusEvent& event)
    {
        std::string objPath = getObjectPath(machine);

        auto processIface = event.objServer->add_interface(
            objPath + std::string(machine.getName()),
            "xyz.openbmc_project.VirtualMedia.Process");

        processIface->register_property(
            "Active", bool(false),
            []([[maybe_unused]] const bool& req,
               [[maybe_unused]] bool& property) { return 0; },
            [&machine = machine]([[maybe_unused]] const bool& property)
                -> bool { return machine.getState().get_if<ActiveState>(); });
        processIface->register_property(
            "ExitCode", int32_t(0),
            []([[maybe_unused]] const int32_t& req,
               [[maybe_unused]] int32_t& property) { return 0; },
            [&machine = machine]([[maybe_unused]] const int32_t& property) {
                return machine.getExitCode();
            });
        processIface->initialize();
    }

    void addMountPointInterface(const RegisterDbusEvent& event)
    {
        std::string objPath = getObjectPath(machine);

        auto iface = event.objServer->add_interface(
            objPath + std::string(machine.getName()),
            "xyz.openbmc_project.VirtualMedia.MountPoint");
        iface->register_property("Device",
                                 machine.getConfig().nbdDevice.toString());
        iface->register_property("EndpointId", machine.getConfig().endPointId);
        iface->register_property("Socket", machine.getConfig().unixSocket);
        iface->register_property(
            "ImageURL", std::string(),
            []([[maybe_unused]] const std::string& req,
               [[maybe_unused]] std::string& property) {
                throw sdbusplus::exception::SdBusError(
                    EPERM, "Setting ImageURL property is not allowed");
                return -1;
            },
            [&target = machine.getTarget()](
                [[maybe_unused]] const std::string& property) {
                return std::string();
            });
        iface->register_property(
            "WriteProtected", bool(true),
            []([[maybe_unused]] const bool& req,
               [[maybe_unused]] bool& property) { return 0; },
            [&target =
                 machine.getTarget()]([[maybe_unused]] const bool& property) {
                if (target)
                {
                    return !target->rw;
                }
                return bool(true);
            });
        iface->register_property(
            "Timeout", machine.getConfig().timeout.value_or(
                           Configuration::MountPoint::defaultTimeout));
        iface->register_property(
            "RemainingInactivityTimeout", 0,
            []([[maybe_unused]] const int& req,
               [[maybe_unused]] int& property) {
                throw sdbusplus::exception::SdBusError(
                    EPERM, "Setting RemainingInactivityTimeout property is "
                           "not allowed");
                return -1;
            },
            [&config = machine.getConfig()](
                [[maybe_unused]] const int& property) -> int {
                return static_cast<int>(
                    config.remainingInactivityTimeout.count());
            });
        iface->initialize();
    }

    void addServiceInterface(const RegisterDbusEvent& event,
                             const bool isLegacy)
    {
        const std::string name = "xyz.openbmc_project.VirtualMedia." +
                                 std::string(isLegacy ? "Legacy" : "Proxy");

        const std::string path =
            getObjectPath(machine) + std::string(machine.getName());

        auto iface = event.objServer->add_interface(path, name);

        iface->register_signal<int32_t>("Completion");
        machine.notificationInitialize(event.bus, path, name, "Completion");

        // Common unmount
        iface->register_method("Unmount", [&machine = machine]() {
            LogMsg(Logger::Info, "[App]: Unmount called on ",
                   machine.getName());

            machine.emitUnmountEvent();

            return true;
        });

        // Mount specialization
        if (isLegacy)
        {
            using sdbusplus::message::unix_fd;
            using optional_fd = std::variant<int, unix_fd>;

            iface->register_method(
                "Mount",
                [&machine = machine]([[maybe_unused]] const std::string& imgUrl,
                                     [[maybe_unused]] bool rw,
                                     [[maybe_unused]] optional_fd fd) {
                    LogMsg(Logger::Info, "[App]: Mount called on ",
                           getObjectPath(machine), machine.getName());

                    return false;
                });
        }
        else // proxy
        {
            iface->register_method("Mount", [&machine = machine]() mutable {
                LogMsg(Logger::Info, "[App]: Mount called on ",
                       getObjectPath(machine), machine.getName());

                machine.emitMountEvent(std::nullopt);

                return true;
            });
        }

        iface->initialize();
    }
};

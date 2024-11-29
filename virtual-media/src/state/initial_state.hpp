#pragma once

#include "basic_state.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "utils/log-wrapper.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/message/types.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

struct InitialState : public BasicStateT<InitialState>
{
    static std::string_view stateName()
    {
        return "InitialState";
    }

    explicit InitialState(interfaces::MountPointStateMachine& machine) :
        BasicStateT(machine)
    {}

    std::unique_ptr<BasicState> handleEvent(const RegisterDbusEvent& event)
    {
        const bool isStandard =
            (machine.getConfig().mode == Configuration::Mode::standard);

        addMountPointInterface(event);
        addProcessInterface(event);
        addServiceInterface(event, isStandard);

        return nullptr;
    }

    template <class AnyEvent>
    std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);

        return nullptr;
    }

  private:
    static std::string
        getObjectPath(interfaces::MountPointStateMachine& machine)
    {
        if (machine.getConfig().mode == Configuration::Mode::proxy)
        {
            return "/xyz/openbmc_project/VirtualMedia/Proxy/";
        }

        return "/xyz/openbmc_project/VirtualMedia/Standard/";
    }

    void addProcessInterface(const RegisterDbusEvent& event)
    {
        std::string objPath = getObjectPath(machine);

        auto processIface = event.objServer->add_unique_interface(
            objPath + std::string(machine.getName()),
            "xyz.openbmc_project.VirtualMedia.Process");

        processIface->register_property_r<bool>(
            "Active", sdbusplus::vtable::property_::emits_change,
            [](const auto&) { return false; });
        processIface->register_property_r<int32_t>(
            "ExitCode", sdbusplus::vtable::property_::emits_change,
            [&machine = machine](const auto&) {
            return machine.getExitCode();
        });
        processIface->initialize();

        machine.setProcessInterface(std::move(processIface));
    }

    void addMountPointInterface(const RegisterDbusEvent& event)
    {
        std::string objPath = getObjectPath(machine);

        auto iface = event.objServer->add_unique_interface(
            objPath + std::string(machine.getName()),
            "xyz.openbmc_project.VirtualMedia.MountPoint");
        iface->register_property_r<std::string>(
            "Device", sdbusplus::vtable::property_::const_,
            [&machine = machine](const auto&) {
            return machine.getConfig().nbdDevice.toString();
        });
        iface->register_property_r<std::string>(
            "EndpointId", sdbusplus::vtable::property_::const_,
            [&machine = machine](const auto&) {
            return machine.getConfig().endPointId;
        });
        iface->register_property_r<std::string>(
            "Socket", sdbusplus::vtable::property_::const_,
            [&machine = machine](const auto&) {
            return machine.getConfig().unixSocket;
        });
        iface->register_property_r<std::string>(
            "ImageURL", sdbusplus::vtable::property_::emits_change,
            [&target = machine.getTarget()](const auto&) {
            return std::string();
        });
        iface->register_property_r<bool>(
            "WriteProtected", sdbusplus::vtable::property_::emits_change,
            [&target = machine.getTarget()](const auto&) { return true; });
        iface->register_property_r<uint64_t>(
            "Timeout", sdbusplus::vtable::property_::const_,
            [](const auto&) { return Configuration::MountPoint::timeout; });
        iface->initialize();

        machine.setMountPointInterface(std::move(iface));
    }

    void addServiceInterface(const RegisterDbusEvent& event,
                             const bool isStandard)
    {
        const std::string name = "xyz.openbmc_project.VirtualMedia." +
                                 std::string(isStandard ? "Standard" : "Proxy");

        const std::string path = getObjectPath(machine) +
                                 std::string(machine.getName());

        auto iface = event.objServer->add_unique_interface(path, name);

        iface->register_signal<int32_t>("Completion");

        // Common unmount
        iface->register_method("Unmount", [&machine = machine]() {
            LOGGER_INFO("[App]: Unmount called on {}", machine.getName());

            return false;
        });

        // Mount specialization
        if (isStandard)
        {
            using optional_fd = std::variant<int, sdbusplus::message::unix_fd>;

            iface->register_method(
                "Mount",
                [&machine = machine]([[maybe_unused]] const std::string& imgUrl,
                                     [[maybe_unused]] bool rw,
                                     [[maybe_unused]] optional_fd fd) {
                LOGGER_INFO("[App]: Mount called on {} {}",
                            getObjectPath(machine), machine.getName());

                return false;
            });
        }
        else // proxy
        {
            iface->register_method("Mount", [&machine = machine]() mutable {
                LOGGER_INFO("[App]: Mount called on {} {}",
                            getObjectPath(machine), machine.getName());

                return false;
            });
        }

        iface->initialize();

        machine.setServiceInterface(std::move(iface));
    }
};

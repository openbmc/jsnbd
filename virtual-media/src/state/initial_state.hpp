#pragma once

#include "configuration.hpp"
#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "state/active_state.hpp"
#include "state/basic_state.hpp"
#include "state/ready_state.hpp"
#include "system.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <sys/mount.h>
#include <unistd.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <sdbusplus/vtable.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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

        if (isStandard)
        {
            cleanUpMountPoint();
        }
        addMountPointInterface(event);
        addProcessInterface(event);
        addServiceInterface(event, isStandard);

        return std::make_unique<ReadyState>(machine);
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
            [&machine = machine](const bool&) {
                return machine.getState().getIf<ActiveState>();
            });
        processIface->register_property_r<int32_t>(
            "ExitCode", sdbusplus::vtable::property_::emits_change,
            [&machine = machine](const auto&) {
                return machine.getExitCode();
            });
        processIface->initialize();

        machine.setProcessInterface(std::move(processIface));
    }

    void cleanUpMountPoint()
    {
        if (UsbGadget::isConfigured(std::string(machine.getName())))
        {
            int result = UsbGadget::configure(std::string(machine.getName()),
                                              machine.getConfig().nbdDevice,
                                              StateChange::removed);
            LOGGER_INFO("UsbGadget cleanup");

            if (result != 0)
            {
                LOGGER_CRITICAL(
                    "{} Some serious failure happened! Cleanup failed.",
                    machine.getName());
            }
        }

        auto localFile = std::filesystem::temp_directory_path() /
                         std::string(machine.getName());

        if (std::filesystem::exists(localFile))
        {
            if (0 == ::umount2(localFile.c_str(), MNT_FORCE))
            {
                LOGGER_INFO("Cleanup directory {}", localFile.string());
                std::error_code ec;
                if (!std::filesystem::remove(localFile, ec))
                {
                    LOGGER_ERROR(
                        "{} Cleanup failed - unable to remove directory {}",
                        ec.message(), localFile.string());
                }

                return;
            }

            LOGGER_ERROR("Cleanup failed - unable to unmount directory {}",
                         localFile.string());
        }
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
                if (target)
                {
                    return target->imgUrl;
                }
                return std::string();
            });
        iface->register_property_r<bool>(
            "WriteProtected", sdbusplus::vtable::property_::emits_change,
            [&target = machine.getTarget()](const auto&) {
                if (target)
                {
                    return !target->rw;
                }
                return true;
            });
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

        const std::string path =
            getObjectPath(machine) + std::string(machine.getName());

        auto iface = event.objServer->add_unique_interface(path, name);

        iface->register_signal<int32_t>("Completion");
        machine.notificationInitialize(event.bus, path, name, "Completion");

        // Common unmount
        iface->register_method("Unmount", [&machine = machine]() {
            LOGGER_INFO("[App]: Unmount called on {}", machine.getName());

            machine.emitUnmountEvent();

            LOGGER_DEBUG("[App]: Unmount returns true");
            return true;
        });

        // Mount specialization
        if (isStandard)
        {
            using optional_fd = std::variant<int, sdbusplus::message::unix_fd>;

            iface->register_method(
                "Mount", [&machine = machine](boost::asio::yield_context yield,
                                              const std::string& imgUrl,
                                              bool rw, optional_fd fd) {
                    LOGGER_INFO("[App]: Mount called on {} {}",
                                getObjectPath(machine), machine.getName());

                    interfaces::MountPointStateMachine::Target target = {
                        imgUrl, rw, nullptr, nullptr};

                    if (std::holds_alternative<sdbusplus::message::unix_fd>(fd))
                    {
                        LOGGER_DEBUG("[App] Extra data available");

                        // Open pipe and prepare output buffer
                        boost::asio::posix::stream_descriptor secretPipe(
                            machine.getIOC(),
                            dup(std::get<sdbusplus::message::unix_fd>(fd).fd));
                        std::array<char, utils::secretLimit> buf{};

                        // Read data
                        auto size = secretPipe.async_read_some(
                            boost::asio::buffer(buf), yield);

                        // Validate number of NULL delimiters, ensures
                        // further operations are safe
                        auto nullCount =
                            std::count(buf.begin(), buf.begin() + size, '\0');
                        if (nullCount != 2)
                        {
                            throw sdbusplus::exception::SdBusError(
                                EINVAL, "Malformed extra data");
                        }

                        // First 'part' of payload
                        std::string user(buf.begin());
                        // Second 'part', after NULL delimiter
                        std::string pass(buf.begin() + user.length() + 1);

                        // Encapsulate credentials into safe buffer
                        target.credentials =
                            std::make_unique<utils::CredentialsProvider>(
                                std::move(user), std::move(pass));

                        // Cover the tracks
                        utils::secureCleanup(buf);
                    }

                    machine.emitMountEvent(std::move(target));

                    return true;
                });
        }
        else // proxy
        {
            iface->register_method("Mount", [&machine = machine]() mutable {
                LOGGER_INFO("[App]: Mount called on {} {}",
                            getObjectPath(machine), machine.getName());

                machine.emitMountEvent(std::nullopt);

                return true;
            });
        }

        iface->initialize();

        machine.setServiceInterface(std::move(iface));
    }
};

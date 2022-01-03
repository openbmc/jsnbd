#pragma once

#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "resources.hpp"
#include "state/basic_state.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <sdbusplus/exception.hpp>

#include <cerrno>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ActivatingState : public BasicStateT<ActivatingState>
{
    static std::string_view stateName()
    {
        return "ActivatingState";
    }

    explicit ActivatingState(interfaces::MountPointStateMachine& machine);

    std::unique_ptr<BasicState> onEnter() override;

    std::unique_ptr<BasicState> handleEvent(UdevStateChangeEvent event);
    std::unique_ptr<BasicState> handleEvent(SubprocessStoppedEvent event);

    template <class AnyEvent>
    [[noreturn]] std::unique_ptr<BasicState> handleEvent(AnyEvent event)
    {
        LOGGER_ERROR("Invalid event: {}", event.eventName);
        throw sdbusplus::exception::SdBusError(EBUSY, "Resource is busy");
    }

  private:
    std::unique_ptr<BasicState> activateProxyMode();
    std::unique_ptr<BasicState> activateStandardMode();
    std::unique_ptr<BasicState> mountSmbShare();
    std::unique_ptr<BasicState> mountHttpsShare();

    static std::unique_ptr<resource::Process>
        spawnNbdKit(interfaces::MountPointStateMachine& machine,
                    std::unique_ptr<utils::VolatileFile<>>&& secret,
                    const std::vector<std::string>& params);
    static std::unique_ptr<resource::Process>
        spawnNbdKit(interfaces::MountPointStateMachine& machine,
                    const std::filesystem::path& file);
    static std::unique_ptr<resource::Process> spawnNbdKit(
        interfaces::MountPointStateMachine& machine, const std::string& url);

    static bool checkUrl(const std::string& urlScheme,
                         const std::string& imageUrl);
    static bool getImagePathFromUrl(const std::string& urlScheme,
                                    const std::string& imageUrl,
                                    std::string* imagePath);
    static bool isHttpsUrl(const std::string& imageUrl);
    static bool getImagePathFromHttpsUrl(const std::string& imageUrl,
                                         std::string* imagePath);

    static bool isCifsUrl(const std::string& imageUrl);
    static bool getImagePathFromCifsUrl(const std::string& imageUrl,
                                        std::string* imagePath);
    static std::filesystem::path getImagePath(const std::string& imageUrl);

    std::unique_ptr<resource::Process> process;
    std::unique_ptr<resource::Gadget> gadget;
};

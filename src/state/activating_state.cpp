#include "state/activating_state.hpp"

#include "configuration.hpp"
#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "resources.hpp"
#include "smb.hpp"
#include "state/active_state.hpp"
#include "state/basic_state.hpp"
#include "state/deactivating_state.hpp"
#include "state/ready_state.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/utils.hpp"

#include <boost/algorithm/string/join.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>
#include <vector>

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
    if (machine.getConfig().mode == Configuration::Mode::legacy)
    {
        return activateLegacyMode();
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
    LOGGER_ERROR << "Process ended prematurely";
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
                            LOGGER_INFO << machine.getName()
                                        << " process ended.";
                            machine.getExitCode() = exitCode;
                            machine.emitSubprocessStoppedEvent();
                        }))
    {
        return std::make_unique<ReadyState>(
            machine, std::errc::operation_canceled, "Failed to spawn process");
    }

    return nullptr;
}

std::unique_ptr<BasicState> ActivatingState::activateLegacyMode()
{
    if (!machine.getTarget())
    {
        LOGGER_ERROR << machine.getName()
                     << " Trying to mount unconfigured target";

        return std::make_unique<ReadyState>(
            machine, std::errc::invalid_argument,
            "Attempted to use unconfigured target");
    }

    LOGGER_INFO << machine.getName() << " Mount requested on address: "
                << machine.getTarget()->imgUrl << "; RW: ",
        machine.getTarget()->rw;

    std::filesystem::path socketPath(machine.getConfig().unixSocket);
    if (!std::filesystem::exists(socketPath.parent_path()))
    {
        LOGGER_INFO << machine.getName()
                    << " Parent path for the socket does not exist << "
                    << socketPath.parent_path();

        std::error_code errc;
        std::filesystem::create_directories(socketPath.parent_path(), errc);
        if (errc)
        {
            LOGGER_ERROR << machine.getName()
                         << " Failed to create parent directory for socket"
                         << errc;

            return std::make_unique<ReadyState>(
                machine, static_cast<std::errc>(errc.value()),
                "Failed to create parent directory for socket");
        }

        std::filesystem::permissions(socketPath.parent_path(),
                                     std::filesystem::perms::owner_all, errc);
        if (errc)
        {
            LOGGER_INFO
                << machine.getName()
                << " Failed to set parent directory permissions for socket"
                << errc;

            return std::make_unique<ReadyState>(
                machine, static_cast<std::errc>(errc.value()),
                "Failed to set parent permissions directory for socket");
        }
    }

    if (isCifsUrl(machine.getTarget()->imgUrl))
    {
        return mountSmbShare();
    }

    return std::make_unique<ReadyState>(machine, std::errc::invalid_argument,
                                        "URL not recognized");
}

std::unique_ptr<BasicState> ActivatingState::mountSmbShare()
{
    try
    {
        auto mountDir =
            std::make_unique<resource::Directory>(machine.getName());

        SmbShare smb(mountDir->getPath());
        std::filesystem::path remote =
            getImagePath(machine.getTarget()->imgUrl);
        auto remoteParent = "/" + remote.parent_path().string();
        auto localFile = mountDir->getPath() / remote.filename();

        LOGGER_INFO << machine.getName() << " Remote name: " << remote
                    << "\n Remote parent: " << remoteParent
                    << "\n Local file: " << localFile;

        machine.getTarget()->mountPoint = std::make_unique<resource::Mount>(
            std::move(mountDir), smb, remoteParent, machine.getTarget()->rw,
            machine.getTarget()->credentials);

        process = spawnNbdKit(machine, localFile);
        if (!process)
        {
            return std::make_unique<ReadyState>(machine,
                                                std::errc::operation_canceled,
                                                "Unable to setup NbdKit");
        }

        return nullptr;
    }
    catch (const resource::Error& e)
    {
        return std::make_unique<ReadyState>(machine, e.errorCode, e.what());
    }
}

std::unique_ptr<resource::Process>
    ActivatingState::spawnNbdKit(interfaces::MountPointStateMachine& machine,
                                 const std::vector<std::string>& params)
{
    // Investigate
    auto process = std::make_unique<resource::Process>(
        machine, std::make_shared<::Process>(
                     machine.getIOC(), std::string(machine.getName()),
                     "/usr/sbin/nbdkit", machine.getConfig().nbdDevice));

    // Cleanup of previous socket
    if (std::filesystem::exists(machine.getConfig().unixSocket))
    {
        LOGGER_DEBUG << machine.getName()
                     << " Removing previously mounted socket: "
                     << machine.getConfig().unixSocket;
        if (!std::filesystem::remove(machine.getConfig().unixSocket))
        {
            LOGGER_ERROR << machine.getName()
                         << " Unable to remove pre-existing socket :"
                         << machine.getConfig().unixSocket;

            return {};
        }
    }

    std::string nbdClient =
        "/usr/sbin/nbd-client " +
        boost::algorithm::join(
            Configuration::MountPoint::toArgs(machine.getConfig()), " ");

    std::vector<std::string> args = {
        // Listen for client on this unix socket...
        "--unix",
        machine.getConfig().unixSocket,

        // ... then connect nbd-client to served image
        "--run",
        nbdClient,

#if VM_VERBOSE_NBDKIT_LOGS
        "--verbose", // swarm of debug logs - only for brave souls
#endif
    };

    if (!machine.getTarget()->rw)
    {
        args.emplace_back("--readonly");
    }

    // Insert extra params
    args.insert(args.end(), params.begin(), params.end());

    if (!process->spawn(args, [&machine = machine](int exitCode) {
            LOGGER_INFO << machine.getName() << " process ended.";
            machine.getExitCode() = exitCode;
            machine.emitSubprocessStoppedEvent();
        }))
    {
        LOGGER_ERROR << machine.getName()
                     << " Failed to spawn Process for: " << machine.getName();

        return {};
    }

    return process;
}

std::unique_ptr<resource::Process>
    ActivatingState::spawnNbdKit(interfaces::MountPointStateMachine& machine,
                                 const std::filesystem::path& file)
{
    return spawnNbdKit(machine,
                       {// Use file plugin ...
                        "file",
                        // ... to mount file at this location
                        "file=" + file.string()});
}

bool ActivatingState::checkUrl(const std::string& urlScheme,
                               const std::string& imageUrl)
{
    return (urlScheme.compare(imageUrl.substr(0, urlScheme.size())) == 0);
}

bool ActivatingState::getImagePathFromUrl(const std::string& urlScheme,
                                          const std::string& imageUrl,
                                          std::string* imagePath)
{
    if (checkUrl(urlScheme, imageUrl))
    {
        if (imagePath != nullptr)
        {
            *imagePath = imageUrl.substr(urlScheme.size() - 1);
            return true;
        }

        LOGGER_ERROR << "Invalid parameter provied";

        return false;
    }

    LOGGER_ERROR << "Provided url does not match scheme";

    return false;
}

bool ActivatingState::isCifsUrl(const std::string& imageUrl)
{
    return checkUrl("smb://", imageUrl);
}

bool ActivatingState::getImagePathFromCifsUrl(const std::string& imageUrl,
                                              std::string* imagePath)
{
    return getImagePathFromUrl("smb://", imageUrl, imagePath);
}

std::filesystem::path ActivatingState::getImagePath(const std::string& imageUrl)
{
    std::string imagePath;

    if (isCifsUrl(imageUrl) && getImagePathFromCifsUrl(imageUrl, &imagePath))
    {
        return {imagePath};
    }

    LOGGER_ERROR << "Unrecognized url's scheme encountered";

    return {};
}

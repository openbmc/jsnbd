#pragma once

#include "system.hpp"
#include "utils/log-wrapper.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace interfaces
{
struct MountPointStateMachine;
}

namespace resource
{

class Error : public std::runtime_error
{
  public:
    Error(std::errc errorCode, const std::string& message) :
        std::runtime_error(message), errorCode(errorCode)
    {}

    const std::errc errorCode;
};

class Directory
{
  public:
    Directory() = delete;
    Directory(const Directory&) = delete;
    Directory(Directory&& other) = delete;
    Directory& operator=(const Directory&) = delete;
    Directory& operator=(Directory&& other) = delete;

    explicit Directory(const std::filesystem::path& name) :
        path(std::filesystem::temp_directory_path() / name)
    {
        std::error_code ec;

        if (!std::filesystem::create_directory(path, ec))
        {
            LOGGER_ERROR("{} : Unable to create mount directory: {}",
                         ec.message(), path.string());
            throw Error(std::errc::io_error,
                        "Failed to create mount directory");
        }
    }

    ~Directory()
    {
        std::error_code ec;

        if (!std::filesystem::remove(path, ec))
        {
            LOGGER_ERROR("{} : Unable to remove directory {}", ec.message(),
                         path.string());
        }
    }

    std::filesystem::path getPath() const
    {
        return path;
    }

  private:
    std::filesystem::path path;
};

class Mount
{
  public:
    Mount() = delete;
    Mount(const Mount&) = delete;
    Mount(Mount&& other) = delete;
    Mount& operator=(const Mount&) = delete;
    Mount& operator=(Mount&& other) = delete;

    explicit Mount(std::unique_ptr<Directory> directory) :
        directory(std::move(directory))
    {}

    ~Mount() = default;

    std::filesystem::path getPath() const
    {
        return directory->getPath();
    }

  private:
    std::unique_ptr<Directory> directory;
};

class Process
{
  public:
    Process() = delete;
    Process(const Process&) = delete;
    Process(Process&& other) = delete;
    Process& operator=(const Process&) = delete;
    Process& operator=(Process&& other) = delete;
    Process(interfaces::MountPointStateMachine& machine,
            std::shared_ptr<::Process> process) :
        machine(&machine), process(std::move(process))
    {
        if (!this->process)
        {
            throw Error(std::errc::io_error, "Failed to create process");
        }
    }

    ~Process();

    template <class... Args>
    auto spawn(Args&&... args)
    {
        if (process->spawn(std::forward<Args>(args)...))
        {
            spawned = true;
            return true;
        }
        return false;
    }

  private:
    interfaces::MountPointStateMachine* machine;
    std::shared_ptr<::Process> process = nullptr;
    bool spawned = false;
};

class Gadget
{
  public:
    Gadget() = delete;
    Gadget& operator=(const Gadget&) = delete;
    Gadget& operator=(Gadget&& other) = delete;
    Gadget(const Gadget&) = delete;
    Gadget(Gadget&& other) = delete;

    Gadget(interfaces::MountPointStateMachine& machine, StateChange devState);
    ~Gadget();

  private:
    interfaces::MountPointStateMachine* machine;
    int32_t status;
};

} // namespace resource

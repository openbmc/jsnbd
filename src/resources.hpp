#pragma once

#include "system.hpp"

#include <sys/mount.h>

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
            LogMsg(Logger::Error, ec,
                   " : Unable to create mount directory: ", path);
            throw Error(std::errc::io_error,
                        "Failed to create mount directory");
        }
    }

    ~Directory()
    {
        std::error_code ec;

        if (!std::filesystem::remove(path, ec))
        {
            LogMsg(Logger::Error, ec, " : Unable to remove directory ", path);
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

} // namespace resource

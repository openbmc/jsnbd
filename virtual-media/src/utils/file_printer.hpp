#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace utils
{

struct FilePrinter
{
    static void createDirs(const std::filesystem::path& path);
    static void createDirSymlink(const std::filesystem::path& target,
                                 const std::filesystem::path& link);
    static bool echo(const std::filesystem::path& path,
                     const std::string& content);
    static bool exists(const std::filesystem::path& path);
    static bool isDir(const std::filesystem::path& path);
    static bool isSymlink(const std::filesystem::path& path);
    static void remove(const std::filesystem::path& path, std::error_code& ec);
};

} // namespace utils

#include "utils/file_printer.hpp"

#include "utils/log-wrapper.hpp"

#include <filesystem>
#include <fstream>

namespace utils
{

void FilePrinter::createDirs(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path);
}

void FilePrinter::createDirSymlink(const std::filesystem::path& target,
                                   const std::filesystem::path& link)
{
    std::filesystem::create_directory_symlink(target, link);
}

bool FilePrinter::echo(const std::filesystem::path& path,
                       const std::string& content)
{
    std::ofstream fileWriter;
    fileWriter.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    fileWriter.open(path, std::ios::out | std::ios::app);
    fileWriter << content << std::endl;
    // Make sure for new line and flush
    fileWriter.close();
    LOGGER_DEBUG << "echo " << content << " > " << path;

    return true;
}

bool FilePrinter::exists(const std::filesystem::path& path)
{
    return std::filesystem::exists(path);
}

bool FilePrinter::isDir(const std::filesystem::path& path)
{
    return std::filesystem::is_directory(path);
}

bool FilePrinter::isSymlink(const std::filesystem::path& path)
{
    return std::filesystem::is_symlink(path);
}

void FilePrinter::remove(const std::filesystem::path& path, std::error_code& ec)
{
    std::filesystem::remove(path, ec);
}

} // namespace utils

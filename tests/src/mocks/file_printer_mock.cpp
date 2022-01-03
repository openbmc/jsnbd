#include "mocks/file_printer_mock.hpp"

#include "utils/file_printer.hpp"

MockFilePrinterEngine* MockFilePrinter::engine = nullptr;

namespace utils
{

void FilePrinter::createDirs(const fs::path& path)
{
    MockFilePrinter::createDirs(path);
}

void FilePrinter::createDirSymlink(const fs::path& target, const fs::path& link)
{
    MockFilePrinter::createDirSymlink(target, link);
}

bool FilePrinter::echo(const fs::path& path, const std::string& content)
{
    return MockFilePrinter::echo(path, content);
}

bool FilePrinter::exists(const fs::path& path)
{
    return MockFilePrinter::exists(path);
}

bool FilePrinter::isDir(const fs::path& path)
{
    return MockFilePrinter::isDir(path);
}

bool FilePrinter::isSymlink(const fs::path& path)
{
    return MockFilePrinter::isSymlink(path);
}

void FilePrinter::remove(const fs::path& path, std::error_code& ec)
{
    MockFilePrinter::remove(path, ec);
}

} // namespace utils

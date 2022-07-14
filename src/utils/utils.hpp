#pragma once

#include "utils/log-wrapper.hpp"

#include <sys/ioctl.h>

#include <boost/beast/core/file_base.hpp>
#include <boost/beast/core/file_posix.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace utils
{

class FileObject
{
  public:
    explicit FileObject(int fd)
    {
        file.native_handle(fd);
    }

    FileObject(const std::string& filename, boost::beast::file_mode mode)
    {
        boost::system::error_code ec;
        file.open(filename.c_str(), mode, ec);
        if (ec)
        {
            throw std::filesystem::filesystem_error("Couldn't open file",
                                                    std::error_code(ec));
        }
    }

    ~FileObject()
    {
        boost::system::error_code ec;
        file.close(ec);
        if (ec)
        {
            LOGGER_ERROR("Problem with file close: {}", ec);
        }
    }

    FileObject() = delete;
    FileObject(const FileObject&) = delete;
    FileObject& operator=(const FileObject&) = delete;
    FileObject(FileObject&&) = delete;
    FileObject& operator=(FileObject&&) = delete;

    int ioctl(unsigned long request) const
    {
        // Ignore C-style vararg usage in ioctl function. It's system method,
        // which doesn't seem to have any reasonable replacement either in STL
        // or Boost.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        return ::ioctl(file.native_handle(), request);
    }

    std::optional<std::string> readContent()
    {
        boost::system::error_code ec;
        uint64_t size = file.size(ec);
        if (ec)
        {
            LOGGER_ERROR("Problem with getting file size: {}", ec);
            return {};
        }

        std::vector<char> readBuffer;
        readBuffer.reserve(size);
        size_t charsRead = file.read(readBuffer.data(), size, ec);
        if (ec)
        {
            LOGGER_ERROR("Problem with file read: {}", ec);
            return {};
        }
        if (charsRead != size)
        {
            LOGGER_ERROR("Read data size doesn't match file size");
            return {};
        }

        return std::string(readBuffer.data(), charsRead);
    }

    size_t write(void* data, size_t nw)
    {
        boost::system::error_code ec;
        size_t written = file.write(data, nw, ec);
        if (ec)
        {
            LOGGER_ERROR("Problem with file write: {}", ec);
            return 0;
        }
        return written;
    }

  private:
    boost::beast::file_posix file;
};

} // namespace utils

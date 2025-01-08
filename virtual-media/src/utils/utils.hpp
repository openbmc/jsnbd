#pragma once

#include "utils/log-wrapper.hpp"

#include <stdlib.h>
#include <sys/ioctl.h>

#include <boost/beast/core/file_base.hpp>
#include <boost/beast/core/file_posix.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace utils
{

constexpr const size_t secretLimit = 1024;

template <typename T>
static void secureCleanup(T& value)
{
    auto raw = const_cast<typename T::value_type*>(&value[0]);
    explicit_bzero(raw, value.size() * sizeof(*raw));
}

class Credentials
{
  public:
    Credentials(std::string&& user, std::string&& password) :
        userBuf(std::move(user)), passBuf(std::move(password))
    {}
    Credentials() = delete;
    Credentials(const Credentials&) = delete;
    Credentials(Credentials&&) = delete;
    Credentials& operator=(const Credentials&) = delete;
    Credentials& operator=(Credentials&&) = delete;

    ~Credentials()
    {
        secureCleanup(userBuf);
        secureCleanup(passBuf);
    }

    const std::string& user()
    {
        return userBuf;
    }

    const std::string& password()
    {
        return passBuf;
    }

    void escapeCommas()
    {
        if (!commasEscaped)
        {
            escapeComma(passBuf);
            commasEscaped = true;
        }
    }

  private:
    /* escape ',' (coma) by ',,' */
    static void escapeComma(std::string& s)
    {
        std::string temp;
        std::for_each(s.begin(), s.end(), [&temp](const auto& c) {
            *std::back_inserter(temp) = c;
            if (c == ',')
            {
                *std::back_inserter(temp) = c;
            }
        });
        std::swap(s, temp);
        secureCleanup(temp);
    }

    std::string userBuf;
    std::string passBuf;
    bool commasEscaped{false};
};

class CredentialsProvider
{
  public:
    template <typename T>
    struct Deleter
    {
        void operator()(T* buff) const
        {
            if (buff)
            {
                secureCleanup(*buff);
                delete buff;
            }
        }
    };

    using Buffer = std::vector<char>;
    using SecureBuffer = std::unique_ptr<Buffer, Deleter<Buffer>>;
    // Using explicit definition instead of std::function to avoid implicit
    // conversions eg. stack copy instead of reference for parameters
    using FormatterFunc = void(const std::string& username,
                               const std::string& password, Buffer& dest);

    CredentialsProvider(std::string&& user, std::string&& password) :
        credentials(std::move(user), std::move(password))
    {}

    void escapeCommas()
    {
        credentials.escapeCommas();
    }

    const std::string& user()
    {
        return credentials.user();
    }

    const std::string& password()
    {
        return credentials.password();
    }

    SecureBuffer pack(FormatterFunc formatter)
    {
        if (formatter == nullptr)
        {
            return nullptr;
        }

        SecureBuffer packed{new Buffer{}};
        formatter(credentials.user(), credentials.password(), *packed);

        return packed;
    }

  private:
    Credentials credentials;
};

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
        readBuffer.reserve(static_cast<size_t>(size));
        size_t charsRead =
            file.read(readBuffer.data(), static_cast<size_t>(size), ec);
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

template <class File = FileObject>
class VolatileFile
{
    using Buffer = CredentialsProvider::SecureBuffer;

  public:
    explicit VolatileFile(Buffer&& contents) : size(contents->size())
    {
        auto data = std::move(contents);
        filePath = std::filesystem::temp_directory_path().c_str();
        filePath.append("/VM-XXXXXX");
        create(data);
    }

    ~VolatileFile()
    {
        purgeFileContents();
        std::filesystem::remove(filePath);
    }

    VolatileFile(const VolatileFile&) = delete;
    VolatileFile(VolatileFile&&) = delete;

    VolatileFile& operator=(const VolatileFile&) = delete;
    VolatileFile& operator=(VolatileFile&&) = delete;

    const std::string& path()
    {
        return filePath;
    }

  protected:
    void create(const Buffer& data)
    {
        auto fd = mkstemp(filePath.data());

        File file(fd);
        if (file.write(data->data(), data->size()) != data->size())
        {
            throw sdbusplus::exception::SdBusError(
                EIO, "I/O error on temporary file");
        }
    }

    void purgeFileContents()
    {
        if (std::ofstream file(filePath); file)
        {
            std::array<char, secretLimit> buf{};
            buf.fill('*');

            std::size_t bytesWritten = 0;
            while (bytesWritten < size)
            {
                std::size_t bytesToWrite =
                    std::min(secretLimit, (size - bytesWritten));
                file.write(buf.data(),
                           static_cast<std::streamsize>(bytesToWrite));
                bytesWritten += bytesToWrite;
            }
        }
    }

  private:
    std::string filePath;
    const std::size_t size;
};

class SignalSender
{
  public:
    SignalSender() = default;
    virtual ~SignalSender() = default;

    SignalSender(const SignalSender&) = delete;
    SignalSender(SignalSender&&) = delete;

    SignalSender& operator=(const SignalSender&) = delete;
    SignalSender& operator=(SignalSender&&) = delete;

    virtual void send(std::optional<const std::error_code> status) = 0;
};

class NotificationWrapper
{
  public:
    NotificationWrapper() = default;
    virtual ~NotificationWrapper() = default;

    NotificationWrapper(const NotificationWrapper&) = delete;
    NotificationWrapper(NotificationWrapper&&) = delete;

    NotificationWrapper& operator=(const NotificationWrapper&) = delete;
    NotificationWrapper& operator=(NotificationWrapper&&) = delete;

    virtual void
        start(std::function<void(const boost::system::error_code&)> handler,
              const std::chrono::seconds& duration) = 0;

    virtual void notify(const std::error_code& ec) = 0;
};

} // namespace utils

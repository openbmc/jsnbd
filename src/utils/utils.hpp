#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <boost/system/error_code.hpp>
#include <sdbusplus/exception.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>
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
    Credentials& operator=(const Credentials&) = delete;

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
    void escapeComma(std::string& s)
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
        if (!formatter)
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
    explicit FileObject(int fd) : fd(fd) {}

    explicit FileObject(const char* filename, int flags)
    {
        fd = ::open(filename, flags);
        if (fd < 0)
        {
            throw std::filesystem::filesystem_error(
                "Couldn't open file",
                std::make_error_code(static_cast<std::errc>(errno)));
        }
    }

    FileObject() = delete;
    FileObject(const FileObject&) = delete;
    FileObject& operator=(const FileObject&) = delete;

    int ioctl(unsigned long request)
    {
        return ::ioctl(fd, request);
    }

    ssize_t write(void* data, ssize_t nw)
    {
        return ::write(fd, data, static_cast<size_t>(nw));
    }

    ~FileObject()
    {
        ::close(fd);
    }

  private:
    int fd;
};

template <class File = FileObject>
class VolatileFile
{
    using Buffer = CredentialsProvider::SecureBuffer;

  public:
    VolatileFile(Buffer&& contents) : size(contents->size())
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

    const std::string& path()
    {
        return filePath;
    }

  protected:
    void create(const Buffer& data)
    {
        auto fd = mkstemp(filePath.data());

        File file(fd);
        if (file.write(data->data(), static_cast<ssize_t>(data->size())) !=
            static_cast<ssize_t>(data->size()))
        {
            throw sdbusplus::exception::SdBusError(
                EIO, "I/O error on temporary file");
        }
    }

    void purgeFileContents()
    {
        if (std::ofstream file(filePath); file)
        {
            std::array<char, secretLimit> buf;
            buf.fill('*');

            std::size_t bytesWritten = 0;
            while (bytesWritten < size)
            {
                std::size_t bytesToWrite = std::min(secretLimit,
                                                    (size - bytesWritten));
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
    virtual ~SignalSender() = default;

    virtual void send(std::optional<const std::error_code> status) = 0;
};

class NotificationWrapper
{
  public:
    virtual ~NotificationWrapper() = default;

    virtual void
        start(std::function<void(const boost::system::error_code&)> handler,
              const std::chrono::seconds& duration) = 0;

    virtual void notify(const std::error_code& ec) = 0;
};

} // namespace utils

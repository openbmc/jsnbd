#pragma once

#include "utils/utils.hpp"

#include <boost/beast/core/file_base.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace virtual_media_test
{

using ::testing::_;
using ::testing::Return;

class MockSignalSender : public ::utils::SignalSender
{
  public:
    MOCK_METHOD(void, send, (std::optional<const std::error_code>), (override));
};

class MockNotificationWrapper : public ::utils::NotificationWrapper
{
  public:
    MOCK_METHOD(void, start,
                (std::function<void(const boost::system::error_code&)>,
                 const std::chrono::seconds&),
                (override));
    MOCK_METHOD(void, notify, (const std::error_code&), (override));
};

class FailableFileObject
{
  public:
    [[noreturn]] FailableFileObject(
        [[maybe_unused]] const char* filename,
        [[maybe_unused]] boost::beast::file_mode mode)
    {
        throw std::filesystem::filesystem_error(
            "Couldn't open file",
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    FailableFileObject() = delete;
    FailableFileObject(const FailableFileObject&) = delete;
    FailableFileObject& operator=(const FailableFileObject&) = delete;
    FailableFileObject(FailableFileObject&&) = delete;
    FailableFileObject& operator=(FailableFileObject&&) = delete;
    ~FailableFileObject() = default;

    MOCK_METHOD(int, ioctl, (unsigned long), (const));
};

class MockFileObject
{
  public:
    explicit MockFileObject([[maybe_unused]] int fd) {}

    MockFileObject(const char* filename,
                   [[maybe_unused]] boost::beast::file_mode mode)
    {
        std::string path(filename);
        if (path == "/dev/nbd1")
        {
            ON_CALL(*this, ioctl(_)).WillByDefault(Return(-1));
        }
        else
        {
            ON_CALL(*this, ioctl(_)).WillByDefault(Return(0));
        }
    }

    MockFileObject() = delete;
    MockFileObject(const MockFileObject&) = delete;
    MockFileObject& operator=(const MockFileObject&) = delete;
    MockFileObject(MockFileObject&&) = delete;
    MockFileObject& operator=(MockFileObject&&) = delete;
    ~MockFileObject() = default;

    MOCK_METHOD(int, ioctl, (unsigned long), (const));
    MOCK_METHOD(size_t, write, (void*, size_t), ());
};

class FailableSecretFileObject
{
  public:
    explicit FailableSecretFileObject([[maybe_unused]] int fd) {}

    static size_t write([[maybe_unused]] void* data, [[maybe_unused]] size_t nw)
    {
        return static_cast<size_t>(-1);
    }
};

} // namespace virtual_media_test

#pragma once

#include <boost/beast/core/file_base.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace virtual_media_test
{

using ::testing::_;
using ::testing::Return;

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

} // namespace virtual_media_test

#pragma once

#include "utils/utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace utils
{

class FailableFileObject
{
  public:
    explicit FailableFileObject([[maybe_unused]] const char* filename,
                                [[maybe_unused]] int flags)
    {
        throw std::filesystem::filesystem_error(
            "Couldn't open file",
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    FailableFileObject() = delete;
    FailableFileObject(const FailableFileObject&) = delete;
    FailableFileObject& operator=(const FailableFileObject&) = delete;

    MOCK_METHOD(int, ioctl, (unsigned long), ());
};

class MockFileObject
{
  public:
    explicit MockFileObject([[maybe_unused]] int fd) {}

    explicit MockFileObject(const char* filename, [[maybe_unused]] int flags)
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

    MOCK_METHOD(int, ioctl, (unsigned long), ());
    MOCK_METHOD(ssize_t, write, (void*, ssize_t), ());
};

} // namespace utils

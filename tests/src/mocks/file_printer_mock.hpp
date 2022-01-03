#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

using ::testing::_;
using ::testing::Return;

struct MockFilePrinterEngine
{
    MockFilePrinterEngine()
    {
        ON_CALL(*this, echo(_, _)).WillByDefault(Return(true));
        ON_CALL(*this, exists(_)).WillByDefault(Return(false));
        ON_CALL(*this, isDir(_)).WillByDefault(Return(true));
        ON_CALL(*this, isSymlink(_)).WillByDefault(Return(false));
    }

    MOCK_METHOD(void, createDirs, (const fs::path&), ());
    MOCK_METHOD(void, createDirSymlink, (const fs::path&, const fs::path&), ());
    MOCK_METHOD(bool, echo, (const fs::path&, const std::string&), ());
    MOCK_METHOD(bool, exists, (const fs::path&), ());
    MOCK_METHOD(bool, isDir, (const fs::path&), ());
    MOCK_METHOD(bool, isSymlink, (const fs::path&), ());
    MOCK_METHOD(void, remove, (const fs::path&, std::error_code&), ());
};

struct MockFilePrinter
{
    static MockFilePrinterEngine* engine;

    static void createDirs(const fs::path& path)
    {
        engine->createDirs(path);
    }

    static void createDirSymlink(const fs::path& target, const fs::path& link)
    {
        engine->createDirSymlink(target, link);
    }

    static bool echo(const fs::path& path, const std::string& content)
    {
        return engine->echo(path, content);
    }

    static bool exists(const fs::path& path)
    {
        return engine->exists(path);
    }

    static bool isDir(const fs::path& path)
    {
        return engine->isDir(path);
    }

    static bool isSymlink(const fs::path& path)
    {
        return engine->isSymlink(path);
    }

    static void remove(const fs::path& path, std::error_code& ec)
    {
        engine->remove(path, ec);
    }
};

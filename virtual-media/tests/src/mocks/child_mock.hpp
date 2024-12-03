#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace virtual_media_test
{

using ::testing::Return;

class MockChildEngine
{
  public:
    MockChildEngine()
    {
        ON_CALL(*this, exitCode()).WillByDefault(Return(0));
        ON_CALL(*this, nativeExitCode()).WillByDefault(Return(0));
        ON_CALL(*this, running()).WillByDefault(Return(false));
    }

    MOCK_METHOD(void, create, (), ());
    MOCK_METHOD(int, exitCode, (), ());
    MOCK_METHOD(int, nativeExitCode, (), ());
    MOCK_METHOD(bool, running, (), ());
    MOCK_METHOD(void, terminate, (), ());
    MOCK_METHOD(void, wait, (), ());
};

class MockChild
{
  public:
    static MockChildEngine* engine;

    static void create()
    {
        engine->create();
    }

    static int exitCode()
    {
        return engine->exitCode();
    }

    static int nativeExitCode()
    {
        return engine->nativeExitCode();
    }

    static bool running()
    {
        return engine->running();
    }

    static void terminate()
    {
        engine->terminate();
    }

    static void wait()
    {
        engine->wait();
    }
};

} // namespace virtual_media_test

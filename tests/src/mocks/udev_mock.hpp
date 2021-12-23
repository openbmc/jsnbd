#pragma once

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockUdevDeviceEngine
{
  public:
    MOCK_METHOD(const std::string, getAction, (), ());
    MOCK_METHOD(const std::string, getSysname, (), ());
    MOCK_METHOD(const std::string, getSysAttrValue, (const std::string&), ());
};

class MockUdevDevice
{
  public:
    static MockUdevDeviceEngine* engine;

    static const std::string getAction()
    {
        return engine->getAction();
    }

    static const std::string getSysname()
    {
        return engine->getSysname();
    }

    static const std::string getSysAttrValue(const std::string& sysAttr)
    {
        return engine->getSysAttrValue(sysAttr);
    }
};

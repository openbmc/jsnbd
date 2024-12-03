#pragma once

#include <string>

#include <gmock/gmock.h>

namespace virtual_media_test
{

class MockUdevDeviceEngine
{
  public:
    MOCK_METHOD(std::string, getAction, (), ());
    MOCK_METHOD(std::string, getSysname, (), ());
    MOCK_METHOD(std::string, getSysAttrValue, (const std::string&), ());
};

class MockUdevDevice
{
  public:
    static MockUdevDeviceEngine* engine;

    static std::string getAction()
    {
        return engine->getAction();
    }

    static std::string getSysname()
    {
        return engine->getSysname();
    }

    static std::string getSysAttrValue(const std::string& sysAttr)
    {
        return engine->getSysAttrValue(sysAttr);
    }
};

} // namespace virtual_media_test

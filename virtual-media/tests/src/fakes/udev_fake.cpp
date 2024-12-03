#include "mocks/udev_mock.hpp"
#include "utils/udev.hpp"

#include <string>

using namespace virtual_media_test;

MockUdevDeviceEngine* MockUdevDevice::engine = nullptr;

namespace utils
{

Udev::Udev() = default;

// NOLINTNEXTLINE(modernize-use-equals-default)
Udev::~Udev() {}

UdevMonitor::UdevMonitor([[maybe_unused]] const Udev& udev,
                         [[maybe_unused]] const std::string& name)
{}

// NOLINTNEXTLINE(modernize-use-equals-default)
UdevMonitor::~UdevMonitor() {}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
int UdevMonitor::addFilter([[maybe_unused]] const std::string& subsystem,
                           [[maybe_unused]] const std::string& devType)
{
    return 0;
}

int UdevMonitor::enable()
{
    return 0;
}

int UdevMonitor::getFd()
{
    return 0;
}
// NOLINTEND(readability-convert-member-functions-to-static)

UdevDevice::UdevDevice([[maybe_unused]] const UdevMonitor& monitor) {}

// NOLINTNEXTLINE(modernize-use-equals-default)
UdevDevice::~UdevDevice() {}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
std::string UdevDevice::getAction()
{
    return MockUdevDevice::getAction();
}

std::string UdevDevice::getSysname()
{
    return MockUdevDevice::getSysname();
}

std::string UdevDevice::getSysAttrValue(const std::string& sysAttr)
{
    return MockUdevDevice::getSysAttrValue(sysAttr);
}
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace utils

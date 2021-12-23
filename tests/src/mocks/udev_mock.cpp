#include "mocks/udev_mock.hpp"

#include "utils/udev.hpp"

MockUdevDeviceEngine* MockUdevDevice::engine = nullptr;

namespace utils
{

Udev::Udev()
{}

Udev::~Udev()
{}

UdevMonitor::UdevMonitor([[maybe_unused]] const Udev& udev,
                         [[maybe_unused]] const std::string& name)
{}

UdevMonitor::~UdevMonitor()
{}

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

UdevDevice::UdevDevice([[maybe_unused]] const UdevMonitor& monitor)
{}

UdevDevice::~UdevDevice()
{}

const std::string UdevDevice::getAction()
{
    return MockUdevDevice::getAction();
}

const std::string UdevDevice::getSysname()
{
    return MockUdevDevice::getSysname();
}

const std::string UdevDevice::getSysAttrValue(const std::string& sysAttr)
{
    return MockUdevDevice::getSysAttrValue(sysAttr);
}

} // namespace utils

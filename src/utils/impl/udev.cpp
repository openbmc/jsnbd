#include "utils/udev.hpp"

#include <string>
#include <system_error>

namespace utils
{

Udev::Udev() : udev(udev_new())
{
    if (!udev)
    {
        throw std::system_error(EFAULT, std::generic_category(),
                                "Unable to create uDev handler.");
    }
}

Udev::~Udev()
{
    udev_unref(udev);
}

UdevMonitor::UdevMonitor(const Udev& udev, const std::string& name) :
    monitor(udev_monitor_new_from_netlink(udev.udev, name.c_str()))
{
    if (!monitor)
    {
        throw std::system_error(EFAULT, std::generic_category(),
                                "Unable to create uDev Monitor handler.");
    }
}

UdevMonitor::~UdevMonitor()
{
    udev_monitor_unref(monitor);
}

int UdevMonitor::addFilter(const std::string& subsystem,
                           const std::string& devType)
{
    return udev_monitor_filter_add_match_subsystem_devtype(
        monitor, subsystem.c_str(), devType.c_str());
}

int UdevMonitor::enable()
{
    return udev_monitor_enable_receiving(monitor);
}

int UdevMonitor::getFd()
{
    return udev_monitor_get_fd(monitor);
}

UdevDevice::UdevDevice(const UdevMonitor& monitor) :
    device(udev_monitor_receive_device(monitor.monitor))
{}

UdevDevice::~UdevDevice()
{
    udev_device_unref(device);
}

const std::string UdevDevice::getAction()
{
    return std::string(udev_device_get_action(device));
}

const std::string UdevDevice::getSysname()
{
    return std::string(udev_device_get_sysname(device));
}

const std::string UdevDevice::getSysAttrValue(const std::string& sysAttr)
{
    return std::string(udev_device_get_sysattr_value(device, sysAttr.c_str()));
}

} // namespace utils

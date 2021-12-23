#pragma once

#include <libudev.h>

#include <optional>
#include <string>

namespace utils
{

class Udev;
class UdevMonitor;
class UdevDevice;

class Udev
{
  public:
    Udev();
    ~Udev();

  private:
    friend class UdevMonitor;

    struct udev* udev;
};

class UdevMonitor
{
  public:
    UdevMonitor(const Udev& udev, const std::string& name);
    ~UdevMonitor();

    int addFilter(const std::string& subsystem, const std::string& devType);
    int enable();
    int getFd();

  private:
    friend class UdevDevice;

    struct udev_monitor* monitor;
};

class UdevDevice
{
  public:
    UdevDevice(const UdevMonitor& monitor);
    ~UdevDevice();

    const std::string getAction();
    const std::string getSysname();
    const std::string getSysAttrValue(const std::string& sysAttr);

  private:
    struct udev_device* device;
};

} // namespace utils

#pragma once

#include <libudev.h>

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

    Udev(const Udev&) = delete;
    Udev(Udev&&) = delete;

    Udev& operator=(const Udev&) = delete;
    Udev& operator=(Udev&&) = delete;

  private:
    friend class UdevMonitor;

    struct udev* udev{};
};

class UdevMonitor
{
  public:
    explicit UdevMonitor(const Udev& udev, const std::string& name);
    ~UdevMonitor();

    UdevMonitor(const UdevMonitor&) = delete;
    UdevMonitor(UdevMonitor&&) = delete;

    UdevMonitor& operator=(const UdevMonitor&) = delete;
    UdevMonitor& operator=(UdevMonitor&&) = delete;

    int addFilter(const std::string& subsystem, const std::string& devType);
    int enable();
    int getFd();

  private:
    friend class UdevDevice;

    struct udev_monitor* monitor{};
};

class UdevDevice
{
  public:
    explicit UdevDevice(const UdevMonitor& monitor);
    ~UdevDevice();

    UdevDevice(const UdevDevice&) = delete;
    UdevDevice(UdevDevice&&) = delete;

    UdevDevice& operator=(const UdevDevice&) = delete;
    UdevDevice& operator=(UdevDevice&&) = delete;

    std::string getAction();
    std::string getSysname();
    std::string getSysAttrValue(const std::string& sysAttr);

  private:
    struct udev_device* device{};
};

} // namespace utils

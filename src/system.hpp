#pragma once

#include "logger.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/process.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace udev
{
#include <libudev.h>
struct udev;
struct udev_monitor;
struct udev_device;

struct udevDeleter
{
    void operator()(udev* desc) const
    {
        udev_unref(desc);
    }
};

struct monitorDeleter
{
    void operator()(udev_monitor* desc) const
    {
        udev_monitor_unref(desc);
    };
};

struct deviceDeleter
{
    void operator()(udev_device* desc) const
    {
        udev_device_unref(desc);
    };
};
} // namespace udev

#define NBD_DISCONNECT _IO(0xab, 8)
#define NBD_CLEAR_SOCK _IO(0xab, 4)

class NBDDevice
{
  public:
    enum Value : uint8_t // This is explicite not a class to avoid naming like
                         // NBDDevice::Device::nbd0
    {
        nbd0 = 0,
        nbd1 = 1,
        nbd2 = 2,
        nbd3 = 3,
        nbd4 = 4,
        nbd5 = 5,
        nbd6 = 6,
        nbd7 = 7,
        nbd8 = 8,
        nbd9 = 9,
        nbd10 = 10,
        unknown = 0xFF
    };

    NBDDevice() = default;
    NBDDevice(Value v) : value(v){};
    explicit NBDDevice(const char* nbdName)
    {
        if (nbdName != nullptr)
        {
            const auto iter =
                std::find(nameMatching.cbegin(), nameMatching.cend(), nbdName);
            if (iter != nameMatching.cend())
            {
                value = static_cast<Value>(
                    std::distance(nameMatching.cbegin(), iter));
            }
        }
    }
    NBDDevice(const NBDDevice&) = default;
    NBDDevice(NBDDevice&&) = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) = default;

    bool operator==(const NBDDevice& rhs) const
    {
        return value == rhs.value;
    }
    bool operator!=(const NBDDevice& rhs) const
    {
        return value != rhs.value;
    }
    bool operator<(const NBDDevice& rhs) const
    {
        return value < rhs.value;
    }
    explicit operator bool()
    {
        return (value != unknown);
    }

    bool isReady() const
    {
        if (value == unknown)
        {
            return false;
        }
        int fd = open(to_path().c_str(), O_EXCL);
        if (fd < 0)
        {
            return false;
        }
        close(fd);
        return true;
    }

    void disconnect() const
    {
        if (value == unknown)
        {
            return;
        }

        int fd = open(to_path().c_str(), O_RDWR);

        if (fd < 0)
        {
            LogMsg(Logger::Error, "Couldn't open device ", to_path().c_str());
            return;
        }
        if (ioctl(fd, NBD_DISCONNECT) < 0)
        {
            LogMsg(Logger::Info, "Ioctl failed: \n");
        }
        if (ioctl(fd, NBD_CLEAR_SOCK) < 0)
        {
            LogMsg(Logger::Info, "Ioctl failed: \n");
        }
        close(fd);
    }

    std::string to_string() const
    {
        if (value == unknown)
        {
            return "";
        }
        return nameMatching[static_cast<uint8_t>(value)];
    }

    fs::path to_path() const
    {
        if (value == unknown)
        {
            return fs::path();
        }
        return fs::path("/dev") /
               fs::path(nameMatching[static_cast<uint8_t>(value)]);
    }

  private:
    Value value = unknown;

    static const inline std::vector<std::string> nameMatching = {
        "nbd0", "nbd1", "nbd2", "nbd3", "nbd4", "nbd5",
        "nbd6", "nbd7", "nbd8", "nbd9", "nbd10"};
};

enum class StateChange
{
    notMonitored,
    unknown,
    removed,
    inserted
};

class DeviceMonitor
{
  public:
    DeviceMonitor(boost::asio::io_context& ioc) : ioc(ioc), monitorSd(ioc)
    {
        udev = std::unique_ptr<udev::udev, udev::udevDeleter>(udev::udev_new());
        if (!udev)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Unable to create uDev handler.");
        }

        monitor = std::unique_ptr<udev::udev_monitor, udev::monitorDeleter>(
            udev::udev_monitor_new_from_netlink(udev.get(), "kernel"));

        if (!monitor)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Unable to create uDev Monitor handler.");
        }
        int rc = udev_monitor_filter_add_match_subsystem_devtype(
            monitor.get(), "block", "disk");

        if (rc)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Could not apply filters.");
        }
        rc = udev_monitor_enable_receiving(monitor.get());
        if (rc)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Enable receiving failed.");
        }
        monitorSd.assign(udev_monitor_get_fd(monitor.get()));
    }

    DeviceMonitor(const DeviceMonitor&) = delete;
    DeviceMonitor(DeviceMonitor&&) = delete;

    DeviceMonitor& operator=(const DeviceMonitor&) = delete;
    DeviceMonitor& operator=(DeviceMonitor&&) = delete;

    template <typename DeviceChangeStateCb>
    void run(DeviceChangeStateCb callback)
    {
        boost::asio::spawn(ioc, [this,
                                 callback](boost::asio::yield_context yield) {
            boost::system::error_code ec;
            while (1)
            {
                monitorSd.async_wait(
                    boost::asio::posix::stream_descriptor::wait_read,
                    yield[ec]);

                std::unique_ptr<udev::udev_device, udev::deviceDeleter> device =
                    std::unique_ptr<udev::udev_device, udev::deviceDeleter>(
                        udev::udev_monitor_receive_device(monitor.get()));
                if (device)
                {
                    const char* devAction =
                        udev_device_get_action(device.get());
                    if (devAction == nullptr)
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Received NULL action.");
                        continue;
                    }
                    if (strcmp(devAction, "change") != 0)
                    {
                        continue;
                    }

                    const char* sysname = udev_device_get_sysname(device.get());
                    if (sysname == nullptr)
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Received NULL sysname.");
                        continue;
                    }

                    NBDDevice nbdDevice(sysname);
                    if (!nbdDevice)
                    {
                        continue;
                    }

                    auto monitoredDevice = devices.find(nbdDevice);
                    if (monitoredDevice == devices.cend())
                    {
                        continue;
                    }

                    const char* sizeStr =
                        udev_device_get_sysattr_value(device.get(), "size");
                    if (sizeStr == nullptr)
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Received NULL size.");
                        continue;
                    }

                    uint64_t size = 0;
                    try
                    {
                        size = std::stoul(sizeStr, 0, 0);
                    }
                    catch (const std::exception& e)
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Could not convert "
                               "size "
                               "to integer.");
                        continue;
                    }
                    if (size > 0 &&
                        monitoredDevice->second != StateChange::inserted)
                    {
                        LogMsg(Logger::Info,
                               "[DeviceMonitor]: ", nbdDevice.to_path(),
                               " inserted.");
                        monitoredDevice->second = StateChange::inserted;
                        callback(nbdDevice, StateChange::inserted);
                    }
                    else if (size == 0 &&
                             monitoredDevice->second != StateChange::removed)
                    {
                        LogMsg(Logger::Info,
                               "[DeviceMonitor]: ", nbdDevice.to_path(),
                               " removed.");
                        monitoredDevice->second = StateChange::removed;
                        callback(nbdDevice, StateChange::removed);
                    }
                }
            }
        });
    }

void addDevice(const NBDDevice& device)
{
    LogMsg(Logger::Info, "[DeviceMonitor]: watch on ", device.to_path());
    devices.insert(
        std::pair<NBDDevice, StateChange>(device, StateChange::unknown));
}

StateChange getState(const NBDDevice& device)
{
    auto monitoredDevice = devices.find(device);
    if (monitoredDevice != devices.cend())
    {
        return monitoredDevice->second;
    }
    return StateChange::notMonitored;
}

private:
boost::asio::io_context& ioc;
boost::asio::posix::stream_descriptor monitorSd;

std::unique_ptr<udev::udev, udev::udevDeleter> udev;
std::unique_ptr<udev::udev_monitor, udev::monitorDeleter> monitor;

boost::container::flat_map<NBDDevice, StateChange> devices;
};

class Process : public std::enable_shared_from_this<Process>
{
  public:
    Process(boost::asio::io_context& ioc, std::string_view name,
            const std::string& app, const NBDDevice& dev) :
        ioc(ioc),
        pipe(ioc), name(name), app(app), dev(dev)
    {
    }

    template <typename ExitCb>
    bool spawn(const std::vector<std::string>& args, ExitCb&& onExit)
    {
        std::error_code ec;
        LogMsg(Logger::Debug, "[Process]: Spawning ", app, " (", args, ")");
        child = boost::process::child(
            app, boost::process::args(args),
            (boost::process::std_out & boost::process::std_err) > pipe, ec,
            ioc);

        if (ec)
        {
            LogMsg(Logger::Error,
                   "[Process]: Error while creating child process: ", ec);
            return false;
        }

        boost::asio::spawn(ioc, [this, self = shared_from_this(),
                                 onExit = std::move(onExit)](
                                    boost::asio::yield_context yield) {
            boost::system::error_code bec;
            std::string line;
            boost::asio::dynamic_string_buffer buffer{line};
            LogMsg(Logger::Info,
                   "[Process]: Start reading console from nbd-client");
            while (1)
            {
                auto x = boost::asio::async_read_until(pipe, std::move(buffer),
                                                       '\n', yield[bec]);
                auto lineBegin = line.begin();
                while (lineBegin != line.end())
                {
                    auto lineEnd = find(lineBegin, line.end(), '\n');
                    LogMsg(Logger::Info, "[Process]: (", name, ") ",
                           std::string(lineBegin, lineEnd));
                    if (lineEnd == line.end())
                    {
                        break;
                    }
                    lineBegin = lineEnd + 1;
                }

                buffer.consume(x);
                if (bec)
                {
                    LogMsg(Logger::Info, "[Process]: (", name,
                           ") Loop Error: ", bec);
                    break;
                }
            }
            LogMsg(Logger::Info, "[Process]: Exiting from COUT Loop");
            // The process shall be dead, or almost here, give it a chance
            LogMsg(Logger::Debug,
                   "[Process]: Waiting process to finish normally");
            boost::asio::steady_timer timer(ioc);
            int32_t waitCnt = 20;
            while (child.running() && waitCnt > 0)
            {
                boost::system::error_code ignored_ec;
                timer.expires_from_now(std::chrono::milliseconds(100));
                timer.async_wait(yield[ignored_ec]);
                waitCnt--;
            }
            if (child.running())
            {
                child.terminate();
            }

            child.wait();
            LogMsg(Logger::Info, "[Process]: running: ", child.running(),
                   " EC: ", child.exit_code(),
                   " Native: ", child.native_exit_code());

            onExit(child.exit_code(), dev.isReady());
        });
        return true;
    }

    template <class OnTerminateCb>
    void stop(OnTerminateCb&& onTerminate)
    {
        boost::asio::spawn(ioc, [this, self = shared_from_this(),
                                 onTerminate = std::move(onTerminate)](
                                    boost::asio::yield_context yield) {
            // The Good
            dev.disconnect();

            // The Ugly(but required)
            boost::asio::steady_timer timer(ioc);
            int32_t waitCnt = 20;
            while (child.running() && waitCnt > 0)
            {
                boost::system::error_code ignored_ec;
                timer.expires_from_now(std::chrono::milliseconds(100));
                timer.async_wait(yield[ignored_ec]);
                waitCnt--;
            }
            if (child.running())
            {
                LogMsg(Logger::Info, "[Process] Terminate if process doesnt "
                                     "want to exit nicely");
                child.terminate();
                onTerminate();
            }
        });
    }

    std::string application()
    {
        return app;
    }

  private:
    boost::asio::io_context& ioc;
    boost::process::child child;
    boost::process::async_pipe pipe;
    std::string name;
    std::string app;
    const NBDDevice& dev;
};


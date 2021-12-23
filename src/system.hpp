#pragma once

#include "logger.hpp"
#include "utils/child.hpp"
#include "utils/pipe_reader.hpp"
#include "utils/stream_descriptor.hpp"
#include "utils/udev.hpp"
#include "utils/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

#include <algorithm>
#include <chrono>
#include <compare>
#include <filesystem>
#include <ranges>
#include <string>
#include <system_error>

#define NBD_DISCONNECT _IO(0xab, 8)
#define NBD_CLEAR_SOCK _IO(0xab, 4)

template <class File = utils::FileObject>
class NBDDevice
{
  public:
    NBDDevice() = default;
    explicit NBDDevice(const std::string_view& nbdName)
    {
        if (!nbdName.empty())
        {
            const auto iter = std::ranges::find(
                std::cbegin(nameMatching), std::cend(nameMatching), nbdName);
            if (iter != std::cend(nameMatching))
            {
                value = *iter;
            }
        }
    }
    NBDDevice(const NBDDevice&) = default;
    NBDDevice(NBDDevice&&) = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) = default;

    auto operator<=>(const NBDDevice& rhs) const = default;

    bool isValid() const
    {
        return (!value.empty());
    }

    explicit operator bool()
    {
        return isValid();
    }

    bool isReady() const
    {
        if (value.empty())
        {
            return false;
        }

        try
        {
            File file(toPath().c_str(), O_EXCL);
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LogMsg(Logger::Error, "Ready check error: ", e.what());
            return false;
        }

        return true;
    }

    bool disconnect() const
    {
        if (value.empty())
        {
            return false;
        }

        try
        {
            File file(toPath().c_str(), O_RDWR);
            if (file.ioctl(NBD_DISCONNECT) < 0)
            {
                LogMsg(Logger::Info, "Ioctl failed: \n");
            }
            if (file.ioctl(NBD_CLEAR_SOCK) < 0)
            {
                LogMsg(Logger::Info, "Ioctl failed: \n");
            }
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LogMsg(Logger::Error, "Disconnect error: ", e.what());
            return false;
        }

        return true;
    }

    const std::string& toString() const
    {
        return value;
    }

    std::filesystem::path toPath() const
    {
        if (value.empty())
        {
            return {};
        }
        return std::filesystem::path("/dev") / std::filesystem::path(value);
    }

  private:
    std::string value{};

    static constexpr std::array<std::string_view, 16> nameMatching = {
        "nbd0", "nbd1", "nbd2",  "nbd3",  "nbd4",  "nbd5",  "nbd6",  "nbd7",
        "nbd8", "nbd9", "nbd10", "nbd11", "nbd12", "nbd13", "nbd14", "nbd15"};
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
    DeviceMonitor(boost::asio::io_context& ioc) : ioc(ioc)
    {
        udev = std::make_unique<utils::Udev>();
        monitor = std::make_unique<utils::UdevMonitor>(*udev, "kernel");

        int rc = monitor->addFilter("block", "disk");
        if (rc)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Could not apply filters.");
        }

        rc = monitor->enable();
        if (rc)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Enable receiving failed.");
        }

        monitorSd =
            std::make_unique<utils::StreamDescriptor>(ioc, monitor->getFd());
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
            while (true)
            {
                monitorSd->asyncWait(
                    boost::asio::posix::stream_descriptor::wait_read,
                    yield[ec]);

                auto device = std::make_unique<utils::UdevDevice>(*monitor);
                if (device)
                {
                    std::string devAction = device->getAction();
                    if (devAction.empty())
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Received NULL action.");
                        continue;
                    }
                    if (devAction != "change")
                    {
                        continue;
                    }

                    std::string sysname = device->getSysname();
                    if (sysname.empty())
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

                    std::string sizeStr = device->getSysAttrValue("size");
                    if (sizeStr.empty())
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Received NULL size.");
                        continue;
                    }

                    uint64_t size = 0;
                    try
                    {
                        size = std::stoul(sizeStr.c_str(), nullptr, 0);
                    }
                    catch (const std::exception& e)
                    {
                        LogMsg(Logger::Error,
                               "[DeviceMonitor]: Could not convert "
                               "size "
                               "to integer: ",
                               e.what());
                        continue;
                    }
                    if (size > 0 &&
                        monitoredDevice->second != StateChange::inserted)
                    {
                        LogMsg(Logger::Info,
                               "[DeviceMonitor]: ", nbdDevice.toPath(),
                               " inserted.");
                        monitoredDevice->second = StateChange::inserted;
                        callback(nbdDevice, StateChange::inserted);
                    }
                    else if (size == 0 &&
                             monitoredDevice->second != StateChange::removed)
                    {
                        LogMsg(Logger::Info,
                               "[DeviceMonitor]: ", nbdDevice.toPath(),
                               " removed.");
                        monitoredDevice->second = StateChange::removed;
                        callback(nbdDevice, StateChange::removed);
                    }
                }
            }
        });
    }

    void addDevice(const NBDDevice<>& device)
    {
        LogMsg(Logger::Info, "[DeviceMonitor]: watch on ", device.toPath());
        devices.emplace(device, StateChange::unknown);
    }

    StateChange getState(const NBDDevice<>& device)
    {
        auto monitoredDevice = devices.find(device);
        if (monitoredDevice != devices.cend())
        {
            return monitoredDevice->second;
        }
        return StateChange::notMonitored;
    }

  protected:
    std::map<NBDDevice<>, StateChange> devices{};

  private:
    boost::asio::io_context& ioc;

    std::unique_ptr<utils::StreamDescriptor> monitorSd;
    std::unique_ptr<utils::Udev> udev;
    std::unique_ptr<utils::UdevMonitor> monitor;
};

class Process : public std::enable_shared_from_this<Process>
{
  public:
    Process(boost::asio::io_context& ioc, std::string_view name,
            const std::string& app, const NBDDevice<>& dev) :
        ioc(ioc),
        name(name), app(app), reader(ioc), dev(dev)
    {}

    template <typename ExitCb>
    bool spawn(const std::vector<std::string>& args, ExitCb&& onExit)
    {
        std::error_code ec;
        LogMsg(Logger::Debug, "[Process]: Spawning ", app, " (", args, ")");

        try
        {
            child = utils::Child(ioc, app, args, reader);
        }
        catch (const std::system_error& e)
        {
            LogMsg(Logger::Error,
                   "[Process]: Error while creating child process: ", e.code(),
                   ": ", e.what());
            return false;
        }

        boost::asio::spawn(
            ioc, [this, self = shared_from_this(), onExit = std::move(onExit)](
                     boost::asio::yield_context yield) {
                boost::system::error_code bec;
                std::string line;
                boost::asio::dynamic_string_buffer buffer{line};
                LogMsg(Logger::Info,
                       "[Process]: Start reading console from nbd-client");
                while (true)
                {
                    auto x = reader.asyncRead(std::move(buffer), yield[bec]);
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
                    boost::system::error_code ignoredEc;
                    timer.expires_from_now(std::chrono::milliseconds(100));
                    timer.async_wait(yield[ignoredEc]);
                    waitCnt--;
                }
                if (child.running())
                {
                    child.terminate();
                }

                child.wait();
                LogMsg(Logger::Info, "[Process]: running: ", child.running(),
                       " EC: ", child.exitCode(),
                       " Native: ", child.nativeExitCode());

                onExit(child.exitCode());
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
                boost::system::error_code ignoredEc;
                timer.expires_from_now(std::chrono::milliseconds(100));
                timer.async_wait(yield[ignoredEc]);
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
    std::string name;
    std::string app;
    utils::Child child;
    utils::PipeReader reader;
    const NBDDevice<>& dev;
};

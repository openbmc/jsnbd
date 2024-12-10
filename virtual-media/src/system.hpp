#pragma once

#include "utils/child.hpp"
#include "utils/file_printer.hpp"
#include "utils/gadget_dirs.hpp"
#include "utils/log-wrapper.hpp"
#include "utils/pipe_reader.hpp"
#include "utils/stream_descriptor.hpp"
#include "utils/udev.hpp"
#include "utils/utils.hpp"

#include <linux/nbd.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/file_base.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

template <class File = utils::FileObject>
class NBDDevice
{
  public:
    NBDDevice() = default;
    explicit NBDDevice(const std::string& nbdName)
    {
        if (!nbdName.empty())
        {
            std::regex nbdRegex("nbd([0-9]+)");
            std::smatch nbdMatch;

            if (std::regex_search(nbdName, nbdMatch, nbdRegex))
            {
                size_t nbdId = std::stoul(nbdMatch[1]);
                if (nbdId < maxNbdCount)
                {
                    value = nbdMatch[0];
                }
            }
        }
    }
    NBDDevice(const NBDDevice&) = default;
    NBDDevice(NBDDevice&&) = default;

    NBDDevice& operator=(const NBDDevice&) = default;
    NBDDevice& operator=(NBDDevice&&) = default;

    ~NBDDevice() = default;

    auto operator<=>(const NBDDevice& rhs) const = default;

    bool isValid() const
    {
        return (!value.empty());
    }

    bool isReady() const
    {
        if (value.empty())
        {
            return false;
        }

        try
        {
            File file(toPath().c_str(), boost::beast::file_mode::read);
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LOGGER_ERROR("Ready check error: {}", e.what());
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
            File file(toPath().c_str(), boost::beast::file_mode::write);
            if (file.ioctl(NBD_DISCONNECT) < 0)
            {
                LOGGER_INFO("Ioctl NBD_DISCONNECT failed");
            }
            if (file.ioctl(NBD_CLEAR_SOCK) < 0)
            {
                LOGGER_INFO("Ioctl NBD_CLEAR_SOCK failed");
            }
        }
        catch (std::filesystem::filesystem_error& e)
        {
            LOGGER_ERROR("Disconnect error: {}", e.what());
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
    std::string value;

    static constexpr size_t maxNbdCount = 16;
};

enum class StateChange
{
    notMonitored,
    removed,
    inserted
};

class DeviceMonitor
{
  public:
    explicit DeviceMonitor(boost::asio::io_context& ioc) : ioc(ioc)
    {
        udev = std::make_unique<utils::Udev>();
        monitor = std::make_unique<utils::UdevMonitor>(*udev, "kernel");

        int rc = monitor->addFilter("block", "disk");
        if (rc != 0)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Could not apply filters.");
        }

        rc = monitor->enable();
        if (rc != 0)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Enable receiving failed.");
        }

        monitorSd =
            std::make_unique<utils::StreamDescriptor>(ioc, monitor->getFd());
    }

    ~DeviceMonitor() = default;

    DeviceMonitor(const DeviceMonitor&) = delete;
    DeviceMonitor(DeviceMonitor&&) = delete;

    DeviceMonitor& operator=(const DeviceMonitor&) = delete;
    DeviceMonitor& operator=(DeviceMonitor&&) = delete;

    template <typename DeviceChangeStateCb>
    void run(const DeviceChangeStateCb& callback)
    {
        boost::asio::spawn(
            ioc,
            [this, callback](const boost::asio::yield_context& yield) {
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
                            LOGGER_ERROR(
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
                            LOGGER_ERROR(
                                "[DeviceMonitor]: Received NULL sysname.");

                            continue;
                        }

                        NBDDevice nbdDevice(sysname);
                        if (!nbdDevice.isValid())
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
                            LOGGER_ERROR(
                                "[DeviceMonitor]: Received NULL size.");

                            continue;
                        }

                        uint64_t size = 0;
                        try
                        {
                            size = std::stoul(sizeStr, nullptr, 0);
                        }
                        catch (const std::exception& e)
                        {
                            LOGGER_ERROR(
                                "[DeviceMonitor]: Could not convert size to integer: {}",
                                e.what());

                            continue;
                        }

                        if (size > 0 &&
                            monitoredDevice->second != StateChange::inserted)
                        {
                            LOGGER_INFO("[DeviceMonitor]: {} inserted.",
                                        nbdDevice.toPath().string());

                            monitoredDevice->second = StateChange::inserted;
                            callback(nbdDevice, StateChange::inserted);
                        }
                        else if (size == 0 && monitoredDevice->second !=
                                                  StateChange::removed)
                        {
                            LOGGER_INFO("[DeviceMonitor]: {} removed. ",
                                        nbdDevice.toPath().string());

                            monitoredDevice->second = StateChange::removed;
                            callback(nbdDevice, StateChange::removed);
                        }
                    }
                }
            },
            boost::asio::detached);
    }

    void addDevice(const NBDDevice<>& device)
    {
        LOGGER_INFO("[DeviceMonitor]: watch on {}", device.toPath().string());

        devices.emplace(device, StateChange::removed);
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
    std::map<NBDDevice<>, StateChange> devices;

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
        ioc(ioc), name(name), app(app), reader(ioc), dev(dev)
    {}

    template <typename ExitCb>
    bool spawn(const std::vector<std::string>& args, ExitCb&& onExit)
    {
        std::error_code ec;
        LOGGER_DEBUG("[Process]: Spawning {} ({})", app, args);

        try
        {
            child = utils::Child(ioc, app, args, reader);
        }
        catch (const std::system_error& e)
        {
            LOGGER_ERROR("[Process]: Error while creating child process: {}",
                         e.what());

            return false;
        }

        boost::asio::spawn(
            ioc,
            [this, self = shared_from_this(),
             onExit = std::forward<ExitCb>(onExit)](
                boost::asio::yield_context yield) {
                boost::system::error_code bec;
                std::string line;
                boost::asio::dynamic_string_buffer buffer{line};
                LOGGER_INFO("[Process]: Start reading console from nbd-client");

                while (true)
                {
                    auto x = reader.asyncRead(buffer, yield, bec);
                    auto lineBegin = line.begin();
                    while (lineBegin != line.end())
                    {
                        auto lineEnd = std::find(lineBegin, line.end(), '\n');
                        LOGGER_INFO("[Process]: ({}) {}", name,
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
                        LOGGER_INFO("[Process]: ({}) Loop Error: ", name, bec);
                        break;
                    }
                }
                LOGGER_INFO("[Process]: Exiting from COUT Loop");
                // The process shall be dead, or almost here, give it a chance
                LOGGER_DEBUG("[Process]: Waiting process to finish normally");

                boost::asio::steady_timer timer(ioc);
                int32_t waitCnt = 20;

                while (child.running() && waitCnt > 0)
                {
                    boost::system::error_code ignoredEc;
                    timer.expires_after(std::chrono::milliseconds(100));
                    timer.async_wait(yield[ignoredEc]);
                    waitCnt--;
                }

                if (child.running())
                {
                    child.terminate();
                }

                child.wait();

                LOGGER_INFO("[Process]: running: {} EC: {} Native: {}",
                            child.running(), child.exitCode(),
                            child.nativeExitCode());

                onExit(child.exitCode());
            },
            boost::asio::detached);
        return true;
    }

    template <class OnTerminateCb>
    void stop(OnTerminateCb&& onTerminate)
    {
        boost::asio::spawn(
            ioc,
            [this, self = shared_from_this(),
             onTerminate = std::forward<OnTerminateCb>(onTerminate)](
                const boost::asio::yield_context& yield) {
                // The Good
                dev.disconnect();

                // The Ugly(but required)
                boost::asio::steady_timer timer(ioc);
                int32_t waitCnt = 20;
                while (child.running() && waitCnt > 0)
                {
                    boost::system::error_code ignoredEc;
                    timer.expires_after(std::chrono::milliseconds(100));
                    timer.async_wait(yield[ignoredEc]);
                    waitCnt--;
                }
                if (child.running())
                {
                    LOGGER_INFO(
                        "[Process] Terminate if process doesn't want to exit nicely");

                    child.terminate();
                    onTerminate();
                }
            },
            boost::asio::detached);
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

struct UsbGadget
{
  private:
    using Printer = utils::FilePrinter;
    using Dirs = utils::GadgetDirs;

  public:
    static int32_t configure(const std::string& name, const NBDDevice<>& nbd,
                             StateChange change, const bool rw = false)
    {
        return configure(name, nbd.toPath(), change, rw);
    }

    static int32_t configure(const std::string& name,
                             const std::filesystem::path& path,
                             StateChange change, const bool rw = false)
    {
        LOGGER_INFO("[App]: Configure USB Gadget (name={}, path={}, State={})",
                    name, path.string(), static_cast<uint32_t>(change));
        bool success = true;

        const std::filesystem::path gadgetDir = Dirs::gadgetPrefix() + name;
        const std::filesystem::path funcMassStorageDir =
            gadgetDir / "functions/mass_storage.usb0";
        const std::filesystem::path stringsDir = gadgetDir / "strings/0x409";
        const std::filesystem::path configDir = gadgetDir / "configs/c.1";
        const std::filesystem::path massStorageDir =
            configDir / "mass_storage.usb0";
        const std::filesystem::path configStringsDir =
            configDir / "strings/0x409";

        if (change == StateChange::inserted)
        {
            try
            {
                Printer::createDirs(gadgetDir);
                Printer::echo(gadgetDir / "idVendor", "0x1d6b");
                Printer::echo(gadgetDir / "idProduct", "0x0104");
                Printer::createDirs(stringsDir);
                Printer::echo(stringsDir / "manufacturer", "OpenBMC");
                Printer::echo(stringsDir / "product", "Virtual Media Device");
                Printer::createDirs(configStringsDir);
                Printer::echo(configStringsDir / "configuration", "config 1");
                Printer::createDirs(funcMassStorageDir);
                Printer::createDirs(funcMassStorageDir / "lun.0");
                Printer::createDirSymlink(funcMassStorageDir, massStorageDir);
                Printer::echo(funcMassStorageDir / "lun.0/removable", "1");
                Printer::echo(funcMassStorageDir / "lun.0/ro", rw ? "0" : "1");
                Printer::echo(funcMassStorageDir / "lun.0/cdrom", "0");
                Printer::echo(funcMassStorageDir / "lun.0/file", path);

                for (const auto& port :
                     std::filesystem::directory_iterator(Dirs::busPrefix()))
                {
                    const std::string portId = port.path().filename();

                    if (portId.find("1e6a0000.usb-vhub:p") != std::string::npos)
                    {
                        constexpr std::string_view portDelimiter = ":p";
                        const std::string portNumber = portId.substr(
                            portId.find(portDelimiter) + portDelimiter.size());

                        // GadgetId is port number minus 1
                        const int gadgetId = std::stoi(portNumber) - 1;

                        if (Printer::isDir(port) && !Printer::isSymlink(port) &&
                            !Printer::exists(
                                port.path() /
                                ("gadget." + std::to_string(gadgetId)) /
                                "suspended"))
                        {
                            LOGGER_DEBUG("Use port : {}", portId);
                            Printer::echo(gadgetDir / "UDC", portId);
                            return 0;
                        }
                    }
                }
                // No ports available - clear success flag and go to cleanup
                success = false;
            }
            catch (std::invalid_argument& e)
            {
                // Got error perform cleanup
                LOGGER_ERROR("[App]: UsbGadget: {}", e.what());

                success = false;
            }
            catch (std::filesystem::filesystem_error& e)
            {
                // Got error perform cleanup
                LOGGER_ERROR("[App]: UsbGadget: {}", e.what());

                success = false;
            }
            catch (std::ofstream::failure& e)
            {
                // Got error perform cleanup
                LOGGER_ERROR("[App]: UsbGadget: {}", e.what());

                success = false;
            }
        }
        // StateChange: notMonitored, inserted were handler
        // earlier. We'll get here only for removed, or cleanup
        try
        {
            Printer::echo(gadgetDir / "UDC", "");
        }
        catch (std::ofstream::failure& e)
        {
            // Got error perform cleanup
            LOGGER_ERROR("[App]: UsbGadget: {}", e.what());
            success = false;
        }
        const std::array<const char*, 6> dirs = {
            massStorageDir.c_str(),   funcMassStorageDir.c_str(),
            configStringsDir.c_str(), configDir.c_str(),
            stringsDir.c_str(),       gadgetDir.c_str()};
        for (const char* dir : dirs)
        {
            std::error_code ec;
            Printer::remove(dir, ec);
            if (ec)
            {
                success = false;
                LOGGER_ERROR("[App]: UsbGadget: {}", ec.message());
            }
        }

        if (success)
        {
            return 0;
        }
        return -1;
    }

    static bool isConfigured(const std::string& name)
    {
        const std::filesystem::path gadgetDir = Dirs::gadgetPrefix() + name;

        return Printer::exists(gadgetDir);
    }
};

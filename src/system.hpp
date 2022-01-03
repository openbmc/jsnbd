#pragma once

#include "logger.hpp"
#include "utils/child.hpp"
#include "utils/file_printer.hpp"
#include "utils/gadget_dirs.hpp"
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
#include <fstream>
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

        boost::asio::spawn(ioc, [this, self = shared_from_this(),
                                 onExit = std::move(onExit)](
                                    boost::asio::yield_context yield) {
            boost::system::error_code bec;
            std::string line;
            boost::asio::dynamic_string_buffer buffer{line};
            LogMsg(Logger::Info,
                   "[Process]: Start reading console from nbd-client");
            while (true)
            {
                auto x =
                    reader.asyncRead(std::move(buffer), std::move(yield), bec);
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
        LogMsg(Logger::Info, "[App]: Configure USB Gadget (name=", name,
               ", path=", path, ", State=", static_cast<uint32_t>(change), ")");
        bool success = true;

        if (change == StateChange::unknown)
        {
            LogMsg(Logger::Critical,
                   "[App]: Change to unknown state is not possible");
            return -1;
        }

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
                    if (Printer::isDir(port) && !Printer::isSymlink(port) &&
                        !Printer::exists(port.path() / "gadget/suspended"))
                    {
                        const std::string portId = port.path().filename();
                        LogMsg(Logger::Debug,
                               "Use port : ", port.path().filename());
                        Printer::echo(gadgetDir / "UDC", portId);
                        return 0;
                    }
                }
                // No ports available - clear success flag and go to cleanup
                success = false;
            }
            catch (std::filesystem::filesystem_error& e)
            {
                // Got error perform cleanup
                LogMsg(Logger::Error, "[App]: UsbGadget: ", e.what());
                success = false;
            }
            catch (std::ofstream::failure& e)
            {
                // Got error perform cleanup
                LogMsg(Logger::Error, "[App]: UsbGadget: ", e.what());
                success = false;
            }
        }
        // StateChange: unknown, notMonitored, inserted were handler
        // earlier. We'll get here only for removed, or cleanup
        try
        {
            Printer::echo(gadgetDir / "UDC", "");
        }
        catch (std::ofstream::failure& e)
        {
            // Got error perform cleanup
            LogMsg(Logger::Error, "[App]: UsbGadget: ", e.what());
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
                LogMsg(Logger::Error, "[App]: UsbGadget ", ec.message());
            }
        }

        if (success)
        {
            return 0;
        }
        return -1;
    }

    static std::optional<std::string> getStats(const std::string& name)
    {
        const std::filesystem::path statsPath =
            Dirs::gadgetPrefix() + name +
            "/functions/mass_storage.usb0/lun.0/stats";

        std::ifstream ifs(statsPath);
        if (!ifs.is_open())
        {
            LogMsg(Logger::Error, name, "Failed to open ", statsPath);
            return {};
        }

        return std::string{std::istreambuf_iterator<char>(ifs),
                           std::istreambuf_iterator<char>()};
    }

    static bool isConfigured(const std::string& name)
    {
        const std::filesystem::path gadgetDir = Dirs::gadgetPrefix() + name;

        if (!Printer::exists(gadgetDir))
        {
            return false;
        }

        return true;
    }
};

class UdevGadget
{
    using Printer = utils::FilePrinter;

  public:
    // This force-triggers udev change events for nbd[0-3] devices, which
    // prevents from disconnection on first mount event after reboot. The actual
    // rootcause is related with kernel changes that occured between 5.10.67 and
    // 5.14.11. This lead will continue to be investigated in order to provide
    // proper fix.
    static bool forceUdevChange()
    {
        std::string changeStr = "change";
        try
        {
            Printer::echo("/sys/block/nbd0/uevent", changeStr);
            Printer::echo("/sys/block/nbd1/uevent", changeStr);
            Printer::echo("/sys/block/nbd2/uevent", changeStr);
            Printer::echo("/sys/block/nbd3/uevent", changeStr);
        }
        catch (std::ofstream::failure& e)
        {
            LogMsg(Logger::Error, "[App]: UdevGadget: ", e.what());
            return false;
        }

        return true;
    }
};

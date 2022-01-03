#include "mocks/child_mock.hpp"
#include "mocks/file_printer_mock.hpp"
#include "mocks/udev_mock.hpp"
#include "mocks/utils_mock.hpp"
#include "system.hpp"

#include <fstream>
#include <functional>
#include <ios>
#include <vector>

#include <gmock-global/gmock-global.h>
#include <gmock/gmock-cardinalities.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Throw;
using ::testing::Values;

/* NBDDevice tests */
class NBDDeviceTest : public ::testing::Test
{
  protected:
    using MockFile = NiceMock<utils::MockFileObject>;

    // object with bad ID
    NBDDevice<utils::FailableFileObject> bad1{"bad"};

    // object that will throw exception on file open
    NBDDevice<utils::FailableFileObject> bad2{"nbd0"};

    // object that will fail on ioctl
    NBDDevice<MockFile> bad3{"nbd1"};

    NBDDevice<MockFile> good{"nbd0"};
};

TEST_F(NBDDeviceTest, DeviceNotReadyWhenBadIdProvided)
{
    EXPECT_FALSE(bad1.isValid());
    EXPECT_FALSE(bad1.isReady());
}

TEST_F(NBDDeviceTest, DeviceNotReadyWhenOpenFileFailed)
{
    ASSERT_TRUE(bad2.isValid());

    EXPECT_FALSE(bad2.isReady());
}

TEST_F(NBDDeviceTest, DeviceReadyWhenOpenFileSucceeded)
{
    ASSERT_TRUE(good.isValid());

    EXPECT_TRUE(good.isReady());
}

TEST_F(NBDDeviceTest, DeviceDisconnectionFailsForBadDevice)
{
    ASSERT_EQ(bad1.isValid(), false);

    EXPECT_FALSE(bad1.disconnect());
}

TEST_F(NBDDeviceTest, DeviceDisconnectionFailsWhenOpenFails)
{
    ASSERT_TRUE(bad2.isValid());

    EXPECT_FALSE(bad2.disconnect());
}

TEST_F(NBDDeviceTest, DeviceDisconnectionCompletesDespiteFailedIoctl)
{
    ASSERT_TRUE(bad3.isValid());

    EXPECT_TRUE(bad3.disconnect());
}

TEST_F(NBDDeviceTest, DeviceDisconnectsCorrectly)
{
    ASSERT_TRUE(good.isValid());

    EXPECT_TRUE(good.disconnect());
}

/* DeviceMonitor tests */
MOCK_GLOBAL_FUNC2(mockCallback, void(const NBDDevice<>&, StateChange));

class TestableDeviceMonitor : public DeviceMonitor
{
  public:
    using DeviceMonitor::devices;

    TestableDeviceMonitor(boost::asio::io_context& ioc) : DeviceMonitor(ioc)
    {}
};

class DeviceMonitorTest : public ::testing::Test
{
  protected:
    DeviceMonitorTest()
    {
        MockUdevDevice::engine = &udevice;
    }

    void prepareCoroutine()
    {
        monitor.addDevice(device);

        monitor.run(mockCallback);
    }

    void run(std::chrono::milliseconds timeout = std::chrono::milliseconds(2))
    {
        ioc.restart();
        ioc.run_for(timeout);
    }

    boost::asio::io_context ioc;
    NBDDevice<> device{"nbd0"};
    MockUdevDeviceEngine udevice;
    TestableDeviceMonitor monitor{ioc};
};

TEST_F(DeviceMonitorTest, DeviceAddedSuccessfully)
{
    monitor.addDevice(device);

    auto it = monitor.devices.find(device);
    EXPECT_NE(it, monitor.devices.cend());
    EXPECT_EQ(it->first, device);
}

TEST_F(DeviceMonitorTest, InitialDeviceStateIsCorrect)
{
    monitor.addDevice(device);

    EXPECT_EQ(monitor.getState(device), StateChange::unknown);
    EXPECT_EQ(monitor.getState(NBDDevice<>{"nbd1"}), StateChange::notMonitored);
}

TEST_F(DeviceMonitorTest, CoroutineIgnoresCorruptedDeviceAction)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return(""));
    EXPECT_CALL(udevice, getSysname()).Times(0);

    prepareCoroutine();
    run();
}

TEST_F(DeviceMonitorTest, CoroutineIgnoresInvalidDeviceAction)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("add"));
    EXPECT_CALL(udevice, getSysname()).Times(0);

    prepareCoroutine();
    run();
}

TEST_F(DeviceMonitorTest, CoroutineIgnoresCorruptedDeviceSysname)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("change"));
    EXPECT_CALL(udevice, getSysname()).WillRepeatedly(Return(""));
    EXPECT_CALL(udevice, getSysAttrValue(StrEq("size"))).Times(0);

    prepareCoroutine();
    run();
}

TEST_F(DeviceMonitorTest, CoroutineIgnoresNotMonitoredDevice)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("change"));
    EXPECT_CALL(udevice, getSysname()).WillRepeatedly(Return("nbd1"));
    EXPECT_CALL(udevice, getSysAttrValue(StrEq("size"))).Times(0);

    prepareCoroutine();
    run();
}

TEST_F(DeviceMonitorTest, CoroutineIgnoresCorruptedSize)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("change"));
    EXPECT_CALL(udevice, getSysname()).WillRepeatedly(Return("nbd0"));
    EXPECT_CALL(udevice, getSysAttrValue(StrEq("size")))
        .WillRepeatedly(Return(""));

    EXPECT_GLOBAL_CALL(mockCallback, mockCallback(_, _)).Times(0);

    prepareCoroutine();
    run();
}

TEST_F(DeviceMonitorTest, CoroutineIgnoresInvalidSizeFormat)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("change"));
    EXPECT_CALL(udevice, getSysname()).WillRepeatedly(Return("nbd0"));
    EXPECT_CALL(udevice, getSysAttrValue(StrEq("size")))
        .WillRepeatedly(Return("zero"));

    EXPECT_GLOBAL_CALL(mockCallback, mockCallback(_, _)).Times(0);

    prepareCoroutine();
    run();
}

TEST_F(DeviceMonitorTest, CoroutineDetectsRemovedDevice)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("change"));
    EXPECT_CALL(udevice, getSysname()).WillRepeatedly(Return("nbd0"));
    EXPECT_CALL(udevice, getSysAttrValue(StrEq("size")))
        .WillRepeatedly(Return("0"));

    EXPECT_GLOBAL_CALL(mockCallback, mockCallback(_, StateChange::removed));

    prepareCoroutine();
    run();

    EXPECT_EQ(monitor.getState(device), StateChange::removed);
}

TEST_F(DeviceMonitorTest, CoroutineDetectsInsertedDevice)
{
    EXPECT_CALL(udevice, getAction()).WillRepeatedly(Return("change"));
    EXPECT_CALL(udevice, getSysname()).WillRepeatedly(Return("nbd0"));
    EXPECT_CALL(udevice, getSysAttrValue(StrEq("size")))
        .WillRepeatedly(Return("1"));

    EXPECT_GLOBAL_CALL(mockCallback, mockCallback(_, StateChange::inserted));

    prepareCoroutine();
    run();

    EXPECT_EQ(monitor.getState(device), StateChange::inserted);
}

/* Process tests */
MOCK_GLOBAL_FUNC1(mockExitCallback, void(int));
MOCK_GLOBAL_FUNC0(mockExitCallback2, void(void));

class ProcessTest : public ::testing::Test
{
  protected:
    ProcessTest()
    {
        MockChild::engine = &child;
        process =
            std::make_shared<Process>(ioc, "Slot_0", "testProcess", device);
    }

    void run()
    {
        ioc.restart();
        ioc.run();
    }

    std::vector<std::string> args{"arg1", "arg2", "arg3"};
    const int ecSuccess = 0;
    const int ecFail = 130;
    NiceMock<MockChildEngine> child;
    boost::asio::io_context ioc;
    NBDDevice<> device{"nbd0"};
    std::shared_ptr<Process> process;
};

TEST_F(ProcessTest, ProcessRunsAndCallsCallbackOnExit)
{
    ASSERT_NE(process.get(), nullptr);

    EXPECT_CALL(child, running())
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(child, terminate()).Times(0);
    EXPECT_CALL(child, wait());
    EXPECT_CALL(child, exitCode()).WillRepeatedly(Return(ecSuccess));
    EXPECT_CALL(child, nativeExitCode()).WillRepeatedly(Return(ecSuccess));
    EXPECT_GLOBAL_CALL(mockExitCallback, mockExitCallback(_));

    process->spawn(args, mockExitCallback);

    run();
}

TEST_F(ProcessTest, ProcessTerminatesOnHang)
{
    ASSERT_NE(process.get(), nullptr);

    EXPECT_CALL(child, running()).WillRepeatedly(Return(true));
    EXPECT_CALL(child, terminate());
    EXPECT_CALL(child, wait());
    EXPECT_CALL(child, exitCode()).WillRepeatedly(Return(ecFail));
    EXPECT_CALL(child, nativeExitCode()).WillRepeatedly(Return(ecFail));
    EXPECT_GLOBAL_CALL(mockExitCallback, mockExitCallback(_));

    process->spawn(args, mockExitCallback);

    run();
}

TEST_F(ProcessTest, StopProcessCheckNicePath)
{
    ASSERT_NE(process.get(), nullptr);

    EXPECT_CALL(child, running())
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(child, terminate()).Times(0);
    EXPECT_GLOBAL_CALL(mockExitCallback2, mockExitCallback2()).Times(0);

    process->stop(mockExitCallback2);

    run();
}

TEST_F(ProcessTest, StopProcessCheckUglyPath)
{
    ASSERT_NE(process.get(), nullptr);

    EXPECT_CALL(child, running()).WillRepeatedly(Return(true));
    EXPECT_CALL(child, terminate());
    EXPECT_GLOBAL_CALL(mockExitCallback2, mockExitCallback2());

    process->stop(mockExitCallback2);

    run();
}

/* UsbGadget tests */
namespace fs = std::filesystem;

using utils::GadgetDirs;

class UsbGadgetTest : public ::testing::Test
{
  protected:
    UsbGadgetTest()
    {
        MockFilePrinter::engine = &printer;
        fs::create_directories(portPath);
    }

    ~UsbGadgetTest()
    {
        fs::remove_all(GadgetDirs::busPrefix());
    }

    NiceMock<MockFilePrinterEngine> printer;
    const std::string name{"Slot_0"};
    const fs::path gadgetDir = GadgetDirs::gadgetPrefix() + name;
    const fs::path portPath = GadgetDirs::busPrefix() + "/1";
};

TEST_F(UsbGadgetTest, ConfigurationFailsForUnknownDeviceState)
{
    EXPECT_EQ(
        UsbGadget::configure(name, "/dev/nbd0", StateChange::unknown, true),
        -1);
}

TEST_F(UsbGadgetTest, InsertFailsOnFsExceptions)
{
    EXPECT_CALL(printer, createDirs(_))
        .WillRepeatedly(Throw(
            fs::filesystem_error("Cannot create directory",
                                 std::make_error_code(std::errc::io_error))));
    EXPECT_CALL(printer, echo(_, _))
        .WillRepeatedly(Throw(std::ofstream::failure("Echo failed")));

    EXPECT_EQ(
        UsbGadget::configure(name, "/dev/nbd0", StateChange::inserted, true),
        -1);
}

TEST_F(UsbGadgetTest, RemoveFailsOnStreamException)
{
    EXPECT_CALL(printer, echo(gadgetDir / "UDC", StrEq("")))
        .WillOnce(Throw(std::ofstream::failure("Echo failed")));

    EXPECT_EQ(
        UsbGadget::configure(name, "/dev/nbd0", StateChange::removed, true),
        -1);
}

TEST_F(UsbGadgetTest, RemoveFailsDueToRemovalError)
{
    EXPECT_CALL(printer, echo(gadgetDir / "UDC", StrEq("")));
    EXPECT_CALL(printer, remove(_, _))
        .Times(AtLeast(1))
        .WillOnce(
            []([[maybe_unused]] const fs::path& path, std::error_code& ec) {
                ec = std::make_error_code(std::errc::no_such_file_or_directory);
            })
        .WillRepeatedly(Return());

    EXPECT_EQ(
        UsbGadget::configure(name, "/dev/nbd0", StateChange::removed, true),
        -1);
}

TEST_F(UsbGadgetTest, RemoveSucceeds)
{
    EXPECT_CALL(printer, echo(gadgetDir / "UDC", StrEq("")));
    EXPECT_CALL(printer, remove(_, _)).Times(AtLeast(1));

    EXPECT_EQ(
        UsbGadget::configure(name, "/dev/nbd0", StateChange::removed, true), 0);
}

TEST_F(UsbGadgetTest, StatisticsNotPresent)
{
    EXPECT_EQ(UsbGadget::getStats(name), std::nullopt);
}

TEST_F(UsbGadgetTest, StatisticsPresent)
{
    const fs::path statsDir = GadgetDirs::gadgetPrefix() + name +
                              "/functions/mass_storage.usb0/lun.0";

    fs::create_directories(statsDir);
    std::ofstream ofs{statsDir / "stats"};
    ofs << "testStats";
    ofs.close();

    EXPECT_EQ(UsbGadget::getStats(name), "testStats");

    fs::remove_all(gadgetDir);
}

class UsbGadgetInsertTest :
    public UsbGadgetTest,
    public ::testing::WithParamInterface<std::pair<bool, bool>>
{
  protected:
    const fs::path funcMassStorageDir =
        gadgetDir / "functions/mass_storage.usb0";
    const fs::path stringsDir = gadgetDir / "strings/0x409";
    const fs::path configDir = gadgetDir / "configs/c.1";
    const fs::path massStorageDir = configDir / "mass_storage.usb0";
    const fs::path configStringsDir = configDir / "strings/0x409";
};

TEST_P(UsbGadgetInsertTest, ConfigurationInsert)
{
    auto [rw, portAvailable] = GetParam();

    EXPECT_CALL(printer, createDirs(gadgetDir));
    EXPECT_CALL(printer, echo(gadgetDir / "idVendor", StrEq("0x1d6b")));
    EXPECT_CALL(printer, echo(gadgetDir / "idProduct", StrEq("0x0104")));
    EXPECT_CALL(printer, createDirs(stringsDir));
    EXPECT_CALL(printer,
                echo(StrEq(stringsDir / "manufacturer"), StrEq("OpenBMC")));
    EXPECT_CALL(printer,
                echo(stringsDir / "product", StrEq("Virtual Media Device")));
    EXPECT_CALL(printer, createDirs(configStringsDir));
    EXPECT_CALL(printer,
                echo(configStringsDir / "configuration", StrEq("config 1")));
    EXPECT_CALL(printer, createDirs(funcMassStorageDir));
    EXPECT_CALL(printer, createDirs(funcMassStorageDir / "lun.0"));
    EXPECT_CALL(printer, createDirSymlink(funcMassStorageDir, massStorageDir));
    EXPECT_CALL(printer,
                echo(funcMassStorageDir / "lun.0/removable", StrEq("1")));
    if (rw)
    {
        EXPECT_CALL(printer, echo(funcMassStorageDir / "lun.0/ro", StrEq("0")));
    }
    else
    {
        EXPECT_CALL(printer, echo(funcMassStorageDir / "lun.0/ro", StrEq("1")));
    }
    EXPECT_CALL(printer, echo(funcMassStorageDir / "lun.0/cdrom", StrEq("0")));
    EXPECT_CALL(printer,
                echo(funcMassStorageDir / "lun.0/file", StrEq("/dev/nbd0")));

    EXPECT_CALL(printer, isDir(portPath)).WillOnce(Return(true));
    EXPECT_CALL(printer, isSymlink(portPath)).WillOnce(Return(false));
    EXPECT_CALL(printer, exists(portPath / "gadget/suspended"))
        .WillOnce(Return(!portAvailable));
    if (portAvailable)
    {
        EXPECT_CALL(printer,
                    echo(gadgetDir / "UDC", portPath.filename().string()));
    }
    else
    {
        EXPECT_CALL(printer, echo(gadgetDir / "UDC", StrEq("")));
        EXPECT_CALL(printer, remove(_, _)).Times(AtLeast(1));
    }

    EXPECT_EQ(
        UsbGadget::configure(name, "/dev/nbd0", StateChange::inserted, rw),
        (portAvailable ? 0 : -1));
}

TEST_P(UsbGadgetInsertTest, IsGadgetConfigured)
{
    bool portAvailable = GetParam().second;

    EXPECT_CALL(printer, exists(gadgetDir)).WillOnce(Return(portAvailable));

    EXPECT_EQ(UsbGadget::isConfigured(name), portAvailable);
}

INSTANTIATE_TEST_SUITE_P(UsbGadgetTest, UsbGadgetInsertTest,
                         Values(
                             //  [ rw, portAvailable ]
                             std::make_pair(false, false),
                             std::make_pair(false, true),
                             std::make_pair(true, false),
                             std::make_pair(true, true)));

/* UdevGadget tests */
class UdevGadgetTest : public ::testing::Test
{
  protected:
    UdevGadgetTest()
    {
        MockFilePrinter::engine = &printer;
    }

    MockFilePrinterEngine printer;
    const std::string changeStr = "change";
};

TEST_F(UdevGadgetTest, ChangeStringIsPrintedToFiles)
{
    EXPECT_CALL(printer, echo(StrEq("/sys/block/nbd0/uevent"), changeStr));
    EXPECT_CALL(printer, echo(StrEq("/sys/block/nbd1/uevent"), changeStr));
    EXPECT_CALL(printer, echo(StrEq("/sys/block/nbd2/uevent"), changeStr));
    EXPECT_CALL(printer, echo(StrEq("/sys/block/nbd3/uevent"), changeStr));

    EXPECT_EQ(UdevGadget::forceUdevChange(), true);
}

TEST_F(UdevGadgetTest, WriteFailsOnException)
{
    EXPECT_CALL(printer, echo(_, _))
        .WillOnce(Throw(std::ofstream::failure("Echo failed")));

    EXPECT_EQ(UdevGadget::forceUdevChange(), false);
}

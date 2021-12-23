#include "mocks/child_mock.hpp"
#include "mocks/udev_mock.hpp"
#include "mocks/utils_mock.hpp"
#include "system.hpp"

#include <fstream>
#include <functional>
#include <ios>
#include <vector>

#include <gmock-global/gmock-global.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

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

#pragma once

#include "mocks/file_printer_mock.hpp"
#include "mocks/utils_mock.hpp"
#include "state_machine.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using MockWrapper = testing::NiceMock<utils::MockNotificationWrapper>;

struct TestableMountPointStateMachine : public MountPointStateMachine
{
    TestableMountPointStateMachine(boost::asio::io_context& ioc,
                                   DeviceMonitor& devMonitor,
                                   const std::string& name,
                                   const Configuration::MountPoint& config) :
        MountPointStateMachine(ioc, devMonitor, name, config)
    {
        bus = std::make_shared<sdbusplus::asio::connection>(ioc);
        wrapper = std::make_unique<MockWrapper>();
    }

    void notificationInitialize(
        [[maybe_unused]] std::shared_ptr<sdbusplus::asio::connection> con,
        [[maybe_unused]] const std::string& svc,
        [[maybe_unused]] const std::string& iface,
        [[maybe_unused]] const std::string& name) override
    {
        completionNotification = std::move(wrapper);
    }

    std::shared_ptr<sdbusplus::asio::connection> bus;
    std::unique_ptr<MockWrapper> wrapper;
};

class StateMachineTestBase
{
  protected:
    StateMachineTestBase()
    {
        MockFilePrinter::engine = &printer;
    }

    testing::NiceMock<MockFilePrinterEngine> printer;
    boost::asio::io_context ioc;
    NBDDevice<> dev{"nbd0"};
    DeviceMonitor monitor{ioc};
    Configuration::MountPoint mp{dev,
                                 "tests/run/virtual-media/nbd0.sock",
                                 "/nbd/0",
                                 30,
                                 512,
                                 std::chrono::seconds(0),
                                 Configuration::Mode::proxy};
    TestableMountPointStateMachine mpsm{ioc, monitor, "Slot_0", mp};
};

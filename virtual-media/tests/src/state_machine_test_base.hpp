#pragma once

#include "configuration.hpp"
#include "mocks/file_printer_mock.hpp"
#include "mocks/utils_mock.hpp"
#include "state_machine.hpp"
#include "system.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>

namespace virtual_media_test
{

using MockWrapper = testing::NiceMock<MockNotificationWrapper>;

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
    Configuration::MountPoint mp{dev, "tests/run/virtual-media/nbd0.sock",
                                 "/nbd/0", Configuration::Mode::proxy};
    TestableMountPointStateMachine mpsm{ioc, monitor, "Slot_0", mp};
};

} // namespace virtual_media_test

#pragma once

#include "mocks/utils_mock.hpp"
#include "state_machine.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

struct TestableMountPointStateMachine : public MountPointStateMachine
{
    TestableMountPointStateMachine(boost::asio::io_context& ioc,
                                   const std::string& name,
                                   const Configuration::MountPoint& config) :
        MountPointStateMachine(ioc, name, config)
    {
        bus = std::make_shared<sdbusplus::asio::connection>(ioc);
    }

    std::shared_ptr<sdbusplus::asio::connection> bus;
};

class StateMachineTestBase
{
  protected:
    boost::asio::io_context ioc;
    NBDDevice<> dev{"nbd0"};
    Configuration::MountPoint mp{dev,
                                 "tests/run/virtual-media/nbd0.sock",
                                 "/nbd/0",
                                 30,
                                 512,
                                 std::chrono::seconds(0),
                                 Configuration::Mode::proxy};
    TestableMountPointStateMachine mpsm{ioc, "Slot_0", mp};
};

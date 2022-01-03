#include "mocks/child_mock.hpp"
#include "state/activating_state.hpp"
#include "state_machine.hpp"
#include "state_machine_test_base.hpp"

#include <chrono>
#include <filesystem>
#include <system_error>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Throw;
using ::testing::Values;

namespace fs = std::filesystem;

enum class Protocol
{
    smb
};

class ActivatingStateBasicTest :
    public StateMachineTestBase,
    public ::testing::TestWithParam<std::pair<Configuration::Mode, Protocol>>
{
  protected:
    ActivatingStateBasicTest()
    {
        auto [mode, protocol] = GetParam();
        mpsm.config.mode = mode;
        if (mode == Configuration::Mode::legacy)
        {
            switch (protocol)
            {
                case Protocol::smb:
                    mpsm.target = std::move(target_smb);
                    break;
            }
        }

        mpsm.notificationInitialize(
            mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
            "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");

        MockChild::engine = &child;
    }

    void changeState()
    {
        mpsm.changeState(std::make_unique<ActivatingState>(mpsm));
    }

    void run()
    {
        ioc.restart();
        ioc.run_for(std::chrono::milliseconds(5));
    }

    NiceMock<MockChildEngine> child;
    interfaces::MountPointStateMachine::Target target_smb{
        "smb://192.168.10.101:445/test.iso", true, nullptr, nullptr};
};

TEST_P(ActivatingStateBasicTest, UnableToSpawnProcess)
{
    EXPECT_CALL(child, create())
        .WillOnce(Throw(std::system_error(
            std::make_error_code(std::errc::no_such_process))));

    changeState();
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

TEST_P(ActivatingStateBasicTest, SpawnedProcess)
{
    EXPECT_CALL(child, create());

    changeState();
    EXPECT_EQ(mpsm.getState().getStateName(), "ActivatingState");
}

TEST_P(ActivatingStateBasicTest, CallbackExecuted)
{
    EXPECT_CALL(child, create());

    changeState();
    EXPECT_EQ(mpsm.getState().getStateName(), "ActivatingState");

    run();
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

INSTANTIATE_TEST_SUITE_P(
    ActivatingStateTest, ActivatingStateBasicTest,
    Values(std::make_pair(Configuration::Mode::proxy, Protocol::smb),
           std::make_pair(Configuration::Mode::legacy, Protocol::smb)));

class ActivatingStateLegacySmbTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  protected:
    ActivatingStateLegacySmbTest()
    {
        MockChild::engine = &child;

        mpsm.config.mode = Configuration::Mode::legacy;
        mpsm.target = interfaces::MountPointStateMachine::Target{
            "smb://192.168.10.101:445/test.iso", true, nullptr, nullptr};
        mpsm.notificationInitialize(
            mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
            "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");

        fs::remove_all(socketParent);
    }

    void changeState()
    {
        mpsm.changeState(std::make_unique<ActivatingState>(mpsm));
    }

    const fs::path socketPath = mpsm.config.unixSocket;
    const fs::path socketParent = socketPath.parent_path();
    NiceMock<MockChildEngine> child;
};

TEST_F(ActivatingStateLegacySmbTest, SocketFolderCreated)
{
    EXPECT_CALL(child, create());

    changeState();

    EXPECT_TRUE(fs::exists(socketParent));
    EXPECT_TRUE(fs::status(socketParent).permissions() == fs::perms::owner_all);

    EXPECT_EQ(mpsm.getState().getStateName(), "ActivatingState");
}

TEST_F(ActivatingStateLegacySmbTest, CannotCreateFolder)
{
    mpsm.config.unixSocket = "/" + mpsm.config.unixSocket;

    changeState();
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}
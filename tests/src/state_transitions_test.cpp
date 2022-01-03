#include "state/activating_state.hpp"
#include "state/active_state.hpp"
#include "state/deactivating_state.hpp"
#include "state/initial_state.hpp"
#include "state/ready_state.hpp"
#include "state_machine_test_base.hpp"

using ::testing::Eq;
/*
 Scheme for states and transitions

                    ┌─────────────────┐
                    │                 │
                    │  initial state  │
                    │                 │
                    └──────┬──────────┘
                           │
                           │RegisterDbusEvent
                           │
                ┌──────────▼────────────┐
                │                       │
                │     ready state       │
                │                       │
                └───┬──────▲────────▲───┘
                    │      │        │
        ┌───────────┘      │        └───────────────┐
        │                  │                        │SubprocessStoppedEvent &&
        │MountEvent        │SubprocessStoppedEvent  │UdevStateChangeEvent
        │                  │                        │
┌───────┴──────────────┐   │                        │
│                      ├───┘                  ┌─────┴────────────────┐
│   activating state   │ UdevStateChangeEvent │                      │
│                      ├──────────────────────►  deactivating state  │
└───────┬──────────────┘       (!insert)      │                      │
        │                                     └──────▲───────────────┘
        │                                            │
        │UdevStateChangeEvent                        │SubprocessStoppedEvent ||
        │      (insert)                              │UdevStateChangeEvent ||
        │                                            │UnmountEvent
        │           ┌──────────────────────┐         │
        │           │                      │         │
        └───────────►     active state     ├─────────┘
                    │                      │
                    └──────────────────────┘
*/

/* InitialState transitions */
class InitialStateTransitionTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  protected:
    void SetUp() override
    {
        initialAssertion();
    }

    void initialAssertion()
    {
        ASSERT_EQ(mpsm.getState().getStateName(), "InitialState");
    }
};

TEST_F(InitialStateTransitionTest, RegisterDBusEventEmitted)
{
    mpsm.emitRegisterDBusEvent(mpsm.bus, std::make_shared<object_server>());
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

TEST_F(InitialStateTransitionTest, MountEventEmitted)
{
    interfaces::MountPointStateMachine::Target target{};
    mpsm.emitEvent(MountEvent(std::move(target)));
    EXPECT_EQ(mpsm.getState().getStateName(), "InitialState");
}

TEST_F(InitialStateTransitionTest, UnmountEventEmitted)
{
    mpsm.emitEvent(UnmountEvent());
    EXPECT_EQ(mpsm.getState().getStateName(), "InitialState");
}

TEST_F(InitialStateTransitionTest, SubprocessStoppedEventEmitted)
{
    mpsm.emitSubprocessStoppedEvent();
    EXPECT_EQ(mpsm.getState().getStateName(), "InitialState");
}

TEST_F(InitialStateTransitionTest, UdevStateChangeEventEmitted)
{
    mpsm.emitUdevStateChangeEvent(dev, StateChange::notMonitored);
    EXPECT_EQ(mpsm.getState().getStateName(), "InitialState");
}

/* ReadyState transitions */
class ReadyStateTransitionTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mpsm.notificationInitialize(
            mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
            "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");
        std::unique_ptr<BasicState> newState =
            std::make_unique<ReadyState>(mpsm);
        mpsm.changeState(std::move(newState));

        initialAssertion();
    }

    void initialAssertion()
    {
        ASSERT_EQ(mpsm.getState().getStateName(), "ReadyState");
    }
};

TEST_F(ReadyStateTransitionTest, RegisterDbusEventEmitted)
{
    mpsm.emitRegisterDBusEvent(mpsm.bus, std::make_shared<object_server>());
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

TEST_F(ReadyStateTransitionTest, MountEventEmitted)
{
    mpsm.config.mode = static_cast<Configuration::Mode>(0xff);

    interfaces::MountPointStateMachine::Target target{};
    mpsm.emitEvent(MountEvent(std::move(target)));

    EXPECT_EQ(mpsm.getState().getStateName(), "ActivatingState");
}

TEST_F(ReadyStateTransitionTest, UnmountEventEmitted)
{
    EXPECT_THROW(mpsm.emitEvent(UnmountEvent()),
                 sdbusplus::exception::SdBusError);
}

TEST_F(ReadyStateTransitionTest, SubprocessStoppedEventEmitted)
{
    mpsm.emitSubprocessStoppedEvent();
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

TEST_F(ReadyStateTransitionTest, UdevStateChangeEventEmitted)
{
    mpsm.emitUdevStateChangeEvent(dev, StateChange::notMonitored);
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

/* ActivatingState transitions */
class ActivatingStateTransitionTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mpsm.config.mode = static_cast<Configuration::Mode>(0xff);
        mpsm.notificationInitialize(
            mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
            "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");

        std::unique_ptr<BasicState> newState =
            std::make_unique<ActivatingState>(mpsm);
        mpsm.changeState(std::move(newState));

        initialAssertion();
    }

    void initialAssertion()
    {
        ASSERT_EQ(mpsm.getState().getStateName(), "ActivatingState");
    }
};

TEST_F(ActivatingStateTransitionTest, RegisterDbusEventEmitted)
{
    EXPECT_THROW(
        mpsm.emitRegisterDBusEvent(mpsm.bus, std::make_shared<object_server>()),
        sdbusplus::exception::SdBusError);
}

TEST_F(ActivatingStateTransitionTest, MountEventEmitted)
{
    interfaces::MountPointStateMachine::Target target{};
    EXPECT_THROW(mpsm.emitEvent(MountEvent(std::move(target))),
                 sdbusplus::exception::SdBusError);
}

TEST_F(ActivatingStateTransitionTest, UnmountEventEmitted)
{
    EXPECT_THROW(mpsm.emitEvent(UnmountEvent()),
                 sdbusplus::exception::SdBusError);
}

TEST_F(ActivatingStateTransitionTest, SubprocessStoppedEventEmitted)
{
    mpsm.emitSubprocessStoppedEvent();
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

TEST_F(ActivatingStateTransitionTest, UdevStateChangeEventInsertEmitted)
{
    mpsm.emitUdevStateChangeEvent(dev, StateChange::inserted);
    EXPECT_EQ(mpsm.getState().getStateName(), "ActiveState");
}

TEST_F(ActivatingStateTransitionTest, UdevStateChangeEventRemoveEmitted)
{
    mpsm.emitUdevStateChangeEvent(dev, StateChange::removed);
    EXPECT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
}

/* ActiveState transitions */
class ActiveStateTransitionTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mpsm.notificationInitialize(
            mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
            "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");

        std::unique_ptr<BasicState> newState =
            std::make_unique<ActiveState>(mpsm, nullptr, nullptr);
        mpsm.changeState(std::move(newState));
    }

    void initialAssertion()
    {
        ASSERT_EQ(mpsm.getState().getStateName(), "ActiveState");
    }
};

TEST_F(ActiveStateTransitionTest, RegisterDbusEventEmitted)
{
    EXPECT_THROW(
        mpsm.emitRegisterDBusEvent(mpsm.bus, std::make_shared<object_server>()),
        sdbusplus::exception::SdBusError);
}

TEST_F(ActiveStateTransitionTest, MountEventEmitted)
{
    interfaces::MountPointStateMachine::Target target{};

    EXPECT_THROW(mpsm.emitEvent(MountEvent(std::move(target))),
                 sdbusplus::exception::SdBusError);
}

TEST_F(ActiveStateTransitionTest, UnmountEventEmitted)
{
    mpsm.emitEvent(UnmountEvent());
    EXPECT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
}

TEST_F(ActiveStateTransitionTest, SubprocessStoppedEventEmitted)
{
    mpsm.emitSubprocessStoppedEvent();
    EXPECT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
}

TEST_F(ActiveStateTransitionTest, UdevStateChangeEventEmitted)
{
    mpsm.emitUdevStateChangeEvent(dev, StateChange::notMonitored);
    EXPECT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
}

/* DeactivatingState transitions */

class DeactivatingStateTransitionTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  protected:
    void SetUp() override
    {
        std::unique_ptr<BasicState> newState =
            std::make_unique<DeactivatingState>(mpsm, nullptr, nullptr);
        mpsm.changeState(std::move(newState));

        initialAssertion();
    }

    void initialAssertion()
    {
        ASSERT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
    }
};

TEST_F(DeactivatingStateTransitionTest, RegisterDbusEventEmitted)
{
    EXPECT_THROW(
        mpsm.emitRegisterDBusEvent(mpsm.bus, std::make_shared<object_server>()),
        sdbusplus::exception::SdBusError);
}

TEST_F(DeactivatingStateTransitionTest, MountEventEmitted)
{
    interfaces::MountPointStateMachine::Target target{};

    EXPECT_THROW(mpsm.emitEvent(MountEvent(std::move(target))),
                 sdbusplus::exception::SdBusError);
}

TEST_F(DeactivatingStateTransitionTest, UnmountEventEmitted)
{
    EXPECT_THROW(mpsm.emitEvent(UnmountEvent()),
                 sdbusplus::exception::SdBusError);
}

TEST_F(DeactivatingStateTransitionTest, SubprocessStoppedEventEmitted)
{
    mpsm.emitSubprocessStoppedEvent();
    EXPECT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
}

TEST_F(DeactivatingStateTransitionTest, UdevStateChangeEventEmitted)
{
    mpsm.emitUdevStateChangeEvent(dev, StateChange::removed);
    EXPECT_EQ(mpsm.getState().getStateName(), "DeactivatingState");
}

TEST_F(DeactivatingStateTransitionTest,
       SubprocessStoppedEventAndUdevStateChangeEventEmitted)
{
    mpsm.notificationInitialize(
        mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",

        "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");
    mpsm.emitUdevStateChangeEvent(dev, StateChange::removed);
    mpsm.emitSubprocessStoppedEvent();
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

TEST_F(DeactivatingStateTransitionTest,
       SubprocessStoppedEventAndUdevStateChangeEventEmittedWithWrongValue)
{
    const std::error_code ec =
        std::make_error_code(std::errc::connection_refused);
    EXPECT_CALL(*(mpsm.wrapper), notify(testing::Eq(ec))).Times(1);
    mpsm.notificationInitialize(
        mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
        "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");
    // mpsm.notify(ec);
    mpsm.emitSubprocessStoppedEvent();
    mpsm.emitUdevStateChangeEvent(dev, StateChange::notMonitored);
    EXPECT_EQ(mpsm.getState().getStateName(), "ReadyState");
}

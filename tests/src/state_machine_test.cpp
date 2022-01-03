#include "mocks/test_states.hpp"
#include "state_machine_test_base.hpp"
#include "types/dbus_types.hpp"

using ::testing::_;
using ::testing::Eq;

class StateMachineTest : public StateMachineTestBase, public ::testing::Test
{
  protected:
    void SetUp() override
    {
        initialAssertion();
    }

    void prepareForEmitEvents()
    {
        std::unique_ptr<BasicState> newState =
            std::make_unique<TestStateI>(mpsm);
        mpsm.changeState(std::move(newState));
    }

    void initialAssertion()
    {
        ASSERT_EQ(mpsm.getState().getStateName(), "InitialState");
    }
};

TEST_F(StateMachineTest, StateChangeSuccessful)
{
    std::unique_ptr<BasicState> newState = std::make_unique<TestStateA>(mpsm);
    mpsm.changeState(std::move(newState));

    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateA");
}

TEST_F(StateMachineTest, StateChangeWithAdditionalChangeOnEnterSuccessful)
{
    // when entering TestStateC, additional change to TestStateB should
    // be invoked
    std::unique_ptr<BasicState> newState = std::make_unique<TestStateC>(mpsm);
    mpsm.changeState(std::move(newState));

    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateB");
}

TEST_F(StateMachineTest, EmitEventWithoutStateChange)
{
    std::unique_ptr<BasicState> newState = std::make_unique<TestStateA>(mpsm);
    mpsm.changeState(std::move(newState));
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateA");

    // emit some event; this shouldn't cause state change when being in
    // TestStateA
    mpsm.emitEvent(SubprocessStoppedEvent());
    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateA");
}

// In order to check events' emission, TestStateI will be used.
// While in TestStateI, state transition shall occur depending on emitted event:
// RegisterDbusEvent        -> TestStateD,
// MountEvent               -> TestStateE,
// UnmountEvent             -> TestStateF,
// SubprocessStoppedEvent   -> TestStateG,
// UdevStateChangeEvent     -> TestStateH
TEST_F(StateMachineTest, EmitRegisterDBusEvent)
{
    prepareForEmitEvents();
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    auto objServer = std::make_shared<object_server>();
    mpsm.emitEvent(RegisterDbusEvent(mpsm.bus, objServer));
    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateD");
}

TEST_F(StateMachineTest, EmitMountEvent)
{
    prepareForEmitEvents();
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    interfaces::MountPointStateMachine::Target target{};
    mpsm.emitEvent(MountEvent(std::move(target)));
    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateE");
}

TEST_F(StateMachineTest, EmitUnmountEvent)
{
    prepareForEmitEvents();
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    mpsm.emitEvent(UnmountEvent());
    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateF");
}

TEST_F(StateMachineTest, EmitSubprocessStoppedEvent)
{
    prepareForEmitEvents();
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    mpsm.emitEvent(SubprocessStoppedEvent());
    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateG");
}

TEST_F(StateMachineTest, EmitUdevStateChangeEvent)
{
    prepareForEmitEvents();
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    mpsm.emitEvent(UdevStateChangeEvent(StateChange::notMonitored));
    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateH");
}

TEST_F(StateMachineTest, NotificationStarted)
{
    EXPECT_CALL(*(mpsm.wrapper), start(_, _)).Times(1);

    mpsm.notificationInitialize(
        mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
        "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");
    mpsm.notificationStart();
}

TEST_F(StateMachineTest, NotificationCaught)
{
    const std::error_code ec =
        std::make_error_code(std::errc::connection_refused);
    EXPECT_CALL(*(mpsm.wrapper), notify(Eq(ec))).Times(1);

    mpsm.notificationInitialize(
        mpsm.bus, "/xyz/openbmc_project/VirtualMedia/Test",
        "xyz.openbmc_project.VirtualMedia.Test", "TestSignal");
    mpsm.notify(ec);
}

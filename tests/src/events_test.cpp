#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "resources.hpp"

#include <gtest/gtest.h>

class EventsTest : public ::testing::Test
{
  protected:
    interfaces::MountPointStateMachine::Target target{
        "smb://192.168.10.101:445/test.iso", true, nullptr, nullptr};
    MountEvent event{std::move(target)};
};

TEST_F(EventsTest, MoveConstructorMountEventSuccesfull)
{
    MountEvent eventB{std::move(event)};
    ASSERT_EQ(event.target, std::nullopt);
    ASSERT_NE(eventB.target, std::nullopt);
}

TEST_F(EventsTest, MoveAssignmentMountEventSuccesfull)
{
    MountEvent eventB = std::move(event);
    ASSERT_EQ(event.target, std::nullopt);
    ASSERT_NE(eventB.target, std::nullopt);
}

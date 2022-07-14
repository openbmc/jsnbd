#include "mocks/utils_mock.hpp"
#include "system.hpp"

#include <fstream>
#include <functional>
#include <ios>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::NiceMock;

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

#include "environments/dbus_environment.hpp"

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    auto* env = new virtual_media_test::DbusEnvironment;

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(env);
    auto ret = RUN_ALL_TESTS();

    return ret;
}

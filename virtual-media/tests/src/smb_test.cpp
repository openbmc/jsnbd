#include "fakes/mounter_dirs_fake.hpp"
#include "smb.hpp"
#include "utils/utils.hpp"

#include <memory>

#include <gtest/gtest.h>

class SmbShareTest : public ::testing::Test
{
  protected:
    SmbShareTest() : smb(directories::mountDir3)
    {
        credentials =
            std::make_unique<utils::CredentialsProvider>("user", "1234");
    }

    std::unique_ptr<utils::CredentialsProvider> credentials;
    SmbShare smb;
};

TEST_F(SmbShareTest, MountWithoutCredentials)
{
    EXPECT_NE(smb.mount(directories::remoteDir, true, nullptr), nullptr);
}

TEST_F(SmbShareTest, InvalidUsername)
{
    EXPECT_EQ(smb.mount(directories::remoteDir, true,
                        std::make_unique<utils::CredentialsProvider>(
                            "user,withcomma", "1234")),
              nullptr);
}

TEST_F(SmbShareTest, MountWithCredentials)
{
    EXPECT_NE(smb.mount(directories::remoteDir, true, credentials), nullptr);
}

TEST_F(SmbShareTest, MountWithDifferentVersion)
{
    SmbShare smb1(directories::mountDir0);
    EXPECT_NE(smb1.mount(directories::remoteDir, true, credentials), nullptr);
}

TEST_F(SmbShareTest, MountFailed)
{
    SmbShare smb2(directories::mountDir1);
    EXPECT_EQ(smb2.mount(directories::remoteDir, true, credentials), nullptr);
}

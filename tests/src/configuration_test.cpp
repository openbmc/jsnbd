#include "configuration.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <gtest/gtest.h>

namespace virtual_media_test
{

TEST(ConfigurationLoadingTest, CfgInvalidForNotExistingFile)
{
    Configuration nonExistent{"non-existent.json"};
    EXPECT_EQ(nonExistent.valid, false);
}

class ConfigurationTest : public ::testing::Test
{
  protected:
    static constexpr const char* configFile = "virtual-media.json";
    static constexpr const char* configNbd15 = "virtual-media-nbd15.json";
    static constexpr const char* configNbd16 = "virtual-media-nbd16.json";
    static constexpr const char* configNonExisting =
        "virtual-media-invalid.json";
    static constexpr const char* configMissingNbd =
        "virtual-media-missing-nbd.json";
    static constexpr std::array<const char*, 4> slotList = {"Slot_0", "Slot_1",
                                                            "Slot_2", "Slot_3"};
    Configuration config{constructPath(configFile)};

  public:
    static std::string constructPath(const std::string& file)
    {
        std::string dataDir = TEST_DATA_DIR;
        return dataDir + "/" + file;
    }

    static void corruptJsonFile(const std::string& filePath)
    {
        std::ofstream file;
        file.open(filePath, std::ofstream::out | std::ofstream::trunc);
        file << "corruption";
        file.close();
    }
};

TEST_F(ConfigurationTest, GetMountPointsSuccessfully)
{
    ASSERT_EQ(config.valid, true);
    for (const auto& i : slotList)
    {
        ASSERT_EQ(config.getMountPoint(i), &config.mountPoints[i]);
    }
}

TEST_F(ConfigurationTest, GetMountPointNBDDeviceSuccessfully)
{
    Configuration nbd15{constructPath(configNbd15)};
    const Configuration::MountPoint* mp =
        nbd15.getMountPoint(slotList[0]).value();
    ASSERT_EQ(mp->nbdDevice.toString(),
              (&nbd15.mountPoints[slotList[0]])->nbdDevice.toString());
}

TEST_F(ConfigurationTest, GetMountPointNBDDeviceUnsuccessfully)
{
    Configuration nbd16{constructPath(configNbd16)};
    ASSERT_EQ(nbd16.getMountPoint(slotList[0]), std::nullopt);
}

TEST_F(ConfigurationTest, GetMountPointNBDDeviceMissing)
{
    Configuration missingNbd{constructPath(configMissingNbd)};
    ASSERT_EQ(missingNbd.getMountPoint(slotList[0]), std::nullopt);
}

TEST_F(ConfigurationTest, CfgLoadedSuccessfully)
{
    ASSERT_EQ(config.valid, true);
}

TEST_F(ConfigurationTest, CfgInvalidForInvalidJsonFile)
{
    std::string invalidPath = constructPath(configNonExisting);
    corruptJsonFile(invalidPath);
    Configuration notValidJson{invalidPath};

    EXPECT_EQ(notValidJson.valid, false);
    std::filesystem::remove(invalidPath);
}

TEST_F(ConfigurationTest, GetMountPointInvalidForNotExistingPoint)
{
    EXPECT_EQ(config.getMountPoint("non-existing"), std::nullopt);
}

} // namespace virtual_media_test

#include "configuration.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

using namespace std;

chrono::seconds Configuration::inactivityTimeout;

TEST(ConfigurationLoadingTest, CfgInvalidForNotExistingFile)
{
    Configuration nonExistent{"non-existent.json"};
    EXPECT_EQ(nonExistent.valid, false);
}

class ConfigurationTest : public ::testing::Test
{
  protected:
    const Configuration::MountPoint* mp;
    const std::string virtual_media_json_path = "virtual-media.json";
    const std::string virtual_media_json_path_nbd15 =
        "virtual-media-nbd15.json";
    const std::string virtual_media_json_path_nbd16 =
        "virtual-media-nbd16.json";
    const std::string virtual_media_json_invalid_path =
        "virtual-media-invalid.json";
    const std::string virtual_media_json_missing_nbd =
        "virtual-media-missing-nbd.json";
    const char* slot_list[4] = {"Slot_0", "Slot_1", "Slot_2", "Slot_3"};
    Configuration config{constructPath(virtual_media_json_path)};

  public:
    inline std::string constructPath(const std::string& file)
    {
        return "/tmp/" + file;
    }

    void corruptJsonFile(const std::string& filePath)
    {
        ofstream file;
        file.open(filePath, ofstream::out | ofstream::trunc);
        file << "corruption";
        file.close();
    }
};

TEST_F(ConfigurationTest, GetMountPointsSuccessfully)
{
    ASSERT_EQ(config.valid, true);
    for (auto& i : slot_list)
    {
        ASSERT_EQ(config.getMountPoint(i), &config.mountPoints[i]);
    }
}

TEST_F(ConfigurationTest, GetMountPointNBDDeviceSuccessfully)
{
    Configuration nbd15{constructPath(virtual_media_json_path_nbd15)};
    mp = nbd15.getMountPoint(slot_list[0]).value();
    ASSERT_EQ(mp->nbdDevice.toString(),
              (&nbd15.mountPoints[slot_list[0]])->nbdDevice.toString());
}

TEST_F(ConfigurationTest, GetMountPointNBDDeviceUnsuccessfully)
{
    Configuration nbd16{constructPath(virtual_media_json_path_nbd16)};
    ASSERT_EQ(nbd16.getMountPoint(slot_list[0]), nullopt);
}

TEST_F(ConfigurationTest, GetMountPointNBDDeviceMissing)
{
    Configuration missingNbd{constructPath(virtual_media_json_missing_nbd)};
    ASSERT_EQ(missingNbd.getMountPoint(slot_list[0]), nullopt);
}

TEST_F(ConfigurationTest, CfgLoadedSuccessfully)
{
    ASSERT_EQ(config.valid, true);
}

TEST_F(ConfigurationTest, CfgInvalidForInvalidJsonFile)
{
    std::string invalidPath = constructPath(virtual_media_json_invalid_path);
    corruptJsonFile(invalidPath);
    Configuration notValidJson{invalidPath};

    EXPECT_EQ(notValidJson.valid, false);
    filesystem::remove(invalidPath);
}

TEST_F(ConfigurationTest, GetMountPointInvalidForNotExistingPoint)
{
    EXPECT_EQ(config.getMountPoint("non-existing"), nullopt);
}

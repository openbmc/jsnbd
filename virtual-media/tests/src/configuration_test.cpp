#include "configuration.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ostream>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace virtual_media_test
{

class ConfigGenerator
{
  public:
    ConfigGenerator(size_t proxyMountPoints, size_t standardMountPoints)
    {
        config["ProxyMountPoints"] = proxyMountPoints;
        config["StandardMountPoints"] = standardMountPoints;

        std::ofstream file(configPath());
        if (file.is_open())
        {
            file << config << std::endl;
            file.close();
        }
    }

    ~ConfigGenerator()
    {
        std::filesystem::remove(configPath());
    }

    ConfigGenerator(const ConfigGenerator&) = delete;
    ConfigGenerator(ConfigGenerator&&) = delete;
    ConfigGenerator& operator=(const ConfigGenerator&) = delete;
    ConfigGenerator& operator=(ConfigGenerator&&) = delete;

    static std::string configPath()
    {
        return "test-config.json";
    }

  private:
    nlohmann::json config;
};

TEST(ConfigurationTestBasic, FailureWhenNonexistentFile)
{
    Configuration config("non-existent.json");
    EXPECT_FALSE(config.valid);
}

TEST(ConfigurationTestBasic, FailureWhenParseFails)
{
    std::string filePath = "wrong.json";
    std::ofstream file(filePath);

    ASSERT_TRUE(file.is_open());
    file << "Not a JSON data" << std::endl;
    file.close();

    Configuration config(filePath);
    EXPECT_FALSE(config.valid);

    std::filesystem::remove(filePath);
}

class ConfigurationTest :
    public ::testing::TestWithParam<std::pair<size_t, size_t>>
{
  protected:
    ConfigurationTest() :
        testParams{GetParam()}, generator{testParams.first, testParams.second},
        config{ConfigGenerator::configPath()}
    {
        EXPECT_TRUE(config.valid);
    }

    std::pair<size_t, size_t> testParams;
    ConfigGenerator generator;
    Configuration config;
};

TEST_P(ConfigurationTest, MountPointCountIsValid)
{
    size_t expectedCount = testParams.first + testParams.second;

    EXPECT_EQ(config.mountPoints.size(), expectedCount);
}

TEST_P(ConfigurationTest, MountPointsCanBeObtainedBySlotName)
{
    size_t totalCount = testParams.first + testParams.second;

    for (size_t i = 0; i < totalCount; i++)
    {
        std::string slotName = std::format("Slot_{}", i);
        auto mountPoint = config.getMountPoint(slotName);

        EXPECT_NE(mountPoint, std::nullopt);
        EXPECT_EQ(mountPoint, &config.mountPoints[slotName]);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ValidConfigurations, ConfigurationTest,
    ::testing::Values(std::make_pair(0, 2), std::make_pair(2, 0),
                      std::make_pair(2, 2), std::make_pair(4, 4),
                      std::make_pair(8, 8), std::make_pair(0, 16),
                      std::make_pair(16, 0)));

TEST(ConfigurationMountPointTest, NonExistingMountPointIsNotFound)
{
    ConfigGenerator generator(2, 2);
    Configuration config(ConfigGenerator::configPath());

    EXPECT_TRUE(config.valid);

    auto mountPoint = config.getMountPoint("Slot_5");
    EXPECT_EQ(mountPoint, std::nullopt);
}

TEST(ConfigurationMountPointTest,
     FailureWhenTotalMountPointCountIsGreaterThanExpectedMax)
{
    ConfigGenerator generator(8, 9);
    Configuration config(ConfigGenerator::configPath());

    EXPECT_FALSE(config.valid);
}

} // namespace virtual_media_test

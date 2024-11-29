#include "environments/dbus_environment.hpp"
#include "state_machine.hpp"

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace virtual_media_test
{

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Types;

using MethodCallback =
    std::function<void(const boost::system::error_code&, bool)>;

static constexpr const char* mountPointIface{
    "xyz.openbmc_project.VirtualMedia.MountPoint"};
static constexpr const char* processIface{
    "xyz.openbmc_project.VirtualMedia.Process"};

class ProxyMountPointScenario
{
  public:
    ProxyMountPointScenario() :
        nbdDev{"nbd0"}, mp{nbdDev, "tests/run/virtual-media/nbd0.sock",
                           "/nbd/0", Configuration::Mode::proxy},
        mpsm{ioc, "Slot_0", mp}
    {
        mpsm.emitRegisterDBusEvent(DbusEnvironment::getBus(),
                                   DbusEnvironment::getObjServer());
    }

    Configuration::MountPoint& getMountPoint()
    {
        return mp;
    }

    void doMount(MethodCallback&& callback)
    {
        DbusEnvironment::getBus()->async_method_call(
            std::move(callback), DbusEnvironment::serviceName(), objectPath,
            serviceIface, "Mount");
    }

    void doUnmount(MethodCallback&& callback)
    {
        DbusEnvironment::getBus()->async_method_call(
            std::move(callback), DbusEnvironment::serviceName(), objectPath,
            serviceIface, "Unmount");
    }

    const std::string objectPath{
        "/xyz/openbmc_project/VirtualMedia/Proxy/Slot_0"};
    const std::string serviceIface{"xyz.openbmc_project.VirtualMedia.Proxy"};

  private:
    boost::asio::io_context ioc;
    NBDDevice<> nbdDev;
    Configuration::MountPoint mp;
    MountPointStateMachine mpsm;
};

class StandardMountPointScenario
{
  public:
    StandardMountPointScenario() :
        nbdDev{"nbd2"}, mp{nbdDev, "tests/run/virtual-media/nbd2.sock", "",
                           Configuration::Mode::standard},
        mpsm{ioc, "Slot_2", mp}
    {
        mpsm.emitRegisterDBusEvent(DbusEnvironment::getBus(),
                                   DbusEnvironment::getObjServer());
    }

    Configuration::MountPoint& getMountPoint()
    {
        return mp;
    }

    void doMount(MethodCallback&& callback)
    {
        DbusEnvironment::getBus()->async_method_call(
            std::move(callback), DbusEnvironment::serviceName(), objectPath,
            serviceIface, "Mount", "https://placeholder.org/image.iso", false,
            std::variant<int, sdbusplus::message::unix_fd>{0});
    }

    void doUnmount(MethodCallback&& callback)
    {
        DbusEnvironment::getBus()->async_method_call(
            std::move(callback), DbusEnvironment::serviceName(), objectPath,
            serviceIface, "Unmount");
    }

    const std::string objectPath{
        "/xyz/openbmc_project/VirtualMedia/Standard/Slot_2"};
    const std::string serviceIface{"xyz.openbmc_project.VirtualMedia.Standard"};

  private:
    boost::asio::io_context ioc;
    NBDDevice<> nbdDev;
    Configuration::MountPoint mp;
    MountPointStateMachine mpsm;
};

template <typename MountPointScenario>
class InitialStateTest : public ::testing::Test
{
  public:
    MountPointScenario scenario;
};

using Scenarios = Types<ProxyMountPointScenario, StandardMountPointScenario>;

TYPED_TEST_SUITE(InitialStateTest, Scenarios);

TYPED_TEST(InitialStateTest, PropertiesAreInitialized)
{
    TypeParam& scenario = this->scenario;

    EXPECT_THAT(DbusEnvironment::getProperty<std::string>(
                    scenario.objectPath, mountPointIface, "Device"),
                Eq(scenario.getMountPoint().nbdDevice.toString()));
    EXPECT_THAT(DbusEnvironment::getProperty<std::string>(
                    scenario.objectPath, mountPointIface, "EndpointId"),
                Eq(scenario.getMountPoint().endPointId));
    EXPECT_THAT(DbusEnvironment::getProperty<std::string>(
                    scenario.objectPath, mountPointIface, "Socket"),
                Eq(scenario.getMountPoint().unixSocket));
    EXPECT_THAT(DbusEnvironment::getProperty<std::string>(
                    scenario.objectPath, mountPointIface, "ImageURL"),
                IsEmpty());
    EXPECT_THAT(DbusEnvironment::getProperty<bool>(
                    scenario.objectPath, mountPointIface, "WriteProtected"),
                IsTrue());
    EXPECT_THAT(DbusEnvironment::getProperty<uint64_t>(
                    scenario.objectPath, mountPointIface, "Timeout"),
                Eq(Configuration::MountPoint::timeout));

    EXPECT_THAT(DbusEnvironment::getProperty<bool>(scenario.objectPath,
                                                   processIface, "Active"),
                IsFalse());
    EXPECT_THAT(DbusEnvironment::getProperty<int>(scenario.objectPath,
                                                  processIface, "ExitCode"),
                Eq(-1));
}

TYPED_TEST(InitialStateTest, MountMethodIsInitialized)
{
    TypeParam& scenario = this->scenario;
    std::promise<bool> returnPromise;

    scenario.doMount(
        [&promise = returnPromise](const boost::system::error_code& ec,
                                   const bool result) {
        EXPECT_FALSE(ec);
        promise.set_value(result);
    });

    bool returnValue =
        DbusEnvironment::waitForFuture(returnPromise.get_future());
    EXPECT_FALSE(returnValue);
}

TYPED_TEST(InitialStateTest, UnmountMethodIsInitialized)
{
    TypeParam& scenario = this->scenario;
    std::promise<bool> returnPromise;

    scenario.doUnmount(
        [&promise = returnPromise](const boost::system::error_code& ec,
                                   const bool result) {
        EXPECT_FALSE(ec);
        promise.set_value(result);
    });

    bool returnValue =
        DbusEnvironment::waitForFuture(returnPromise.get_future());
    EXPECT_FALSE(returnValue);
}

} // namespace virtual_media_test

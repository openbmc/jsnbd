#include "events.hpp"
#include "interfaces/mount_point_state_machine.hpp"
#include "mocks/file_printer_mock.hpp"
#include "mocks/object_server_mock.hpp"
#include "state/basic_state.hpp"
#include "state_machine.hpp"
#include "state_machine_test_base.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/process/async_pipe.hpp>

#include <chrono>
#include <iterator>
#include <vector>

#include <gmock/gmock-nice-strict.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::An;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrEq;

class VirtualMediaDbBaseTest :
    public StateMachineTestBase,
    public ::testing::Test
{
  public:
    void prepareEnv(Configuration::Mode mode)
    {
        mpsm.config.mode = mode;
        objServer = std::make_shared<virtual_media_mock::object_server_mock>();
        if (mode == Configuration::Mode::proxy)
        {
            virtualMediaBaseObject =
                "/xyz/openbmc_project/VirtualMedia/Proxy/Slot_0";
            virtualMediaServiceInterface = objServer->backdoor.add_interface(
                virtualMediaBaseObject,
                "xyz.openbmc_project.VirtualMedia.Proxy");
        }
        else
        {
            virtualMediaBaseObject =
                "/xyz/openbmc_project/VirtualMedia/Legacy/Slot_0";
            virtualMediaServiceInterface = objServer->backdoor.add_interface(
                virtualMediaBaseObject,
                "xyz.openbmc_project.VirtualMedia.Legacy");
        }

        virtualMediaProcessInterface = objServer->backdoor.add_interface(
            virtualMediaBaseObject, "xyz.openbmc_project.VirtualMedia.Process");
        virtualMediaMountPointInterface = objServer->backdoor.add_interface(
            virtualMediaBaseObject,
            "xyz.openbmc_project.VirtualMedia.MountPoint");
    }

  protected:
    std::string virtualMediaBaseObject;
    std::shared_ptr<virtual_media_mock::object_server_mock> objServer;
    std::shared_ptr<virtual_media_mock::dbus_interface_mock>
        virtualMediaProcessInterface;
    std::shared_ptr<virtual_media_mock::dbus_interface_mock>
        virtualMediaMountPointInterface;
    std::shared_ptr<virtual_media_mock::dbus_interface_mock>
        virtualMediaServiceInterface;
    testing::NiceMock<MockFilePrinterEngine> printer;
};

TEST_F(VirtualMediaDbBaseTest, InitialStateBaseProxyTest)
{
    prepareEnv(Configuration::Mode::proxy);

    /* Set test pass conditions */
    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("Device"), An<std::string>()))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("EndpointId"), An<std::string>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("Socket"), An<std::string>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("ImageURL"), An<std::string>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("WriteProtected"), An<bool>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("Timeout"), An<int>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(
        *virtualMediaMountPointInterface,
        register_property(StrEq("RemainingInactivityTimeout"), An<int>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaProcessInterface,
                register_property(StrEq("Active"), An<bool>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaProcessInterface,
                register_property(StrEq("ExitCode"), An<int32_t>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface,
                register_signal(StrEq("Completion")))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface,
                register_method(StrEq("Unmount"),
                                An<virtual_media_mock::DefaultMethod&&>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface,
                register_method(StrEq("Mount"),
                                An<virtual_media_mock::DefaultMethod&&>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface, initialize())
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaProcessInterface, initialize())
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface, initialize())
        .Times(1)
        .WillRepeatedly(Return(true));

    /*Invoke constructor */
    mpsm.emitEvent(RegisterDbusEvent(mpsm.bus, objServer));
}

TEST_F(VirtualMediaDbBaseTest, InitialStateBaseLegacyTest)
{
    prepareEnv(Configuration::Mode::legacy);

    /* Set test pass conditions */
    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("Device"), An<std::string>()))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("EndpointId"), An<std::string>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("Socket"), An<std::string>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("ImageURL"), An<std::string>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("WriteProtected"), An<bool>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface,
                register_property(StrEq("Timeout"), An<int>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(
        *virtualMediaMountPointInterface,
        register_property(StrEq("RemainingInactivityTimeout"), An<int>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaProcessInterface,
                register_property(StrEq("Active"), An<bool>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaProcessInterface,
                register_property(StrEq("ExitCode"), An<int32_t>(), _, _))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface,
                register_signal(StrEq("Completion")))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface,
                register_method(StrEq("Unmount"),
                                An<virtual_media_mock::DefaultMethod&&>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface,
                register_method(StrEq("Mount"),
                                An<virtual_media_mock::LegacyMountMethod&&>()))
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaMountPointInterface, initialize())
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaProcessInterface, initialize())
        .Times(1)
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*virtualMediaServiceInterface, initialize())
        .Times(1)
        .WillRepeatedly(Return(true));

    /*Invoke constructor */
    mpsm.emitEvent(RegisterDbusEvent(mpsm.bus, objServer));
}

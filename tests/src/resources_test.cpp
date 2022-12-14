#include "mocks/file_printer_mock.hpp"
#include "mocks/mounter_test_dirs.hpp"
#include "mocks/test_states.hpp"
#include "resources.hpp"
#include "state_machine.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using namespace resource;

namespace fs = std::filesystem;

/* Mount tests */
class MountTest : public ::testing::Test
{
  protected:
    MountTest()
    {
        credentials =
            std::make_unique<utils::CredentialsProvider>("user", "1234");
    }

    void doMountAndUnmount(const fs::path& name)
    {
        auto mountDir = std::make_unique<Directory>(name);
        SmbShare smb(mountDir->getPath());
        mount = std::make_unique<Mount>(std::move(mountDir), smb,
                                        directories::remoteDir, true,
                                        std::move(credentials));
        mount = nullptr;
    }

    std::unique_ptr<utils::CredentialsProvider> credentials;
    std::unique_ptr<Mount> mount;
};

TEST_F(MountTest, MountFailed)
{
    EXPECT_THROW(doMountAndUnmount("Slot_2"), Error);
}

namespace utils
{
extern int result;
}

TEST_F(MountTest, UnmountSuccessful)
{
    EXPECT_NO_THROW(doMountAndUnmount("Slot_0"));

    EXPECT_EQ(utils::result, 0);
}

TEST_F(MountTest, UnmountFailed)
{
    EXPECT_NO_THROW(doMountAndUnmount("Slot_3"));

    EXPECT_EQ(utils::result, 1);
}

/* Gadget tests */
class GadgetTest : public ::testing::Test
{
  protected:
    GadgetTest()
    {
        MockFilePrinter::engine = &printer;
        const fs::path busPort = utils::GadgetDirs::busPrefix() + "/1";
        fs::create_directories(busPort);

        // move to TestStateI, as this specific state handles all events
        // differently
        std::unique_ptr<BasicState> newState =
            std::make_unique<TestStateI>(mpsm);
        mpsm.changeState(std::move(newState));
    }

    ~GadgetTest() override
    {
        fs::remove_all(utils::GadgetDirs::busPrefix());
    }

    void run(std::chrono::milliseconds timeout = std::chrono::milliseconds(5))
    {
        ioc.restart();
        ioc.run_for(timeout);
    }

    testing::NiceMock<MockFilePrinterEngine> printer;
    boost::asio::io_context ioc;
    DeviceMonitor monitor{ioc};
    Configuration::MountPoint mp{NBDDevice<>{"nbd0"},
                                 "/run/virtual-media/nbd0.sock",
                                 "/nbd/0",
                                 30,
                                 512,
                                 std::chrono::seconds(0),
                                 Configuration::Mode::proxy};
    MountPointStateMachine mpsm{ioc, monitor, "Slot_0", mp};
};

TEST_F(GadgetTest, GadgetRemovalSuccessful)
{
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    auto gadget = std::make_unique<Gadget>(mpsm, StateChange::inserted);
    gadget = nullptr;

    run();

    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateI");
}

TEST_F(GadgetTest, GadgetRemovalFailed)
{
    ASSERT_EQ(mpsm.getState().getStateName(), "TestStateI");

    // force some failure in gadget removal path
    EXPECT_CALL(printer, remove(_, _))
        .Times(AtLeast(1))
        .WillOnce(
            []([[maybe_unused]] const fs::path& path, std::error_code& ec) {
                ec = std::make_error_code(std::errc::no_such_file_or_directory);
            })
        .WillRepeatedly(Return());

    auto gadget = std::make_unique<Gadget>(mpsm, StateChange::inserted);
    gadget = nullptr;

    run();

    EXPECT_EQ(mpsm.getState().getStateName(), "TestStateH");
}

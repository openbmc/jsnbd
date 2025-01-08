#include "configuration.hpp"
#include "fakes/mounter_dirs_fake.hpp"
#include "fakes/mounter_fake.hpp"
#include "fakes/states_fake.hpp"
#include "mocks/file_printer_mock.hpp"
#include "resources.hpp"
#include "smb.hpp"
#include "state/basic_state.hpp"
#include "state_machine.hpp"
#include "system.hpp"
#include "utils/gadget_dirs.hpp"
#include "utils/utils.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace virtual_media_test
{

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
    EXPECT_THROW(doMountAndUnmount("Slot_1"), Error);
}

TEST_F(MountTest, UnmountSuccessful)
{
    EXPECT_NO_THROW(doMountAndUnmount("Slot_3"));

    EXPECT_EQ(MounterHelper::unmountResult, 0);
}

TEST_F(MountTest, UnmountFailed)
{
    EXPECT_NO_THROW(doMountAndUnmount("Slot_2"));

    EXPECT_EQ(MounterHelper::unmountResult, 1);
}

/* Gadget tests */
class GadgetTest : public ::testing::Test
{
  public:
    GadgetTest()
    {
        MockFilePrinter::engine = &printer;
        const fs::path busPort = ::utils::GadgetDirs::busPrefix() + "/1";
        fs::create_directories(busPort);

        // move to TestStateI, as this specific state handles all events
        // differently
        std::unique_ptr<BasicState> newState =
            std::make_unique<TestStateI>(mpsm);
        mpsm.changeState(std::move(newState));
    }

    ~GadgetTest() override
    {
        fs::remove_all(::utils::GadgetDirs::busPrefix());
    }

    GadgetTest(const GadgetTest&) = delete;
    GadgetTest(GadgetTest&&) = delete;
    GadgetTest& operator=(const GadgetTest&) = delete;
    GadgetTest& operator=(GadgetTest&&) = delete;

    void run(std::chrono::milliseconds timeout = std::chrono::milliseconds(5))
    {
        ioc.restart();
        ioc.run_for(timeout);
    }

    testing::NiceMock<MockFilePrinterEngine> printer;
    boost::asio::io_context ioc;
    DeviceMonitor monitor{ioc};
    Configuration::MountPoint mp{NBDDevice<>{"nbd0"},
                                 "/run/virtual-media/nbd0.sock", "/nbd/0",
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

} // namespace virtual_media_test

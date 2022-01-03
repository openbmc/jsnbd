#include "mocks/file_printer_mock.hpp"
#include "mocks/test_states.hpp"
#include "resources.hpp"
#include "state_machine.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using namespace resource;

namespace fs = std::filesystem;

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

    ~GadgetTest()
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

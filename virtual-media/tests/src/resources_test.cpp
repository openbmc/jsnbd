#include "configuration.hpp"
#include "fakes/states_fake.hpp"
#include "mocks/file_printer_mock.hpp"
#include "resources.hpp"
#include "state/basic_state.hpp"
#include "state_machine.hpp"
#include "system.hpp"
#include "utils/gadget_dirs.hpp"

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

/* Gadget tests */
class GadgetTest : public ::testing::Test
{
  public:
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

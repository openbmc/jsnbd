#include "mocks/utils_mock.hpp"
#include "utils/impl/dbus_notify_wrapper.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace virtual_media_test
{

using ::testing::_;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Ne;

using namespace ::utils;

/* NotificationWrapper tests */
class NotificationWrapperTest : public ::testing::Test
{
  protected:
    NotificationWrapperTest()
    {
        sender = std::make_unique<MockSignalSender>();
    }

    void initWrapper()
    {
        auto timer = std::make_unique<boost::asio::steady_timer>(ioc);

        wrapper = std::make_unique<DbusNotificationWrapper>(std::move(sender),
                                                            std::move(timer));

        ASSERT_EQ(wrapper->isStarted(), false);
    }

    void notify(const std::error_code& ec)
    {
        boost::asio::spawn(
            ioc,
            [this, ec](const boost::asio::yield_context& yield) {
                boost::asio::steady_timer tmr{ioc};
                boost::system::error_code ignoredEc;

                tmr.expires_after(std::chrono::milliseconds(500));
                tmr.async_wait(yield[ignoredEc]);

                wrapper->notify(ec);
            },
            boost::asio::detached);
    }

    void run(std::chrono::seconds timeout = std::chrono::seconds(1))
    {
        ioc.restart();
        ioc.run_for(timeout);
    }

    MockFunction<void(const boost::system::error_code&)> mockTimerCallback;
    boost::asio::io_context ioc;
    std::unique_ptr<DbusNotificationWrapper> wrapper;
    std::unique_ptr<MockSignalSender> sender;
};

TEST_F(NotificationWrapperTest, TimerCallsCallbackOnExpire)
{
    initWrapper();

    EXPECT_CALL(mockTimerCallback,
                Call(Ne(boost::asio::error::operation_aborted)))
        .Times(1);

    wrapper->start(mockTimerCallback.AsStdFunction(), std::chrono::seconds(1));
    run();

    EXPECT_EQ(wrapper->isStarted(), false);
}

TEST_F(NotificationWrapperTest, NotificationSuppression)
{
    EXPECT_CALL(*sender, send(_)).Times(0);

    initWrapper();

    wrapper->notify(std::error_code{});
}

TEST_F(NotificationWrapperTest, NotificationSentWithoutErrorCode)
{
    EXPECT_CALL(*sender, send(Eq(std::nullopt))).Times(1);
    EXPECT_CALL(mockTimerCallback,
                Call(Eq(boost::asio::error::operation_aborted)))
        .Times(1);

    initWrapper();

    wrapper->start(mockTimerCallback.AsStdFunction(), std::chrono::seconds(1));
    notify(std::error_code{});

    run();

    EXPECT_EQ(wrapper->isStarted(), false);
}

TEST_F(NotificationWrapperTest, NotificationSentWithErrorCode)
{
    const std::error_code notificationEc =
        std::make_error_code(std::errc::connection_refused);

    EXPECT_CALL(*sender, send(Eq(notificationEc))).Times(1);
    EXPECT_CALL(mockTimerCallback,
                Call(Eq(boost::asio::error::operation_aborted)))
        .Times(1);

    initWrapper();

    wrapper->start(mockTimerCallback.AsStdFunction(), std::chrono::seconds(1));
    notify(notificationEc);

    run();

    EXPECT_EQ(wrapper->isStarted(), false);
}

} // namespace virtual_media_test

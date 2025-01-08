#include "mocks/utils_mock.hpp"
#include "utils/impl/dbus_notify_wrapper.hpp"
#include "utils/utils.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <sdbusplus/exception.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
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
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Ne;
using ::testing::Not;

using namespace ::utils;

namespace fs = std::filesystem;

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

/* VolatileFile tests */
template <class File>
class TestableVolatileFile : public VolatileFile<File>
{
  public:
    using VolatileFile<File>::purgeFileContents;
    using VolatileFile<File>::create;

    explicit TestableVolatileFile(
        CredentialsProvider::SecureBuffer&& contents) :
        VolatileFile<File>(std::move(contents))
    {}
};

class VolatileFileTest : public ::testing::Test
{
  public:
    VolatileFileTest()
    {
        CredentialsProvider::SecureBuffer buffer{
            new CredentialsProvider::Buffer};
        std::copy(password.begin(), password.end(),
                  std::back_inserter(*buffer));

        file = std::make_unique<TestableVolatileFile<FileObject>>(
            std::move(buffer));
    }

    ~VolatileFileTest() override
    {
        for (const auto& entry :
             fs::directory_iterator(fs::temp_directory_path()))
        {
            std::string path = entry.path().c_str();
            if (path.find("VM-") != std::string::npos)
            {
                fs::remove(entry);
            }
        }
    }

    VolatileFileTest(const VolatileFileTest&) = delete;
    VolatileFileTest(VolatileFileTest&&) = delete;

    VolatileFileTest& operator=(const VolatileFileTest&) = delete;
    VolatileFileTest& operator=(VolatileFileTest&&) = delete;

    void SetUp() override
    {
        initialAssertion();
    }

    void initialAssertion() const
    {
        ASSERT_EQ(fs::exists(file->path()), true);
    }

    std::string readFile() const
    {
        std::ifstream ifs{file->path()};
        if (!ifs.is_open())
        {
            return "";
        }

        return std::string{std::istreambuf_iterator<char>(ifs),
                           std::istreambuf_iterator<char>()};
    }

    std::string password = "1234";
    std::unique_ptr<TestableVolatileFile<FileObject>> file;
};

TEST_F(VolatileFileTest, ContainsPassword)
{
    EXPECT_EQ(readFile(), password);
}

TEST_F(VolatileFileTest, PurgeFile)
{
    file->purgeFileContents();
    EXPECT_NE(readFile(), password);
}

TEST_F(VolatileFileTest, CleanUp)
{
    file = nullptr;

    for (const auto& entry : fs::directory_iterator(fs::temp_directory_path()))
    {
        EXPECT_THAT(entry.path().c_str(), Not(HasSubstr("VM-")));
    }
}

TEST_F(VolatileFileTest, WriteFailed)
{
    CredentialsProvider::SecureBuffer buffer{new CredentialsProvider::Buffer};
    std::copy(password.begin(), password.end(), std::back_inserter(*buffer));

    EXPECT_THROW(
        {
            TestableVolatileFile<FailableSecretFileObject> failingFile{
                std::move(std::move(buffer))};
        },
        sdbusplus::exception::SdBusError);
}

/*CredentialsProvider class test */

class CredentialsProviderTest : public ::testing::Test
{
  protected:
    CredentialsProvider provider{"User", "12,3"};
};

TEST_F(CredentialsProviderTest, UserLoaded)
{
    EXPECT_EQ(provider.user(), "User");
}

TEST_F(CredentialsProviderTest, PasswordLoaded)
{
    EXPECT_EQ(provider.password(), "12,3");
}

TEST_F(CredentialsProviderTest, CommasEscape)
{
    provider.escapeCommas();
    EXPECT_EQ(provider.password(), "12,,3");
    provider.escapeCommas();
    EXPECT_EQ(provider.password(), "12,,3");
}

TEST_F(CredentialsProviderTest, PackWithoutFormatter)
{
    auto buffer = provider.pack(nullptr);
    EXPECT_FALSE(buffer);
}

TEST_F(CredentialsProviderTest, PackWithFormatter)
{
    auto buffer =
        provider.pack([](const auto& user, const auto& pass, auto& buff) {
            std::copy(user.begin(), user.end(), std::back_inserter(buff));
            std::copy(pass.begin(), pass.end(), std::back_inserter(buff));
        });

    std::string str(buffer->begin(), buffer->end());
    EXPECT_NE(buffer->size(), 0);
    EXPECT_EQ(str, "User12,3");
}

TEST(SecureCleanupTests, SecureCleanupFunctionTest)
{
    std::array<int, 5> tab = {1, 2, 3, 4, 5};
    secureCleanup(tab);
    for (int i : tab)
    {
        EXPECT_EQ(i, 0);
    }
}

} // namespace virtual_media_test

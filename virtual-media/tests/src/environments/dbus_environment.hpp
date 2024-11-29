#pragma once

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace virtual_media_test
{

using Milliseconds = std::chrono::duration<uint64_t, std::milli>;

namespace utils
{

template <class T>
inline void setException(std::promise<T>& promise, const std::string& message)
{
    promise.set_exception(std::make_exception_ptr(std::runtime_error(message)));
}

} // namespace utils

class DbusEnvironment : public ::testing::Environment
{
  public:
    DbusEnvironment() = default;
    ~DbusEnvironment() override;

    DbusEnvironment(const DbusEnvironment&) = delete;
    DbusEnvironment(DbusEnvironment&&) = delete;

    DbusEnvironment& operator=(const DbusEnvironment&) = delete;
    DbusEnvironment& operator=(DbusEnvironment&&) = delete;

    void SetUp() override;
    void TearDown() override;

    static boost::asio::io_context& getIoc();
    static std::shared_ptr<sdbusplus::asio::connection> getBus();
    static std::shared_ptr<sdbusplus::asio::object_server> getObjServer();
    static const char* serviceName();
    static std::function<void()> setPromise(std::string_view name);
    static void sleepFor(Milliseconds timeout);
    static Milliseconds measureTime(const std::function<void()>& fun);

    static void synchronizeIoc()
    {
        while (ioc.poll() > 0)
        {}
    }

    template <class Functor>
    static void synchronizedPost(Functor&& functor)
    {
        boost::asio::post(ioc, std::forward<Functor>(functor));
        synchronizeIoc();
    }

    template <class T, class F>
    static T waitForFutures(std::vector<std::future<T>> futures, T init,
                            F&& accumulator,
                            Milliseconds timeout = std::chrono::seconds(10))
    {
        constexpr auto precission = Milliseconds(10);
        auto elapsed = Milliseconds(0);

        auto sum = std::move(init);
        for (auto& future : futures)
        {
            while (future.valid() && elapsed < timeout)
            {
                synchronizeIoc();

                if (future.wait_for(precission) == std::future_status::ready)
                {
                    sum = accumulator(sum, future.get());
                }
                else
                {
                    elapsed += precission;
                }
            }
        }

        if (elapsed >= timeout)
        {
            throw std::runtime_error("Timed out while waiting for future");
        }

        return sum;
    }

    template <class T>
    static T waitForFuture(std::future<T> future,
                           Milliseconds timeout = std::chrono::seconds(10))
    {
        std::vector<std::future<T>> futures;
        futures.emplace_back(std::move(future));

        return waitForFutures(
            std::move(futures), T{},
            [](const auto&, const auto& value) { return value; }, timeout);
    }

    static bool waitForFuture(std::string_view name,
                              Milliseconds timeout = std::chrono::seconds(10));

    static bool waitForFutures(std::string_view name,
                               Milliseconds timeout = std::chrono::seconds(10));

    template <class T>
    static T getProperty(const std::string& path,
                         const std::string& interfaceName,
                         const std::string& property)
    {
        auto propertyPromise = std::promise<T>();
        auto propertyFuture = propertyPromise.get_future();
        sdbusplus::asio::getProperty<T>(
            *DbusEnvironment::getBus(), DbusEnvironment::serviceName(), path,
            interfaceName, property,
            [&propertyPromise](const boost::system::error_code& ec,
                               const T& t) {
                if (ec)
                {
                    std::cout << "ec: " << ec.what() << std::endl;
                    utils::setException(propertyPromise, "GetProperty failed");
                    return;
                }
                propertyPromise.set_value(t);
            });
        return DbusEnvironment::waitForFuture(std::move(propertyFuture));
    }

    template <class T>
    static boost::system::error_code
        setProperty(const std::string& path, const std::string& interfaceName,
                    const std::string& property, const T& newValue)
    {
        auto promise = std::promise<boost::system::error_code>();
        auto future = promise.get_future();
        sdbusplus::asio::setProperty(
            *DbusEnvironment::getBus(), DbusEnvironment::serviceName(), path,
            interfaceName, property, std::move(newValue),
            [promise = std::move(promise)](
                boost::system::error_code ec) mutable {
                promise.set_value(ec);
            });
        return DbusEnvironment::waitForFuture(std::move(future));
    }

    template <class... Args>
    static boost::system::error_code
        callMethod(const std::string& path, const std::string& interface,
                   const std::string& method, Args&&... args)
    {
        auto promise = std::promise<boost::system::error_code>();
        auto future = promise.get_future();
        DbusEnvironment::getBus()->async_method_call(
            [promise = std::move(promise)](
                boost::system::error_code ec) mutable {
                promise.set_value(ec);
            },
            DbusEnvironment::serviceName(), path, interface, method,
            std::forward<Args>(args)...);
        return DbusEnvironment::waitForFuture(std::move(future));
    }

  private:
    static std::future<bool> getFuture(std::string_view name);

    static boost::asio::io_context ioc;
    static std::shared_ptr<sdbusplus::asio::connection> bus;
    static std::shared_ptr<sdbusplus::asio::object_server> objServer;
    static std::map<std::string, std::vector<std::future<bool>>> futures;
    static bool setUp;
};

} // namespace virtual_media_test

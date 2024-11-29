#include "dbus_environment.hpp"

#include <future>
#include <thread>

namespace virtual_media_test
{

DbusEnvironment::~DbusEnvironment()
{
    if (setUp)
    {
        setUp = false;

        objServer = nullptr;
        bus = nullptr;
    }
}

void DbusEnvironment::SetUp()
{
    if (!setUp)
    {
        setUp = true;

        bus = std::make_shared<sdbusplus::asio::connection>(ioc);
        bus->request_name(serviceName());

        objServer = std::make_unique<sdbusplus::asio::object_server>(bus);
    }
}

void DbusEnvironment::TearDown()
{
    ioc.poll();

    futures.clear();
}

boost::asio::io_context& DbusEnvironment::getIoc()
{
    return ioc;
}

std::shared_ptr<sdbusplus::asio::connection> DbusEnvironment::getBus()
{
    return bus;
}

std::shared_ptr<sdbusplus::asio::object_server> DbusEnvironment::getObjServer()
{
    return objServer;
}

const char* DbusEnvironment::serviceName()
{
    return "virtual-media.ut";
}

std::function<void()> DbusEnvironment::setPromise(std::string_view name)
{
    auto promise = std::make_shared<std::promise<bool>>();
    futures[std::string(name)].emplace_back(promise->get_future());
    return [p = std::move(promise)]() { p->set_value(true); };
}

bool DbusEnvironment::waitForFuture(std::string_view name, Milliseconds timeout)
{
    return waitForFuture(getFuture(name), timeout);
}

bool DbusEnvironment::waitForFutures(std::string_view name,
                                     Milliseconds timeout)
{
    auto& data = futures[std::string(name)];
    auto ret = waitForFutures(std::move(data), true, [](auto sum, auto val) {
        return sum && val;
    }, timeout);
    data = std::vector<std::future<bool>>{};
    return ret;
}

std::future<bool> DbusEnvironment::getFuture(std::string_view name)
{
    auto& data = futures[std::string(name)];
    auto it = data.begin();

    if (it != data.end())
    {
        auto result = std::move(*it);
        data.erase(it);
        return result;
    }

    return {};
}

void DbusEnvironment::sleepFor(Milliseconds timeout)
{
    auto end = std::chrono::high_resolution_clock::now() + timeout;

    while (std::chrono::high_resolution_clock::now() < end)
    {
        synchronizeIoc();
        std::this_thread::yield();
    }

    synchronizeIoc();
}

Milliseconds DbusEnvironment::measureTime(const std::function<void()>& fun)
{
    auto begin = std::chrono::high_resolution_clock::now();
    fun();
    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<Milliseconds>(end - begin);
}

boost::asio::io_context DbusEnvironment::ioc;
std::shared_ptr<sdbusplus::asio::connection> DbusEnvironment::bus;
std::shared_ptr<sdbusplus::asio::object_server> DbusEnvironment::objServer;
std::map<std::string, std::vector<std::future<bool>>> DbusEnvironment::futures;
bool DbusEnvironment::setUp = false;

} // namespace virtual_media_test

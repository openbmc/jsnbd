#include "configuration.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <filesystem>
#include <memory>
#include <system_error>

class App
{
  public:
    explicit App(boost::asio::io_context& ioc)
    {
        auto bus = std::make_shared<sdbusplus::asio::connection>(ioc);
        objServer = std::make_shared<sdbusplus::asio::object_server>(bus);
        objManager = std::make_shared<sdbusplus::server::manager_t>(
            *bus, "/xyz/openbmc_project/VirtualMedia");
        bus->request_name("xyz.openbmc_project.VirtualMedia");
    }

  private:
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<sdbusplus::server::manager_t> objManager;
};

int main()
{
    Configuration config;

    // setup secure ownership for newly created files (always succeeds)
    umask(077);

    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](const boost::system::error_code&, const int&) { ioc.stop(); });

    App app(ioc);

    ioc.run();

    return 0;
}

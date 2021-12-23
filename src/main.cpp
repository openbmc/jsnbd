#include "configuration.hpp"
#include "logger.hpp"
#include "state_machine.hpp"
#include "system.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>

std::chrono::seconds Configuration::inactivityTimeout;

class App
{
  public:
    App(boost::asio::io_context& ioc, const Configuration& config)
    {
        auto bus = std::make_shared<sdbusplus::asio::connection>(ioc);
        objServer = std::make_shared<sdbusplus::asio::object_server>(bus);
        bus->request_name("xyz.openbmc_project.VirtualMedia");
        objManager = std::make_shared<sdbusplus::server::manager_t>(
            *bus, "/xyz/openbmc_project/VirtualMedia");

        for (const auto& [name, entry] : config.mountPoints)
        {
            mpsm.emplace(name, std::make_shared<MountPointStateMachine>(
                                   ioc, name, entry));
        }
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<MountPointStateMachine>>
        mpsm;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<sdbusplus::server::manager_t> objManager;
};

int main()
{
    Configuration config("/etc/virtual-media/config.json");
    if (!config.valid)
    {
        return -1;
    }

    // setup secure ownership for newly created files (always succeeds)
    umask(Configuration::defaultUmask);

    // Create directory with limited access rights to hold sockets
    std::error_code ec;
    std::filesystem::create_directories("/run/virtual-media/sock", ec);
    if (ec)
    {
        LogMsg(Logger::Error,
               "Cannot create secure directory for sockets: ", ec.message());
        return -1;
    }

    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](const boost::system::error_code&, const int&) { ioc.stop(); });

    App app(ioc, config);

    ioc.run();

    return 0;
}

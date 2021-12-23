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
    App(boost::asio::io_context& ioc, const Configuration& config) :
        devMonitor(ioc)
    {
        auto bus = std::make_shared<sdbusplus::asio::connection>(ioc);
        objServer = std::make_shared<sdbusplus::asio::object_server>(bus);
        bus->request_name("xyz.openbmc_project.VirtualMedia");
        objManager = std::make_shared<sdbusplus::server::manager_t>(
            *bus, "/xyz/openbmc_project/VirtualMedia");

        for (const auto& [name, entry] : config.mountPoints)
        {
            mpsm.emplace(name, std::make_shared<MountPointStateMachine>(
                                   ioc, devMonitor, name, entry));
            mpsm[name]->emitRegisterDBusEvent(bus, objServer);
        }

        devMonitor.run([this](const NBDDevice& device, StateChange change) {
            for (auto& [name, entry] : mpsm)
            {
                entry->emitUdevStateChangeEvent(device, change);
            }
        });
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<MountPointStateMachine>>
        mpsm;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<sdbusplus::server::manager_t> objManager;
    DeviceMonitor devMonitor;
};

int main()
{
    Configuration config("/usr/share/virtual-media/config.json");
    if (!config.valid)
    {
        return -1;
    }

    // setup secure ownership for newly created files (always succeeds)
    umask(Configuration::defaultUmask);

    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](const boost::system::error_code&, const int&) { ioc.stop(); });

    App app(ioc, config);

    ioc.run();

    return 0;
}

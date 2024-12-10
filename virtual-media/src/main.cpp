#include "configuration.hpp"
#include "state_machine.hpp"
#include "system.hpp"
#include "utils/log-wrapper.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/server/manager.hpp>

#include <csignal>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

class App
{
  public:
    App(boost::asio::io_context& ioc, const Configuration& config) :
        bus(std::make_shared<sdbusplus::asio::connection>(ioc)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(bus)),
        objManager(*bus, "/xyz/openbmc_project/VirtualMedia"), devMonitor(ioc)
    {
        bus->request_name("xyz.openbmc_project.VirtualMedia");

        for (const auto& [name, entry] : config.mountPoints)
        {
            mpsm.emplace(name, std::make_shared<MountPointStateMachine>(
                                   ioc, devMonitor, name, entry));
            mpsm[name]->emitRegisterDBusEvent(bus, objServer);
        }

        devMonitor.run([this](const NBDDevice<>& device, StateChange change) {
            for (auto& [name, entry] : mpsm)
            {
                entry->emitUdevStateChangeEvent(device, change);
            }
        });
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<MountPointStateMachine>>
        mpsm;
    std::shared_ptr<sdbusplus::asio::connection> bus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    sdbusplus::server::manager_t objManager;
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
    static constexpr mode_t defaultUmask = 077;
    umask(defaultUmask);

    try
    {
        boost::asio::io_context ioc;
        App app(ioc, config);

        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
            [&ioc](const boost::system::error_code& ec, const int signal) {
            if (ec)
            {
                LOGGER_ERROR("Error while waiting for signals: {}", ec);
                if (ec == boost::asio::error::operation_aborted)
                {
                    ioc.stop();
                }

                return;
            }

            LOGGER_INFO("Received signal {}", signal);
            ioc.stop();
        });

        ioc.run();

        return 0;
    }
    catch (const std::exception& e)
    {
        LOGGER_CRITICAL("Error thrown to main: {}", e.what());
        return -1;
    }
    catch (...)
    {
        LOGGER_CRITICAL("Error thrown to main.");
        return -1;
    }
}

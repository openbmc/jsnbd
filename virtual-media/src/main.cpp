#include "utils/log-wrapper.hpp"

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

class App
{
  public:
    explicit App(boost::asio::io_context& ioc) :
        bus(std::make_shared<sdbusplus::asio::connection>(ioc)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(bus)),
        objManager(*bus, "/xyz/openbmc_project/VirtualMedia")
    {
        bus->request_name("xyz.openbmc_project.VirtualMedia");
    }

  private:
    std::shared_ptr<sdbusplus::asio::connection> bus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    sdbusplus::server::manager_t objManager;
};

int main()
{
    try
    {
        boost::asio::io_context ioc;
        App app(ioc);

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

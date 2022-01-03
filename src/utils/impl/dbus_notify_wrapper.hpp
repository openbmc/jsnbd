#pragma once

#include "logger.hpp"
#include "utils/utils.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <memory>
#include <optional>
#include <string>

namespace utils
{

class DbusSignalSender : public SignalSender
{
  public:
    DbusSignalSender(std::shared_ptr<sdbusplus::asio::connection> con,
                     const std::string& obj, const std::string& iface,
                     const std::string& name) :
        con(con),
        interface(iface), object(obj), name(name)
    {}

    DbusSignalSender() = delete;
    DbusSignalSender(const DbusSignalSender&) = delete;
    DbusSignalSender& operator=(const DbusSignalSender&) = delete;

    void send(std::optional<const std::error_code> status) override
    {
        auto msgSignal =
            con->new_signal(object.c_str(), interface.c_str(), name.c_str());

        msgSignal.append(status.value_or(std::error_code{}).value());
        LogMsg(Logger::Debug, "Sending signal: Object: ", object,
               ", Interface: ", interface, ", Name: ", name,
               "Status: ", status.value_or(std::error_code{}).value());
        msgSignal.signal_send();
    }

  private:
    std::shared_ptr<sdbusplus::asio::connection> con;
    std::string interface;
    std::string object;
    std::string name;
};

class DbusNotificationWrapper : public NotificationWrapper
{
  public:
    DbusNotificationWrapper(std::unique_ptr<SignalSender> signal,
                            std::unique_ptr<boost::asio::steady_timer> timer) :
        signal(std::move(signal)),
        timer(std::move(timer))
    {}

    void start(std::function<void(const boost::system::error_code&)> handler,
               const std::chrono::seconds& duration) override
    {
        LogMsg(Logger::Debug, "Notification initiated");
        started = true;
        timer->expires_from_now(duration);
        timer->async_wait([this, handler{std::move(handler)}](
                              const boost::system::error_code& ec) {
            started = false;
            handler(ec);
        });
    }

    void notify(const std::error_code& ec) override
    {
        if (started)
        {
            timer->cancel();
            if (ec)
            {
                signal->send((ec));
            }
            else
            {
                signal->send(std::nullopt);
            }
            started = false;
            return;
        }
        LogMsg(Logger::Debug, "Notification(ec) supressed (not started)");
    }

    const bool& isStarted()
    {
        return started;
    }

  private:
    std::unique_ptr<SignalSender> signal;
    std::unique_ptr<boost::asio::steady_timer> timer;
    bool started{false};
};

} // namespace utils

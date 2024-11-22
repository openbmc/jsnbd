#pragma once

#include <boost/system/error_code.hpp>
#include <phosphor-logging/log.hpp>

#include <format>
#include <source_location>
#include <sstream>
#include <string_view>

static constexpr auto kCurrentLogLevel = phosphor::logging::level::DEBUG;

template <>
struct std::formatter<boost::system::error_code>
{
    static constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const boost::system::error_code& ec, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}", ec.what());
    }
};

template <phosphor::logging::level level, typename... Args>
inline void vlog(std::format_string<Args...>&& format, Args&&... args,
                 const std::source_location& loc) noexcept
{
    if constexpr (kCurrentLogLevel < level)
    {
        return;
    }

    std::string_view filename = loc.file_name();
    filename = filename.substr(filename.rfind('/'));
    if (!filename.empty())
    {
        filename.remove_prefix(1);
    }

    std::ostringstream msgStream;
    msgStream << std::format("[{}:{}] ", filename, loc.line())
              << std::format(std::move(format), std::forward<Args>(args)...);
    phosphor::logging::log<level>(msgStream.str().c_str());
}

template <typename... Args>
struct LOGGER_CRITICAL
{
    // This needs to be capable for implicit conversions. It could become
    // bothersome to create object in explicit manner whenever we'd like to log
    // something. (This applies to all structures)
    // NOLINTNEXTLINE(google-explicit-constructor)
    LOGGER_CRITICAL(std::format_string<Args...> format, Args&&... args,
                    const std::source_location& loc =
                        std::source_location::current()) noexcept
    {
        vlog<phosphor::logging::level::CRIT, Args...>(
            std::move(format), std::forward<Args>(args)..., loc);
    }
};

template <typename... Args>
struct LOGGER_ERROR
{
    // NOLINTNEXTLINE(google-explicit-constructor)
    LOGGER_ERROR(std::format_string<Args...> format, Args&&... args,
                 const std::source_location& loc =
                     std::source_location::current()) noexcept
    {
        vlog<phosphor::logging::level::ERR, Args...>(
            std::move(format), std::forward<Args>(args)..., loc);
    }
};

template <typename... Args>
struct LOGGER_WARNING
{
    // NOLINTNEXTLINE(google-explicit-constructor)
    LOGGER_WARNING(std::format_string<Args...> format, Args&&... args,
                   const std::source_location& loc =
                       std::source_location::current()) noexcept
    {
        vlog<phosphor::logging::level::WARNING, Args...>(
            std::move(format), std::forward<Args>(args)..., loc);
    }
};

template <typename... Args>
struct LOGGER_INFO
{
    // NOLINTNEXTLINE(google-explicit-constructor)
    LOGGER_INFO(std::format_string<Args...> format, Args&&... args,
                const std::source_location& loc =
                    std::source_location::current()) noexcept
    {
        vlog<phosphor::logging::level::INFO, Args...>(
            std::move(format), std::forward<Args>(args)..., loc);
    }
};

template <typename... Args>
struct LOGGER_DEBUG
{
    // NOLINTNEXTLINE(google-explicit-constructor)
    LOGGER_DEBUG(std::format_string<Args...> format, Args&&... args,
                 const std::source_location& loc =
                     std::source_location::current()) noexcept
    {
        vlog<phosphor::logging::level::DEBUG, Args...>(
            std::move(format), std::forward<Args>(args)..., loc);
    }
};

template <typename... Args>
LOGGER_CRITICAL(std::format_string<Args...>,
                Args&&...) -> LOGGER_CRITICAL<Args...>;

template <typename... Args>
LOGGER_ERROR(std::format_string<Args...>, Args&&...) -> LOGGER_ERROR<Args...>;

template <typename... Args>
LOGGER_WARNING(std::format_string<Args...>,
               Args&&...) -> LOGGER_WARNING<Args...>;

template <typename... Args>
LOGGER_INFO(std::format_string<Args...>, Args&&...) -> LOGGER_INFO<Args...>;

template <typename... Args>
LOGGER_DEBUG(std::format_string<Args...>, Args&&...) -> LOGGER_DEBUG<Args...>;

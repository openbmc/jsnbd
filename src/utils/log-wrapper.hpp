#pragma once

#include <boost/system/error_code.hpp>
#include <phosphor-logging/log.hpp>

#include <format>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string_view>

static constexpr const auto kCurrentLogLevel = phosphor::logging::level::DEBUG;

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

struct FormatString
{
    std::string_view str;
    std::source_location loc;

    // This needs to be capable for implicit conversions. It could become
    // bothersome to create FormatString object whenever we'd like to log
    // something.
    // NOLINTNEXTLINE(google-explicit-constructor)
    FormatString(const char* stringIn, const std::source_location& locIn =
                                           std::source_location::current()) :
        str(stringIn), loc(locIn)
    {}
};

template <phosphor::logging::level level>
inline void vlog(const FormatString& format, std::format_args&& args)
{
    if constexpr (kCurrentLogLevel < level)
    {
        return;
    }

    std::string_view filename = format.loc.file_name();
    filename = filename.substr(filename.rfind('/') + 1);

    std::ostringstream msgStream;
    msgStream << std::format("[{}:{}] ", filename, format.loc.line())
              << std::vformat(format.str, args) << std::endl;
    phosphor::logging::log<level>(msgStream.str().c_str());
}

template <typename... Args>
inline void LOGGER_CRITICAL(const FormatString& format, Args&&... args)
{
    vlog<phosphor::logging::level::CRIT>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void LOGGER_ERROR(const FormatString& format, Args&&... args)
{
    vlog<phosphor::logging::level::ERR>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void LOGGER_WARNING(const FormatString& format, Args&&... args)
{
    vlog<phosphor::logging::level::WARNING>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void LOGGER_INFO(const FormatString& format, Args&&... args)
{
    vlog<phosphor::logging::level::INFO>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void LOGGER_DEBUG(const FormatString& format, Args&&... args)
{
    vlog<phosphor::logging::level::DEBUG>(
        format, std::make_format_args(std::forward<Args>(args)...));
}
